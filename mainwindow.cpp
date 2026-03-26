#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMediaMetaData>
#include <QDir>
#include <QCoreApplication>
#include <QResizeEvent>
#include <QScrollBar>
#include <QMouseEvent>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QFileDialog>
#include <QScrollArea>
#include <QListWidget>
#include <QWindow>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QStandardPaths>
#include <QDateTime>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QMessageBox>
#include <QCheckBox>
// #include "songunit.h"  <-- 这行已删除

/** @brief 根据是否有歌曲启用/禁用播放相关控件，列表按钮始终可用；空列表时复位播放按钮图标。 */
void MainWindow::updatePlaybackControlsEnabled(bool enabled)
{
    if (!ui) return;

    if (ui->prevButton) ui->prevButton->setEnabled(enabled);
    if (ui->playButton) ui->playButton->setEnabled(enabled);
    if (ui->nextButton) ui->nextButton->setEnabled(enabled);
    if (ui->modeButton) ui->modeButton->setEnabled(enabled);
    if (ui->Slider) ui->Slider->setEnabled(enabled);

    // 列表按钮始终可用（用于“空列表时也能打开列表窗口看占位”）
    if (ui->listButton) ui->listButton->setEnabled(true);

    // 空列表时把播放按钮状态复位到“播放”
    if (!enabled && ui->playButton) {
        InitButtonIcon(ui->playButton, ":/res/play.png");
    }
}

/** @brief 显示或隐藏“当前没有加载的音乐”占位标签，显示时置顶。 */
void MainWindow::updateEmptyOverlayVisible(bool visible)
{
    if (!m_emptyOverlayLabel) return;
    m_emptyOverlayLabel->setVisible(visible);
    if (visible) {
        m_emptyOverlayLabel->raise();
    }
}

/** @brief 从资源 :/style.qss 读取样式表并应用到主窗口。 */
void MainWindow::loadStyleSheet()
{
    QFile styleFile(":/style.qss");

    if (styleFile.open(QIODevice::ReadOnly)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        this->setStyleSheet(styleSheet);
        styleFile.close();
    }
}

/** @brief 初始化主窗口：标题、大小、无边框、样式、封面尺寸、空列表 overlay、进度条与窗口状态、事件过滤器。 */
void MainWindow::InitWindow()
{
    setWindowTitle("MusicPlayer");
    this->resize(1235, 833);

    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    // 加载样式表
    loadStyleSheet();

    ui->imagelabel->setFixedSize(300, 300);
    ui->imagelabel->setScaledContents(true);
    {
        QSettings settings("misaka", "MusicPlayer");
        QString coverPath = settings.value("DefaultCover", ":/res/misaka.png").toString();
        ui->imagelabel->setPixmap(QPixmap(coverPath));
    }

    // 设置右键菜单策略
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->imagelabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &MainWindow::showBackgroundContextMenu);
    connect(ui->imagelabel, &QWidget::customContextMenuRequested, this, &MainWindow::showCoverContextMenu);

    // 空列表占位 overlay（不拦截鼠标事件，避免挡住按钮）
    m_emptyOverlayLabel = new QLabel(this);
    m_emptyOverlayLabel->setText(QStringLiteral("当前没有加载的音乐"));
    m_emptyOverlayLabel->setAlignment(Qt::AlignCenter);
    m_emptyOverlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_emptyOverlayLabel->setStyleSheet(
        "QLabel{"
        "color: rgba(255,255,255,200);"
        "background-color: rgba(0,0,0,120);"
        "border-radius: 12px;"
        "padding: 14px 22px;"
        "font-size: 18px;"
        "}"
    );
    m_emptyOverlayLabel->hide();

    m_sliderPressed = false;
    m_ignoreSliderUpdate = false;
    m_pendingSeek = -1;
    m_isDragging = false;
    m_lastLrcIndex = -1; // 初始化歌词索引
    updateBackground();  // 初始加载背景图
    InitTrayIcon();      // 初始化系统托盘

    m_moremenuwindow = new MoreMenu(this);
    m_moremenuwindow->hide();
    connect(m_moremenuwindow, &MoreMenu::addMusicClicked, this, &MainWindow::onAddMusicFromMoreMenu);
    connect(m_moremenuwindow, &MoreMenu::setMusicDirClicked, this, &MainWindow::onSetMusicDirClicked);

    // 初始化窗口调整大小相关变量
    m_isResizing = false;
    m_resizeEdge = NoEdge;
    
    // 启用鼠标追踪，确保即使不按下鼠标，悬停在边缘时也能改变光标样式
    this->setMouseTracking(true);
    if (ui->centralwidget) {
        ui->centralwidget->setMouseTracking(true);
    }

    ui->Slider->installEventFilter(this);
    this->installEventFilter(this);
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

void MainWindow::onAddMusicFromMoreMenu()
{
    if (!m_playerController) return;

    const QString filter = QStringLiteral("Audio Files (*.mp3 *.wav *.flac *.aac *.ogg *.m4a *.wma);;All Files (*.*)");
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择音乐文件"), QString(), filter);
    if (files.isEmpty()) return;

    m_playerController->AddLocalFiles(files);
}

void MainWindow::onSetMusicDirClicked()
{
    QSettings settings("misaka", "MusicPlayer");
    QString defaultMusicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/MusicPlayer";
    QString currentDir = settings.value("MusicDir", defaultMusicPath).toString();

    QString newDir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择默认音乐目录"), currentDir);
    if (!newDir.isEmpty()) {
        settings.setValue("MusicDir", newDir);
        // Note: Changing dir at runtime may require reloading or restarting
        // For simplicity, we just save it. The user can restart or add music manually.
    }
}

