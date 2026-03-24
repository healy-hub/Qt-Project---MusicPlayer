#include "playliststore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QUrl>
#include <QStandardPaths>

namespace {
constexpr int kPlaylistVersion = 1;
} // namespace

PlaylistStore::PlaylistStore()
    : m_appDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
{
    QDir dir(m_appDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString PlaylistStore::playlistJsonAbsPath() const
{
    return QDir(m_appDir).filePath(QStringLiteral("playlist.json"));
}

QString PlaylistStore::metadataDirAbsPath() const
{
    return QDir(m_appDir).filePath(QStringLiteral("Metadata"));
}

bool PlaylistStore::ensureMetadataDir() const
{
    QDir d(metadataDirAbsPath());
    if (d.exists()) return true;
    return QDir(m_appDir).mkpath(QStringLiteral("Metadata"));
}

QString PlaylistStore::makeKeyFromUrlString(const QString& urlString)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(urlString));
}

QString PlaylistStore::coverRelPathForKey(const QString& key)
{
    return QStringLiteral("Metadata/%1.png").arg(key);
}

QString PlaylistStore::coverAbsPathForKey(const QString& key) const
{
    return QDir(m_appDir).filePath(coverRelPathForKey(key));
}

int PlaylistStore::findIndexByKey(const QString& key) const
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].key == key) return i;
    }
    return -1;
}

int PlaylistStore::findIndexByUrl(const QString& urlString) const
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].url == urlString) return i;
    }
    return -1;
}

bool PlaylistStore::load()
{
    m_tracks.clear();

    QFile f(playlistJsonAbsPath());
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("tracks")) || !root.value(QStringLiteral("tracks")).isArray()) {
        root.insert(QStringLiteral("tracks"), QJsonArray{});
    }
    if (!root.contains(QStringLiteral("version")) || !root.value(QStringLiteral("version")).isDouble()) {
        root.insert(QStringLiteral("version"), kPlaylistVersion);
    }

    const QJsonArray tracks = root.value(QStringLiteral("tracks")).toArray();
    m_tracks.reserve(tracks.size());
    for (const QJsonValue& v : tracks) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        Track t;
        t.key = o.value(QStringLiteral("key")).toString();
        t.url = o.value(QStringLiteral("url")).toString();
        t.hasMetadata = o.value(QStringLiteral("hasMetadata")).toBool(false);
        t.title = o.value(QStringLiteral("title")).toString();
        t.artist = o.value(QStringLiteral("artist")).toString();
        t.coverPath = o.value(QStringLiteral("coverPath")).toString();

        if (t.url.isEmpty()) continue;
        if (t.key.isEmpty()) t.key = makeKeyFromUrlString(t.url);
        if (t.coverPath.isEmpty()) t.coverPath = coverRelPathForKey(t.key);

        m_tracks.push_back(std::move(t));
    }

    return true;
}

bool PlaylistStore::saveAtomic() const
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), kPlaylistVersion);

    QJsonArray tracks;
    for (const auto& t : m_tracks) {
        QJsonObject o;
        o.insert(QStringLiteral("key"), t.key);
        o.insert(QStringLiteral("url"), t.url);
        o.insert(QStringLiteral("hasMetadata"), t.hasMetadata);
        o.insert(QStringLiteral("title"), t.title);
        o.insert(QStringLiteral("artist"), t.artist);
        o.insert(QStringLiteral("coverPath"), t.coverPath);
        tracks.append(o);
    }
    root.insert(QStringLiteral("tracks"), tracks);

    QSaveFile f(playlistJsonAbsPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QJsonDocument doc(root);
    f.write(doc.toJson(QJsonDocument::Indented));
    return f.commit();
}

int PlaylistStore::upsertTrack(const QString& urlString)
{
    if (urlString.isEmpty()) return -1;
    const QString key = makeKeyFromUrlString(urlString);

    int idx = findIndexByKey(key);
    if (idx >= 0) return idx;

    // If key not found but url exists, reuse that entry.
    idx = findIndexByUrl(urlString);
    if (idx >= 0) {
        if (m_tracks[idx].key.isEmpty()) m_tracks[idx].key = key;
        if (m_tracks[idx].coverPath.isEmpty()) m_tracks[idx].coverPath = coverRelPathForKey(m_tracks[idx].key);
        return idx;
    }

    Track t;
    t.key = key;
    t.url = urlString;
    t.hasMetadata = false;
    t.coverPath = coverRelPathForKey(key);
    m_tracks.push_back(std::move(t));
    return m_tracks.size() - 1;
}

bool PlaylistStore::markMetadata(const QString& urlString, const QPixmap& cover, const QString& title, const QString& artist)
{
    const int idx = upsertTrack(urlString);
    if (idx < 0) return false;

    ensureMetadataDir();

    Track& t = m_tracks[idx];
    t.hasMetadata = true;
    t.title = title;
    t.artist = artist;
    if (t.key.isEmpty()) t.key = makeKeyFromUrlString(urlString);
    t.coverPath = coverRelPathForKey(t.key);

    // 只有当封面不为空时才保存到本地文件系统
    if (!cover.isNull()) {
        QPixmap finalCover = cover;
        // 缩放缓存图片到 200x200 以节省磁盘空间并提高加载速度
        if (finalCover.width() > 200 || finalCover.height() > 200) {
            finalCover = finalCover.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        finalCover.save(coverAbsPathForKey(t.key), "PNG");
    }

    return true;
}

QPixmap PlaylistStore::loadCoverForTrack(const Track& t) const
{
    // Prefer the explicit coverPath stored in json (relative path).
    if (!t.coverPath.isEmpty()) {
        const QString abs = QDir(m_appDir).filePath(t.coverPath);
        QPixmap pix;
        if (pix.load(abs)) return pix;
    }

    // Fallback by key.
    if (!t.key.isEmpty()) {
        QPixmap pix;
        if (pix.load(coverAbsPathForKey(t.key))) return pix;
    }

    return {};
}

