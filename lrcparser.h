#ifndef LRCPARSER_H
#define LRCPARSER_H

#include <QObject>
#include <QVector>

struct LrcLine {
    qint64 time;   // 毫秒
    QString text;
};

class LrcParser : public QObject
{
    Q_OBJECT
public:
    explicit LrcParser(QObject *parent = nullptr);  // 无参构造
    bool parseFile(const QString &filePath);         // 解析 LRC 文件，多编码尝试，按时间排序
    const QVector<LrcLine>& lyrics() const { return m_lyrics; }
    int currentIndex(qint64 position) const;   // 二分查找 position 对应歌词行索引

private:
    QVector<LrcLine> m_lyrics;
};

#endif // LRCPARSER_H