/** @brief 设置各按钮图标并连接信号：模式切换、上一首/下一首、播放/暂停、列表、最小化/最大化/关闭。 */
void MainWindow::InitButtons()
{
    InitButtonIcon(ui->prevButton, ":/res/prev song.png");
    InitButtonIcon(ui->playButton, ":/res/play.png");
    InitButtonIcon(ui->nextButton, ":/res/next song.png");
    InitButtonIcon(ui->modeButton, ":/res/list play.png");
    InitButtonIcon(ui->listButton, ":/res/playlist.png");
    InitButtonIcon(ui->minimizeButton, ":/res/Minimize.png", Qt::black);
    InitButtonIcon(ui->maximizeButton, ":/res/Maximize.png", Qt::black);
    InitButtonIcon(ui->closeButton, ":/res/close.png", Qt::black);
    InitButtonIcon(ui->moreButton, ":/res/more.png");

    // 禁用所有按钮的焦点，防止空格键意外触发上次点击的按钮
    ui->prevButton->setFocusPolicy(Qt::NoFocus);
    ui->playButton->setFocusPolicy(Qt::NoFocus);
    ui->nextButton->setFocusPolicy(Qt::NoFocus);
    ui->modeButton->setFocusPolicy(Qt::NoFocus);
    ui->listButton->setFocusPolicy(Qt::NoFocus);
    ui->minimizeButton->setFocusPolicy(Qt::NoFocus);
    ui->maximizeButton->setFocusPolicy(Qt::NoFocus);
    ui->closeButton->setFocusPolicy(Qt::NoFocus);
    ui->volumeSlider->setFocusPolicy(Qt::NoFocus);

    // 初始化音量状态：默认 100% (1.0)
    ui->volumeSlider->setValue(100);
    if (m_playerController && m_playerController->GetAudioOutput()) {
        m_playerController->GetAudioOutput()->setVolume(1.0);
    }

    connect(ui->volumeSlider, &QSlider::valueChanged, this, [this](int value){
        if (m_playerController && m_playerController->GetAudioOutput()) {
            // 使用 qreal 确保精度，并映射到 0.0 - 1.0 范围
            qreal volume = qBound(0.0, static_cast<qreal>(value) / 100.0, 1.0);
            m_playerController->GetAudioOutput()->setVolume(volume);
        }
    });

    connect(ui->modeButton, &QPushButton::clicked, this, [this](){

        if (!m_playerController) return;
        nextmode mode = m_playerController->GetPlayMode();
        if (mode == List_Play) { m_playerController->SetPlayMode(Repeat_Play); InitButtonIcon(ui->modeButton, ":/res/repeat play.png"); }
        else if (mode == Loop_Play) { m_playerController->SetPlayMode(List_Play); InitButtonIcon(ui->modeButton, ":/res/list play.png"); }
        else { m_playerController->SetPlayMode(Loop_Play); InitButtonIcon(ui->modeButton, ":/res/loop play.png"); }
    });
    connect(ui->prevButton, &QPushButton::clicked, this, [this](){ if (m_playerController) m_playerController->PlayPrevSong(); });
    connect(ui->nextButton, &QPushButton::clicked, this, [this](){ if (m_playerController) m_playerController->PlayNextSong(); });
    connect(ui->playButton, &QPushButton::clicked, this, [this](){
        if (!m_playerController) return;
        QMediaPlayer *player = m_playerController->GetPlayer();
        if (player->isPlaying()) { player->pause(); InitButtonIcon(ui->playButton, ":/res/play.png"); }
        else { player->play(); InitButtonIcon(ui->playButton, ":/res/stop.png"); }
    });
    connect(ui->listButton, &QPushButton::clicked, this, [this](){ togglePlaylist(); });
    connect(ui->minimizeButton, &QPushButton::clicked, this, [this](){ showMinimized(); });
    connect(ui->maximizeButton, &QPushButton::clicked, this, [this](){
        if (isMaximized()) { showNormal(); InitButtonIcon(ui->maximizeButton, ":/res/Maximize.png"); }
        else { showMaximized(); InitButtonIcon(ui->maximizeButton, ":/res/Windowed.png"); }
    });
    connect(ui->closeButton, &QPushButton::clicked, this, [this](){ close(); });
    connect(ui->moreButton, &QPushButton::clicked, this, &MainWindow::moremenubuttonclick);
}

/** @brief 设置按钮固定 30x30 及图标与图标尺寸。支持可选的图标着色。 */
void MainWindow::InitButtonIcon(QPushButton *button, const QString & path, const QColor & color)
{
    button->setFixedSize(30, 30);
    QPixmap pixmap(path);
    if (color != Qt::transparent) {
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
    }
    button->setIcon(QIcon(pixmap));
    button->setIconSize(QSize(button->width(), button->height()));
}

/** @brief 创建 MusicList 目录（若不存在）、MusicPlaylist 控件，更新位置并交给 PlayerController 初始化。 */
void MainWindow::InitPlayList()
{
    QSettings settings("misaka", "MusicPlayer");
    QString defaultMusicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/MusicPlayer";
    QString musicListPath = settings.value("MusicDir", defaultMusicPath).toString();

    QDir dir;
    if (!dir.exists(musicListPath))
        dir.mkpath(musicListPath);

    m_musicplaylist = new MusicPlaylist(this);
    m_musicplaylist->hide();
    UpdateMusicListPosition();
    if (m_playerController) {
        m_playerController->InitPlayList(m_musicplaylist);
    }
}

