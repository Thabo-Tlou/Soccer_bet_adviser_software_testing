#pragma once
// Persistent user settings. API keys are NEVER stored in source code: they come from
// environment variables (preferred) or from the Settings page (stored per user via QSettings).
#include <QString>
#include "core/ReasoningEngine.h"

struct AppSettings {
    QString footballDataKey, oddsApiKey;
    QString competitionCode = "PL";          // football-data.org competition
    QString oddsSportKey = "soccer_epl";     // The Odds API sport key
    QString oddsRegions = "uk,eu";
    int liveRefreshSec = 60;                 // only while a match is live and auto-refresh is on
    int fixturesRefreshMin = 15;
    int oddsRefreshMin = 30;
    bool autoRefresh = true;
    // staking / bankroll (used by every betting-market screen)
    double bankroll = 0;                     // 0 = not set: advice is shown as a % of bankroll
    QString currency = "M";                  // symbol shown next to money amounts
    int riskProfile = 1;                     // 0 careful (10% Kelly), 1 balanced (25%), 2 bolder (50%)
    double maxStakePct = 5.0;                // never stake more than this % of the bankroll on one bet
    double minStake = 0;                     // stakes below this are not advised
    double stakeRound = 1.0;                 // round stakes down to a multiple of this
    double kellyFraction() const { return riskProfile == 0 ? 0.10 : (riskProfile == 2 ? 0.50 : 0.25); }
    QString timeZone = "Africa/Maseru";     // all times are shown in this zone (UTC+2)
    bool autoUpdateResults = true;           // download the latest results at start-up
    bool demoLive = false;                   // simulated live match so the live UI can be previewed without an API key
    bool reduceMotion = false;               // stops decorative animation
    double rfWeight = 0.5;
    sba::RiskConfig risk;

    static AppSettings load();
    void save() const;
    QString effectiveFootballKey() const;    // env var overrides stored value
    QString effectiveOddsKey() const;
    static QString mask(const QString& key);
};
