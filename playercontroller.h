#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMap>
#include <QUrl>
#include <QStringList>
#include <QFileSystemWatcher>
#include "musicplaylist.h"
#include "mediaplayerpool.h"
#include "playliststore.h"

/** 播放模式：List_Play 列表循环，Loop_Play 随机，Repeat_Play 单曲循环 */
enum nextmode {
    List_Play,
    Loop_Play,
    Repeat_Play
};

/** 管理音乐播放核心逻辑：播放器、播放列表、元数据解析池、上一首/下一首/模式切换。 */
class PlayerController : public QObject
{
    Q_OBJECT
public:
    explicit PlayerController(QObject *parent = nullptr);
    QMediaPlayer *GetPlayer() const { return m_player; }       // 供 MainWindow 连接信号槽
    QAudioOutput *GetAudioOutput() const { return m_audioOutput; }
    void InitPlayList(MusicPlaylist *playlist);               // 扫描 MusicList、追加 SongUnit、提交元数据任务、设置初始源
    void AddLocalFiles(const QStringList& filePaths);         // 运行期追加本地音乐（去重、写入 playlist.json、提交解析）
    void PlayPrevSong();                                      // 上一首（按当前模式）
    void PlayNextSong();                                      // 下一首（按当前模式）
    void PlaySong(bool startPlaying = true);                                          // 根据 m_playnum 设置源并在需要时 play
    void MusicEnd();                                          // 播放结束入口，用于自动下一首
    void SetPlayMode(nextmode mode);
    nextmode GetPlayMode() const { return m_nextmode; }
    int GetCurrentIndex() const { return m_playnum; }         // 当前歌曲在列表中的索引
    void SetAutoPlay(bool flag) { m_autoplay = flag; }

signals:
    void playlistAvailabilityChanged(bool hasSongs);          // 是否有歌曲，供 UI 更新控件与占位

public slots:
    void OnChooseMusic(int id);                               // 由 MusicPlaylist 选中信号触发，切换并播放
    void onDirectoryChanged(const QString &path);             // 目录变化处理

private:
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    int m_playnum;
    MusicPlaylist *m_musicplaylist;
    QMap<QUrl, int> m_urlToIndex;
    MediaPlayerPool *m_pool;
    PlaylistStore m_store;
    QFileSystemWatcher *m_watcher;
    QString m_monitoringPath;
    bool m_autoplay;
    nextmode m_nextmode;
    QList<int> m_shuffleOrder;
    int m_shuffleIndex;
    int m_playToken;

    void InitPool();                  // 创建 MediaPlayerPool 并连接 taskFinished/taskFailed
    void UpdateRandomArray();        // 生成随机播放顺序（Fisher-Yates），仅 Loop_Play 使用
    bool ensureValidPlayIndex();      // 将 m_playnum 限制在 [0, size)，空列表返回 false
};

#endif // PLAYERCONTROLLER_H