/** @brief 创建 LrcParser、连接 positionChanged、设置歌词列表滚动条与滚轮定时器、点击槽、事件过滤器。 */
void MainWindow::InitLrcParser()
{
    m_lrcParser = new LrcParser(this);
    if (m_playerController)
        connect(m_playerController->GetPlayer(), &QMediaPlayer::positionChanged, this, &MainWindow::onPositionChanged);

    ui->lyricsListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->lyricsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_wheelTimer = new QTimer(this);
    m_wheelTimer->setSingleShot(true);
    m_wheelTimer->setInterval(3000);
    connect(m_wheelTimer, &QTimer::timeout, this, &MainWindow::onWheelTimerTimeout);
    connect(ui->lyricsListWidget, &QListWidget::clicked, this, &MainWindow::onLyricsListWidgetClicked);
    ui->lyricsListWidget->installEventFilter(this);
    ui->lyricsListWidget->viewport()->installEventFilter(this);
    m_manualScroll = false;
    m_lastLrcIndex = -1;
}

/** @brief 根据当前窗口宽高计算播放列表目标位置与高度并设置。 */
void MainWindow::UpdateMusicListPosition()
{
    int window_width = this->width();
    int window_height = this->height();
    int target_x = window_width - 390;
    int target_h = window_height - 300;
    const QPoint targetPos(target_x, 100);
    if (m_musicplaylist) {
        m_musicplaylist->setFixedHeight(target_h);
        m_musicplaylist->setTargetPos(targetPos);
    }
}

/** @brief 若播放列表可见则带动画隐藏，否则先更新位置再带动画显示。 */
void MainWindow::togglePlaylist()
{
    if (!m_musicplaylist) return;

    if (m_musicplaylist->isVisible()) {
        m_musicplaylist->hideAnimated();
    } else {
        UpdateMusicListPosition();
        m_musicplaylist->showAnimated();
    }
}

/** @brief 若播放列表当前可见则带动画隐藏。 */
void MainWindow::hidePlaylistIfVisible()
{
    if (!m_musicplaylist) return;
    if (!m_musicplaylist->isVisible()) return;
    m_musicplaylist->hideAnimated();
}

/** @brief 构造：setupUi、InitWindow、创建 PlayerController、连接可用性信号、InitButtons/PlayList/LrcParser、连接播放器与列表信号、初始化 overlay 与控件状态。 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    InitWindow();
    m_playerController = new PlayerController(this);

    // 播放列表可用性变化：统一更新 overlay 与控件可用性
    connect(m_playerController, &PlayerController::playlistAvailabilityChanged, this, [this](bool hasSongs) {
        updateEmptyOverlayVisible(!hasSongs);
        updatePlaybackControlsEnabled(hasSongs);
    });

    InitButtons();
    InitPlayList();
    InitLrcParser();

    QMediaPlayer *player = m_playerController->GetPlayer();

    // 音乐准备完毕信号槽
    connect(player, &QMediaPlayer::mediaStatusChanged, this, &MainWindow::StatusChanged);

    // 音乐播放结束信号槽
    connect(player, &QMediaPlayer::playbackStateChanged, this, &MainWindow::StateChange);

    // 进度条相关槽函数
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::updateSliderPosition);
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::updateSliderRange);

    // 使用音乐播放列表选择播放音乐
    connect(m_musicplaylist, &MusicPlaylist::ChooseMusicpass, m_playerController, &PlayerController::OnChooseMusic);

    // 初始化一次空/非空状态（避免错过 InitPlayList 内部 emit）
    const bool hasSongs = (m_musicplaylist && !m_musicplaylist->isempty());
    updateEmptyOverlayVisible(!hasSongs);
    updatePlaybackControlsEnabled(hasSongs);
}

/** @brief 槽：播放时长变化时设置进度条范围，根据 duration 启用/禁用。 */
void MainWindow::updateSliderRange(qint64 duration)
{
    ui->Slider->setRange(0, static_cast<int>(duration));
    ui->Slider->setEnabled(duration > 0);
}

/** @brief 根据窗口宽度设置歌词列表控件高度（宽的一半）。 */
void MainWindow::updatalyricsListWidget()
{
    int window_width = this->width();
    ui->lyricsListWidget->setFixedHeight(window_width / 2);
}

/** @brief 槽：播放位置变化时同步到进度条；拖动或 ignore 窗口内不更新。限制每 100ms 更新一次。 */
void MainWindow::updateSliderPosition(qint64 position)
{
    if (m_sliderPressed || m_ignoreSliderUpdate) return;
    
    // 节流：每 100ms 更新一次进度条 UI
    static qint64 lastUpdate = 0;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - lastUpdate < 100) return;
    lastUpdate = currentTime;

    ui->Slider->setValue(static_cast<int>(position));
}

/** 占位槽：进度条 seek 在 eventFilter 的 MouseButtonRelease 中统一处理。 */
void MainWindow::onProgressSliderMoved(int value)
{
    Q_UNUSED(value);
}

/** @brief 析构：释放 UI。 */
MainWindow::~MainWindow()
{
    delete ui;
}

