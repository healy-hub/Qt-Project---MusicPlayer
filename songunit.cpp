#include "songunit.h"
#include "ui_songunit.h"

#include <QLabel>

/** @brief 设置尺寸策略、背景、最小高度，并同步封面/标题/艺术家到子控件。 */
void SongUnit::InitUnit()
{
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(80);  // 保持最小高度
    setAttribute(Qt::WA_StyledBackground, true);

    ui->imagelabel->setScaledContents(true);
    ui->imagelabel->setPixmap(m_music_pixmap);
    ui->namelabel->setText(m_music_name);
    ui->artistlabel->setText(m_music_artist);
}

/** @brief 构造：保存 id/封面/url/标题/艺术家，setupUi 后 InitUnit。封面缩放为 60x60 缩略图以节省内存。 */
SongUnit::SongUnit(int id, QPixmap pix, QUrl url, QString name, QString artist, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SongUnit), m_music_id(id), m_music_url(url), m_music_name(name), m_music_artist(artist)
{
    // 将封面图缩放到 60x60 以大幅减少内存占用
    if (!pix.isNull()) {
        m_music_pixmap = pix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        m_music_pixmap = pix;
    }
    
    ui->setupUi(this);
    InitUnit();
}

/** @brief 析构：释放 UI。 */
SongUnit::~SongUnit()
{
    delete ui;
}

/** @brief 将当前封面、标题、艺术家写入子控件。 */
void SongUnit::UiUpdate()
{
    ui->imagelabel->setPixmap(m_music_pixmap);
    ui->namelabel->setText(m_music_name);
    ui->artistlabel->setText(m_music_artist);
}

/** @brief 设置列表中的索引（用于 ChooseMusic 信号）。 */
void SongUnit::SetId(int id)
{
    m_music_id = id;
}

/** @brief 设置封面图（自动缩放为缩略图）。 */
void SongUnit::SetPixmap(const QPixmap pix)
{
    if (!pix.isNull()) {
        m_music_pixmap = pix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        m_music_pixmap = pix;
    }
}

/** @brief 设置曲目名称。 */
void SongUnit::SetName(const QString name)
{
    m_music_name = name;
}

/** @brief 设置艺术家。 */
void SongUnit::SetArtist(const QString artist)
{
    m_music_artist = artist;
}

/** @brief 返回该单元对应的音乐 URL。 */
QUrl SongUnit::Geturl()
{
    return m_music_url;
}

/** @brief 点击时发射 ChooseMusic(m_music_id)。 */
void SongUnit::mouseReleaseEvent(QMouseEvent *event)
{
    emit ChooseMusic(m_music_id);
    QWidget::mouseReleaseEvent(event);
}

