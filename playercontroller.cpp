#include "playercontroller.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QRandomGenerator>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>

namespace {
bool isSupportedAudioFile(const QString& filePath)
{
    static const QStringList audioSuffixes = {"mp3", "wav", "flac", "aac", "ogg", "m4a", "wma"};
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return audioSuffixes.contains(suffix);
}
} // namespace

/**
 * @brief PlayerController 构造函数
 *
 * 这里只做播放器相关对象的基础创建与绑定；
 * 播放列表和文件扫描在 InitPlayList 中延迟完成，以避免构造阶段做太多工作。
 */
PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_playnum(0)
    , m_musicplaylist(nullptr)
    , m_pool(nullptr)
    , m_autoplay(false)
    , m_nextmode(List_Play)
    , m_shuffleIndex(0)
    , m_playToken(0)
{
    m_player->setAudioOutput(m_audioOutput);

    // 部分音频在 play() 后会停留在 0ms 不前进（直到发生一次 seek），这里做一次“卡住检测”自动唤醒。
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state != QMediaPlayer::PlayingState) return;

        const int token = ++m_playToken;
        QTimer::singleShot(200, this, [this, token]() {
            if (token != m_playToken) return; // 已经切换了歌曲/状态，丢弃
            if (m_player->playbackState() != QMediaPlayer::PlayingState) return;
            if (m_player->mediaStatus() != QMediaPlayer::LoadedMedia &&
                m_player->mediaStatus() != QMediaPlayer::BufferedMedia &&
                m_player->mediaStatus() != QMediaPlayer::StalledMedia)
            {
                return;
            }

            // 如果已经开始前进就不处理；否则轻微 seek 1ms 触发解码/时钟启动
            if (m_player->position() == 0 && m_player->duration() > 0) {
                m_player->setPosition(1);
            }
        });
    });
}

/**
 * @brief 初始化媒体元数据解析对象池
 *
 * - 创建 MediaPlayerPool，并限制最大并发数（当前为 4）
 * - 将 taskFinished / taskFailed 信号转发为对 MusicPlaylist 的 UI 更新与调试输出
 */
void PlayerController::InitPool()
{
    // 创建对象池（最大并发数设为4）
    m_pool = new MediaPlayerPool(4, this);

    // 连接任务完成信号：更新播放列表对应项
    connect(m_pool, &MediaPlayerPool::taskFinished, this, [this](int taskId, const QPixmap &cover, const QString &title, const QString &artist) {
        if (!m_musicplaylist) return;
        if (taskId >= 0 && taskId < m_musicplaylist->Getsize()) {
            QPixmap finalCover = cover;
            if (finalCover.isNull()) {
                finalCover = QPixmap(":/res/misaka.png");
            }
            m_musicplaylist->updateItem(taskId, finalCover, title, artist);

            const QUrl url = m_musicplaylist->Geturl(taskId);
            if (url.isValid()) {
                m_store.load();
                m_store.markMetadata(url.toString(), finalCover, title, artist);
                m_store.saveAtomic();
            }
        }
    });

    connect(m_pool, &MediaPlayerPool::taskFailed, this, [this](int taskId, const QString &error) {
        Q_UNUSED(taskId);
        qDebug() << "任务失败:" << error;
    });
}

/**
 * @brief 初始化播放列表，并与外部的 MusicPlaylist 视图绑定
 *
 * 该函数会：
 *  - 记录外部传入的 MusicPlaylist 指针（不负责释放）
 *  - 确保 MediaPlayerPool 已经初始化
 *  - 扫描应用目录下的 MusicList 文件夹，以支持的后缀过滤音频文件
 *  - 向 MusicPlaylist 追加 SongUnit 占位项，并为每个文件提交一个元数据解析任务
 *  - 在存在歌曲时，为 QMediaPlayer 设置初始播放源和音量
 */
