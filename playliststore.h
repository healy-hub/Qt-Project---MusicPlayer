#ifndef PLAYLISTSTORE_H
#define PLAYLISTSTORE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPixmap>

class PlaylistStore
{
public:
    struct Track {
        QString key;
        QString url;        // file:///...
        bool hasMetadata{false};
        QString title;
        QString artist;
        QString coverPath;  // relative path, e.g. Metadata/<key>.png
        bool isFavorite{false}; // 是否收藏
    };

    struct Playlist {
        QString name;
        QStringList trackUrls;
    };

    PlaylistStore();

    QString playlistJsonAbsPath() const;
    QString metadataDirAbsPath() const;

    bool ensureMetadataDir() const;

    // Percent-encoded url string as stable filename-safe key.
    static QString makeKeyFromUrlString(const QString& urlString);
    static QString coverRelPathForKey(const QString& key);
    QString coverAbsPathForKey(const QString& key) const;

    bool load();                 // loads internal tracks list
    bool saveAtomic() const;     // writes internal tracks list to disk

    const QVector<Track>& tracks() const { return m_tracks; }
    const QVector<Playlist>& playlists() const { return m_playlists; }

    // Insert if missing; returns index.
    int upsertTrack(const QString& urlString);
    
    // 设置收藏状态
    void setFavorite(const QString& urlString, bool favorite);
    bool isFavorite(const QString& urlString) const;

    // 管理自定义列表
    void addPlaylist(const QString& name);
    void removePlaylist(const QString& name);
    void addTrackToPlaylist(const QString& playlistName, const QString& urlString);
    void removeTrackFromPlaylist(const QString& playlistName, const QString& urlString);
    QStringList getPlaylistTracks(const QString& playlistName) const;

    // Mark metadata as loaded; caches cover (png) and updates track fields.
    bool markMetadata(const QString& urlString, const QPixmap& cover, const QString& title, const QString& artist);

    QPixmap loadCoverForTrack(const Track& t) const;

private:
    QString m_appDir;
    QVector<Track> m_tracks;
    QVector<Playlist> m_playlists;

    int findIndexByKey(const QString& key) const;
    int findIndexByUrl(const QString& urlString) const;
    int findPlaylistIndex(const QString& name) const;
};

#endif // PLAYLISTSTORE_H
