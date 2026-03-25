#include "marqueelabel.h"

#include <QTimerEvent>

/** @brief 设置文本、不换行、尺寸策略，更新滚动状态并启动定时器。 */
MarqueeLabel::MarqueeLabel(QWidget *parent, const QString &text)
    : QLabel(parent)
{
    setText(text);
    setWordWrap(false);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    // 首次更新滚动状态（如果文本已设置，内部会按需启动定时器）
    updateScrollState();
}

/** @brief 设置每帧滚动像素并发射 scrollSpeedChanged。 */
void MarqueeLabel::setScrollSpeed(int pixels)
{
    if (m_scrollSpeed != pixels) {
        m_scrollSpeed = pixels;
        emit scrollSpeedChanged(pixels);
    }
}

/** @brief 设置循环间隔并发射 gapChanged，调用 update。 */
void MarqueeLabel::setGap(int gap)
{
    if (m_gap != gap) {
        m_gap = gap;
        emit gapChanged(gap);
        update();
    }
}

/** @brief 设置文本并重新计算是否需要滚动。 */
void MarqueeLabel::setText(const QString &text)
{
    QLabel::setText(text);
    updateScrollState();
}

/** @brief 若定时器未运行则启动（50ms 间隔，更省电）。 */
void MarqueeLabel::startScroll()
{
    if (!m_timer.isActive()) {
        m_timer.start(50, this);
    }
}

/** @brief 停止定时器、偏移归零并重绘。 */
void MarqueeLabel::stopScroll()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    }
    m_offset = 0;
    update();
}

/** @brief 需要滚动时自绘两段文本（首尾相接），否则调用基类。 */
void MarqueeLabel::paintEvent(QPaintEvent *event)
{
    if (!m_needScroll) {
        QLabel::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setPen(palette().windowText().color());
    painter.setFont(font());
    int textHeight = height();
    QFontMetrics fm = painter.fontMetrics();
    int textY = (textHeight - fm.height()) / 2 + fm.ascent();

    // 计算两个副本的位置
    int firstX = m_offset;
    int secondX = m_offset + m_textWidth + m_gap;

    // 绘制两个副本
    painter.drawText(firstX, textY, text());
    painter.drawText(secondX, textY, text());
}

/** @brief 定时器到时且需要滚动时更新 m_offset，超出一周期则回绕。 */
void MarqueeLabel::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer.timerId()) {
        if (m_needScroll) {
            m_offset -= m_scrollSpeed;
            int period = m_textWidth + m_gap;
            if (m_offset < -period) {
                m_offset += period;
            }
            update();
        } else {
            // 安全起见，如果不再需要滚动，关闭定时器
            stopScroll();
        }
    }
}

/** @brief 大小变化时重新判断是否需要滚动。 */
void MarqueeLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateScrollState();
}

/** @brief 显示时重置偏移并更新滚动状态。 */
void MarqueeLabel::showEvent(QShowEvent *event)
{
    QLabel::showEvent(event);
    updateScrollState();
    m_offset = 0;
}

/** @brief 隐藏时停止定时器以省电。 */
void MarqueeLabel::hideEvent(QHideEvent *event)
{
    stopScroll();
    QLabel::hideEvent(event);
}

/** @brief 根据文本宽度与标签宽度更新 m_needScroll，并物理开启/关闭定时器。 */
void MarqueeLabel::updateScrollState()
{
    QFontMetrics fm(font());
    m_textWidth = fm.horizontalAdvance(text());
    bool need = (m_textWidth > width());
    
    m_needScroll = need;
    if (m_needScroll) {
        startScroll();
    } else {
        stopScroll();
    }
}