/** @brief 根据音乐路径查找同目录同名 .lrc，解析后填充歌词列表或显示“无歌词”；重置手动滚动并停止滚轮定时器。 */
void MainWindow::loadLyrics(const QString &musicFilePath)
{
    QFileInfo info(musicFilePath);
    QString lrcPath = info.absolutePath() + "/" + info.completeBaseName() + ".lrc";

    bool ok = m_lrcParser->parseFile(lrcPath);
    m_lyrics = m_lrcParser->lyrics();   // 同步歌词数据

    ui->lyricsListWidget->clear();
    if (ok && !m_lyrics.isEmpty()) {
        // 有歌词：填充真实歌词行
        for (const auto &line : m_lyrics) {
            ui->lyricsListWidget->addItem(line.text);
        }
        // 添加空白行，便于最后几行居中
        for (int i = 0; i < 5; ++i) {
            ui->lyricsListWidget->addItem("");
        }
        // 重置手动滚动标志，并居中当前行
        m_manualScroll = false;
        if (m_playerController) {
            qint64 pos = m_playerController->GetPlayer()->position();
            int idx = m_lrcParser->currentIndex(pos);
            if (idx >= 0 && idx < m_lyrics.size()) {
                ui->lyricsListWidget->scrollToItem(ui->lyricsListWidget->item(idx), QAbstractItemView::PositionAtCenter);
            }
        }
    } else {
        // 无歌词：显示占位项
        QListWidgetItem *noLrcItem = new QListWidgetItem("无歌词");
        QFont font = noLrcItem->font();
        font.setItalic(true);
        noLrcItem->setFont(font);
        noLrcItem->setForeground(QColor(128, 128, 128));
        noLrcItem->setTextAlignment(Qt::AlignCenter);
        ui->lyricsListWidget->addItem(noLrcItem);
        m_manualScroll = false;   // 同样重置手动滚动标志
    }

    // 停止可能正在运行的滚轮定时器
    m_wheelTimer->stop();
}

/** @brief 槽：播放位置变化时高亮对应歌词行，非手动滚动时自动居中。 */
void MainWindow::onPositionChanged(qint64 position)
{
    if (m_lyrics.isEmpty()) return;

    int index = m_lrcParser->currentIndex(position);
    if (index >= 0 && index < m_lyrics.size()) {
        // 增量更新：只有当行号发生变化时才更新 UI
        if (index == m_lastLrcIndex) return;
        
        m_lastLrcIndex = index;

        // 清除所有选中状态
        for (int i = 0; i < ui->lyricsListWidget->count(); ++i) {
            ui->lyricsListWidget->item(i)->setSelected(false);
        }
        // 高亮当前行
        ui->lyricsListWidget->item(index)->setSelected(true);

        // 只有当用户没有手动滚动时，才自动居中
        if (!m_manualScroll) {
            ui->lyricsListWidget->scrollToItem(ui->lyricsListWidget->item(index), QAbstractItemView::PositionAtCenter);
        }
    } else if (index == -1 && m_lastLrcIndex != -1) {
        // 如果位置回到开头或超出范围，清除状态
        m_lastLrcIndex = -1;
        for (int i = 0; i < ui->lyricsListWidget->count(); ++i) {
            ui->lyricsListWidget->item(i)->setSelected(false);
        }
    }
}

/** @brief 槽：点击某行歌词时跳转到对应时间并短暂忽略 positionChanged，居中该行。 */
void MainWindow::onLyricsListWidgetClicked(QModelIndex index)
{
    if (index.row() >= 0 && index.row() < m_lyrics.size()) {
        // 获取点击行对应的时间
        qint64 time = m_lyrics[index.row()].time;
        // 设置播放器位置
        if (m_playerController) {
            QMediaPlayer *player = m_playerController->GetPlayer();
            player->setPosition(time);
            
            // 主动 seek 之后，短时间内忽略播放器发来的 positionChanged，避免旧位置把滑块“拉回去”
            m_ignoreSliderUpdate = true;
            QTimer::singleShot(150, this, [this]() {
                m_ignoreSliderUpdate = false;
            });
        }
        
        // 点击后自动居中显示当前行
        m_manualScroll = false;
        ui->lyricsListWidget->scrollToItem(ui->lyricsListWidget->item(index.row()), QAbstractItemView::PositionAtCenter);
    }
}

/** @brief 槽：滚轮定时器超时后恢复自动跟随，将歌词列表滚动到当前播放行居中。 */
void MainWindow::onWheelTimerTimeout()
{
    m_manualScroll = false;
    if (!m_lyrics.isEmpty() && m_playerController) {
        qint64 position = m_playerController->GetPlayer()->position();
        int index = m_lrcParser->currentIndex(position);
        if (index >= 0 && index < m_lyrics.size()) {
            ui->lyricsListWidget->scrollToItem(ui->lyricsListWidget->item(index), QAbstractItemView::PositionAtCenter);
        }
    }
}

/** @brief 应用媒体加载前记录的待 seek 进度，短暂忽略 positionChanged 后清除 m_pendingSeek。 */
void MainWindow::applyPendingSeek()
{
    if (m_pendingSeek < 0 || !m_playerController) return;
    m_playerController->GetPlayer()->setPosition(m_pendingSeek);
    m_ignoreSliderUpdate = true;
    QTimer::singleShot(150, this, [this]() { m_ignoreSliderUpdate = false; });
    m_pendingSeek = -1;
}

/** 媒体状态变化：LoadedMedia 时更新元数据并应用 pendingSeek；EndOfMedia 且未拖动时自动下一首。 */
void MainWindow::StatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadingMedia:
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::InvalidMedia:
        break;
    case QMediaPlayer::LoadedMedia:
        UpdateMetadata();
        applyPendingSeek();
        break;
    case QMediaPlayer::BufferedMedia:
        applyPendingSeek();
        break;
    case QMediaPlayer::EndOfMedia:
        if (!m_sliderPressed) {
            m_playerController->SetAutoPlay(true);
            MusicEnd();
        }
        break;
    }
}

