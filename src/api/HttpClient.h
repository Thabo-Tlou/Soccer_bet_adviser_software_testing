#pragma once
// GET-only JSON client with timeout, on-disk cache, stale-cache fallback and rate-limit handling.
#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPair>
#include <QUrl>
#include <functional>

struct HttpResult {
    bool ok = false;
    int httpStatus = 0;
    QByteArray body;
    QString error;              // human-readable, never contains API keys
    bool fromCache = false;
    bool stale = false;         // served from cache because the network request failed or was blocked
    qint64 fetchedEpoch = 0;    // when `body` was originally downloaded
    int remainingRequests = -1; // from provider headers when available
    int retryAfterSec = 0;
};

class HttpClient : public QObject {
    Q_OBJECT
public:
    using Headers = QList<QPair<QByteArray, QByteArray>>;
    using Callback = std::function<void(const HttpResult&)>;

    explicit HttpClient(QObject* parent = nullptr);

    // cacheTtlSec: serve a cached response younger than this without hitting the network.
    // minIntervalMs: minimum spacing between network requests to the same host (rate limiting).
    void get(const QUrl& url, const Headers& headers, int cacheTtlSec, int minIntervalMs, Callback cb, bool forceNetwork = false);

    static QString redact(QString text);   // removes apiKey=... / tokens from text before showing/logging

private:
    bool readCache(const QString& key, QByteArray& body, qint64& epoch) const;
    void writeCache(const QString& key, const QByteArray& body) const;
    QString cacheKey(const QUrl& url) const;
    QNetworkAccessManager m_net;
    QHash<QString, qint64> m_lastRequestMs;      // host -> last request time
    QHash<QString, qint64> m_blockedUntilMs;     // host -> rate-limited until
};
