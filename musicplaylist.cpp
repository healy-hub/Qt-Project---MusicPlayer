#include "musicplaylist.h"
#include "ui_musicplaylist.h"
#include "songitemdelegate.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QMouseEvent>

QVariant SongModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_songs.size())
        return QVariant();

    const SongItem &song = m_songs[index.row()];
    if (role == Qt::DisplayRole)
        return song.title;
    else if (role == SongItemDelegate::ArtistRole)
        return song.artist;
    else if (role == SongItemDelegate::UrlRole)
        return song.url;
    else if (role == SongItemDelegate::FavoriteRole)
        return song.isFavorite;
    else if (role == SongItemDelegate::CoverPathRole)
        return song.coverPath;

    return QVariant();
}

bool PlaylistFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    
    // 如果是收藏列表模式，且该项不是收藏，则过滤掉
    if (m_filterType == 1) {
        bool isFav = sourceModel()->data(index, SongItemDelegate::FavoriteRole).toBool();
        if (!isFav) return false;
    }

    // 搜索词过滤
    QString name = sourceModel()->data(index, Qt::DisplayRole).toString();
    QString artist = sourceModel()->data(index, SongItemDelegate::ArtistRole).toString();
    QRegularExpression re = filterRegularExpression();
    if (re.pattern().isEmpty()) return true;
    return name.contains(re) || artist.contains(re);
}

MusicPlaylist::MusicPlaylist(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MusicPlaylist)
{
    ui->setupUi(this);

    m_model = new SongModel(this);
    m_proxyModel = new PlaylistFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_delegate = new SongItemDelegate(this);
    
    ui->listView->setModel(m_proxyModel);
    ui->listView->setItemDelegate(m_delegate);
    ui->listView->setMouseTracking(true);
    ui->listView->viewport()->setAttribute(Qt::WA_Hover);
    ui->listView->setFrameShape(QFrame::NoFrame);
    ui->listView->setAttribute(Qt::WA_TranslucentBackground);
    ui->listView->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 禁用双击编辑文件名

    // 初始化侧边栏项
    ui->sidebar->addItem(QStringLiteral("全部歌曲"));
    ui->sidebar->addItem(QStringLiteral("我的收藏"));
    ui->sidebar->setCurrentRow(0);

    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &MusicPlaylist::onSearchTextChanged);
    connect(ui->listView, &QListView::clicked, this, &MusicPlaylist::onListViewClicked);
    connect(ui->listView, &QListView::doubleClicked, this, &MusicPlaylist::onListViewDoubleClicked);
    connect(ui->sidebar, &QListWidget::currentRowChanged, this, &MusicPlaylist::onSidebarRowChanged);

    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);
    m_opacityAnim = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_posAnim = new QPropertyAnimation(this, "pos", this);
    m_animGroup = new QParallelAnimationGroup(this);
    m_animGroup->addAnimation(m_opacityAnim);
    m_animGroup->addAnimation(m_posAnim);

    connect(m_animGroup, &QParallelAnimationGroup::finished, this, [this]() {
        m_isAnimating = false;
        if (!m_shouldBeVisible) QWidget::hide();
    });

    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setText(QStringLiteral("当前没有音乐"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_emptyLabel->setStyleSheet(QStringLiteral("QLabel { color: rgba(255, 255, 255, 180); font-size: 16px; }"));
    updateEmptyStateUi();
}

MusicPlaylist::~MusicPlaylist() { delete ui; }

void MusicPlaylist::onSidebarRowChanged(int row)
{
    m_proxyModel->setFilterType(row);
    updateEmptyStateUi();
    emit PlaylistChanged(row);
}

void MusicPlaylist::onListViewClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;

    QPoint pos = ui->listView->viewport()->mapFromGlobal(QCursor::pos());
    QRect rect = ui->listView->visualRect(proxyIndex);
    
    int margin = 15;
    int favSize = 20;
    QRect favRect(rect.right() - margin - favSize - 10, rect.top() + (rect.height() - favSize) / 2, favSize, favSize);

    if (favRect.contains(pos)) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        emit FavoriteToggleRequested(sourceIndex.row());
    }
}

