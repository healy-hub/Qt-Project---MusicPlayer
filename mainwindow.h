#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVector>
#include <QQueue>
#include <QMap>
#include <QUrl>
#include <QPointF>
#include <QTimer>
#include <QModelIndex>
#include <QResizeEvent>
#include <QLabel>
#include <QKeyEvent>
#include <QColor>
#include <QSystemTrayIcon>
#include <QMenu>
#include "musicplaylist.h"
#include "playercontroller.h"
#include "lrcparser.h"
#include "moremenu.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    MusicPlaylist *m_musicplaylist;
    PlayerController *m_playerController;
    QLabel *m_emptyOverlayLabel;
    bool m_sliderPressed;
    bool m_ignoreSliderUpdate;
    qint64 m_pendingSeek;
    bool m_isDragging;
    QPointF m_dragStartPos;
    MoreMenu *m_moremenuwindow;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;

    enum Edge {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8,
        TopLeftEdge = TopEdge | LeftEdge,
        TopRightEdge = TopEdge | RightEdge,
        BottomLeftEdge = BottomEdge | LeftEdge,
        BottomRightEdge = BottomEdge | RightEdge
    };

    Edge m_resizeEdge;
    bool m_isResizing;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeometry;
    const int m_resizeBorderWidth = 5;

    LrcParser *m_lrcParser;
    QVector<LrcLine> m_lyrics;
    QTimer *m_wheelTimer;
    bool m_manualScroll;
    int m_lastLrcIndex;                                   // 记录上一行歌词索引，避免重复刷新
    QPixmap m_backgroundPixmap;                           // 缓存背景图片
    void updateBackground();                              // 加载并缓存背景图

    void loadLyrics(const QString &musicFilePath);        // 根据音乐路径加载同目录 .lrc
    void applyPendingSeek();                              // 应用媒体加载前记录的待 seek，并清除

    void InitWindow();                                    // 初始化窗口：标题、无边框、样式、overlay、事件过滤
    void loadStyleSheet();                                // 从 :/style.qss 加载并应用样式
    void InitButtons();                                   // 设置按钮图标并连接信号
    void InitButtonIcon(QPushButton *button, const QString &path, const QColor &color = Qt::transparent);  // 设置按钮尺寸与图标
    void InitPlayList();                                  // 创建 MusicList 目录、播放列表控件并交给 PlayerController
    void InitLrcParser();                                 // 创建 LrcParser、连接 positionChanged 与歌词列表
    void updatePlaybackControlsEnabled(bool enabled);     // 根据是否有歌曲启用/禁用播放相关控件
    void updateEmptyOverlayVisible(bool visible);         // 显示或隐藏“当前没有加载的音乐”占位
    void UpdateMusicListPosition();                       // 根据窗口尺寸更新播放列表位置与高度
    void togglePlaylist();                                // 切换播放列表显示/隐藏（带动画）
    void hidePlaylistIfVisible();                         // 若播放列表可见则带动画隐藏
    Edge getEdge(const QPoint &pos);                      // 根据鼠标位置返回可拖拽边缘
    void updateCursor(Edge edge);                         // 根据边缘设置窗口光标
    void startResize(Edge edge, const QPoint &pos);       // 开始调整大小时记录状态
    void performResize(const QPoint &pos);                // 根据鼠标移动计算并设置新几何
    void moremenubuttonclick();                           // 按下更多按钮执行操作
    void InitTrayIcon();                                  // 初始化系统托盘图标与菜单

private slots:
    void StatusChanged(QMediaPlayer::MediaStatus status); // 媒体状态变化：Loaded/Buffered 时应用 pendingSeek，EndOfMedia 时自动下一首
    void StateChange(QMediaPlayer::PlaybackState state);  // 播放状态变化（预留）
    void UpdateMetadata();                                // 从当前播放器读取元数据并更新封面/标题/艺术家/歌词
    void MusicEnd();                                      // 当前曲目结束，通知 PlayerController 切下一首
    void onAddMusicFromMoreMenu();                        // MoreMenu -> 添加音乐
    void onSetMusicDirClicked();                          // MoreMenu -> 设置音乐目录
    void updateSliderPosition(qint64 position);           // 同步播放位置到进度条
    void updateSliderRange(qint64 duration);              // 设置进度条范围
    void updatalyricsListWidget();                        // 根据窗口宽度设置歌词列表高度
    void onProgressSliderMoved(int value);                // 占位槽，seek 在 eventFilter 中处理
    void onPositionChanged(qint64 position);              // 高亮对应歌词行并自动居中
    void onLyricsListWidgetClicked(QModelIndex index);    // 点击歌词行跳转时间并居中
    void onWheelTimerTimeout();                           // 滚轮定时器超时后恢复自动跟随
    void showBackgroundContextMenu(const QPoint &pos);    // 主窗口背景右键菜单
    void showCoverContextMenu(const QPoint &pos);         // 封面图片右键菜单

protected:
    void paintEvent(QPaintEvent *event) override;         // 自定义绘制圆角背景
    void resizeEvent(QResizeEvent *event) override;       // 重绘圆角遮罩、更新 overlay 与列表位置
    bool eventFilter(QObject *obj, QEvent *event) override;  // 列表外点击隐藏、进度条 seek、歌词滚轮、窗口拖拽与边缘缩放
    void keyPressEvent(QKeyEvent *event) override;        // 键盘快进/后退
    void closeEvent(QCloseEvent *event) override;         // 关闭时隐藏到托盘
};

#endif // MAINWINDOW_H
