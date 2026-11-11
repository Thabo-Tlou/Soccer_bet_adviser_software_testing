#pragma once
// Keeps the match history current: downloads the current season's results from football-data.co.uk
// (free, no API key - the same source the bundled dataset was built from) and caches the file.
// The cached file is merged into the dataset at start-up (see PredictionService / Dataset::mergeCsv).
#include <QDate>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>

class ResultsUpdater : public QObject {
    Q_OBJECT
public:
    explicit ResultsUpdater(QObject* parent = nullptr);

    static QString seasonCode(const QDate& d);                       // "2627" for any date in 2026/27
    static QString cacheDir();                                       // <AppData>/results
    static QStringList cachedFiles();                                // every cached season file (merged at start-up)
    static bool looksLikeResultsCsv(const QByteArray& bytes);        // header check before a download replaces the cache

    // Downloads the current season file. Emits finished() exactly once (also on timeout / failure).
    void fetch(const QString& baseUrl = QStringLiteral("https://www.football-data.co.uk/mmz4281"), int timeoutMs = 9000);
    qint64 lastSuccessEpoch() const;                                 // from the cache file's timestamp (0 = never)

signals:
    void finished(bool ok, const QString& message);

private:
    void done(bool ok, const QString& msg);
    QNetworkAccessManager m_net;
    QTimer m_guard;
    bool m_running = false;
};
