#pragma once
// Phase 6 - transparent hybrid reasoning engine.
// Every output is derived from model probabilities, available evidence and the
// explicit thresholds in RiskConfig. No hard-coded example values.
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Types.h"

namespace sba {

struct RiskConfig {
    double minExpectedValue   = 0.05;  // model-implied EV needed per 1 unit stake (same as training backtest)
    double minEdge            = 0.03;  // model probability minus de-margined market probability
    double maxDisagreement    = 0.08;  // max |RF - Poisson| on any outcome
    double minCompleteness    = 0.85;  // share of non-missing model inputs
    int    minMatchesPlayed   = 8;     // history depth required for both teams
    double shrinkTowardMarket = 0.5;   // 0 = trust market, 1 = trust model (used as a conservative EV check)
    int    maxOddsAgeMinutes  = 60;    // API odds older than this are stale
    double minOdds            = 1.15;
    double maxOdds            = 15.0;
};

struct OddsQuote {
    double odds[3] = {kNaN, kNaN, kNaN};
    std::string source;                 // e.g. "The Odds API / Bet365" or "manual"
    std::int64_t fetchedAtEpoch = 0;    // 0 = unknown
    bool manual = false;
    bool complete() const;
};

struct ModelQuality {                   // from the held-out backtest (NaN = not run yet)
    double modelLogLoss = kNaN, marketLogLoss = kNaN;
    int matches = 0;
};

struct Evidence {
    std::string home, away, kickoffIso;
    std::int64_t kickoffEpoch = 0;      // 0 = unknown
    std::int64_t nowEpoch = 0;
    bool matchStarted = false;

    bool rfAvailable = false;
    Probs3 rf{}, poisson{}, blended{};
    double rfWeight = 0.0;
    double lambdaHome = kNaN, lambdaAway = kNaN;

    int missingFeatures = 0, totalFeatures = 0;
    int homeMatchesPlayed = 0, awayMatchesPlayed = 0;
    std::map<std::string, double> features;

    bool hasOdds = false;
    OddsQuote odds;
    ModelQuality quality;
};

enum class Decision { PossibleValue, InsufficientEvidence, NoRecommendation };

struct ReasoningResult {
    Decision decision = Decision::InsufficientEvidence;
    std::string headline;
    std::string summary;

    Probs3 model{}, market{};
    bool hasMarket = false;
    double overround = kNaN;
    Probs3 edge{}, ev{}, evShrunk{};
    int mostLikely = 0;
    int candidate = -1;                 // outcome examined for value (-1 = none)
    double disagreement = 0.0;
    double completeness = 1.0;
    bool oddsStale = false;

    std::vector<std::string> checks;        // validation results (pass/fail lines)
    std::vector<std::string> supporting;
    std::vector<std::string> counter;
    std::vector<std::string> withheld;      // reasons for withholding a recommendation
};

const char* decisionLabel(Decision d);

class ReasoningEngine {
public:
    explicit ReasoningEngine(RiskConfig cfg = {}) : m_cfg(cfg) {}
    void setConfig(const RiskConfig& c) { m_cfg = c; }
    const RiskConfig& config() const { return m_cfg; }
    ReasoningResult analyse(const Evidence& e) const;

private:
    RiskConfig m_cfg;
};

}  // namespace sba
