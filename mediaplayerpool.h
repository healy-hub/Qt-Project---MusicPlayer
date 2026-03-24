#ifndef MEDIAPLAYERPOOL_H
#define MEDIAPLAYERPOOL_H

#include <QObject>
#include <QQueue>
#include <QList>
#include <QMediaPlayer>
#include <QUrl>
#include <QPixmap>

/** 使用多个 QMediaPlayer 异步解析音频元数据（封面、标题、艺术家），一次只调度一个 worker。 */
class MediaPlayerPool : public QObject
{
    Q_OBJECT
public:
    explicit MediaPlayerPool(int maxConcurrent = 4, QObject *parent = nullptr);
    ~MediaPlayerPool();
    void addTask(const QUrl &url, int taskId);   // 将任务加入队列
    void start();                                // 若有空闲 worker 与待处理任务则分配一个

signals:
    void taskFinished(int taskId, const QPixmap &cover, const QString &title, const QString &artist);
    void taskFailed(int taskId, const QString &error);

private:
    struct Task { QUrl url; int id; };
    struct Worker : public QObject {
        Worker(QObject *parent) : QObject(parent) {}
        ~Worker() {}

        QMediaPlayer *player;
        bool busy = false;
        Task currentTask;
    };

    void assignTask(Worker *worker);    // 从队列取任务并交给 worker 加载
    void releaseWorker(Worker *worker); // 任务完成或失败时回收 worker 并继续调度
    void processMetaData(Worker *worker); // 处理元数据提取与任务完成逻辑

    QList<Worker*> m_workers;
    QQueue<Worker*> m_idleWorkers;
    QQueue<Task> m_pendingTasks;
    int m_maxConcurrent;
};

#endif // MEDIAPLAYERPOOL_H
