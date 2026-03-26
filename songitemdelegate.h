#ifndef SONGITEMDELEGATE_H
#define SONGITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QCache>
#include <QDir>

class SongItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SongItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {
        m_coverCache.setMaxCost(4 * 1024 * 1024); // 限制缩略图内存占用约为 4MiB
    }

    enum DataRole {
        IdRole = Qt::UserRole + 1,
        UrlRole = Qt::UserRole + 2,
        ArtistRole = Qt::UserRole + 3,
        FavoriteRole = Qt::UserRole + 4,
        CoverPathRole = Qt::UserRole + 5
    };

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 绘制背景 (悬停与选中效果)
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, QColor(255, 255, 255, 20));
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(option.rect, QColor(255, 255, 255, 12));
        }

        // 绘制底部细分割线
        painter->setPen(QColor(255, 255, 255, 10));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

        // 获取数据
        QString name = index.data(Qt::DisplayRole).toString();
        QString artist = index.data(ArtistRole).toString();
        QString coverPath = index.data(CoverPathRole).toString();

        // 1. 绘制封面图 (50x50, 居中垂直) - 延迟加载与缓存
        int margin = 15;
        int imgSize = 50;
        QRect imgRect(option.rect.left() + margin + 5, option.rect.top() + (option.rect.height() - imgSize) / 2, imgSize, imgSize);
        
        QPixmap *cachedPix = nullptr;
        if (!coverPath.isEmpty()) {
            cachedPix = m_coverCache.object(coverPath);
            if (!cachedPix) {
                // 如果是相对路径，尝试拼接（假设封面在 AppData 下的 Metadata 目录，由 PlaylistStore 维护）
                // 这里的处理逻辑需要与 PlaylistStore 存储路径对齐。
                // 简单起见，如果 load 失败且路径不含 "/"，尝试拼接默认路径或让 PlayerController 传绝对路径。
                // 事实上，PlayerController 现在可以传绝对路径。
                QPixmap pix(coverPath);
                if (!pix.isNull()) {
                    if (pix.width() > 100 || pix.height() > 100) {
                        pix = pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    cachedPix = new QPixmap(pix);
                    // 传入字节大小作为成本 (width * height * 4)
                    m_coverCache.insert(coverPath, cachedPix, cachedPix->width() * cachedPix->height() * 4);
                }
            }
        }

        if (cachedPix && !cachedPix->isNull()) {
            painter->drawPixmap(imgRect, *cachedPix);
        } else {
            // 默认灰色占位背景
            painter->fillRect(imgRect, QColor(40, 40, 40));
        }

        // 2. 绘制文字 (标题加粗，艺术家稍小且变淡)
        int textLeft = imgRect.right() + 15;
        int textWidth = option.rect.right() - textLeft - margin;
        
        // 标题 (QColor(255, 255, 255, 230))
        QFont nameFont = option.font;
        nameFont.setBold(true);
        painter->setFont(nameFont);
        painter->setPen(QColor(255, 255, 255, 230));
        
        QRect nameRect = option.rect;
        nameRect.setLeft(textLeft);
        nameRect.setTop(option.rect.top() + 18);
        nameRect.setHeight(25);
        nameRect.setWidth(textWidth);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, painter->fontMetrics().elidedText(name, Qt::ElideRight, textWidth));

        // 艺术家 (QColor(255, 255, 255, 160))
        QFont artistFont = option.font;
        artistFont.setPointSize(qMax(1, artistFont.pointSize() - 1));
        painter->setFont(artistFont);
        painter->setPen(QColor(255, 255, 255, 160));
        
        QRect artistRect = nameRect;
        artistRect.setTop(nameRect.bottom() + 2);
        artistRect.setHeight(20);
        painter->drawText(artistRect, Qt::AlignLeft | Qt::AlignVCenter, painter->fontMetrics().elidedText(artist, Qt::ElideRight, textWidth));

        // 3. 绘制收藏“心形”图标 (右侧)
        bool isFav = index.data(FavoriteRole).toBool();
        int favSize = 20;
        QRect favRect(option.rect.right() - margin - favSize - 10, option.rect.top() + (option.rect.height() - favSize) / 2, favSize, favSize);
        
        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        qreal x = favRect.x();
        qreal y = favRect.y();
        qreal w = favRect.width();
        qreal h = favRect.height();

        // 优化心形：由底部顶点向上绘制两组对称的贝塞尔曲线
        path.moveTo(x + w / 2, y + h * 0.9);
        // 左半边
        path.cubicTo(x - w * 0.1, y + h * 0.5, x + w * 0.05, y - h * 0.1, x + w / 2, y + h * 0.3);
        // 右半边 (对称)
        path.cubicTo(x + w * 0.95, y - h * 0.1, x + w * 1.1, y + h * 0.5, x + w / 2, y + h * 0.9);

        if (isFav) {
            painter->fillPath(path, QColor(255, 64, 64)); // 实心红
        } else {
            painter->setPen(QPen(QColor(255, 255, 255, 100), 1.5)); // 半透明白框
            painter->drawPath(path);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(200, 80); // 行高固定为 80px
    }

private:
    mutable QCache<QString, QPixmap> m_coverCache;
};

#endif // SONGITEMDELEGATE_H
