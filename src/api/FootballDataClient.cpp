#include "FootballDataClient.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

FixturesResult FootballDataClient::parse(const QByteArray& json) {
    FixturesResult r;
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { r.error = "Malformed response from football-data.org"; return r; }
    const auto matches = doc.object().value("matches");
    if (!matches.isArray()) { r.error = "Response did not contain a 'matches' list"; return r; }
    for (const auto& v : matches.toArray()) {
        const auto o = v.toObject();
        sba::Fixture f;
        f.id = QString::number(o.value("id").toInteger()).toStdString();
        f.competition = o.value("competition").toObject().value("name").toString().toStdString();
        f.kickoffIso = o.value("utcDate").toString().toStdString();
        f.kickoffEpoch = QDateTime::fromString(o.value("utcDate").toString(), Qt::ISODate).toSecsSinceEpoch();
        f.status = o.value("status").toString().toStdString();
        f.home = o.value("homeTeam").toObject().value("name").toString().toStdString();
        f.away = o.value("awayTeam").toObject().value("name").toString().toStdString();
        f.homeCrest = o.value("homeTeam").toObject().value("crest").toString().toStdString();
        f.awayCrest = o.value("awayTeam").toObject().value("crest").toString().toStdString();
        if (f.home.empty() || f.away.empty() || f.kickoffEpoch <= 0) continue;   // skip malformed entries, never crash
        const auto ft = o.value("score").toObject().value("fullTime").toObject();
        if (!ft.value("home").isNull() && ft.contains("home")) f.homeGoals = ft.value("home").toInt(-1);
        if (!ft.value("away").isNull() && ft.contains("away")) f.awayGoals = ft.value("away").toInt(-1);
        if (o.contains("minute") && !o.value("minute").isNull()) f.minute = o.value("minute").toInt(-1);
        f.source = "football-data.org";
        r.fixtures.push_back(std::move(f));
    }
    r.ok = true;
    return r;
}

void FootballDataClient::fetchMatches(const QString& key, const QString& comp, const QString& from, const QString& to,
                                      std::function<void(const FixturesResult&)> cb, bool force) {
    if (key.isEmpty()) { FixturesResult r; r.error = "No football-data.org API key configured (Settings, or FOOTBALL_DATA_API_KEY)."; cb(r); return; }
    QUrl url(QString("https://api.football-data.org/v4/competitions/%1/matches").arg(comp));
    QUrlQuery q; q.addQueryItem("dateFrom", from); q.addQueryItem("dateTo", to);
    url.setQuery(q);
    m_http->get(url, {{"X-Auth-Token", key.toUtf8()}}, 300, 6500, [cb](const HttpResult& h) {
        FixturesResult r;
        if (!h.ok) { r.error = h.error; cb(r); return; }
        r = parse(h.body);
        r.stale = h.stale; r.fetchedEpoch = h.fetchedEpoch; r.remaining = h.remainingRequests;
        if (h.stale && !h.error.isEmpty()) r.error = h.error;
        cb(r);
    }, force);
}
