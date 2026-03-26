#ifndef MEDIAPLAYERPOOL_H
#define MEDIAPLAYERPOOL_H

#include <QObject>
#include <QQueue>
#include <QList>
#include <QUrl>
#include <QImage>
#include <QFutureWatcher>

/** 
 * @brief 使用 TagLib 在后台线程池中解析音频元数据。
 */
class MediaPlayerPool : public QObject
{
    Q_OBJECT
public:
    explicit MediaPlayerPool(int maxConcurrent = 4, QObject *parent = nullptr);
    ~MediaPlayerPool();
    void addTask(const QUrl &url, int taskId);
    void start();

signals:
    void taskFinished(int taskId, const QImage &cover, const QString &title, const QString &artist);
    void taskFailed(int taskId, const QString &error);

private:
    struct Task { QUrl url; int id; };
    struct Result { 
        int taskId; 
        QImage cover; 
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
