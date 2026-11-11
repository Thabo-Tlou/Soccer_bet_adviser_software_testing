#include <algorithm>
#include "AppSettings.h"
#include <algorithm>
#include <QProcessEnvironment>
#include <QSettings>

static QSettings make() { return QSettings("SoccerBetAdviser", "SoccerBetAdviser"); }

AppSettings AppSettings::load() {
    QSettings q = make();
    AppSettings s;
    s.footballDataKey = q.value("api/footballData").toString();
    s.oddsApiKey = q.value("api/oddsApi").toString();
    s.competitionCode = q.value("api/competition", s.competitionCode).toString();
    s.oddsSportKey = q.value("api/oddsSport", s.oddsSportKey).toString();
    s.oddsRegions = q.value("api/oddsRegions", s.oddsRegions).toString();
    s.liveRefreshSec = q.value("refresh/liveSec", s.liveRefreshSec).toInt();
    s.fixturesRefreshMin = q.value("refresh/fixturesMin", s.fixturesRefreshMin).toInt();
    s.oddsRefreshMin = q.value("refresh/oddsMin", s.oddsRefreshMin).toInt();
    s.autoRefresh = q.value("refresh/auto", s.autoRefresh).toBool();
    s.bankroll = q.value("stake/bankroll", s.bankroll).toDouble();
    s.currency = q.value("stake/currency", s.currency).toString();
    s.riskProfile = std::clamp(q.value("stake/riskProfile", s.riskProfile).toInt(), 0, 2);
    s.maxStakePct = std::clamp(q.value("stake/maxStakePct", s.maxStakePct).toDouble(), 0.5, 25.0);
    s.minStake = std::max(0.0, q.value("stake/minStake", s.minStake).toDouble());
    s.stakeRound = std::max(0.0, q.value("stake/round", s.stakeRound).toDouble());
    s.timeZone = q.value("ui/timeZone", s.timeZone).toString();
    s.autoUpdateResults = q.value("data/autoUpdateResults", s.autoUpdateResults).toBool();
    s.demoLive = q.value("ui/demoLive", s.demoLive).toBool();
    s.reduceMotion = q.value("ui/reduceMotion", s.reduceMotion).toBool();
    s.rfWeight = q.value("model/rfWeight", s.rfWeight).toDouble();
    s.risk.minExpectedValue = q.value("risk/minEv", s.risk.minExpectedValue).toDouble();
    s.risk.minEdge = q.value("risk/minEdge", s.risk.minEdge).toDouble();
    s.risk.maxDisagreement = q.value("risk/maxDisagreement", s.risk.maxDisagreement).toDouble();
    s.risk.shrinkTowardMarket = q.value("risk/shrink", s.risk.shrinkTowardMarket).toDouble();
    s.risk.maxOddsAgeMinutes = q.value("risk/maxOddsAge", s.risk.maxOddsAgeMinutes).toInt();
    s.risk.minMatchesPlayed = q.value("risk/minMatches", s.risk.minMatchesPlayed).toInt();
    return s;
}

void AppSettings::save() const {
    QSettings q = make();
    q.setValue("api/footballData", footballDataKey);
    q.setValue("api/oddsApi", oddsApiKey);
    q.setValue("api/competition", competitionCode);
    q.setValue("api/oddsSport", oddsSportKey);
    q.setValue("api/oddsRegions", oddsRegions);
    q.setValue("refresh/liveSec", liveRefreshSec);
    q.setValue("refresh/fixturesMin", fixturesRefreshMin);
    q.setValue("refresh/oddsMin", oddsRefreshMin);
    q.setValue("refresh/auto", autoRefresh);
    q.setValue("stake/bankroll", bankroll); q.setValue("stake/currency", currency); q.setValue("stake/riskProfile", riskProfile);
    q.setValue("stake/maxStakePct", maxStakePct); q.setValue("stake/minStake", minStake); q.setValue("stake/round", stakeRound);
    q.setValue("ui/timeZone", timeZone);
    q.setValue("data/autoUpdateResults", autoUpdateResults);
    q.setValue("ui/demoLive", demoLive);
    q.setValue("ui/reduceMotion", reduceMotion);
    q.setValue("model/rfWeight", rfWeight);
    q.setValue("risk/minEv", risk.minExpectedValue);
    q.setValue("risk/minEdge", risk.minEdge);
    q.setValue("risk/maxDisagreement", risk.maxDisagreement);
    q.setValue("risk/shrink", risk.shrinkTowardMarket);
    q.setValue("risk/maxOddsAge", risk.maxOddsAgeMinutes);
    q.setValue("risk/minMatches", risk.minMatchesPlayed);
    q.sync();
}

QString AppSettings::effectiveFootballKey() const {
    const QString e = QProcessEnvironment::systemEnvironment().value("FOOTBALL_DATA_API_KEY").trimmed();
    return e.isEmpty() ? footballDataKey.trimmed() : e;
}

QString AppSettings::effectiveOddsKey() const {
    const QString e = QProcessEnvironment::systemEnvironment().value("ODDS_API_KEY").trimmed();
    return e.isEmpty() ? oddsApiKey.trimmed() : e;
}

QString AppSettings::mask(const QString& k) {
    if (k.isEmpty()) return "not set";
    if (k.size() <= 6) return QString(k.size(), QChar(0x2022));
    return QString(k.size() - 4, QChar(0x2022)) + k.right(4);
}
