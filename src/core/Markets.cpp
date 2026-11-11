#include "Markets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace sba {

namespace {
constexpr double kEps = 1e-9;
const double kInf = std::numeric_limits<double>::infinity();

int sgn(double m) { return m > kEps ? 1 : (m < -kEps ? -1 : 0); }
bool isQuarter(double line) { const double q = std::fabs(line) * 4.0; return std::fabs(std::fmod(q, 2.0) - 1.0) < 1e-9; }

// margin > 0 wins, margin == 0 pushes, margin < 0 loses. Quarter lines split the stake over the two neighbouring lines.
BetResult fromMargin(double margin, double line) {
    if (!isQuarter(line)) {
        const int s = sgn(margin);
        return s > 0 ? BetResult::Win : (s == 0 ? BetResult::Push : BetResult::Lose);
    }
    const int s = sgn(margin + 0.25) + sgn(margin - 0.25);          // two half-stakes
    switch (s) {
        case 2: return BetResult::Win;
        case 1: return BetResult::HalfWin;
        case -1: return BetResult::HalfLose;
        case -2: return BetResult::Lose;
        default: return BetResult::Push;
    }
}
BetResult yesNo(bool win) { return win ? BetResult::Win : BetResult::Lose; }
int resultOf(int h, int a) { return h > a ? 0 : (h == a ? 1 : 2); }
std::string fmtLine(double v) { char b[32]; std::snprintf(b, sizeof b, std::fabs(v - std::round(v)) < 1e-9 ? "%.0f" : (std::fabs(v * 10 - std::round(v * 10)) < 1e-9 ? "%.1f" : "%.2f"), v); return b; }
std::string fmtSigned(double v) { return (v > 0 ? "+" : "") + fmtLine(v); }
}  // namespace

BetResult evaluate(const BetSpec& s, int h, int a) {
    const int total = h + a;
    switch (s.kind) {
        case BetKind::Result: return yesNo(resultOf(h, a) == s.side);
        case BetKind::DoubleChance: {
            const int r = resultOf(h, a);
            return yesNo(s.side == 0 ? r != 2 : (s.side == 1 ? r != 1 : r != 0));
        }
        case BetKind::DrawNoBet: {
            if (h == a) return BetResult::Push;
            return yesNo(s.side == 0 ? h > a : a > h);
        }
        case BetKind::Total: return fromMargin(s.over ? total - s.line : s.line - total, s.line);
        case BetKind::Btts: { const bool both = h > 0 && a > 0; return yesNo(s.over ? both : !both); }
        case BetKind::TeamTotal: { const int g = s.side == 0 ? h : a; return fromMargin(s.over ? g - s.line : s.line - g, s.line); }
        case BetKind::Handicap: { const int d = s.side == 0 ? h - a : a - h; return fromMargin(d + s.line, s.line); }
        case BetKind::ExactTotal: return yesNo(s.b == 1 ? total >= s.a : total == s.a);
        case BetKind::CorrectScore: return yesNo(h == s.a && a == s.b);
        case BetKind::OtherScore: {
            for (const auto& e : s.excluded) if (e.first == h && e.second == a) return BetResult::Lose;
            return BetResult::Win;
        }
        case BetKind::WinToNil: return yesNo(s.side == 0 ? (h > a && a == 0) : (a > h && h == 0));
        case BetKind::CleanSheet: { const bool cs = s.side == 0 ? a == 0 : h == 0; return yesNo(s.over ? cs : !cs); }
        case BetKind::OddEven: { const bool odd = total % 2 == 1; return yesNo(s.over ? odd : !odd); }
        case BetKind::ResultAndTotal: {
            const bool r = resultOf(h, a) == s.side;
            const bool t = s.over ? total > s.line : total < s.line;
            return yesNo(r && t);
        }
    }
    return BetResult::Lose;
}

double payoffPerUnit(BetResult o, double odds) {
    switch (o) {
        case BetResult::Win: return odds - 1.0;
        case BetResult::HalfWin: return (odds - 1.0) / 2.0;
        case BetResult::Push: return 0.0;
        case BetResult::HalfLose: return -0.5;
        default: return -1.0;
    }
}

double Selection::expectedValue(double odds) const {
    return pWin * (odds - 1.0) + pHalfWin * (odds - 1.0) / 2.0 - pHalfLose * 0.5 - pLose;
}