/** @brief 从当前播放器读取元数据，更新封面/标题/艺术家标签及歌词列表。 */
void MainWindow::UpdateMetadata()
{
    if (!m_playerController) return;
    QMediaMetaData metaData = m_playerController->GetPlayer()->metaData();

    MarqueeLabel *artistlabel = ui->Controlwidget->findChild<MarqueeLabel*>("artistlabel");
    MarqueeLabel *namelabel = ui->Controlwidget->findChild<MarqueeLabel*>("namelabel");

    bool image_flag = true;

    // 设置默认文本
    if (namelabel) namelabel->setText("未知曲目");
    ui->songnamelabel->setText("未知曲目");
    if (artistlabel) artistlabel->setText("未知艺术家");

    for (auto &&[key, value] : metaData.asKeyValueRange())
    {
        if(key == QMediaMetaData::ThumbnailImage)
        {
            image_flag = false;
            ui->imagelabel->setPixmap(QPixmap::fromImage(value.value<QImage>()));
        }
        else if(key == QMediaMetaData::ContributingArtist)
        {
            if (artistlabel) artistlabel->setText(value.toString());
        }
        else if(key == QMediaMetaData::Title)
        {
            if (namelabel) namelabel->setText(value.toString());
            ui->songnamelabel->setText(value.toString());
        }
    }

    if(image_flag) {
        QSettings settings("misaka", "MusicPlayer");
        QString coverPath = settings.value("DefaultCover", ":/res/misaka.png").toString();
        ui->imagelabel->setPixmap(QPixmap(coverPath));
    }

    // 设置歌词：根据当前播放索引加载对应 .lrc 文件
    if (!m_musicplaylist) return;

    int currentIndex = m_playerController->GetCurrentIndex();
    if (currentIndex < 0 || currentIndex >= m_musicplaylist->Getsize()) {
        return;
    }

    QString filePath = m_musicplaylist->Geturl(currentIndex).toLocalFile();
    if (!filePath.isEmpty()) {
        loadLyrics(filePath);
    }
}

/** 播放状态变化（预留，可按需更新 UI）。 */
void MainWindow::StateChange(QMediaPlayer::PlaybackState state)
{
    Q_UNUSED(state);
}

/** @brief 当前曲目结束，通知 PlayerController 切下一首。 */
void MainWindow::MusicEnd()
{
    if (m_playerController) {
        m_playerController->MusicEnd();
    }
}

/** @brief 加载并缓存背景图，避免在 paintEvent 中频繁读取磁盘。 */
void MainWindow::updateBackground()
{
    QSettings settings("misaka", "MusicPlayer");
    QString bgPath = settings.value("BackgroundImage", ":/res/background_dark.png").toString();
    m_backgroundPixmap = QPixmap(bgPath);
    if (m_backgroundPixmap.isNull()) {
        // 如果加载失败，可以设置一个默认颜色或尝试加载默认背景
        m_backgroundPixmap = QPixmap(":/res/background_dark.png");
    }
    this->update(); // 触发重绘
}

/** @brief 自定义绘制窗口的圆角背景，以避免使用 setMask 带来的锯齿问题。 */
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // 只有在需要高质量抗锯齿时才开启，背景图绘制通常不需要
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(rect(), 20, 20);
    painter.setClipPath(path);

    if (!m_backgroundPixmap.isNull()) {
        painter.drawPixmap(rect(), m_backgroundPixmap);
    } else {
        painter.fillPath(path, QColor(33, 33, 41)); // Fallback color
    }
}

/** @brief 窗口大小变化时重绘圆角遮罩、更新空列表 overlay 几何、播放列表位置与歌词列表高度。 */
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_emptyOverlayLabel) {
        // 让 overlay 始终覆盖窗口区域，文本自然居中
        m_emptyOverlayLabel->setGeometry(this->rect());
        m_emptyOverlayLabel->raise();
    }

    UpdateMusicListPosition();
    updatalyricsListWidget();
}

