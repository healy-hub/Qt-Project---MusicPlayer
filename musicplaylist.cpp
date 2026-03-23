#include "musicplaylist.h"
#include "ui_musicplaylist.h"
#include "songitemdelegate.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QStandardItem>

/** @brief 构造：setupUi、QListView 属性、Model 与 Delegate 初始化、动画、空列表占位。 */
MusicPlaylist::MusicPlaylist(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MusicPlaylist)
{
    ui->setupUi(this);

    // 1. 初始化 Model 和 Delegate
    m_model = new QStandardItemModel(this);
    m_delegate = new SongItemDelegate(this);
    
    ui->listView->setModel(m_model);
    ui->listView->setItemDelegate(m_delegate);
    
    // 允许鼠标追踪以便 Delegate 接收 Hover 状态
    ui->listView->setMouseTracking(true);
    ui->listView->viewport()->setAttribute(Qt::WA_Hover);
    
    ui->listView->setFrameShape(QFrame::NoFrame);
    ui->listView->setAttribute(Qt::WA_TranslucentBackground);
    ui->listView->viewport()->setAttribute(Qt::WA_TranslucentBackground);

    // 连接点击信号，发射 id（行号）
    connect(ui->listView, &QListView::clicked, this, [this](const QModelIndex &index){
        if (index.isValid()) {
            emit ChooseMusicpass(index.row());
        }
    });

    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);

    // 2. 初始化动画相关（保持原有逻辑）
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
        if (!m_shouldBeVisible) {
            QWidget::hide();
        } else {
            move(m_targetPos);
            if (m_opacityEffect) m_opacityEffect->setOpacity(1.0);
        }
    });

    // 3. 空列表占位
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setText(QStringLiteral("当前没有加载的音乐"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: rgba(255, 255, 255, 180);"
        "  font-size: 16px;"
        "  background: transparent;"
        "}"
    ));
    updateEmptyStateUi();
}

MusicPlaylist::~MusicPlaylist()
{
    delete ui;
}

int MusicPlaylist::slideOffsetPx() const
{
    return qMax(120, width() / 3);
}

int MusicPlaylist::animDurationMs() const
{
    return 180;
}

void MusicPlaylist::setTargetPos(const QPoint& p)
{
    m_targetPos = p;
    if (isVisible() && !m_isAnimating) {
        move(m_targetPos);
    }
}

QPoint MusicPlaylist::targetPos() const
{
    return m_targetPos;
}

bool MusicPlaylist::isAnimating() const
{
    return m_isAnimating;
}

void MusicPlaylist::showAnimated()
{
    m_shouldBeVisible = true;
    if (!m_opacityEffect || !m_animGroup || !m_opacityAnim || !m_posAnim) {
        move(m_targetPos);
        show();
        return;
    }

    m_animGroup->stop();
    m_isAnimating = true;

    if (!isVisible()) {
        move(m_targetPos + QPoint(slideOffsetPx(), 0));
        m_opacityEffect->setOpacity(0.0);
        show();
    }

    m_opacityAnim->setDuration(animDurationMs());
    m_opacityAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_opacityAnim->setStartValue(m_opacityEffect->opacity());
    m_opacityAnim->setEndValue(1.0);

    m_posAnim->setDuration(animDurationMs());
    m_posAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_posAnim->setStartValue(pos());
    m_posAnim->setEndValue(m_targetPos);

    m_animGroup->start();
}

void MusicPlaylist::hideAnimated()
{
    m_shouldBeVisible = false;
    if (!isVisible()) return;

    m_animGroup->stop();
    m_isAnimating = true;

    m_opacityAnim->setDuration(animDurationMs());
    m_opacityAnim->setEasingCurve(QEasingCurve::InCubic);
    m_opacityAnim->setStartValue(m_opacityEffect->opacity());
    m_opacityAnim->setEndValue(0.0);

    m_posAnim->setDuration(animDurationMs());
    m_posAnim->setEasingCurve(QEasingCurve::InCubic);
    m_posAnim->setStartValue(pos());
    m_posAnim->setEndValue(m_targetPos + QPoint(slideOffsetPx(), 0));

    m_animGroup->start();
}

/** @brief 追加数据到 Model。 */
void MusicPlaylist::AppendMusic(QPixmap pix, QUrl url, QString name, QString artist)
{
    const bool wasEmpty = (m_model->rowCount() == 0);

    QStandardItem *item = new QStandardItem();
    // 确保 pix 已经是缩略图，避免 Model 占用过多内存
    if (!pix.isNull() && (pix.width() > 60 || pix.height() > 60)) {
        pix = pix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    item->setData(pix, Qt::DecorationRole);
    item->setData(name, Qt::DisplayRole);
    item->setData(artist, SongItemDelegate::ArtistRole);
    item->setData(url, SongItemDelegate::UrlRole);
    
    m_model->appendRow(item);

    updateEmptyStateUi();
    emit songsChanged(m_model->rowCount());
    if (wasEmpty) {
        emit hasSongsChanged(true);
    }
}

int MusicPlaylist::appendSong(const QPixmap& pix, const QUrl& url, const QString& name, const QString& artist)
{
    int index = m_model->rowCount();
    AppendMusic(pix, url, name, artist);
    return index;
}

bool MusicPlaylist::removeSongAt(int index)
{
    if (index < 0 || index >= m_model->rowCount()) return false;

    const bool wasNonEmpty = (m_model->rowCount() > 0);
    m_model->removeRow(index);

    updateEmptyStateUi();
    emit songsChanged(m_model->rowCount());
    if (wasNonEmpty && m_model->rowCount() == 0) {
        emit hasSongsChanged(false);
    }
    return true;
}

void MusicPlaylist::clearSongs()
{
    const bool wasNonEmpty = (m_model->rowCount() > 0);
    m_model->clear();
    updateEmptyStateUi();
    emit songsChanged(0);
    if (wasNonEmpty) {
        emit hasSongsChanged(false);
    }
}

bool MusicPlaylist::isempty()
{
    return m_model->rowCount() == 0;
}

QUrl MusicPlaylist::Geturl(const int n)
{
    if (n < 0 || n >= m_model->rowCount()) return QUrl();
    return m_model->item(n)->data(SongItemDelegate::UrlRole).toUrl();
}

int MusicPlaylist::Getsize()
{
    return m_model->rowCount();
}

/** @brief 更新 Model 中的某一项数据。 */
void MusicPlaylist::updateItem(int idx, QPixmap image, QString name, QString artist)
{
    if (idx < 0 || idx >= m_model->rowCount()) return;
    QStandardItem *item = m_model->item(idx);
    
    if (!image.isNull() && (image.width() > 60 || image.height() > 60)) {
        image = image.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    item->setData(image, Qt::DecorationRole);
    item->setData(name, Qt::DisplayRole);
    item->setData(artist, SongItemDelegate::ArtistRole);
}

void MusicPlaylist::updateEmptyStateUi()
{
    if (!m_emptyLabel) return;
    const bool empty = (m_model->rowCount() == 0);
    m_emptyLabel->setVisible(empty);
    m_emptyLabel->raise();
    m_emptyLabel->setGeometry(rect());
}

void MusicPlaylist::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateEmptyStateUi();
}