double Selection::oddsForEv(double minEv) const {
    const double w = pWin + 0.5 * pHalfWin;
    if (w < 1e-12) return kInf;
    return 1.0 + (minEv + pLose + 0.5 * pHalfLose) / w;
}

// ------------------------------------------------------------------------------------------------------ grid
ScoreGrid ScoreGrid::fromLambdas(double lh, double la) {
    ScoreGrid g;
    auto pmf = [](double lam) {
        std::array<double, N> p{};
        for (int k = 0; k < N; ++k) p[k] = std::exp(-lam + k * std::log(lam) - std::lgamma(k + 1.0));
        return p;
    };
    const auto ph = pmf(lh), pa = pmf(la);
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) g.p[i][j] = ph[i] * pa[j];
    const double s = g.sum();
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) g.p[i][j] /= s;
    return g;
}

double ScoreGrid::sum() const { double s = 0; for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) s += p[i][j]; return s; }

Probs3 ScoreGrid::result() const {
    Probs3 r{0, 0, 0};
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) r[resultOf(i, j)] += p[i][j];
    return r;
}

void ScoreGrid::rescaleToResult(const Probs3& t) {
    const Probs3 cur = result();
    double f[3];
    for (int k = 0; k < 3; ++k) f[k] = (cur[k] > 1e-12 && std::isfinite(t[k]) && t[k] > 0) ? t[k] / cur[k] : 1.0;
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) p[i][j] *= f[resultOf(i, j)];
    const double s = sum();
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) p[i][j] /= s;
}

// ------------------------------------------------------------------------------------------------------ book
const Selection* MarketBook::find(const std::string& key) const {
    for (const auto& g : groups) for (const auto& s : g.items) if (s.key == key) return &s;
    return nullptr;
}