/** @brief 事件过滤：播放列表外点击隐藏、进度条按下/释放 seek、歌词滚轮、窗口拖拽与边缘缩放。 */
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 播放列表弹出后：点击主窗口其它区域自动隐藏
    if (event->type() == QEvent::MouseButtonPress && m_musicplaylist && m_musicplaylist->isVisible()) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            const QPoint globalPos = mouseEvent->globalPosition().toPoint();

            const QRect playlistGlobalRect(
                m_musicplaylist->mapToGlobal(QPoint(0, 0)),
                m_musicplaylist->size()
            );

            // 点击在播放列表内部：不隐藏
            if (playlistGlobalRect.contains(globalPos)) {
                return QMainWindow::eventFilter(obj, event);
            }

            // 点击在 listButton：交给按钮的 togglePlaylist 逻辑处理
            if (ui && ui->listButton) {
                const QRect listBtnGlobalRect(
                    ui->listButton->mapToGlobal(QPoint(0, 0)),
                    ui->listButton->size()
                );
                if (listBtnGlobalRect.contains(globalPos)) {
                    return QMainWindow::eventFilter(obj, event);
                }
            }

            // 其它任意位置：隐藏播放列表（带动画）
            hidePlaylistIfVisible();
        }
    }

    // 更多菜单弹出后：点击主窗口其它区域自动隐藏
    if (event->type() == QEvent::MouseButtonPress && m_moremenuwindow && m_moremenuwindow->isVisible()) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            const QPoint globalPos = mouseEvent->globalPosition().toPoint();

            const QRect menuGlobalRect(
                m_moremenuwindow->mapToGlobal(QPoint(0, 0)),
                m_moremenuwindow->size()
            );

            // 点击在菜单内部：不隐藏
            if (menuGlobalRect.contains(globalPos)) {
                return QMainWindow::eventFilter(obj, event);
            }

            // 点击在 moreButton：交给按钮的 moremenubuttonclick 逻辑处理
            if (ui && ui->moreButton) {
                const QRect moreBtnGlobalRect(
                    ui->moreButton->mapToGlobal(QPoint(0, 0)),
                    ui->moreButton->size()
                );
                if (moreBtnGlobalRect.contains(globalPos)) {
                    return QMainWindow::eventFilter(obj, event);
                }
            }

            // 其它任意位置：隐藏菜单
            m_moremenuwindow->hide();
        }
    }

    if (obj == ui->Slider) {
        // 鼠标按下：开始一次拖动/点选，不立刻改变播放进度
        if (event->type() == QEvent::MouseButtonPress)
            m_sliderPressed = true;
        if (event->type() == QEvent::MouseButtonRelease && m_sliderPressed) {
            QSlider *slider = ui->Slider;

            m_sliderPressed = false;

            int value = slider->value(); // 使用当前滑块位置，不再手动计算

            if (m_playerController) {
                QMediaPlayer *player = m_playerController->GetPlayer();
                QMediaPlayer::MediaStatus status = player->mediaStatus();

                // 如果媒体还在加载中，先记录一个待应用的进度，等 LoadedMedia/BufferedMedia 再真正 seek
                if (status == QMediaPlayer::LoadingMedia ||
                    status == QMediaPlayer::NoMedia)
                {
                    m_pendingSeek = static_cast<qint64>(value);
                }
                else
                {
                    player->setPosition(static_cast<qint64>(value));

                    // 主动 seek 之后，短时间内忽略播放器发来的 positionChanged，避免旧位置把滑块“拉回去”
                    m_ignoreSliderUpdate = true;
                    QTimer::singleShot(150, this, [this]() {
                        m_ignoreSliderUpdate = false;
                    });
                }
            }

            // 如果释放位置在末尾，视为用户主动把歌拖到末尾，此时再切到下一首
            if (value >= slider->maximum())
                MusicEnd();
        }
    }

    if ((obj == ui->lyricsListWidget || obj == ui->lyricsListWidget->viewport())
        && event->type() == QEvent::Wheel) {
        m_manualScroll = true;
        m_wheelTimer->start();
        return false;   // 允许事件继续传递给 QListWidget，使其能够滚动
    }

    // 全局处理窗口拖动和调整大小
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease) {
        QWidget *widget = qobject_cast<QWidget*>(obj);
        // 只处理属于当前主窗口及其子控件的鼠标事件
        if (widget && widget->window() == this) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint posInWindow = this->mapFromGlobal(mouseEvent->globalPosition().toPoint());

            if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
                // 排除按钮、滑块、列表等交互控件
                if (!qobject_cast<QPushButton*>(obj) && !qobject_cast<QSlider*>(obj) && !qobject_cast<QListWidget*>(obj) && !qobject_cast<QScrollArea*>(obj)) {
                    Edge edge = getEdge(posInWindow);
                    if (edge != NoEdge) {
                        // 边缘缩放（适配 Wayland 和 X11）
                        if (QWindow *window = this->windowHandle()) {
                            Qt::Edges qtEdges;
                            if (edge & LeftEdge) qtEdges |= Qt::LeftEdge;
                            if (edge & RightEdge) qtEdges |= Qt::RightEdge;
                            if (edge & TopEdge) qtEdges |= Qt::TopEdge;
                            if (edge & BottomEdge) qtEdges |= Qt::BottomEdge;
                            window->startSystemResize(qtEdges);
                            return true;
                        }
                    } else if (posInWindow.y() < 150 || widget == ui->centralwidget || qobject_cast<QLabel*>(obj)) {
                        // 扩大拖拽区域：只要是在顶部 150px 或者是点击到了背景、文本标签（如标题）就执行拖动
                        if (QWindow *window = this->windowHandle()) {
                            window->startSystemMove();
                            return true;
                        }
                    }
                }
            } else if (event->type() == QEvent::MouseMove) {
                if (!(mouseEvent->buttons() & Qt::LeftButton)) {
                    // 当没有拖拽时，悬浮在边缘改变光标
                    if (!qobject_cast<QPushButton*>(obj) && !qobject_cast<QSlider*>(obj)) {
                        Edge edge = getEdge(posInWindow);
                        updateCursor(edge);
                    }
                }
            }
        }
    }

    // 处理键盘控制
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        Qt::Key key = static_cast<Qt::Key>(keyEvent->key());
        if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Space || 
            key == Qt::Key_Up || key == Qt::Key_Down) {
            this->keyPressEvent(keyEvent);
            return true;
        }
    }

    // 其他事件交给父类处理
    return QMainWindow::eventFilter(obj, event);
}

