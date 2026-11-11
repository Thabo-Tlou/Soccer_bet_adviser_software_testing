#include "HttpClient.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>
#include <algorithm>

HttpClient::HttpClient(QObject* parent) : QObject(parent) {}

QString HttpClient::redact(QString t) {
    t.replace(QRegularExpression("(?i)(apikey|api_key|token|key)=[^&\\s\"']+"), "\\1=***");
    t.replace(QRegularExpression("[A-Za-z0-9]{24,}"), "***");
    return t;
}

QString HttpClient::cacheKey(const QUrl& url) const {
    QUrl u = url;
    QUrlQuery q(u);
    q.removeAllQueryItems("apiKey");          // keys must never influence (or appear in) cache file names
    u.setQuery(q);
    return QCryptographicHash::hash(u.toString().toUtf8(), QCryptographicHash::Sha1).toHex();
}

bool HttpClient::readCache(const QString& key, QByteArray& body, qint64& epoch) const {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/api/" + key + ".json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    epoch = static_cast<qint64>(doc.object().value("epoch").toDouble());
    body = QByteArray::fromBase64(doc.object().value("body").toString().toLatin1());
    return epoch > 0 && !body.isEmpty();
}

void HttpClient::writeCache(const QString& key, const QByteArray& body) const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/api";
    QDir().mkpath(dir);
    QFile f(dir + "/" + key + ".json");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QJsonObject o;
    o["epoch"] = static_cast<double>(QDateTime::currentSecsSinceEpoch());
    o["body"] = QString::fromLatin1(body.toBase64());
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void HttpClient::get(const QUrl& url, const Headers& headers, int ttl, int minIntervalMs, Callback cb, bool force) {
    const QString key = cacheKey(url);
    const QString host = url.host();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QByteArray cached; qint64 cachedEpoch = 0;
    const bool haveCache = readCache(key, cached, cachedEpoch);
    auto serveCache = [&](bool stale, const QString& why) {
        HttpResult r; r.ok = true; r.httpStatus = 200; r.body = cached; r.fromCache = true; r.stale = stale;
        r.fetchedEpoch = cachedEpoch; r.error = why;
        QMetaObject::invokeMethod(this, [cb, r]() { cb(r); }, Qt::QueuedConnection);
    };

    if (haveCache && !force && QDateTime::currentSecsSinceEpoch() - cachedEpoch < ttl) { serveCache(false, QString()); return; }

    // Respect rate limits: back off after 429 and space requests per host.
    if (m_blockedUntilMs.value(host) > nowMs || nowMs - m_lastRequestMs.value(host, 0) < minIntervalMs) {
        const int wait = static_cast<int>(std::max<qint64>(m_blockedUntilMs.value(host) - nowMs, minIntervalMs - (nowMs - m_lastRequestMs.value(host, 0))) / 1000) + 1;
        if (haveCache) { serveCache(true, QString("Rate limit protection: showing cached data (retry in %1 s)").arg(wait)); return; }
        HttpResult r; r.error = QString("Rate limit protection: please retry in %1 s").arg(wait); r.retryAfterSec = wait;
        QMetaObject::invokeMethod(this, [cb, r]() { cb(r); }, Qt::QueuedConnection);
        return;
    }
    m_lastRequestMs[host] = nowMs;

    QNetworkRequest req(url);
    req.setTransferTimeout(12000);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SoccerBetAdviser/1.0");
    req.setRawHeader("Accept", "application/json");
    for (const auto& h : headers) req.setRawHeader(h.first, h.second);

    QNetworkReply* reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        HttpResult r;
        r.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto rem = reply->rawHeader("X-Requests-Available-Minute");
        const auto rem2 = reply->rawHeader("x-requests-remaining");
        if (!rem.isEmpty()) r.remainingRequests = rem.toInt();
        else if (!rem2.isEmpty()) r.remainingRequests = rem2.toInt();

        if (reply->error() == QNetworkReply::NoError && r.httpStatus >= 200 && r.httpStatus < 300) {
            if (QJsonDocument::fromJson(body).isNull()) { r.error = "The server returned a response that is not valid JSON"; }
            else { r.ok = true; r.body = body; r.fetchedEpoch = QDateTime::currentSecsSinceEpoch(); writeCache(key, body); cb(r); return; }
        } else if (r.httpStatus == 429) {
            r.retryAfterSec = reply->rawHeader("Retry-After").toInt();
            if (r.retryAfterSec <= 0) r.retryAfterSec = 60;
            m_blockedUntilMs[host] = QDateTime::currentMSecsSinceEpoch() + r.retryAfterSec * 1000LL;
            r.error = QString("Rate limit reached (HTTP 429). Retry in %1 s.").arg(r.retryAfterSec);
        } else if (r.httpStatus == 401 || r.httpStatus == 403) {
            r.error = "The API rejected the request (HTTP " + QString::number(r.httpStatus) + "). Check the API key and your plan's coverage.";
        } else if (reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::TimeoutError) {
            r.error = "The request timed out.";
        } else if (r.httpStatus > 0) {
            r.error = "HTTP " + QString::number(r.httpStatus) + " from " + host;
        } else {
            r.error = redact(reply->errorString());
        }
        if (haveCache) {   // network failed: show the last good response, clearly marked stale
            r.ok = true; r.body = cached; r.fromCache = true; r.stale = true; r.fetchedEpoch = cachedEpoch;
        }
        cb(r);
    });
}
