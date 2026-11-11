#pragma once
// Club crests. Resolution order for every team:
//   1. a real logo file dropped into a "logos" folder (PNG / SVG / WEBP, named after the team),
//   2. the crest URL supplied by football-data.org (downloaded once, then cached on disk),
//   3. a generated crest in the club's colours (always available, works offline).
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QTimer>

struct ClubStyle {
    QColor primary, secondary;
    enum Pattern { Plain, Stripes, Hoops, Halves, Sash } pattern = Plain;
    QString code;
};

class CrestProvider : public QObject {
    Q_OBJECT
public:
    static CrestProvider& instance();

    void setLogoDirs(const QStringList& dirs);          // folders scanned for user supplied logos
    QStringList logoDirs() const { return m_dirs; }
    void registerUrl(const QString& team, const QString& url);   // from the API; downloaded lazily

    static ClubStyle styleFor(const QString& team);
    static QString normalise(const QString& team);

    // Paints the crest of `team` centred in `r`. `onDark` adds a soft plate behind real logos.
    void paint(QPainter& p, const QRectF& r, const QString& team, bool onDark = false);
    QPixmap pixmap(const QString& team, int px, bool onDark = false);
    QIcon icon(const QString& team, int px = 22);
    bool hasRealLogo(const QString& team);

signals:
    void crestsChanged();

private:
    CrestProvider();
    const QImage* realImage(const QString& team);
    void scanDirs();
    void fetch(const QString& key, const QString& url);
    static QImage decode(const QByteArray& bytes, const QString& hint);
    static void paintGenerated(QPainter& p, const QRectF& r, const ClubStyle& s, bool onDark);

    QStringList m_dirs;
    QHash<QString, QString> m_localFiles;     // normalised name -> file path
    QHash<QString, QImage> m_real;            // decoded logos
    QSet<QString> m_missing, m_requested;
    QHash<QString, QString> m_urls;
    QNetworkAccessManager m_net;
    QTimer m_notify;
    QString m_cacheDir;
};