void PlayerController::InitPlayList(MusicPlaylist *playlist)
{
    m_musicplaylist = playlist;
    if (!m_musicplaylist) {
        emit playlistAvailabilityChanged(false);
        return;
    }

    if (!m_pool) {
        InitPool();
    }

    m_musicplaylist->clearSongs();
    m_urlToIndex.clear();

    int index = 0;

    const QPixmap defaultCover(":/res/misaka.png");

    m_store.load();
    const auto tracks = m_store.tracks();

    for (const auto& t : tracks) {
        const QString urlString = t.url;
        if (urlString.isEmpty()) continue;

        const QUrl url(urlString);
        if (!url.isValid()) continue;

        if (url.isLocalFile()) {
            const QString localPath = url.toLocalFile();
            if (!QFileInfo::exists(localPath)) {
                continue; // 不破坏用户数据：文件不存在先跳过
            }
        }

        if (t.hasMetadata) {
            QPixmap cover = m_store.loadCoverForTrack(t);
            if (cover.isNull()) cover = defaultCover;

            QString title = t.title;
            if (title.isEmpty()) title = "未知曲目";
            QString artist = t.artist;
            if (artist.isEmpty()) artist = "未知艺术家";

            m_musicplaylist->AppendMusic(cover, url, title, artist);
        } else {
            m_musicplaylist->AppendMusic(defaultCover, url, "加载中", "加载中");
            if (m_pool) {
                m_pool->addTask(url, index);
            }
        }

        m_urlToIndex[url] = index;
        ++index;
    }

    // 若没有 playlist.json（或为空），保持兼容：扫描 MusicList 并写入 playlist.json（hasMetadata=false）
    if (index == 0) {
        QSettings settings("misaka", "MusicPlayer");
        QString defaultMusicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/MusicPlayer";
        const QString musicListPath = settings.value("MusicDir", defaultMusicPath).toString();

        QDir dir;
        if (!dir.exists(musicListPath)) {
            dir.mkpath(musicListPath);
        }

        QDir musicDir(musicListPath);
        const QFileInfoList allFiles = musicDir.entryInfoList(QDir::Files);

        bool playlistChanged = false;
        for (const QFileInfo& fileInfo : allFiles) {
            const QString absPath = fileInfo.absoluteFilePath();
            if (!isSupportedAudioFile(absPath)) continue;

            const QUrl url = QUrl::fromLocalFile(absPath);
            m_musicplaylist->AppendMusic(defaultCover, url, "加载中", "加载中");
            m_urlToIndex[url] = index;
            if (m_pool) {
                m_pool->addTask(url, index);
            }

            m_store.upsertTrack(url.toString());
            playlistChanged = true;

            ++index;
        }
        if (playlistChanged) {
            m_store.saveAtomic();
        }
    }

    if (m_pool) m_pool->start();

    m_shuffleOrder.clear();
    m_shuffleIndex = 0;

    if (!m_musicplaylist->isempty()) {
        ensureValidPlayIndex();
        m_player->setSource(m_musicplaylist->Geturl(m_playnum));
        m_audioOutput->setVolume(1);
    }

    emit playlistAvailabilityChanged(!m_musicplaylist->isempty());
}

void PlayerController::AddLocalFiles(const QStringList& filePaths)
{
    if (!m_musicplaylist) return;
    if (!m_pool) InitPool();

    const bool wasEmpty = m_musicplaylist->isempty();
    const QPixmap defaultCover(":/res/misaka.png");

    m_store.load();
    bool playlistChanged = false;
    int addedCount = 0;

    for (const QString& filePath : filePaths) {
        if (filePath.trimmed().isEmpty()) continue;
        if (!QFileInfo::exists(filePath)) continue;
        if (!isSupportedAudioFile(filePath)) continue;

        const QUrl url = QUrl::fromLocalFile(filePath);
        if (!url.isValid()) continue;
        if (m_urlToIndex.contains(url)) continue;

        const int newIndex = m_musicplaylist->Getsize();
        m_musicplaylist->AppendMusic(defaultCover, url, "加载中", "加载中");
        m_urlToIndex[url] = newIndex;

        if (m_pool) {
            m_pool->addTask(url, newIndex);
        }

        m_store.upsertTrack(url.toString());
        playlistChanged = true;
        ++addedCount;
    }

    if (playlistChanged) {
        m_store.saveAtomic();
    }

    if (addedCount > 0 && m_pool) {
        m_pool->start();
    }

    if (wasEmpty && !m_musicplaylist->isempty()) {
        ensureValidPlayIndex();
        m_player->setSource(m_musicplaylist->Geturl(m_playnum));
        m_audioOutput->setVolume(1);
        emit playlistAvailabilityChanged(true);
    }

    if (addedCount > 0 && m_nextmode == Loop_Play) {
        UpdateRandomArray();
    }
}

/**
 * @brief 生成/刷新随机播放顺序数组
 *
 * 只在 Loop_Play（随机播放）模式下使用。
 * 通过 Fisher-Yates 洗牌算法生成 0~size-1 的随机排列，
 * 并同步当前 m_playnum 在随机列表中的位置到 m_shuffleIndex。
 */
void PlayerController::UpdateRandomArray()
{
    if (!m_musicplaylist) return;

    int size = m_musicplaylist->Getsize();
    m_shuffleOrder.clear();
    if (size == 0) return;

    ensureValidPlayIndex();
    for (int i = 0; i < size; ++i) {
        m_shuffleOrder.append(i);
    }

    for (int i = size - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        m_shuffleOrder.swapItemsAt(i, j);
    }

    m_shuffleIndex = m_shuffleOrder.indexOf(m_playnum);
    if (m_shuffleIndex < 0) m_shuffleIndex = 0;
}

/** @brief 确保 m_playnum 落在 [0, size) 内；空列表返回 false。 */
bool PlayerController::ensureValidPlayIndex()
{
    if (!m_musicplaylist) return false;
    const int size = m_musicplaylist->Getsize();
    if (size <= 0) return false;

    if (m_playnum < 0) m_playnum = 0;
    if (m_playnum >= size) m_playnum = size - 1;
    return true;
}

/**
 * @brief 根据当前 m_playnum 播放对应的歌曲
 *
 * 只负责设置播放源并在需要时自动调用 play()，
 * 不改变 m_playnum 值本身。
 */
