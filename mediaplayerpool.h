#ifndef MEDIAPLAYERPOOL_H
#define MEDIAPLAYERPOOL_H

#include <QObject>
#include <QQueue>
#include <QList>
#include <QUrl>
#include <QPixmap>
#include <QFutureWatcher>

/** 
 * @brief 使用 TagLib 在后台线程池中解析音频元数据。
 * 
 * 架构优化：从 QMediaPlayer 异步监听重构为同步 TagLib 多线程读取，
 * 彻底解决某些格式封面读不出、速度慢及 resource contention 问题。
 */
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
    struct Result { 
        int taskId; 
        QPixmap cover; 
        QString title; 
        QString artist; 
        bool success;
        QString error;
    };

    void processTask(Task task); // 同步执行 TagLib 解析

    QQueue<Task> m_pendingTasks;
    int m_maxConcurrent;
    int m_activeTasks;
};

#endif // MEDIAPLAYERPOOL_H
