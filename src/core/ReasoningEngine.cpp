#include "ReasoningEngine.h"
#include "Metrics.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace sba {
namespace {

std::string fmt(const char* f, ...) {
    char buf[512];
    va_list a; va_start(a, f);
    std::vsnprintf(buf, sizeof buf, f, a);
    va_end(a);
    return buf;
}

double feat(const std::map<std::string, double>& m, const char* k) {
    auto it = m.find(k);
    return it == m.end() ? kNaN : it->second;
}

}  // namespace

bool OddsQuote::complete() const {
    for (double o : odds) if (!std::isfinite(o) || o <= 1.0 || o > 1000.0) return false;
    return true;
}

const char* decisionLabel(Decision d) {
    switch (d) {
        case Decision::PossibleValue: return "Possible value opportunity";
        case Decision::InsufficientEvidence: return "Insufficient evidence";
        case Decision::NoRecommendation: return "No recommendation";
    }
    return "";
}

ReasoningResult ReasoningEngine::analyse(const Evidence& e) const {
    ReasoningResult r;
    bool blockingData = false;          // evidence invalid -> cannot assess value at all
    std::vector<std::string> unreliable; // edge may exist but is not trustworthy

    // ---------- 1. Validate the evidence ----------
    if (e.home.empty() || e.away.empty() || e.home == e.away) {
        r.checks.push_back("FAIL  Match identity: home/away teams missing or identical");
        blockingData = true;
    } else r.checks.push_back("PASS  Match identity: " + e.home + " vs " + e.away);

    if (e.matchStarted || (e.kickoffEpoch > 0 && e.nowEpoch > 0 && e.nowEpoch >= e.kickoffEpoch)) {
        r.checks.push_back("FAIL  Timing: the match has already started - pre-match probabilities no longer apply");
        blockingData = true;
    } else r.checks.push_back("PASS  Timing: match has not started");

    // ---------- 2. Probabilities ----------
    r.model = e.blended;
    double sum = 0; bool probsOk = true;
    for (double v : r.model) { if (!std::isfinite(v) || v < 0 || v > 1) probsOk = false; sum += v; }
    if (!probsOk || std::fabs(sum - 1.0) > 0.01) {
        r.checks.push_back(fmt("FAIL  Model probabilities invalid (sum = %.4f)", sum));
        blockingData = true;
    } else r.checks.push_back(fmt("PASS  Model probabilities valid (sum = %.4f)", sum));
    r.mostLikely = argmax(r.model);

    r.completeness = e.totalFeatures > 0 ? 1.0 - static_cast<double>(e.missingFeatures) / e.totalFeatures : 0.0;
    if (r.completeness < m_cfg.minCompleteness) {
        r.checks.push_back(fmt("WARN  Inputs: %d of %d model inputs missing (%.0f%% complete, need %.0f%%)", e.missingFeatures,
                               e.totalFeatures, r.completeness * 100, m_cfg.minCompleteness * 100));
        unreliable.push_back(fmt("Only %.0f%% of model inputs are available.", r.completeness * 100));
    } else r.checks.push_back(fmt("PASS  Inputs: %.0f%% of model inputs available", r.completeness * 100));

    const int depth = std::min(e.homeMatchesPlayed, e.awayMatchesPlayed);
    if (depth < m_cfg.minMatchesPlayed) {
        r.checks.push_back(fmt("WARN  History: only %d matches on record for the less-known team (need %d)", depth, m_cfg.minMatchesPlayed));
        unreliable.push_back(fmt("One team has only %d matches of history.", depth));
    } else r.checks.push_back(fmt("PASS  History: %d / %d matches on record", e.homeMatchesPlayed, e.awayMatchesPlayed));

    // ---------- 3. Uncertainty: disagreement between prediction methods ----------
    if (e.rfAvailable) {
        for (int i = 0; i < 3; ++i) r.disagreement = std::max(r.disagreement, std::fabs(e.rf[i] - e.poisson[i]));
        if (r.disagreement > m_cfg.maxDisagreement) {
            r.checks.push_back(fmt("WARN  Models disagree by up to %.1f points (limit %.1f)", r.disagreement * 100, m_cfg.maxDisagreement * 100));
            unreliable.push_back(fmt("Random Forest and Poisson models disagree by %.1f percentage points.", r.disagreement * 100));
        } else r.checks.push_back(fmt("PASS  Models agree within %.1f points", r.disagreement * 100));
    } else {
        r.checks.push_back("INFO  Random Forest not loaded - single-model estimate (Poisson only)");
        unreliable.push_back("Only one model is available, so model disagreement cannot be measured.");
    }

    // ---------- 4. Compare with the market ----------
    if (e.hasOdds && e.odds.complete()) {
        double inv[3], s = 0;
        for (int i = 0; i < 3; ++i) { inv[i] = 1.0 / e.odds.odds[i]; s += inv[i]; }
        r.overround = s - 1.0;
        for (int i = 0; i < 3; ++i) r.market[i] = inv[i] / s;
        r.hasMarket = true;

        bool oddsRange = true;
        for (double o : e.odds.odds) if (o < m_cfg.minOdds || o > m_cfg.maxOdds) oddsRange = false;

        if (!e.odds.manual) {
            if (e.odds.fetchedAtEpoch <= 0 || e.nowEpoch <= 0) {
                r.checks.push_back("WARN  Odds freshness unknown (no timestamp)");
                r.oddsStale = true;
            } else {
                const double age = (e.nowEpoch - e.odds.fetchedAtEpoch) / 60.0;
                r.oddsStale = age > m_cfg.maxOddsAgeMinutes;
                r.checks.push_back(r.oddsStale ? fmt("FAIL  Odds are stale (%.0f min old, limit %d min)", age, m_cfg.maxOddsAgeMinutes)
                                               : fmt("PASS  Odds are fresh (%.0f min old)", age));
            }
        } else r.checks.push_back("INFO  Odds entered manually - freshness is your responsibility");
        r.checks.push_back(fmt("INFO  Bookmaker margin removed: %.1f%% overround (%s)", r.overround * 100, e.odds.source.c_str()));
        if (!oddsRange) {
            r.checks.push_back("WARN  Odds outside the supported range - value rules are not applied");
            unreliable.push_back("Odds are outside the range the rules were designed for.");
        }
        for (int i = 0; i < 3; ++i) {
            r.edge[i] = r.model[i] - r.market[i];
            r.ev[i] = r.model[i] * e.odds.odds[i] - 1.0;
            const double shrunk = r.market[i] + m_cfg.shrinkTowardMarket * (r.model[i] - r.market[i]);
            r.evShrunk[i] = shrunk * e.odds.odds[i] - 1.0;
        }
        r.candidate = static_cast<int>(std::max_element(r.evShrunk.begin(), r.evShrunk.end()) - r.evShrunk.begin());
    } else {
        r.checks.push_back("FAIL  Market: no complete, valid odds available");
    }

    // ---------- 5. Decide ----------
    const bool haveMarket = r.hasMarket;
    const int c = r.candidate;
    const bool edgeLooksReal = haveMarket && c >= 0 && r.ev[c] >= m_cfg.minExpectedValue && r.edge[c] >= m_cfg.minEdge && r.evShrunk[c] > 0;

    if (blockingData) {
        r.decision = Decision::InsufficientEvidence;
        r.withheld.push_back("The evidence checks above contain a blocking failure.");
    } else if (!haveMarket) {
        r.decision = Decision::InsufficientEvidence;
        r.withheld.push_back("No valid odds, so a value comparison is impossible. The probabilities are still shown.");
    } else if (r.oddsStale) {
        r.decision = Decision::InsufficientEvidence;
        r.withheld.push_back("Odds are stale or have no timestamp. Refresh them before relying on any value estimate.");
    } else if (!edgeLooksReal) {
        r.decision = Decision::NoRecommendation;
        r.withheld.push_back(fmt("No outcome clears the thresholds (best: %s, EV %+.1f%%, edge %+.1f pts; need EV >= %.0f%% and edge >= %.0f pts after shrinkage check).",
                                 outcomeName(c), r.ev[c] * 100, r.edge[c] * 100, m_cfg.minExpectedValue * 100, m_cfg.minEdge * 100));
    } else if (!unreliable.empty()) {
        r.decision = Decision::InsufficientEvidence;
        r.withheld.push_back(fmt("%s shows a model edge (%+.1f pts, EV %+.1f%%), but the edge is too uncertain to justify a recommendation:",
                                 outcomeName(c), r.edge[c] * 100, r.ev[c] * 100));
        for (const auto& u : unreliable) r.withheld.push_back("- " + u);
    } else {
        r.decision = Decision::PossibleValue;
    }

    // ---------- 6. Explain ----------
    const char* ml = outcomeName(r.mostLikely);
    r.headline = decisionLabel(r.decision);
    if (r.decision == Decision::PossibleValue)
        r.headline += std::string(": ") + outcomeName(c) + fmt(" at %.2f", e.odds.odds[c]);

    r.supporting.push_back(fmt("Most likely outcome is %s (%.1f%%). A likely outcome is not the same as a valuable price.", ml, r.model[r.mostLikely] * 100));
    if (std::isfinite(e.lambdaHome)) r.supporting.push_back(fmt("Expected goals: %s %.2f - %.2f %s.", e.home.c_str(), e.lambdaHome, e.lambdaAway, e.away.c_str()));
    const double eh = feat(e.features, "home_elo"), ea = feat(e.features, "away_elo");
    if (std::isfinite(eh) && std::isfinite(ea))
        r.supporting.push_back(fmt("Elo ratings: %s %.0f vs %s %.0f (home advantage adds 60).", e.home.c_str(), eh, e.away.c_str(), ea));
    const double fh = feat(e.features, "home_form_points_5"), fa = feat(e.features, "away_form_points_5");
    if (std::isfinite(fh) && std::isfinite(fa))
        r.supporting.push_back(fmt("Recent form (points per game, last 5): %s %.2f vs %s %.2f.", e.home.c_str(), fh, e.away.c_str(), fa));
    if (haveMarket && c >= 0) {
        r.supporting.push_back(fmt("For %s the model gives %.1f%% vs %.1f%% implied by the market (edge %+.1f pts, EV %+.1f%% per unit).",
                                   outcomeName(c), r.model[c] * 100, r.market[c] * 100, r.edge[c] * 100, r.ev[c] * 100));
    }

    if (haveMarket) {
        r.counter.push_back(fmt("Market-neutral check: shrinking the model %.0f%% toward the market gives EV %+.1f%% for %s.",
                                (1 - m_cfg.shrinkTowardMarket) * 100, r.evShrunk[c] * 100, outcomeName(c)));
        if (r.mostLikely != c && c >= 0) r.counter.push_back(fmt("The value candidate (%s) is not the most likely outcome (%s).", outcomeName(c), ml));
    }
    if (std::isfinite(e.quality.modelLogLoss) && std::isfinite(e.quality.marketLogLoss)) {
        if (e.quality.modelLogLoss >= e.quality.marketLogLoss)
            r.counter.push_back(fmt("On %d held-out matches this model did not beat bookmaker probabilities (log loss %.4f vs %.4f), so apparent edges are often noise.",
                                    e.quality.matches, e.quality.modelLogLoss, e.quality.marketLogLoss));
        else
            r.counter.push_back(fmt("Held-out log loss %.4f vs bookmaker %.4f on %d matches - an edge over the market is possible but small samples are noisy.",
                                    e.quality.modelLogLoss, e.quality.marketLogLoss, e.quality.matches));
    } else {
        r.counter.push_back("Backtest has not been run in this session, so model quality versus the market is unverified.");
    }
    r.counter.push_back("No model can guarantee a result; every bet can lose.");

    r.summary = r.headline + ". " + (r.withheld.empty() ? std::string() : r.withheld.front());
    return r;
}

}  // namespace sba
