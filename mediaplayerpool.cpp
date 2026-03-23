#include "mediaplayerpool.h"
#include <QMediaMetaData>
#include <QTimer>
#include <QDebug>

/** @brief 创建 maxConcurrent 个 Worker，每个连接 metaDataChanged/errorOccurred；
 *  任务完成或失败时回收 worker 并延迟调用 start() 避免重入；
 *  start() 内 while 分配所有空闲 worker 实现并发。 */
MediaPlayerPool::MediaPlayerPool(int maxConcurrent, QObject *parent)
    : QObject(parent), m_maxConcurrent(maxConcurrent)
{
    for (int i = 0; i < m_maxConcurrent; ++i) {
        Worker *worker = new Worker(this);
        worker->player = new QMediaPlayer(worker);
        worker->busy = false;
        m_workers.append(worker);
        m_idleWorkers.enqueue(worker);

        connect(worker->player, &QMediaPlayer::metaDataChanged, this, [this, worker]() {
            if (!worker->busy) return;
            int taskId = worker->currentTask.id;
            const auto &meta = worker->player->metaData();

            // 读取标题和艺术家
            QString title = meta.value(QMediaMetaData::Title).toString();
            if (title.isEmpty()) title = "未知曲目";

            QString artist = meta.value(QMediaMetaData::ContributingArtist).toString();
            if (artist.isEmpty()) artist = meta.value(QMediaMetaData::AlbumArtist).toString();
            if (artist.isEmpty()) artist = "未知艺术家";

            QPixmap cover;
            QVariant thumb = meta.value(QMediaMetaData::ThumbnailImage);
            if (thumb.isValid() && thumb.canConvert<QImage>()) {
                cover = QPixmap::fromImage(thumb.value<QImage>());
            } else {
                QVariant coverArt = meta.value(QMediaMetaData::CoverArtImage);
                if (coverArt.isValid() && coverArt.canConvert<QImage>()) {
                    cover = QPixmap::fromImage(coverArt.value<QImage>());
                }
            }

            emit taskFinished(taskId, cover, title, artist);
            releaseWorker(worker);
            QTimer::singleShot(0, this, [this](){ start(); });
        });

        connect(worker->player, &QMediaPlayer::errorOccurred, this,
                [this, worker](QMediaPlayer::Error error, const QString &errorString) {
                    if (!worker->busy) return;
                    int taskId = worker->currentTask.id;
                    emit taskFailed(taskId, errorString);
                    releaseWorker(worker);
                    QTimer::singleShot(0, this, [this](){ start(); });
                });
    }
}

MediaPlayerPool::~MediaPlayerPool() {}

/** @brief 将 (url, taskId) 入队。 */
void MediaPlayerPool::addTask(const QUrl &url, int taskId)
{
    m_pendingTasks.enqueue({url, taskId});
}

/** @brief 若有空闲 Worker 与待处理任务则全部分配（并发）；完成/失败回调中通过 singleShot 延迟再调 start 避免重入。 */
void MediaPlayerPool::start()
{
    while (!m_idleWorkers.isEmpty() && !m_pendingTasks.isEmpty()) {
        Worker *worker = m_idleWorkers.dequeue();
        assignTask(worker);
    }
}

/** @brief 从 m_pendingTasks 取一任务，标记 worker 忙并 setSource。 */
void MediaPlayerPool::assignTask(Worker *worker)
{
    if (m_pendingTasks.isEmpty()) return;
    Task task = m_pendingTasks.dequeue();
    worker->busy = true;
    worker->currentTask = task;
    worker->player->setSource(task.url);
}

/** @brief 标记 worker 空闲并重新入队，供下次 start 使用。 */
void MediaPlayerPool::releaseWorker(Worker *worker)
{
    worker->busy = false;
    m_idleWorkers.enqueue(worker);
}
