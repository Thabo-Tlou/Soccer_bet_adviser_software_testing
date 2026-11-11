// Parser / safety tests for the API layer (needs Qt Core + Network, no network access).
#include <QCoreApplication>
#include <cmath>
#include <cstdio>
#include "api/FootballDataClient.h"
#include "api/ResultsUpdater.h"
#include "ui/Formatting.h"
#include "core/DateUtil.h"
#include "api/LiveDataService.h"
#include "api/HttpClient.h"
#include "api/OddsApiClient.h"

static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) ++passes; else { ++fails; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // ---- football-data.org
    const QByteArray fd = R"({"matches":[
      {"id":1,"utcDate":"2026-11-07T15:00:00Z","status":"TIMED","competition":{"name":"Premier League"},
       "homeTeam":{"name":"Arsenal FC","crest":"https://crests.football-data.org/57.png"},"awayTeam":{"name":"Chelsea FC"},"score":{"fullTime":{"home":null,"away":null}}},
      {"id":2,"utcDate":"2026-10-30T12:00:00Z","status":"IN_PLAY","homeTeam":{"name":"Everton FC"},"awayTeam":{"name":"Fulham FC"},
       "score":{"fullTime":{"home":1,"away":0}},"minute":63},
      {"id":3,"utcDate":"not-a-date","status":"TIMED","homeTeam":{"name":"X"},"awayTeam":{"name":"Y"}},
      {"id":4,"utcDate":"2026-11-08T15:00:00Z","status":"TIMED","homeTeam":{},"awayTeam":{"name":"Z"}},
      {"id":5,"utcDate":"2026-10-25T15:00:00Z","status":"FINISHED","homeTeam":{"name":"Leeds United FC"},"awayTeam":{"name":"Sunderland AFC"},
       "score":{"fullTime":{"home":2,"away":2}}}]})";
    auto r = FootballDataClient::parse(fd);
    CHECK(r.ok); CHECK(r.fixtures.size() == 3);           // malformed entries are skipped, not fatal
    CHECK(r.fixtures[0].homeGoals == -1 && r.fixtures[0].isUpcoming());
    CHECK(r.fixtures[1].isLive() && r.fixtures[1].homeGoals == 1 && r.fixtures[1].minute == 63);
    CHECK(r.fixtures[2].isFinished() && r.fixtures[2].homeGoals == 2 && r.fixtures[2].awayGoals == 2);
    CHECK(r.fixtures[0].homeCrest == "https://crests.football-data.org/57.png" && r.fixtures[0].awayCrest.empty());   // crest URLs flow through
    CHECK(!FootballDataClient::parse("not json").ok);
    CHECK(!FootballDataClient::parse("{\"error\":\"x\"}").ok);
    CHECK(!FootballDataClient::parse("").ok);
    CHECK(FootballDataClient::parse("{\"matches\":[]}").ok);

    // ---- The Odds API
    const QByteArray od = R"([{"id":"a","commence_time":"2026-11-07T15:00:00Z","home_team":"Arsenal","away_team":"Chelsea",
      "bookmakers":[
        {"title":"Bet365","last_update":"2026-10-30T10:00:00Z","markets":[{"key":"h2h","outcomes":[{"name":"Arsenal","price":1.95},{"name":"Draw","price":3.6},{"name":"Chelsea","price":4.1}]}]},
        {"title":"Broken","markets":[{"key":"h2h","outcomes":[{"name":"Arsenal","price":1.9}]}]},
        {"title":"Bad prices","markets":[{"key":"h2h","outcomes":[{"name":"Arsenal","price":0.5},{"name":"Draw","price":3.0},{"name":"Chelsea","price":3.0}]}]},
        {"title":"Spreads","markets":[{"key":"spreads","outcomes":[]}]}]},
      {"home_team":"","away_team":"Z"}])";
    auto o = OddsApiClient::parse(od);
    CHECK(o.ok); CHECK(o.events.size() == 1); CHECK(o.events[0].books.size() == 1);
    CHECK(o.events[0].books[0].bookmaker == "Bet365" && std::fabs(o.events[0].books[0].odds[1] - 3.6) < 1e-9);
    CHECK(!OddsApiClient::parse("{}").ok); CHECK(!OddsApiClient::parse("garbage").ok);

    // ---- time zones: dataset times are UK local time; everything is shown in the configured zone (UTC+2 by default)
    {
        Fmt::setZone("Africa/Maseru");
        CHECK(Fmt::zoneLabel() == "UTC+2");
        const int oct25 = sba::daysFromCivil(2026, 10, 25), aug15 = sba::daysFromCivil(2026, 8, 15);
        // 25 Oct 2026: UK is back on GMT, so 14:00 UK = 14:00 UTC = 16:00 in Maseru
        CHECK(Fmt::ukEpoch(oct25, 14 * 60) == QDateTime(QDate(2026, 10, 25), QTime(14, 0), Qt::UTC).toSecsSinceEpoch());
        CHECK(Fmt::local(Fmt::ukEpoch(oct25, 14 * 60)).toString("HH:mm") == "16:00");
        // 15 Aug 2026: UK is on BST, so 15:00 UK = 14:00 UTC = 16:00 in Maseru
        CHECK(Fmt::ukEpoch(aug15, 15 * 60) == QDateTime(QDate(2026, 8, 15), QTime(14, 0), Qt::UTC).toSecsSinceEpoch());
        CHECK(Fmt::local(Fmt::ukEpoch(aug15, 15 * 60)).toString("HH:mm") == "16:00");
        // late kickoff rolls into the next day in Maseru
        CHECK(Fmt::local(Fmt::ukEpoch(oct25, 23 * 60 + 30)).date() == QDate(2026, 10, 26));
        Fmt::setZone("not/a_zone");                               // unknown id falls back to a fixed UTC+2
        CHECK(Fmt::zoneLabel() == "UTC+2");
        Fmt::setZone("Africa/Maseru");
        CHECK(ResultsUpdater::seasonCode(QDate(2026, 11, 7)) == "2627" && ResultsUpdater::seasonCode(QDate(2027, 3, 1)) == "2627" && ResultsUpdater::seasonCode(QDate(2026, 7, 1)) == "2627" && ResultsUpdater::seasonCode(QDate(2026, 6, 30)) == "2526");
        CHECK(!ResultsUpdater::looksLikeResultsCsv("<html>404</html>") && ResultsUpdater::looksLikeResultsCsv(QByteArray("Div,Date,Time,HomeTeam,AwayTeam,FTHG,FTAG\n").append(QByteArray(300, 'x'))));
    }

    // ---- demo live match: flagged, simulated, never part of the real (cached) fixture list
    {
        LiveDataService live(nullptr);
        AppSettings st; st.demoLive = true;
        live.setSettings(st);
        CHECK(live.fixtures().size() == 1);
        CHECK(live.fixtures()[0].demo && live.fixtures()[0].isLive() && live.fixtures()[0].source == "demo");
        CHECK(live.fixtures()[0].minute >= 1);
        st.demoLive = false; live.setSettings(st);
        CHECK(live.fixtures().empty());
    }

    // ---- secrets must never leak into messages
    const QString red = HttpClient::redact("GET https://api.the-odds-api.com/v4/x?apiKey=abcDEF1234567890abcDEF1234567890&regions=uk failed");
    CHECK(!red.contains("abcDEF")); CHECK(red.contains("regions=uk"));
    CHECK(!HttpClient::redact("token=SECRETSECRET").contains("SECRETSECRET"));

    std::printf("%d checks passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