/** @brief 键盘按下事件：实现方向键快进/后退、切换歌曲及空格键播放/暂停。单次按下 5s，长按 1s 步进。 */
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        if (m_playerController) {
            QMediaPlayer *player = m_playerController->GetPlayer();
            if (player->isPlaying()) {
                player->pause();
                InitButtonIcon(ui->playButton, ":/res/play.png");
            } else if (player->playbackState() == QMediaPlayer::PausedState || player->playbackState() == QMediaPlayer::StoppedState) {
                player->play();
                InitButtonIcon(ui->playButton, ":/res/stop.png");
            }
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Up) {
        if (m_playerController) m_playerController->PlayPrevSong();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Down) {
        if (m_playerController) m_playerController->PlayNextSong();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        if (!m_playerController || m_playerController->GetCurrentIndex() < 0) {
            QMainWindow::keyPressEvent(event);
            return;
        }

        QMediaPlayer *player = m_playerController->GetPlayer();
        if (!player) return;

        qint64 duration = player->duration();
        qint64 currentPos = player->position();
        
        // 如果正在手动拖动进度条，不处理键盘 seek
        if (m_sliderPressed) {
            event->ignore();
            return;
        }

        // 长按时 1s 步进，单次按下 5s 步进
        qint64 step = event->isAutoRepeat() ? 1000 : 5000;
        
        if (event->key() == Qt::Key_Left) {
            qint64 newPos = qMax<qint64>(0, currentPos - step);
            player->setPosition(newPos);
            ui->Slider->setValue(static_cast<int>(newPos));
        } else {
            qint64 newPos = qMin<qint64>(duration, currentPos + step);
            player->setPosition(newPos);
            ui->Slider->setValue(static_cast<int>(newPos));
            
            // 如果快进到末尾，触发切歌
            if (duration > 0 && newPos >= duration) {
                MusicEnd();
            }
        }

        // 短时间内忽略 positionChanged 信号，防止滑块跳回
        m_ignoreSliderUpdate = true;
        QTimer::singleShot(150, this, [this]() {
            m_ignoreSliderUpdate = false;
        });

        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

/** @brief 根据鼠标在窗口内的位置返回可拖拽边缘（左/右/上/下及其组合）。 */
MainWindow::Edge MainWindow::getEdge(const QPoint &pos)
{
    Edge edge = NoEdge;
    int x = pos.x();
    int y = pos.y();
    
    // 获取窗口几何形状
    QRect rect = this->rect();
    
    // 检查左右边缘
    if (x <= m_resizeBorderWidth) {
        edge = LeftEdge;
    } else if (x >= rect.width() - m_resizeBorderWidth) {
        edge = RightEdge;
    }
    
    // 检查上下边缘
    if (y <= m_resizeBorderWidth) {
        edge = static_cast<Edge>(edge | TopEdge);
    } else if (y >= rect.height() - m_resizeBorderWidth) {
        edge = static_cast<Edge>(edge | BottomEdge);
    }
    
    return edge;
}

/** @brief 根据边缘类型设置窗口光标（水平/垂直/对角箭头）。 */
void MainWindow::updateCursor(Edge edge)
{
    Qt::CursorShape cursor = Qt::ArrowCursor;
    
    switch (edge) {
    case LeftEdge:
    case RightEdge:
        cursor = Qt::SizeHorCursor;
        break;
    case TopEdge:
    case BottomEdge:
        cursor = Qt::SizeVerCursor;
        break;
    case TopLeftEdge:
    case BottomRightEdge:
        cursor = Qt::SizeFDiagCursor;
        break;
    case TopRightEdge:
    case BottomLeftEdge:
        cursor = Qt::SizeBDiagCursor;
        break;
    default:
        cursor = Qt::ArrowCursor;
        break;
    }
    
    this->setCursor(cursor);
}

/** @brief 开始调整大小时记录边缘、鼠标位置与当前窗口几何。 */
void MainWindow::startResize(Edge edge, const QPoint &pos)
{
    m_isResizing = true;
    m_resizeEdge = edge;
    m_resizeStartPos = pos;
    m_resizeStartGeometry = this->geometry();
}

/** @brief 根据当前鼠标位置与起始几何、边缘计算新几何并 setGeometry，保证不小于最小尺寸。 */
void MainWindow::performResize(const QPoint &pos)
{
    if (!m_isResizing) return;
    
    QRect geometry = m_resizeStartGeometry;
    QPoint globalPos = this->mapToGlobal(pos);
    QPoint globalStartPos = this->mapToGlobal(m_resizeStartPos);
    int deltaX = globalPos.x() - globalStartPos.x();
    int deltaY = globalPos.y() - globalStartPos.y();
    
    // 根据边缘调整几何形状
    if (m_resizeEdge & LeftEdge) {
        geometry.setLeft(geometry.left() + deltaX);
    }
    if (m_resizeEdge & RightEdge) {
        geometry.setRight(geometry.right() + deltaX);
    }
    if (m_resizeEdge & TopEdge) {
        geometry.setTop(geometry.top() + deltaY);
    }
    if (m_resizeEdge & BottomEdge) {
        geometry.setBottom(geometry.bottom() + deltaY);
    }
    
    // 确保窗口有最小尺寸
    if (geometry.width() < minimumWidth() || geometry.height() < minimumHeight()) {
        return;
    }
    
    this->setGeometry(geometry);
    
    // 更新起始位置为当前位置，用于连续调整
    m_resizeStartPos = pos;
    m_resizeStartGeometry = geometry;
}

/** @brief 槽：当按下moreButton按钮时触发。 */
void MainWindow::moremenubuttonclick()
{
    if(m_moremenuwindow)
    {
        if(m_moremenuwindow->isVisible())
        {
            m_moremenuwindow->hide();
        }
        else
        {
            int target_x = ui->moreButton->x() + ui->Controlwidget->x() + ui->toolWidget->x();
            int target_y = ui->moreButton->y() + ui->Controlwidget->y() + ui->toolWidget->y() - 50;
            m_moremenuwindow->move(target_x, target_y);
            m_moremenuwindow->show();
        }
    }
}

void MainWindow::showBackgroundContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);
    QAction *changeBgAction = new QAction("更换背景图片", this);
    contextMenu.addAction(changeBgAction);

    connect(changeBgAction, &QAction::triggered, this, [this]() {
        QString filter = QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif)");
        QString newPath = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), filter);
        if (!newPath.isEmpty()) {
            QSettings settings("misaka", "MusicPlayer");
            settings.setValue("BackgroundImage", newPath);
            updateBackground(); // 加载新背景并重绘
        }
    });

    contextMenu.exec(this->mapToGlobal(pos));
}

