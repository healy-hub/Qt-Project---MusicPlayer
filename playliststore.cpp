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
constexpr int kPlaylistVersion = 2; // 升级版本号以反映多列表支持
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

int PlaylistStore::findPlaylistIndex(const QString& name) const
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists[i].name == name) return i;
    }
    return -1;
}

bool PlaylistStore::load()
{
    m_tracks.clear();
    m_playlists.clear();

    QFile f(playlistJsonAbsPath());
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    
    // 加载歌曲元数据
    const QJsonArray tracksArray = root.value(QStringLiteral("tracks")).toArray();
    m_tracks.reserve(tracksArray.size());
    for (const QJsonValue& v : tracksArray) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        Track t;
        t.key = o.value(QStringLiteral("key")).toString();
        t.url = o.value(QStringLiteral("url")).toString();
        t.hasMetadata = o.value(QStringLiteral("hasMetadata")).toBool(false);
        t.title = o.value(QStringLiteral("title")).toString();
        t.artist = o.value(QStringLiteral("artist")).toString();
        t.coverPath = o.value(QStringLiteral("coverPath")).toString();
        t.isFavorite = o.value(QStringLiteral("isFavorite")).toBool(false);

        if (t.url.isEmpty()) continue;
        if (t.key.isEmpty()) t.key = makeKeyFromUrlString(t.url);
        if (t.coverPath.isEmpty()) t.coverPath = coverRelPathForKey(t.key);

        m_tracks.push_back(std::move(t));
    }

    // 加载自定义播放列表
    const QJsonArray playlistsArray = root.value(QStringLiteral("playlists")).toArray();
    for (const QJsonValue& v : playlistsArray) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        Playlist p;
        p.name = o.value(QStringLiteral("name")).toString();
        
        QJsonArray trackUrlsArray = o.value(QStringLiteral("trackUrls")).toArray();
        for (const QJsonValue& urlVal : trackUrlsArray) {
            p.trackUrls.append(urlVal.toString());
        }
        m_playlists.push_back(std::move(p));
    }

    return true;
}

bool PlaylistStore::saveAtomic() const
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), kPlaylistVersion);

    // 保存所有歌曲元数据
    QJsonArray tracksArray;
    for (const auto& t : m_tracks) {
        QJsonObject o;
        o.insert(QStringLiteral("key"), t.key);
        o.insert(QStringLiteral("url"), t.url);
        o.insert(QStringLiteral("hasMetadata"), t.hasMetadata);
        o.insert(QStringLiteral("title"), t.title);
        o.insert(QStringLiteral("artist"), t.artist);
        o.insert(QStringLiteral("coverPath"), t.coverPath);
        o.insert(QStringLiteral("isFavorite"), t.isFavorite);
        tracksArray.append(o);
    }
    root.insert(QStringLiteral("tracks"), tracksArray);

    // 保存自定义列表
    QJsonArray playlistsArray;
    for (const auto& p : m_playlists) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), p.name);
        
        QJsonArray trackUrlsArray;
        for (const auto& url : p.trackUrls) {
            trackUrlsArray.append(url);
        }
        o.insert(QStringLiteral("trackUrls"), trackUrlsArray);
        playlistsArray.append(o);
    }
    root.insert(QStringLiteral("playlists"), playlistsArray);

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
    t.isFavorite = false;
    m_tracks.push_back(std::move(t));
    return m_tracks.size() - 1;
}

void PlaylistStore::setFavorite(const QString& urlString, bool favorite)
{
    int idx = findIndexByUrl(urlString);
    if (idx >= 0) {
        m_tracks[idx].isFavorite = favorite;
    }
}

bool PlaylistStore::isFavorite(const QString& urlString) const
{
    int idx = findIndexByUrl(urlString);
    return (idx >= 0) ? m_tracks[idx].isFavorite : false;
}

void PlaylistStore::addPlaylist(const QString& name)
{
    if (findPlaylistIndex(name) < 0) {
        m_playlists.push_back({name, {}});
    }
}

void PlaylistStore::removePlaylist(const QString& name)
{
    int idx = findPlaylistIndex(name);
    if (idx >= 0) {
        m_playlists.removeAt(idx);
    }
}

void PlaylistStore::addTrackToPlaylist(const QString& playlistName, const QString& urlString)
{
    int idx = findPlaylistIndex(playlistName);
    if (idx >= 0) {
        if (!m_playlists[idx].trackUrls.contains(urlString)) {
            m_playlists[idx].trackUrls.append(urlString);
        }
    }
}

void PlaylistStore::removeTrackFromPlaylist(const QString& playlistName, const QString& urlString)
{
    int idx = findPlaylistIndex(playlistName);
    if (idx >= 0) {
        m_playlists[idx].trackUrls.removeAll(urlString);
    }
}

QStringList PlaylistStore::getPlaylistTracks(const QString& playlistName) const
{
    int idx = findPlaylistIndex(playlistName);
    return (idx >= 0) ? m_playlists[idx].trackUrls : QStringList{};
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

    if (!cover.isNull()) {
        QPixmap finalCover = cover;
        if (finalCover.width() > 200 || finalCover.height() > 200) {
            finalCover = finalCover.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        finalCover.save(coverAbsPathForKey(t.key), "PNG");
    }

    return true;
}