void PlayerController::PlaySong()
{
    if (!m_musicplaylist || m_musicplaylist->isempty()) return;
    if (!ensureValidPlayIndex()) return;

    m_player->setSource(m_musicplaylist->Geturl(m_playnum));

    if (m_autoplay)
    {
        QTimer::singleShot(500, this, [this]() {
            m_autoplay = false;
            m_player->play();
        });
    }
}

/**
 * @brief 切换到上一首歌曲
 *
 * 根据当前播放模式执行不同的索引更新策略：
 *  - List_Play   : 按顺序向前移动（支持从第一首跳到最后一首）
 *  - Loop_Play   : 在洗牌序列中向前移动一位
 *  - Repeat_Play : 不切换歌曲，只是将进度条回到开头位置
 */
void PlayerController::PlayPrevSong()
{
    if (!m_musicplaylist || m_musicplaylist->isempty()) return;
    if (!ensureValidPlayIndex()) return;
    if (m_player->isPlaying()) m_autoplay = true;

    // 单曲循环不需要换源文件
    if(m_nextmode == Repeat_Play)
    {
        m_player->setPosition(1);
        m_player->play();
        return;
    }

    if (m_nextmode == List_Play)
    {
        m_playnum--;
        if (m_playnum == -1) m_playnum = m_musicplaylist->Getsize() - 1;
    }
    else if (m_nextmode == Loop_Play)
    {
        if (m_shuffleOrder.isEmpty()) {
            UpdateRandomArray();
        }
        if (!m_shuffleOrder.isEmpty()) {
            m_shuffleIndex = (m_shuffleIndex - 1 + m_shuffleOrder.size()) % m_shuffleOrder.size();
            m_playnum = m_shuffleOrder[m_shuffleIndex];
        }
    }

    PlaySong();
}

/**
 * @brief 切换到下一首歌曲
 *
 * 根据当前播放模式执行不同的索引更新策略：
 *  - List_Play   : 按顺序向后移动（尾首相连）
 *  - Loop_Play   : 在洗牌序列中向后移动一位
 *  - Repeat_Play : 不切换歌曲，只是将进度条回到开头位置
 */
void PlayerController::PlayNextSong()
{
    if (!m_musicplaylist || m_musicplaylist->isempty()) return;
    if (!ensureValidPlayIndex()) return;
    if (m_player->isPlaying()) m_autoplay = true;

    // 单曲循环不需要换源文件
    if(m_nextmode == Repeat_Play)
    {
        m_player->setPosition(1);
        m_player->play();
        return;
    }

    if (m_nextmode == List_Play)
    {
        m_playnum = (m_playnum + 1) % m_musicplaylist->Getsize();
    }
    else if (m_nextmode == Loop_Play)
    {
        if (m_shuffleOrder.isEmpty()) {
            UpdateRandomArray();
        }
        if (!m_shuffleOrder.isEmpty()) {
            m_shuffleIndex = (m_shuffleIndex + 1) % m_shuffleOrder.size();
            m_playnum = m_shuffleOrder[m_shuffleIndex];
        }
    }

    PlaySong();
}

/**
 * @brief 播放结束时的统一处理函数
 *
 * 一般由 MainWindow 在进度条走到末尾时调用，
 * 内部通过设置 m_autoplay 标志并调用 PlayNextSong() 来实现“自动下一首”。
 */
void PlayerController::MusicEnd()
{
    if (m_musicplaylist && !m_musicplaylist->isempty()) {
        PlayNextSong();
    }
}

/**
 * @brief 设置当前播放模式
 *
 * 切换到 Loop_Play 时会自动生成新的随机队列；
 * 切换回 List_Play 时会清空随机队列。
 */
void PlayerController::SetPlayMode(nextmode mode)
{
    m_nextmode = mode;
    if (m_nextmode == Loop_Play) {
        UpdateRandomArray();
    } else if (m_nextmode == List_Play) {
        m_shuffleOrder.clear();
    }
}

/**
 * @brief 响应 MusicPlaylist 的选中信号，切换到指定歌曲
 *
 * @param id 在 MusicPlaylist 中的歌曲索引
 *
 * 在 Loop_Play 模式下会确保随机队列中存在对应索引，并同步 m_shuffleIndex；
 * 其他模式下仅简单设置 m_playnum。
 */
void PlayerController::OnChooseMusic(int id)
{
    if (!m_musicplaylist || m_musicplaylist->isempty()) return;
    const int size = m_musicplaylist->Getsize();
    if (id < 0 || id >= size) return;

    if (m_nextmode == Loop_Play)
    {
        m_playnum = id;
        if (m_shuffleOrder.isEmpty()) {
            UpdateRandomArray();
        }
        int idx = m_shuffleOrder.indexOf(m_playnum);
        if (idx == -1) {
            UpdateRandomArray();
        } else {
            m_shuffleIndex = idx;
        }
    }
    else
    {
        m_playnum = id;
    }
    if (m_player->isPlaying()) m_autoplay = true;
    PlaySong();
}