MarketBook buildMarketBook(const std::string& home, const std::string& away, double lh, double la, const Probs3& result) {
    MarketBook mb;
    if (!std::isfinite(lh) || !std::isfinite(la) || lh <= 0 || la <= 0) return mb;
    mb.home = home; mb.away = away; mb.lambdaHome = lh; mb.lambdaAway = la; mb.expectedTotal = lh + la;

    ScoreGrid grid = ScoreGrid::fromLambdas(lh, la);
    const double rs = result[0] + result[1] + result[2];
    if (std::isfinite(rs) && rs > 0.99 && rs < 1.01) grid.rescaleToResult(result);
    mb.result = grid.result();

    for (int i = 0; i < ScoreGrid::N; ++i) for (int j = 0; j < ScoreGrid::N; ++j) mb.totalGoals[std::min(5, i + j)] += grid.at(i, j);
    std::vector<ScoreProb> all;
    for (int i = 0; i < ScoreGrid::N; ++i) for (int j = 0; j < ScoreGrid::N; ++j) all.push_back({i, j, grid.at(i, j)});
    std::sort(all.begin(), all.end(), [](const ScoreProb& x, const ScoreProb& y) { return x.p > y.p; });
    mb.topScores.assign(all.begin(), all.begin() + 10);

    auto sel = [&](const std::string& key, const std::string& group, const std::string& label, const BetSpec& spec) {
        Selection s; s.key = key; s.group = group; s.label = label; s.spec = spec;
        for (int i = 0; i < ScoreGrid::N; ++i)
            for (int j = 0; j < ScoreGrid::N; ++j) {
                const double p = grid.at(i, j);
                switch (evaluate(spec, i, j)) {
                    case BetResult::Win: s.pWin += p; break;
                    case BetResult::HalfWin: s.pHalfWin += p; break;
                    case BetResult::Push: s.pPush += p; break;
                    case BetResult::HalfLose: s.pHalfLose += p; break;
                    default: s.pLose += p; break;
                }
            }
        s.fairOdds = s.oddsForEv(0.0);
        return s;
    };
    auto spec = [](BetKind k, int side = 0, double line = 0, bool over = true, int a = 0, int b = 0) { BetSpec s; s.kind = k; s.side = side; s.line = line; s.over = over; s.a = a; s.b = b; return s; };
    auto group = [&](const char* id, const char* title, const char* blurb) { mb.groups.push_back({id, title, blurb, {}}); return &mb.groups.back(); };

    {   // Match result
        auto* g = group("result", "Match result (1X2)", "Who wins after 90 minutes.");
        g->items.push_back(sel("r:1", g->id, home + " to win", spec(BetKind::Result, 0)));
        g->items.push_back(sel("r:X", g->id, "Draw", spec(BetKind::Result, 1)));
        g->items.push_back(sel("r:2", g->id, away + " to win", spec(BetKind::Result, 2)));
    }
    {   // Double chance
        auto* g = group("double_chance", "Double chance", "Two of the three results cover your bet.");
        g->items.push_back(sel("dc:1X", g->id, home + " or draw", spec(BetKind::DoubleChance, 0)));
        g->items.push_back(sel("dc:12", g->id, home + " or " + away + " (no draw)", spec(BetKind::DoubleChance, 1)));
        g->items.push_back(sel("dc:X2", g->id, "Draw or " + away, spec(BetKind::DoubleChance, 2)));
    }
    {   // Draw no bet
        auto* g = group("dnb", "Draw no bet", "If the match is drawn your stake is returned.");
        g->items.push_back(sel("dnb:1", g->id, home + " (draw no bet)", spec(BetKind::DrawNoBet, 0)));
        g->items.push_back(sel("dnb:2", g->id, away + " (draw no bet)", spec(BetKind::DrawNoBet, 2)));
    }
    {   // Over / under
        auto* g = group("goals", "Total goals: over / under", "Goals scored by both teams in 90 minutes.");
        for (double line : {0.5, 1.5, 2.5, 3.5, 4.5}) {
            g->items.push_back(sel("ou:" + fmtLine(line) + ":o", g->id, "Over " + fmtLine(line) + " goals", spec(BetKind::Total, 0, line, true)));
            g->items.push_back(sel("ou:" + fmtLine(line) + ":u", g->id, "Under " + fmtLine(line) + " goals", spec(BetKind::Total, 0, line, false)));
        }
    }
    {   // BTTS
        auto* g = group("btts", "Both teams to score", "Both sides must find the net.");
        g->items.push_back(sel("btts:y", g->id, "Both teams to score: Yes", spec(BetKind::Btts, 0, 0, true)));
        g->items.push_back(sel("btts:n", g->id, "Both teams to score: No", spec(BetKind::Btts, 0, 0, false)));
    }
    {   // Result + total
        auto* g = group("result_total", "Result and total goals (2.5)", "Both parts must come true.");
        const char* names[3] = {"home", "draw", "away"};
        for (int side = 0; side < 3; ++side) {
            const std::string who = side == 0 ? home : (side == 1 ? std::string("Draw") : away);
            g->items.push_back(sel(std::string("rt:") + names[side] + ":o", g->id, who + " & over 2.5", spec(BetKind::ResultAndTotal, side, 2.5, true)));
            g->items.push_back(sel(std::string("rt:") + names[side] + ":u", g->id, who + " & under 2.5", spec(BetKind::ResultAndTotal, side, 2.5, false)));
        }
    }
    {   // Team totals
        auto* g = group("team_goals", "Team goals", "Goals scored by one team only.");
        for (int side = 0; side < 2; ++side)
            for (double line : {0.5, 1.5, 2.5}) {
                const std::string who = side == 0 ? home : away, k = std::string("tt:") + (side == 0 ? "h:" : "a:") + fmtLine(line);
                g->items.push_back(sel(k + ":o", g->id, who + " over " + fmtLine(line), spec(BetKind::TeamTotal, side, line, true)));
                g->items.push_back(sel(k + ":u", g->id, who + " under " + fmtLine(line), spec(BetKind::TeamTotal, side, line, false)));
            }
    }
    {   // Asian handicap
        auto* g = group("handicap", "Asian handicap", "A goal head-start (or deficit) for one side. Whole lines return your stake on a tie; quarter lines split it.");
        for (double line : {-1.5, -1.25, -1.0, -0.75, -0.5, -0.25, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5}) {
            g->items.push_back(sel("ah:h:" + fmtSigned(line), g->id, home + " " + fmtSigned(line), spec(BetKind::Handicap, 0, line)));
            g->items.push_back(sel("ah:a:" + fmtSigned(-line), g->id, away + " " + fmtSigned(-line), spec(BetKind::Handicap, 2, -line)));
        }
    }
    {   // Exact total
        auto* g = group("exact_goals", "Exact total goals", "The precise number of goals in the match.");
        for (int n = 0; n <= 5; ++n) g->items.push_back(sel("eg:" + std::to_string(n), g->id, n == 5 ? "5 or more goals" : (std::to_string(n) + (n == 1 ? " goal" : " goals")), spec(BetKind::ExactTotal, 0, 0, true, n, n == 5 ? 1 : 0)));
    }
    {   // Correct score
        auto* g = group("correct_score", "Correct score", "The exact final score. Long shots by nature: even the most likely score is rarely above 15%.");
        BetSpec other; other.kind = BetKind::OtherScore;
        for (const auto& sp : mb.topScores) {
            g->items.push_back(sel("cs:" + std::to_string(sp.home) + "-" + std::to_string(sp.away), g->id, std::to_string(sp.home) + " - " + std::to_string(sp.away), spec(BetKind::CorrectScore, 0, 0, true, sp.home, sp.away)));
            other.excluded.push_back({sp.home, sp.away});
        }
        g->items.push_back(sel("cs:other", g->id, "Any other score", other));
    }
    {   // Clean sheets / win to nil
        auto* g = group("nil", "Clean sheets and win to nil", "Defensive markets.");
        g->items.push_back(sel("wtn:h", g->id, home + " to win to nil", spec(BetKind::WinToNil, 0)));
        g->items.push_back(sel("wtn:a", g->id, away + " to win to nil", spec(BetKind::WinToNil, 1)));
        g->items.push_back(sel("cs:h:y", g->id, home + " clean sheet: Yes", spec(BetKind::CleanSheet, 0, 0, true)));
        g->items.push_back(sel("cs:h:n", g->id, home + " clean sheet: No", spec(BetKind::CleanSheet, 0, 0, false)));
        g->items.push_back(sel("cs:a:y", g->id, away + " clean sheet: Yes", spec(BetKind::CleanSheet, 1, 0, true)));
        g->items.push_back(sel("cs:a:n", g->id, away + " clean sheet: No", spec(BetKind::CleanSheet, 1, 0, false)));
    }
    {   // Odd / even
        auto* g = group("odd_even", "Odd or even total goals", "A coin-flip market: the model has almost no edge here.");
        g->items.push_back(sel("oe:odd", g->id, "Total goals odd", spec(BetKind::OddEven, 0, 0, true)));
        g->items.push_back(sel("oe:even", g->id, "Total goals even", spec(BetKind::OddEven, 0, 0, false)));
    }
    mb.ok = true;
    return mb;
}

