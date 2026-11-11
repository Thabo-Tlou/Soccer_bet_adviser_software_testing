#pragma once
// Betting markets beyond 1X2: over/under goals, both teams to score, double chance, draw no bet, team totals, Asian handicaps,
// exact goals, correct score, win to nil, clean sheets, odd/even and result+total combos.
//
// Everything is priced from ONE scoreline grid (the probability of every exact score), so all markets are consistent with each
// other and with the blended 1X2 forecast. A single evaluate() function both prices a bet (by summing grid cells) and settles it
// (from a final score), so pricing and settlement can never disagree.
#include <array>
#include <string>
#include <utility>
#include <vector>
#include "ReasoningEngine.h"
#include "Types.h"

namespace sba {

enum class BetResult { Win, HalfWin, Push, HalfLose, Lose };

enum class BetKind {
    Result,        // side 0 home, 1 draw, 2 away
    DoubleChance,  // side 0 = 1X, 1 = 12, 2 = X2
    DrawNoBet,     // side 0 home, 2 away (a draw is a push)
    Total,         // over/under `line` total goals
    Btts,          // over = Yes
    TeamTotal,     // side 0 home / 1 away, over/under `line`
    Handicap,      // side 0 home / 2 away, `line` applied to that side (Asian; quarter lines split the stake)
    ExactTotal,    // a goals (b == 1 means "a or more")
    CorrectScore,  // a-b
    OtherScore,    // none of the scores listed in `excluded`
    WinToNil,      // side 0 home / 1 away wins without conceding
    CleanSheet,    // side 0 home / 1 away concedes nothing (over = Yes)
    OddEven,       // over = Odd
    ResultAndTotal // side 0/1/2 result AND over/under `line`
};

struct BetSpec {
    BetKind kind = BetKind::Result;
    int side = 0;
    double line = 0.0;
    int a = 0, b = 0;
    bool over = true;
    std::vector<std::pair<int, int>> excluded;
};

BetResult evaluate(const BetSpec& spec, int homeGoals, int awayGoals);        // settle (and price) a bet
double payoffPerUnit(BetResult o, double odds);                                // profit per 1 unit staked

struct Selection {
    std::string key, group, label;
    BetSpec spec;
    double pWin = 0, pHalfWin = 0, pPush = 0, pHalfLose = 0, pLose = 0;
    double fairOdds = kNaN;                  // odds at which the bet breaks even on the model
    double expectedValue(double odds) const; // profit per unit staked at `odds`
    double oddsForEv(double minEv) const;    // smallest odds that still give an expected value of `minEv`
    double winLike() const { return pWin + pHalfWin; }
    bool hasPush() const { return pPush > 1e-12 || pHalfWin > 1e-12 || pHalfLose > 1e-12; }
};

struct MarketGroup {
    std::string id, title, blurb;
    std::vector<Selection> items;
};

struct ScoreProb { int home = 0, away = 0; double p = 0; };

class ScoreGrid {
public:
    static constexpr int N = 13;                         // 0..12 goals each
    static ScoreGrid fromLambdas(double lambdaHome, double lambdaAway);
    void rescaleToResult(const Probs3& target);          // make home/draw/away sums match `target`
    double at(int h, int a) const { return p[h][a]; }
    Probs3 result() const;
    double sum() const;
    double p[N][N] = {};
};

struct MarketBook {
    bool ok = false;
    std::string home, away;
    double lambdaHome = kNaN, lambdaAway = kNaN, expectedTotal = kNaN;
    Probs3 result{};
    std::array<double, 6> totalGoals{};                  // P(0..4 goals), P(5+)
    std::vector<ScoreProb> topScores;                    // most likely exact scores
    std::vector<MarketGroup> groups;

    const Selection* find(const std::string& key) const;
};

// `result` is the (blended) home/draw/away forecast the grid is rescaled to; pass {} to keep the pure Poisson shape.
MarketBook buildMarketBook(const std::string& home, const std::string& away, double lambdaHome, double lambdaAway, const Probs3& result);

// ---- staking -------------------------------------------------------------------------------------------------------
struct StakeSettings {
    double bankroll = 0;               // 0 = unknown: advice is given as a % of bankroll instead of a money amount
    double kellyFraction = 0.25;       // fraction of the full Kelly stake to use
    double maxStakeFraction = 0.05;    // hard cap per bet as a fraction of bankroll
    double minStake = 0;               // stakes below this are not recommended
    double roundTo = 0;                // round stakes down to a multiple of this (0 = no rounding)
};

enum class Verdict { NoOdds, Invalid, NoValue, Marginal, Value };

struct BetAdvice {
    Verdict verdict = Verdict::NoOdds;
    double odds = kNaN, fairOdds = kNaN, valueFromOdds = kNaN;
    double modelProb = kNaN, impliedProb = kNaN;
    double ev = kNaN, evShrunk = kNaN;         // profit per 1 unit staked
    double kellyFull = 0, kellyApplied = 0;    // fractions of bankroll
    double stake = 0;                          // money amount (0 when no bet is advised or the bankroll is unknown)
    double stakeFraction = 0;                  // recommended stake as a fraction of bankroll
    double returnIfWin = 0, profitIfWin = 0;
    std::string headline;
    std::vector<std::string> notes;
};

double kellyFraction(const Selection& s, double odds);   // full-Kelly fraction of bankroll (maximises expected log growth)

// `oppositeOdds` is the other side of a two-way market (NaN if unknown); it lets the bookmaker margin be measured exactly.
// `shrink` (0..1) is how much the model is trusted against the market (0.5 = half and half).
BetAdvice adviseBet(const Selection& s, double odds, double oppositeOdds, const StakeSettings& stake, const RiskConfig& risk, double shrink);

// ---- bet slip (singles and accumulators) --------------------------------------------------------------------------------
struct SlipLeg {
    std::string matchKey, matchLabel, selectionKey, selectionLabel;
    double pWin = 0, pLose = 0, pOther = 0;   // pOther = push / half results (not supported inside accumulators)
    double odds = kNaN;
};

struct SlipResult {
    bool valid = false;
    std::string problem;
    int legs = 0;
    double combinedOdds = kNaN, combinedProb = kNaN;
    double stake = 0, returnIfWin = 0, profitIfWin = 0;
    double evPerUnit = kNaN, evMoney = kNaN, evShrunkPerUnit = kNaN;
    double suggestedStake = 0;                // from Kelly on the combined bet (0 = not advised)
    Verdict verdict = Verdict::NoOdds;
    std::vector<std::string> warnings;
};

SlipResult evaluateSlip(const std::vector<SlipLeg>& legs, double stake, const StakeSettings& cfg, const RiskConfig& risk, double shrink);

const char* verdictName(Verdict v);

}  // namespace sba
