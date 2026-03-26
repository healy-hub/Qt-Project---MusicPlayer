#ifndef MUSICPLAYLIST_H
#define MUSICPLAYLIST_H

#include <QWidget>
#include <QVector>
#include <QUrl>
#include <QPoint>
#include <QLabel>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QAbstractListModel>

namespace Ui {
class MusicPlaylist;
}

struct SongItem {
    QString title;
    QString artist;
    QUrl url;
    bool isFavorite{false};
    QString coverPath;
};

class SongModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SongModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_songs.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void appendSong(const SongItem &song) {
        beginInsertRows(QModelIndex(), m_songs.size(), m_songs.size());
        m_songs.append(song);
        endInsertRows();
    }

    void updateSong(int row, const SongItem &song) {
        if (row < 0 || row >= m_songs.size()) return;
        m_songs[row] = song;
        emit dataChanged(index(row), index(row));
    }

    void removeSong(int row) {
        if (row < 0 || row >= m_songs.size()) return;
        beginRemoveRows(QModelIndex(), row, row);
        m_songs.removeAt(row);
        endRemoveRows();
    }

    void clear() {
        beginResetModel();
        m_songs.clear();
        endResetModel();
    }

    const SongItem& getSong(int row) const { return m_songs[row]; }
    int count() const { return m_songs.size(); }

private:
    QVector<SongItem> m_songs;
};

class PlaylistFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit PlaylistFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
    
    void setFilterType(int type) { m_filterType = type; invalidate(); }
    int filterType() const { return m_filterType; }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    int m_filterType{0}; // 0: All, 1: Favorites
};

class MusicPlaylist : public QWidget
{
    Q_OBJECT

public:
    enum class PlaylistType { All = 0, Favorites = 1, Custom = 2 };

    explicit MusicPlaylist(QWidget *parent = nullptr);
    ~MusicPlaylist();

    void AppendMusic(const QUrl& url, const QString& name, const QString& artist, bool isFav = false, const QString& coverPath = QString());
    int appendSong(const QUrl& url, const QString& name, const QString& artist, bool isFav = false, const QString& coverPath = QString());
    bool removeSongAt(int index);
    void clearSongs();
    bool isempty();
    QUrl Geturl(const int n);
    int Getsize();
    void updateItem(int idx, const QString& name, const QString& artist, bool isFav = false, const QString& coverPath = QString());
    
    bool hasSongs() const { return m_model->count() > 0; }
    void setTargetPos(const QPoint& p);
    QPoint targetPos() const;
    void showAnimated();
    void hideAnimated();
    bool isAnimating() const;

private slots:
    void onSearchTextChanged(const QString &text);
    void onListViewClicked(const QModelIndex &proxyIndex);
    void onListViewDoubleClicked(const QModelIndex &proxyIndex);
    void onSidebarRowChanged(int row);

private:
    Ui::MusicPlaylist *ui;
    SongModel *m_model;
    PlaylistFilterProxyModel *m_proxyModel;
    class SongItemDelegate *m_delegate;

    QLabel *m_emptyLabel{nullptr};
    QPoint m_targetPos{0, 0};
    class QGraphicsOpacityEffect* m_opacityEffect{nullptr};
    class QPropertyAnimation* m_opacityAnim{nullptr};
    class QPropertyAnimation* m_posAnim{nullptr};
    class QParallelAnimationGroup* m_animGroup{nullptr};
    bool m_isAnimating{false};
    bool m_shouldBeVisible{false};

    int slideOffsetPx() const;
    int animDurationMs() const;
    void updateEmptyStateUi();

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void ChooseMusicpass(int id);
    void FavoriteToggleRequested(int id);
    void PlaylistChanged(int type); // 0: All, 1: Favorites
    void songsChanged(int size);
    void hasSongsChanged(bool hasSongs);
};

#endif // MUSICPLAYLIST_H