// ------------------------------------------------------------------------------------------------------ staking
const char* verdictName(Verdict v) {
    switch (v) {
        case Verdict::Value: return "Value";
        case Verdict::Marginal: return "Marginal";
        case Verdict::NoValue: return "No value";
        case Verdict::Invalid: return "Check odds";
        default: return "";
    }
}

namespace {
// Expected log growth of bankroll when staking fraction f of it.
double growth(const Selection& s, double odds, double f) {
    double g = 0;
    auto add = [&](double p, double pay) { if (p > 0) g += p * std::log(1.0 + f * pay); };
    add(s.pWin, odds - 1.0); add(s.pHalfWin, (odds - 1.0) / 2.0); add(s.pHalfLose, -0.5); add(s.pLose, -1.0);
    return g;
}
}  // namespace

double kellyFraction(const Selection& s, double odds) {
    if (!(odds > 1.0) || s.expectedValue(odds) <= 0) return 0.0;
    double lo = 0.0, hi = 0.999;
    for (int i = 0; i < 80; ++i) {                    // growth() is concave in f: ternary search for the maximum
        const double m1 = lo + (hi - lo) / 3.0, m2 = hi - (hi - lo) / 3.0;
        if (growth(s, odds, m1) < growth(s, odds, m2)) lo = m1; else hi = m2;
    }
    return (lo + hi) / 2.0;
}

