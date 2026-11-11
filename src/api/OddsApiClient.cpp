#include "OddsApiClient.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <cmath>

OddsResult OddsApiClient::parse(const QByteArray& json) {
    OddsResult r;
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) { r.error = "Malformed response from The Odds API"; return r; }
    for (const auto& ev : doc.array()) {
        const auto o = ev.toObject();
        OddsEvent e;
        e.home = o.value("home_team").toString().toStdString();
        e.away = o.value("away_team").toString().toStdString();
        e.commenceEpoch = QDateTime::fromString(o.value("commence_time").toString(), Qt::ISODate).toSecsSinceEpoch();
        if (e.home.empty() || e.away.empty()) continue;
        for (const auto& bk : o.value("bookmakers").toArray()) {
            const auto b = bk.toObject();
            for (const auto& mk : b.value("markets").toArray()) {
                const auto m = mk.toObject();
                if (m.value("key").toString() != "h2h") continue;
                sba::BookOdds bo;
                bo.bookmaker = b.value("title").toString().toStdString();
                bo.updatedEpoch = QDateTime::fromString(b.value("last_update").toString(), Qt::ISODate).toSecsSinceEpoch();
                for (const auto& oc : m.value("outcomes").toArray()) {
                    const auto c = oc.toObject();
                    const std::string name = c.value("name").toString().toStdString();
                    const double price = c.value("price").toDouble(std::nan(""));
                    if (name == e.home) bo.odds[0] = price;
                    else if (name == "Draw") bo.odds[1] = price;
                    else if (name == e.away) bo.odds[2] = price;
                }
                if (std::isfinite(bo.odds[0]) && std::isfinite(bo.odds[1]) && std::isfinite(bo.odds[2]) && bo.odds[0] > 1 && bo.odds[1] > 1 && bo.odds[2] > 1)
                    e.books.push_back(bo);
            }
        }
        r.events.push_back(std::move(e));
    }
    r.ok = true;
    return r;
}

void OddsApiClient::fetchOdds(const QString& key, const QString& sport, const QString& regions,
                              std::function<void(const OddsResult&)> cb, bool force) {
    if (key.isEmpty()) { OddsResult r; r.error = "No The Odds API key configured (Settings, or ODDS_API_KEY)."; cb(r); return; }
    QUrl url(QString("https://api.the-odds-api.com/v4/sports/%1/odds/").arg(sport));
    QUrlQuery q; q.addQueryItem("apiKey", key); q.addQueryItem("regions", regions);
    q.addQueryItem("markets", "h2h"); q.addQueryItem("oddsFormat", "decimal");
    url.setQuery(q);
    m_http->get(url, {}, 600, 2000, [cb](const HttpResult& h) {
        OddsResult r;
        if (!h.ok) { r.error = h.error; cb(r); return; }
        r = parse(h.body);
        r.stale = h.stale; r.fetchedEpoch = h.fetchedEpoch; r.remaining = h.remainingRequests;
        if (h.stale && !h.error.isEmpty()) r.error = h.error;
        cb(r);
    }, force);
}