void MainWindow::showCoverContextMenu(const QPoint &pos)
{
    QMenu contextMenu(ui->imagelabel);
    QAction *changeCoverAction = new QAction("更换默认封面图片", this);
    contextMenu.addAction(changeCoverAction);

    connect(changeCoverAction, &QAction::triggered, this, [this]() {
        QString filter = QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif)");
        QString newPath = QFileDialog::getOpenFileName(this, "选择封面图片", QString(), filter);
        if (!newPath.isEmpty()) {
            QSettings settings("misaka", "MusicPlayer");
            settings.setValue("DefaultCover", newPath);
            // Immediately update if there's no custom metadata cover currently playing
            UpdateMetadata();
        }
    });

    contextMenu.exec(ui->imagelabel->mapToGlobal(pos));
}

/** @brief 初始化系统托盘图标与菜单。 */
void MainWindow::InitTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/res/misaka.png"));
    m_trayIcon->setToolTip("MusicPlayer");

    m_trayMenu = new QMenu(this);
    
    QAction *playAction = new QAction("播放/暂停", this);
    connect(playAction, &QAction::triggered, this, [this]() {
        if (m_playerController) {
            QMediaPlayer *player = m_playerController->GetPlayer();
            if (player->isPlaying()) player->pause();
            else player->play();
        }
    });

    QAction *prevAction = new QAction("上一首", this);
    connect(prevAction, &QAction::triggered, this, [this]() {
        if (m_playerController) m_playerController->PlayPrevSong();
    });

    QAction *nextAction = new QAction("下一首", this);
    connect(nextAction, &QAction::triggered, this, [this]() {
        if (m_playerController) m_playerController->PlayNextSong();
    });

    QAction *showAction = new QAction("显示主界面", this);
    connect(showAction, &QAction::triggered, this, &MainWindow::showNormal);

    QAction *quitAction = new QAction("退出", this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_trayMenu->addAction(playAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(prevAction);
    m_trayMenu->addAction(nextAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(showAction);
    m_trayMenu->addAction(quitAction);

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (isVisible()) hide();
            else {
                showNormal();
                raise();
                activateWindow();
            }
        }
    });

    m_trayIcon->show();
}

#include <QVBoxLayout>
#include <QHBoxLayout>

/** @brief 自定义退出确认对话框，采用与主窗口一致的 UI 风格。 */
class ExitConfirmDialog : public QDialog {
public:
    bool remember = false;
    int choice = 0; // 1: minimize, 2: exit

    ExitConfirmDialog(QWidget *parent) : QDialog(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(360, 220);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(30, 25, 30, 25);
        layout->setSpacing(15);

        QLabel *titleLabel = new QLabel("退出确认", this);
        titleLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);

        QLabel *msgLabel = new QLabel("您想要如何处理该窗口？", this);
        msgLabel->setStyleSheet("color: rgba(255,255,255,200); font-size: 14px;");
        msgLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(msgLabel);

        m_rememberCheckBox = new QCheckBox("记住我的选择，以后不再提示", this);
        m_rememberCheckBox->setStyleSheet(
            "QCheckBox { color: rgba(255,255,255,160); font-size: 13px; }"
            "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid rgba(255,255,255,60); }"
            "QCheckBox::indicator:checked { background-color: #2E86AB; border: 1px solid #2E86AB; }"
        );
        layout->addWidget(m_rememberCheckBox);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(12);

        QPushButton *minBtn = new QPushButton("最小化到托盘", this);
        QPushButton *exitBtn = new QPushButton("退出程序", this);
        QPushButton *cancelBtn = new QPushButton("取消", this);

        QString btnStyle = 
            "QPushButton { background-color: rgba(255,255,255,15); color: white; border-radius: 6px; padding: 8px 12px; font-size: 13px; }"
            "QPushButton:hover { background-color: rgba(255,255,255,25); }"
            "QPushButton:pressed { background-color: rgba(255,255,255,10); }";
        
        minBtn->setStyleSheet(btnStyle);
        exitBtn->setStyleSheet(btnStyle + "QPushButton { background-color: rgba(255,80,80,40); } QPushButton:hover { background-color: rgba(255,80,80,60); }");
        cancelBtn->setStyleSheet(btnStyle);

        btnLayout->addWidget(minBtn);
        btnLayout->addWidget(exitBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);

        connect(minBtn, &QPushButton::clicked, this, [this](){ choice = 1; remember = m_rememberCheckBox->isChecked(); accept(); });
        connect(exitBtn, &QPushButton::clicked, this, [this](){ choice = 2; remember = m_rememberCheckBox->isChecked(); accept(); });
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(33, 33, 41));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect(), 20, 20);
        
        // 绘制一条精致的边框
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
        painter.drawRoundedRect(rect().adjusted(1,1,-1,-1), 20, 20);
    }

private:
    QCheckBox *m_rememberCheckBox;
};

/** @brief 重写关闭事件，实现关闭或最小化到托盘的询问逻辑。 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings("misaka", "MusicPlayer");
    bool remember = settings.value("CloseBehaviorRemember", false).toBool();
    int behavior = settings.value("CloseBehavior", 0).toInt(); // 0: ask, 1: minimize, 2: exit

    if (remember) {
        if (behavior == 1) {
            hide();
            event->ignore();
            return;
        } else if (behavior == 2) {
            qApp->quit();
            return;
        }
    }

    // 弹出自定义美化对话框
    ExitConfirmDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        if (dialog.remember) {
            settings.setValue("CloseBehaviorRemember", true);
            settings.setValue("CloseBehavior", dialog.choice);
        }
        
        if (dialog.choice == 1) {
            hide();
            event->ignore();
        } else {
            qApp->quit(); // 彻底退出
        }
    } else {
        event->ignore();
    }
}