BetAdvice adviseBet(const Selection& s, double odds, double opp, const StakeSettings& cfg, const RiskConfig& risk, double shrink) {
    BetAdvice a;
    a.modelProb = s.winLike();
    a.fairOdds = s.fairOdds;
    a.valueFromOdds = s.oddsForEv(risk.minExpectedValue);
    auto pct = [](double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f%%", v * 100.0); return std::string(b); };
    auto num = [](double v, int d = 2) { char b[32]; std::snprintf(b, sizeof b, "%.*f", d, v); return std::string(b); };

    if (!std::isfinite(odds) || odds <= 0) { a.verdict = Verdict::NoOdds; a.headline = "Enter the bookmaker's odds to see whether this bet has value."; return a; }
    if (odds <= 1.0) { a.verdict = Verdict::Invalid; a.odds = odds; a.headline = "Decimal odds must be above 1.00."; return a; }
    a.odds = odds; a.impliedProb = 1.0 / odds;
    a.returnIfWin = odds; a.profitIfWin = odds - 1.0;

    // bookmaker margin: exact when the other side of a two-way market is known, otherwise assumed
    double overround = 0.05; bool known = false;
    if (std::isfinite(opp) && opp > 1.0) {
        const double o = 1.0 / odds + 1.0 / opp - 1.0;
        if (o >= 0.005) { overround = o; known = true; }          // a realistic margin: measured exactly
        else a.notes.push_back("The two odds you entered add up to less than 100%, which bookmakers do not offer. Please check them.");
    }
    const double evMarket = 1.0 / (1.0 + overround) - 1.0;           // what you expect to lose if the market is right

    a.ev = s.expectedValue(odds);
    shrink = std::clamp(shrink, 0.0, 1.0);
    a.evShrunk = shrink * a.ev + (1.0 - shrink) * evMarket;

    const bool inRange = odds >= risk.minOdds && odds <= risk.maxOdds;
    if (a.ev >= risk.minExpectedValue && a.evShrunk > 0 && inRange) a.verdict = Verdict::Value;
    else if (a.ev > 0) a.verdict = Verdict::Marginal;
    else a.verdict = Verdict::NoValue;

    // Kelly on a shrunk (more cautious) copy of the probabilities when there are no pushes
    Selection adj = s;
    if (!s.hasPush()) {
        const double m = (1.0 / odds) / (1.0 + overround);
        adj.pWin = shrink * s.pWin + (1.0 - shrink) * m; adj.pLose = 1.0 - adj.pWin;
    }
    a.kellyFull = kellyFraction(adj, odds);
    a.kellyApplied = a.kellyFull * std::clamp(cfg.kellyFraction, 0.0, 1.0);
    a.stakeFraction = a.verdict == Verdict::Value ? std::min(a.kellyApplied, std::clamp(cfg.maxStakeFraction, 0.0, 1.0)) : 0.0;
    if (cfg.bankroll > 0 && a.stakeFraction > 0) {
        double st = a.stakeFraction * cfg.bankroll;
        if (cfg.roundTo > 0) st = std::floor(st / cfg.roundTo) * cfg.roundTo;
        if (st < cfg.minStake || st <= 0) { st = 0; a.notes.push_back("The suggested stake is below your minimum stake, so no bet is advised."); }
        a.stake = st;
        a.returnIfWin = st * odds; a.profitIfWin = st * (odds - 1.0);
    }

    // ---- explanation
    a.notes.insert(a.notes.begin(), "Model: " + pct(a.modelProb) + " (break-even odds " + num(a.fairOdds) + "). The odds you entered imply " + pct(a.impliedProb) + ".");
    a.notes.push_back(std::string(known ? "Bookmaker margin on this market: " : "Assumed bookmaker margin: ") + pct(overround) + ".");
    switch (a.verdict) {
        case Verdict::Value:
            a.headline = "Value: expected profit " + num(a.ev * 100, 1) + "% per unit staked.";
            a.notes.push_back("It stays positive (" + num(a.evShrunk * 100, 1) + "%) even if the market is partly right, so it passes the caution check.");
            break;
        case Verdict::Marginal:
            a.headline = !inRange ? "Marginal: the odds are outside your allowed range." : "Marginal: a small edge that is within model error.";
            a.notes.push_back("Model expected value is " + num(a.ev * 100, 1) + "%, but it must reach " + num(risk.minExpectedValue * 100, 1) + "% and survive the caution check (" + num(a.evShrunk * 100, 1) + "%) to count as value.");
            a.notes.push_back("Value starts at odds of about " + num(a.valueFromOdds) + ".");
            break;
        default:
            a.headline = "No value: you would lose " + num(-a.ev * 100, 1) + "% per unit staked on average.";
            a.notes.push_back("You would need odds of about " + num(a.valueFromOdds) + " or better.");
            break;
    }
    if (s.pPush > 1e-9) a.notes.push_back("If this exact result happens (" + pct(s.pPush) + ") your stake is returned.");
    return a;
}

// ------------------------------------------------------------------------------------------------------ slip
SlipResult evaluateSlip(const std::vector<SlipLeg>& legs, double stake, const StakeSettings& cfg, const RiskConfig& risk, double shrink) {
    SlipResult r;
    r.legs = static_cast<int>(legs.size());
    if (legs.empty()) { r.problem = "Add at least one selection."; return r; }
    std::set<std::string> matches;
    double odds = 1.0, prob = 1.0, probShrunk = 1.0;
    shrink = std::clamp(shrink, 0.0, 1.0);
    for (const auto& l : legs) {
        if (!std::isfinite(l.odds) || l.odds <= 1.0) { r.problem = "Enter valid odds (above 1.00) for \"" + l.selectionLabel + "\"."; return r; }
        if (!matches.insert(l.matchKey).second) { r.problem = "Two selections are from the same match (" + l.matchLabel + "). Bookmakers price those together, so they can't be multiplied here. Keep one per match."; return r; }
        if (legs.size() > 1 && l.pOther > 1e-9) { r.problem = "\"" + l.selectionLabel + "\" can end in a refund or half result, which this slip calculator can't combine. Use it as a single."; return r; }
        odds *= l.odds; prob *= l.pWin;
        const double m = (1.0 / l.odds) / 1.05;                                  // assumed 5% margin per leg
        probShrunk *= shrink * l.pWin + (1.0 - shrink) * m;
    }
    r.valid = true;
    r.combinedOdds = odds; r.combinedProb = prob;
    r.stake = std::max(0.0, stake);
    r.returnIfWin = r.stake * odds; r.profitIfWin = r.stake * (odds - 1.0);
    if (legs.size() == 1) r.evPerUnit = legs[0].pWin * (odds - 1.0) - legs[0].pLose;   // refunds count as zero
    else r.evPerUnit = prob * odds - 1.0;
    r.evShrunkPerUnit = (legs.size() == 1 && legs[0].pOther > 1e-9) ? r.evPerUnit : probShrunk * odds - 1.0;
    r.evMoney = r.evPerUnit * r.stake;

    const bool value = r.evPerUnit >= risk.minExpectedValue && r.evShrunkPerUnit > 0 && odds >= risk.minOdds;
    r.verdict = value ? Verdict::Value : (r.evPerUnit > 0 ? Verdict::Marginal : Verdict::NoValue);
    if (value && cfg.bankroll > 0) {
        Selection s; s.pWin = probShrunk; s.pLose = 1.0 - probShrunk;                  // Kelly on the cautious probability
        const double f = std::min(kellyFraction(s, odds) * std::clamp(cfg.kellyFraction, 0.0, 1.0), cfg.maxStakeFraction);
        double st = f * cfg.bankroll;
        if (cfg.roundTo > 0) st = std::floor(st / cfg.roundTo) * cfg.roundTo;
        r.suggestedStake = st >= cfg.minStake ? st : 0.0;
    }

    char b[160];
    if (legs.size() >= 2) {
        std::snprintf(b, sizeof b, "Chance that all %d selections win: %.1f%%. A single slip like this loses far more often than it wins.", r.legs, prob * 100.0);
        r.warnings.push_back(b);
        r.warnings.push_back("Every extra selection multiplies the bookmaker's margin, so accumulators are usually worse value than singles.");
    }
    if (cfg.bankroll > 0 && r.stake > cfg.bankroll) { r.valid = false; r.problem = "The stake is larger than your bankroll."; return r; }
    if (cfg.bankroll > 0 && r.stake > cfg.maxStakeFraction * cfg.bankroll + 1e-9) {
        std::snprintf(b, sizeof b, "This stake is %.1f%% of your bankroll, above your %.1f%% limit per bet.", r.stake / cfg.bankroll * 100.0, cfg.maxStakeFraction * 100.0);
        r.warnings.push_back(b);
    }
    return r;
}

}  // namespace sba
