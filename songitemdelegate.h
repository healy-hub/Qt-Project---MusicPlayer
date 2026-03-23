#ifndef SONGITEMDELEGATE_H
#define SONGITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>

class SongItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    enum DataRole {
        IdRole = Qt::UserRole + 1,
        UrlRole = Qt::UserRole + 2,
        ArtistRole = Qt::UserRole + 3
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

        // 获取数据 (由 Model 维护，确保是 60x60 缩略图)
        QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
        QString name = index.data(Qt::DisplayRole).toString();
        QString artist = index.data(ArtistRole).toString();

        // 1. 绘制封面图 (50x50, 居中垂直)
        int margin = 15;
        int imgSize = 50;
        QRect imgRect(option.rect.left() + margin + 5, option.rect.top() + (option.rect.height() - imgSize) / 2, imgSize, imgSize);
        if (!pixmap.isNull()) {
            painter->drawPixmap(imgRect, pixmap);
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

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(200, 80); // 行高固定为 80px
    }
};

#endif // SONGITEMDELEGATE_H