void MusicPlaylist::onListViewDoubleClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;

    QPoint pos = ui->listView->viewport()->mapFromGlobal(QCursor::pos());
    QRect rect = ui->listView->visualRect(proxyIndex);
    
    int margin = 15;
    int favSize = 20;
    QRect favRect(rect.right() - margin - favSize - 10, rect.top() + (rect.height() - favSize) / 2, favSize, favSize);

    // 双击心形区域不触发播放
    if (!favRect.contains(pos)) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        emit ChooseMusicpass(sourceIndex.row());
    }
}

void MusicPlaylist::AppendMusic(const QUrl& url, const QString& name, const QString& artist, bool isFav, const QString& coverPath)
{
    SongItem item;
    item.url = url;
    item.title = name;
    item.artist = artist;
    item.isFavorite = isFav;
    item.coverPath = coverPath;
    m_model->appendSong(item);
    updateEmptyStateUi();
    emit songsChanged(m_model->count());
}

int MusicPlaylist::appendSong(const QUrl& url, const QString& name, const QString& artist, bool isFav, const QString& coverPath)
{
    int index = m_model->count();
    AppendMusic(url, name, artist, isFav, coverPath);
    return index;
}

void MusicPlaylist::updateItem(int idx, const QString& name, const QString& artist, bool isFav, const QString& coverPath)
{
    if (idx < 0 || idx >= m_model->count()) return;
    SongItem item = m_model->getSong(idx);
    if (!name.isEmpty()) item.title = name;
    if (!artist.isEmpty()) item.artist = artist;
    item.isFavorite = isFav;
    if (!coverPath.isEmpty()) item.coverPath = coverPath;
    
    m_model->updateSong(idx, item);
    
    // 如果当前处于收藏视图，数据变更可能需要重新过滤
    if (m_proxyModel->filterType() == 1) m_proxyModel->invalidate();
}

void MusicPlaylist::clearSongs() { m_model->clear(); updateEmptyStateUi(); emit songsChanged(0); }
bool MusicPlaylist::removeSongAt(int index) { if (index<0 || index>=m_model->count()) return false; m_model->removeSong(index); updateEmptyStateUi(); return true; }
bool MusicPlaylist::isempty() { return m_model->count() == 0; }
QUrl MusicPlaylist::Geturl(const int n) { if (n<0 || n>=m_model->count()) return QUrl(); return m_model->getSong(n).url; }
int MusicPlaylist::Getsize() { return m_model->count(); }
void MusicPlaylist::onSearchTextChanged(const QString &text) { QRegularExpression re(QRegularExpression::escape(text), QRegularExpression::CaseInsensitiveOption); m_proxyModel->setFilterRegularExpression(re); }
void MusicPlaylist::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); updateEmptyStateUi(); }
void MusicPlaylist::updateEmptyStateUi() { if (!m_emptyLabel) return; m_emptyLabel->setVisible(m_proxyModel->rowCount() == 0); m_emptyLabel->setGeometry(ui->listView->geometry()); m_emptyLabel->raise(); }
void MusicPlaylist::setTargetPos(const QPoint& p) { m_targetPos = p; if (isVisible() && !m_isAnimating) move(m_targetPos); }
QPoint MusicPlaylist::targetPos() const { return m_targetPos; }
bool MusicPlaylist::isAnimating() const { return m_isAnimating; }
void MusicPlaylist::showAnimated() { m_shouldBeVisible = true; m_animGroup->stop(); m_isAnimating = true; if (!isVisible()) { move(m_targetPos + QPoint(400, 0)); m_opacityEffect->setOpacity(0.0); show(); } m_opacityAnim->setStartValue(m_opacityEffect->opacity()); m_opacityAnim->setEndValue(1.0); m_posAnim->setStartValue(pos()); m_posAnim->setEndValue(m_targetPos); m_animGroup->start(); }
void MusicPlaylist::hideAnimated() { m_shouldBeVisible = false; if (!isVisible()) return; m_animGroup->stop(); m_isAnimating = true; m_opacityAnim->setEndValue(0.0); m_posAnim->setEndValue(m_targetPos + QPoint(400, 0)); m_animGroup->start(); }
