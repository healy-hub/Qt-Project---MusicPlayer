#include "mediaplayerpool.h"
#include <QMediaMetaData>
#include <QTimer>
#include <QDebug>
#include <QFileInfo>

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
            processMetaData(worker);
        });

        connect(worker->player, &QMediaPlayer::mediaStatusChanged, this, [this, worker](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::EndOfMedia) {
                processMetaData(worker);
            }
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

/** @brief 处理元数据提取与任务完成逻辑（带重试机制） */
void MediaPlayerPool::processMetaData(Worker *worker)
{
    if (!worker || !worker->busy) return;
    
    const auto &meta = worker->player->metaData();
    QMediaPlayer::MediaStatus status = worker->player->mediaStatus();
    bool isFullyLoaded = (status == QMediaPlayer::LoadedMedia || 
                          status == QMediaPlayer::BufferedMedia || 
                          status == QMediaPlayer::EndOfMedia);

    QPixmap cover;
    // 1. 尝试可能的封面键值 (Qt 6 仅支持 ThumbnailImage 和 CoverArtImage)
    QList<QMediaMetaData::Key> keys = {
        QMediaMetaData::ThumbnailImage,
        QMediaMetaData::CoverArtImage
    };

    for (auto key : keys) {
        QVariant v = meta.value(key);
        if (v.isValid() && v.canConvert<QImage>()) {
            cover = QPixmap::fromImage(v.value<QImage>());
            if (!cover.isNull()) break;
        }
    }

    // 2. 如果还没有获取到封面，且媒体还没有加载完成，我们继续等待信号
    if (cover.isNull() && !isFullyLoaded) {
        return; 
    }

    // 3. 核心改进：即使 Loaded 了，如果封面还是空的，可能元数据还在解析中，延迟再试一次
    static QMap<int, int> retries; // taskId -> retryCount
    int taskId = worker->currentTask.id;
    
    if (cover.isNull() && isFullyLoaded && retries[taskId] < 3) {
        retries[taskId]++;
        QTimer::singleShot(200, this, [this, worker]() {
            processMetaData(worker);
        });
        return;
    }
    retries.remove(taskId); // 清除重试计数

    // 4. 获取标题和艺术家（回退到文件名）
    QString title = meta.value(QMediaMetaData::Title).toString();
    if (title.isEmpty()) {
        title = QFileInfo(worker->currentTask.url.toLocalFile()).baseName();
    }
    if (title.isEmpty()) title = "未知曲目";

    QString artist = meta.value(QMediaMetaData::ContributingArtist).toString();
    if (artist.isEmpty()) artist = meta.value(QMediaMetaData::AlbumArtist).toString();
    if (artist.isEmpty()) artist = "未知艺术家";

    // 5. 缩放封面图并完成任务
    if (!cover.isNull() && (cover.width() > 200 || cover.height() > 200)) {
        cover = cover.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    emit taskFinished(taskId, cover, title, artist);
    
    // 清理该 worker 的状态并处理下一个任务
    releaseWorker(worker);
    QTimer::singleShot(0, this, [this](){ start(); });
}

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
