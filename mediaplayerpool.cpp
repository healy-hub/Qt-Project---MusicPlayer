#include "mediaplayerpool.h"
#include <QtConcurrent>
#include <QFileInfo>
#include <QDebug>
#include <QImageReader>
#include <QBuffer>
#include <QByteArray>

// TagLib headers
#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/tbytevector.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>

MediaPlayerPool::MediaPlayerPool(int maxConcurrent, QObject *parent)
    : QObject(parent), m_maxConcurrent(maxConcurrent), m_activeTasks(0)
{
}

MediaPlayerPool::~MediaPlayerPool()
{
}

void MediaPlayerPool::addTask(const QUrl &url, int taskId)
{
    m_pendingTasks.enqueue({url, taskId});
}

void MediaPlayerPool::start()
{
    while (m_activeTasks < m_maxConcurrent && !m_pendingTasks.isEmpty()) {
        Task task = m_pendingTasks.dequeue();
        m_activeTasks++;
        
        // 使用 QtConcurrent 在线程池中执行解析任务
        QFuture<Result> future = QtConcurrent::run([task]() {
            Result res;
            res.taskId = task.id;
            res.success = false;
            
            QString filePath = task.url.toLocalFile();
            if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
                res.error = "File not found";
                return res;
            }

            try {
                // 使用 TagLib 读取元数据
                TagLib::FileRef f(filePath.toLocal8Bit().data());
                if (!f.isNull() && f.tag()) {
                    TagLib::Tag *tag = f.tag();
                    res.title = QString::fromStdString(tag->title().to8Bit(true));
                    res.artist = QString::fromStdString(tag->artist().to8Bit(true));
                    
                    // 提取封面
                    QImage cover;
                    std::string suffix = QFileInfo(filePath).suffix().toLower().toStdString();
                    
                    if (suffix == "mp3") {
                        TagLib::MPEG::File mpegFile(filePath.toLocal8Bit().data());
                        if (mpegFile.ID3v2Tag()) {
                            TagLib::ID3v2::FrameList frames = mpegFile.ID3v2Tag()->frameList("APIC");
                            if (!frames.isEmpty()) {
                                auto *frame = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                                QByteArray data(reinterpret_cast<const char*>(frame->picture().data()), frame->picture().size());
                                QBuffer buffer(&data);
                                buffer.open(QIODevice::ReadOnly);
                                QImageReader reader(&buffer);
                                if (reader.canRead()) {
                                    QSize imgSize = reader.size();
                                    if (imgSize.isValid() && (imgSize.width() > 200 || imgSize.height() > 200)) {
                                        imgSize.scale(200, 200, Qt::KeepAspectRatio);
                                        reader.setScaledSize(imgSize);
                                    }
                                    QImage image = reader.read();
                                    if (!image.isNull()) cover = image;
                                }
                            }
                        }
                    } else if (suffix == "flac") {
                        TagLib::FLAC::File flacFile(filePath.toLocal8Bit().data());
                        const TagLib::List<TagLib::FLAC::Picture*>& pictures = flacFile.pictureList();
                        if (!pictures.isEmpty()) {
                            TagLib::FLAC::Picture* pic = pictures.front();
                            QByteArray data(reinterpret_cast<const char*>(pic->data().data()), pic->data().size());
                            QBuffer buffer(&data);
                            buffer.open(QIODevice::ReadOnly);
                            QImageReader reader(&buffer);
                            if (reader.canRead()) {
                                QSize imgSize = reader.size();
                                if (imgSize.isValid() && (imgSize.width() > 200 || imgSize.height() > 200)) {
                                    imgSize.scale(200, 200, Qt::KeepAspectRatio);
                                    reader.setScaledSize(imgSize);
                                }
                                QImage image = reader.read();
                                if (!image.isNull()) cover = image;
                            }
                        }
                    }
                    
                    res.cover = cover;
                    res.success = true;
                }
            } catch (...) {
                res.error = "TagLib exception";
            }
            
            return res;
        });

        // 监听任务完成
        auto watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher]() {
            Result res = watcher->result();
            m_activeTasks--;
            
            if (res.success) {
                emit taskFinished(res.taskId, res.cover, res.title, res.artist);
            } else {
                emit taskFailed(res.taskId, res.error);
            }
            
            watcher->deleteLater();
            start(); // 继续处理剩余任务
        });
        watcher->setFuture(future);
    }
}
