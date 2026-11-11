// Self-contained test runner for the Qt-free core (no test framework needed).
// Run from the project root:  ctest --output-on-failure   (or ./sba_tests)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>

#include "core/Csv.h"
#include "core/Database.h"
#include "core/DateUtil.h"
#include "core/Dataset.h"
#include "core/FeatureEngineer.h"
#include "core/Markets.h"
#include "core/Metrics.h"
#include "core/OnnxInference.h"
#include "core/PoissonModel.h"
#include "core/PredictionService.h"
#include "core/PredictionStore.h"
#include "core/ReasoningEngine.h"
#include "core/Resources.h"
#include "core/TeamMapper.h"
#include "core/TfIdfIndex.h"

using namespace sba;
namespace fs = std::filesystem;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond)                                                                    \
    do { if (cond) ++g_pass; else { ++g_fail; std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol)                                                          \
    do { const double _a = (a), _b = (b); if (std::fabs(_a - _b) <= (tol)) ++g_pass;   \
         else { ++g_fail; std::printf("    FAIL %s:%d  %s=%.8g vs %s=%.8g\n", __FILE__, __LINE__, #a, _a, #b, _b); } } while (0)

static std::map<std::string, std::function<void()>>& registry() { static std::map<std::string, std::function<void()>> r; return r; }
struct Reg { Reg(const char* n, std::function<void()> f) { registry()[n] = std::move(f); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()

static std::string g_root = SBA_SOURCE_DIR;
static ResourcePaths g_paths;
static PredictionService& service() {
    static PredictionService s; static bool init = false;
    if (!init) { init = true; s.initialise(g_paths); }
    return s;
}

// ---------------------------------------------------------------- basics
TEST(csv_and_dates) {
    auto v = splitCsvLine("a,\"b,c\",\"d\"\"e\",,f\r");
    CHECK(v.size() == 5 && v[1] == "b,c" && v[2] == "d\"e" && v[3].empty() && v[4] == "f");
    CHECK(std::isnan(parseDouble("")) && std::isnan(parseDouble("abc")) && parseDouble("2.5") == 2.5);
    int s = 0; CHECK(parseIsoDate("2026-10-30", s)); CHECK(isoFromSerial(s) == "2026-10-30");
    CHECK(!parseIsoDate("30/10/2026", s)); CHECK(!parseIsoDate("2026-13-01", s));
    int a, b; parseIsoDate("2024-02-28", a); parseIsoDate("2024-03-01", b); CHECK(b - a == 2);  // leap year
    CHECK(seasonStartYearFor(b) == 2023);
}

TEST(odds_math) {
    const double o[3] = {2.0, 3.5, 4.0}; double p[3];
    CHECK(marketProbabilities(o, p));
    CHECK_NEAR(p[0] + p[1] + p[2], 1.0, 1e-12);
    CHECK(p[0] > p[1] && p[1] > p[2]);
    const double bad1[3] = {2.0, 1.0, 4.0}, bad2[3] = {2.0, kNaN, 4.0};
    CHECK(!marketProbabilities(bad1, p)); CHECK(!marketProbabilities(bad2, p));
}

TEST(metrics) {
    std::vector<Probs3> u(3, Probs3{1 / 3.0, 1 / 3.0, 1 / 3.0});
    auto s = scoreProbabilities(u, {0, 1, 2});
    CHECK_NEAR(s.logLoss, std::log(3.0), 1e-9);
    CHECK_NEAR(s.brier, 2.0 / 3.0, 1e-9);
    std::vector<Probs3> sure = {{1, 0, 0}, {0, 1, 0}};
    auto s2 = scoreProbabilities(sure, {0, 1}); CHECK_NEAR(s2.accuracy, 1.0, 1e-12); CHECK(s2.logLoss < 1e-9);
    auto cal = calibration(sure, {0, 1}, 10); int total = 0; for (auto& b : cal) total += b.count; CHECK(total == 6);
    auto cm = confusion({0, 1, 2, 2}, {0, 1, 1, 2}); CHECK(cm.m[1][2] == 1 && cm.m[0][0] == 1);
}

TEST(team_mapper) {
    std::vector<std::string> t = {"Man City", "Man United", "Newcastle", "Nott'm Forest", "Wolves", "Brighton", "West Brom", "West Ham", "Tottenham", "Arsenal", "Leeds"};
    CHECK(mapTeamName("Manchester City FC", t) == "Man City");
    CHECK(mapTeamName("Manchester United FC", t) == "Man United");
    CHECK(mapTeamName("Newcastle United FC", t) == "Newcastle");
    CHECK(mapTeamName("Nottingham Forest FC", t) == "Nott'm Forest");
    CHECK(mapTeamName("Wolverhampton Wanderers FC", t) == "Wolves");
    CHECK(mapTeamName("Brighton & Hove Albion FC", t) == "Brighton");
    CHECK(mapTeamName("West Bromwich Albion FC", t) == "West Brom");
    CHECK(mapTeamName("West Ham United FC", t) == "West Ham");
    CHECK(mapTeamName("Tottenham Hotspur FC", t) == "Tottenham");
    CHECK(mapTeamName("Arsenal FC", t) == "Arsenal");
    CHECK(mapTeamName("Real Madrid", t).empty());
}

// ---------------------------------------------------------------- data + models
TEST(dataset_cleaning) {
    auto& s = service();
    CHECK(s.status().dataset);
    CHECK(s.dataset().matches().size() == 3500);
    CHECK(s.dataset().teams().size() == 34);
    for (size_t i = 1; i < s.dataset().matches().size(); ++i) CHECK(s.dataset().matches()[i - 1].date <= s.dataset().matches()[i].date);
    // malformed inputs must fail cleanly, not crash
    const std::string tmp = (fs::temp_directory_path() / "sba_bad.csv").string();
    { std::ofstream o(tmp); o << "Date,HomeTeam\n2020-01-01,Arsenal\n"; }
    Dataset d; CHECK(!d.load(tmp)); CHECK(!d.error().empty());
    CHECK(!d.load("/nonexistent/file.csv"));
    { std::ofstream o(tmp); o << "Date,HomeTeam,AwayTeam,FTHG,FTAG,FTR\n2020-01-01,A,B,1,0,H\n"; }
    CHECK(!d.load(tmp));  // too few rows
    fs::remove(tmp);
}

TEST(dataset_merge_updates) {
    // A fresh football-data.co.uk style download (dd/mm/yyyy dates) must add only genuinely new matches.
    int ser = 0;
    CHECK(parseFlexibleDate("2026-11-01", ser) && parseFlexibleDate("01/11/2026", ser) && ser == daysFromCivil(2026, 11, 1));
    CHECK(parseFlexibleDate("01/11/26", ser) && ser == daysFromCivil(2026, 11, 1));
    CHECK(!parseFlexibleDate("31/13/2026", ser) && !parseFlexibleDate("garbage", ser));

    Dataset d; CHECK(d.load(g_paths.dataCsv));
    const size_t before = d.matches().size();
    const int lastBefore = d.lastDate();
    const HistMatch last = d.matches().back();
    int y, m, dd; civilFromDays(last.date, y, m, dd);
    char dup[64]; std::snprintf(dup, sizeof dup, "%02d/%02d/%04d", dd, m, y);
    civilFromDays(lastBefore + 7, y, m, dd);
    char fresh[64]; std::snprintf(fresh, sizeof fresh, "%02d/%02d/%04d", dd, m, y);
    const std::string tmp = (fs::temp_directory_path() / "sba_update.csv").string();
    {
        std::ofstream o(tmp);
        o << "Div,Date,Time,HomeTeam,AwayTeam,FTHG,FTAG,FTR,HS,AS,HST,AST,HC,AC\n";
        o << "E0," << dup << ",15:00," << last.home << "," << last.away << "," << last.fthg << "," << last.ftag << "," << last.ftr << ",,,,,,\n";   // already present
        o << "E0," << fresh << ",17:30,Arsenal,Liverpool,0,1,A,12,8,3,4,6,2\n";                                                                     // new
        o << "E0," << fresh << ",15:00,Everton,Fulham,2,2,H,,,,,,\n";                                                                                 // inconsistent result: dropped
        o << "E0," << fresh << ",15:00,Chelsea,Wolves,,,,,,,,,,\n";                                                                                   // not played yet: dropped
    }
    CHECK(d.mergeCsv(tmp) == 1);
    CHECK(d.matches().size() == before + 1);
    CHECK(d.lastDate() == lastBefore + 7);
    CHECK(d.matches().back().home == "Arsenal" && d.matches().back().away == "Liverpool" && d.matches().back().ftr == 'A');
    CHECK(d.matches().back().timeMin == 17 * 60 + 30 && d.matches().back().stat[0][0] == 12);
    CHECK(d.mergeCsv(tmp) == 0);                              // merging the same file twice adds nothing
    CHECK(d.mergeCsv("/nonexistent/file.csv") == -1);
    for (size_t i = 1; i < d.matches().size(); ++i) CHECK(d.matches()[i - 1].date <= d.matches()[i].date);
    fs::remove(tmp);
}

// ---- betting markets ---------------------------------------------------------------------------------------------
static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

TEST(markets_are_consistent) {
    const Probs3 target{0.50, 0.26, 0.24};
    const MarketBook b = buildMarketBook("Home FC", "Away FC", 1.65, 1.10, target);
    CHECK(b.ok && b.groups.size() >= 11);
    CHECK(near(b.result[0], target[0], 1e-9) && near(b.result[1], target[1], 1e-9) && near(b.result[2], target[2], 1e-9));   // grid matches the blended 1X2
    ScoreGrid g = ScoreGrid::fromLambdas(1.65, 1.10); CHECK(near(g.sum(), 1.0));
    // every selection: outcome probabilities form a distribution
    for (const auto& grp : b.groups) for (const auto& s : grp.items) {
        CHECK(near(s.pWin + s.pHalfWin + s.pPush + s.pHalfLose + s.pLose, 1.0, 1e-9));
        CHECK(s.pWin >= 0 && s.pLose >= 0);
    }
    auto P = [&](const char* k) { const Selection* s = b.find(k); CHECK(s != nullptr); return s ? s->pWin : -1.0; };
    CHECK(near(P("r:1") + P("r:X") + P("r:2"), 1.0));
    CHECK(near(P("dc:1X"), P("r:1") + P("r:X")) && near(P("dc:12"), P("r:1") + P("r:2")) && near(P("dc:X2"), P("r:X") + P("r:2")));
    for (const char* line : {"0.5", "1.5", "2.5", "3.5", "4.5"}) CHECK(near(P((std::string("ou:") + line + ":o").c_str()) + P((std::string("ou:") + line + ":u").c_str()), 1.0));
    CHECK(P("ou:0.5:o") > P("ou:1.5:o") && P("ou:1.5:o") > P("ou:2.5:o") && P("ou:2.5:o") > P("ou:3.5:o"));            // lines get harder
    CHECK(near(P("btts:y") + P("btts:n"), 1.0) && near(P("oe:odd") + P("oe:even"), 1.0));
    double ex = 0; for (int n = 0; n <= 5; ++n) ex += P(("eg:" + std::to_string(n)).c_str()); CHECK(near(ex, 1.0));
    double cs = P("cs:other"); for (const auto& sp : b.topScores) cs += P(("cs:" + std::to_string(sp.home) + "-" + std::to_string(sp.away)).c_str()); CHECK(near(cs, 1.0));
    CHECK(near(P("cs:h:y") + P("cs:h:n"), 1.0) && near(P("wtn:h"), b.find("wtn:h")->pWin) && P("wtn:h") <= P("cs:h:y"));
    CHECK(near(P("ah:h:-0.5"), P("r:1")) && near(P("ah:a:+0.5") , P("dc:X2")));                                        // -0.5 is just "to win"; +0.5 is double chance
    CHECK(near(P("ah:h:-0.5") + P("ah:a:+0.5"), 1.0));
    const Selection* dnb = b.find("dnb:1"); CHECK(dnb && near(dnb->pPush, P("r:X")) && near(dnb->pWin, P("r:1")));
    const Selection* ah1 = b.find("ah:h:-1"); CHECK(ah1 && ah1->pPush > 0.1 && near(ah1->pWin + ah1->pPush + ah1->pLose, 1.0));
    // the stronger side is favoured everywhere it should be
    CHECK(P("r:1") > P("r:2") && P("tt:h:1.5:o") > P("tt:a:1.5:o") && P("ah:h:-0.5") > P("ah:a:-0.5"));
    // a pure Poisson book (no 1X2 target) still works
    CHECK(buildMarketBook("H", "A", 1.4, 1.2, Probs3{}).ok);
    CHECK(!buildMarketBook("H", "A", kNaN, 1.2, target).ok && !buildMarketBook("H", "A", 0.0, 1.2, target).ok);
    CHECK(b.expectedTotal > 2.7 && b.expectedTotal < 2.8);
}

TEST(markets_settlement) {
    auto spec = [](BetKind k, int side, double line, bool over, int a = 0, int bb = 0) { BetSpec s; s.kind = k; s.side = side; s.line = line; s.over = over; s.a = a; s.b = bb; return s; };
    // 2-1 home win
    CHECK(evaluate(spec(BetKind::Total, 0, 2.5, true), 2, 1) == BetResult::Win && evaluate(spec(BetKind::Total, 0, 2.5, false), 2, 1) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::Total, 0, 3.0, true), 2, 1) == BetResult::Push);                                  // whole line: refund
    CHECK(evaluate(spec(BetKind::Total, 0, 2.75, true), 2, 1) == BetResult::HalfWin);                              // 2.5 wins, 3.0 refunds
    CHECK(evaluate(spec(BetKind::Total, 0, 3.25, true), 2, 1) == BetResult::HalfLose);                             // 3.0 refunds, 3.5 loses
    CHECK(evaluate(spec(BetKind::Btts, 0, 0, true), 2, 1) == BetResult::Win && evaluate(spec(BetKind::Btts, 0, 0, false), 2, 1) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::Btts, 0, 0, true), 2, 0) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::Handicap, 0, -1.0, true), 2, 1) == BetResult::Push);                              // won by exactly the handicap
    CHECK(evaluate(spec(BetKind::Handicap, 0, -0.5, true), 2, 1) == BetResult::Win);
    CHECK(evaluate(spec(BetKind::Handicap, 0, -1.5, true), 2, 1) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::Handicap, 0, -0.75, true), 2, 1) == BetResult::HalfWin);                          // quarter line: -0.5 half wins, -1.0 half is refunded
    CHECK(evaluate(spec(BetKind::Handicap, 2, 1.0, true), 2, 1) == BetResult::Push);                               // away +1 loses by exactly one
    CHECK(evaluate(spec(BetKind::Handicap, 2, 0.25, true), 1, 1) == BetResult::HalfWin);                           // draw on +0.25: 0 refunds, +0.5 wins
    CHECK(evaluate(spec(BetKind::Handicap, 0, -0.25, true), 1, 1) == BetResult::HalfLose);
    CHECK(evaluate(spec(BetKind::DoubleChance, 0, 0, true), 1, 1) == BetResult::Win && evaluate(spec(BetKind::DoubleChance, 0, 0, true), 0, 1) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::DrawNoBet, 0, 0, true), 1, 1) == BetResult::Push && evaluate(spec(BetKind::DrawNoBet, 0, 0, true), 1, 0) == BetResult::Win);
    CHECK(evaluate(spec(BetKind::TeamTotal, 1, 0.5, true), 2, 0) == BetResult::Lose && evaluate(spec(BetKind::TeamTotal, 0, 1.5, true), 2, 0) == BetResult::Win);
    CHECK(evaluate(spec(BetKind::ExactTotal, 0, 0, true, 3, 0), 2, 1) == BetResult::Win && evaluate(spec(BetKind::ExactTotal, 0, 0, true, 5, 1), 4, 2) == BetResult::Win);
    CHECK(evaluate(spec(BetKind::CorrectScore, 0, 0, true, 2, 1), 2, 1) == BetResult::Win && evaluate(spec(BetKind::CorrectScore, 0, 0, true, 2, 1), 1, 2) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::WinToNil, 0, 0, true), 2, 0) == BetResult::Win && evaluate(spec(BetKind::WinToNil, 0, 0, true), 2, 1) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::CleanSheet, 1, 0, true), 2, 0) == BetResult::Lose && evaluate(spec(BetKind::CleanSheet, 1, 0, false), 2, 0) == BetResult::Win);
    CHECK(evaluate(spec(BetKind::OddEven, 0, 0, true), 2, 1) == BetResult::Win && evaluate(spec(BetKind::OddEven, 0, 0, true), 1, 1) == BetResult::Lose);
    CHECK(evaluate(spec(BetKind::ResultAndTotal, 0, 2.5, true), 2, 1) == BetResult::Win && evaluate(spec(BetKind::ResultAndTotal, 0, 2.5, true), 1, 0) == BetResult::Lose);
    CHECK(near(payoffPerUnit(BetResult::Win, 2.5), 1.5) && near(payoffPerUnit(BetResult::HalfWin, 2.5), 0.75) && near(payoffPerUnit(BetResult::HalfLose, 2.5), -0.5) && near(payoffPerUnit(BetResult::Push, 2.5), 0.0));
}

TEST(markets_staking_and_advice) {
    Selection s; s.pWin = 0.55; s.pLose = 0.45;
    CHECK(near(kellyFraction(s, 2.0), 0.10, 1e-6));                         // (b p - q)/b with b = 1
    CHECK(near(kellyFraction(s, 1.5), 0.0));                                // negative expected value -> no stake
    Selection pu; pu.pWin = 0.45; pu.pPush = 0.20; pu.pLose = 0.35;         // refund scenario: f = EV / (b (1 - push))
    CHECK(near(kellyFraction(pu, 2.2), (0.45 * 1.2 - 0.35) / (1.2 * 0.80), 1e-6));
    CHECK(near(s.expectedValue(2.0), 0.10) && near(s.oddsForEv(0.0), 1.0 + 0.45 / 0.55) && near(s.expectedValue(s.oddsForEv(0.05)), 0.05));
    CHECK(near(pu.expectedValue(pu.oddsForEv(0.05)), 0.05));

    RiskConfig risk; StakeSettings cfg; cfg.bankroll = 1000; cfg.kellyFraction = 0.25; cfg.maxStakeFraction = 0.05; cfg.roundTo = 1;
    Selection strong; strong.pWin = 0.60; strong.pLose = 0.40; strong.fairOdds = strong.oddsForEv(0);
    BetAdvice v = adviseBet(strong, 2.2, kNaN, cfg, risk, 0.5);
    CHECK(v.verdict == Verdict::Value && v.stake > 0 && v.stake <= 50.0 + 1e-9);                           // capped at 5% of 1000
    CHECK(near(std::fmod(v.stake, 1.0), 0.0));                                                              // rounded to whole units
    CHECK(near(v.profitIfWin, v.stake * 1.2) && near(v.returnIfWin, v.stake * 2.2));
    BetAdvice nv = adviseBet(strong, 1.55, kNaN, cfg, risk, 0.5);
    CHECK(nv.verdict == Verdict::NoValue && nv.stake == 0.0 && nv.ev < 0);
    Selection thin; thin.pWin = 0.52; thin.pLose = 0.48;
    BetAdvice mg = adviseBet(thin, 2.0, kNaN, cfg, risk, 0.5);                                              // +4% EV: below the 5% bar
    CHECK(mg.verdict == Verdict::Marginal && mg.stake == 0.0);
    CHECK(adviseBet(strong, kNaN, kNaN, cfg, risk, 0.5).verdict == Verdict::NoOdds && adviseBet(strong, 1.0, kNaN, cfg, risk, 0.5).verdict == Verdict::Invalid);
    StakeSettings nobank = cfg; nobank.bankroll = 0;
    BetAdvice nb = adviseBet(strong, 2.2, kNaN, nobank, risk, 0.5);
    CHECK(nb.verdict == Verdict::Value && nb.stake == 0.0 && nb.stakeFraction > 0);                         // no bankroll: percentage only
    // trusting the market more can only make a bet look worse
    CHECK(adviseBet(strong, 2.2, kNaN, cfg, risk, 0.2).evShrunk < adviseBet(strong, 2.2, kNaN, cfg, risk, 0.8).evShrunk);
    // a known two-way margin is measured exactly
    BetAdvice tw = adviseBet(strong, 2.0, 1.9, cfg, risk, 0.5); CHECK(tw.notes.size() >= 2);
    StakeSettings minst = cfg; minst.minStake = 100;
    CHECK(adviseBet(strong, 2.2, kNaN, minst, risk, 0.5).stake == 0.0);                                    // below the minimum stake
}

TEST(markets_bet_slip) {
    RiskConfig risk; StakeSettings cfg; cfg.bankroll = 500;
    SlipLeg a; a.matchKey = "m1"; a.matchLabel = "A v B"; a.selectionLabel = "Over 2.5"; a.pWin = 0.60; a.pLose = 0.40; a.odds = 2.0;
    SlipLeg b; b.matchKey = "m2"; b.matchLabel = "C v D"; b.selectionLabel = "BTTS"; b.pWin = 0.55; b.pLose = 0.45; b.odds = 2.0;
    SlipResult one = evaluateSlip({a}, 10, cfg, risk, 0.5);
    CHECK(one.valid && near(one.combinedOdds, 2.0) && near(one.returnIfWin, 20.0) && near(one.profitIfWin, 10.0) && near(one.evPerUnit, 0.2));
    SlipResult acc = evaluateSlip({a, b}, 10, cfg, risk, 0.5);
    CHECK(acc.valid && near(acc.combinedOdds, 4.0) && near(acc.combinedProb, 0.33) && near(acc.returnIfWin, 40.0) && near(acc.evPerUnit, 0.33 * 4.0 - 1.0) && acc.warnings.size() >= 2);
    SlipLeg same = b; same.matchKey = "m1";
    CHECK(!evaluateSlip({a, same}, 10, cfg, risk, 0.5).valid);                                             // two legs from one match are refused
    SlipLeg ah = a; ah.matchKey = "m3"; ah.pOther = 0.2;
    CHECK(!evaluateSlip({a, ah}, 10, cfg, risk, 0.5).valid && evaluateSlip({ah}, 10, cfg, risk, 0.5).valid);   // refund results: singles only
    SlipLeg bad = a; bad.odds = 1.0;
    CHECK(!evaluateSlip({bad}, 10, cfg, risk, 0.5).valid && !evaluateSlip({}, 10, cfg, risk, 0.5).valid);
    CHECK(!evaluateSlip({a}, 600, cfg, risk, 0.5).valid);                                                   // more than the bankroll
    CHECK(!evaluateSlip({a}, 100, cfg, risk, 0.5).warnings.empty());                                       // above the 5% per-bet limit
}

TEST(feature_parity_with_python) {
    auto& s = service();
    CHECK(s.status().dataset);
    FeatureEngineer fe(s.featureNames());
    const auto rows = fe.buildHistorical(s.dataset());
    CHECK(rows.size() == s.dataset().matches().size());

    CsvTable ref; std::string err;
    CHECK(loadCsv(g_root + "/ml_training/fixtures/parity_reference.csv", ref, err));
    const size_t nRef = ref.rows.size(), off = rows.size() - nRef;
    double worst = 0; std::string worstName; int nanMismatch = 0;
    for (size_t r = 0; r < nRef; ++r) {
        const auto& m = s.dataset().matches()[off + r];
        CHECK(ref.rows[r][ref.col("HomeTeam")] == m.home);
        CHECK(ref.rows[r][ref.col("Date")] == isoFromSerial(m.date));
        for (size_t f = 0; f < s.featureNames().size(); ++f) {
            const double pv = parseDouble(ref.rows[r][ref.col(s.featureNames()[f])]);
            const double cv = rows[off + r].values[f];
            if (std::isnan(pv) != std::isnan(cv)) { ++nanMismatch; continue; }
            if (std::isnan(pv)) continue;
            const double d = std::fabs(pv - cv) / std::max(1.0, std::fabs(pv));
            if (d > worst) { worst = d; worstName = s.featureNames()[f]; }
        }
    }
    std::printf("    feature parity: %zu rows x %zu features, worst relative diff %.2e (%s), NaN mismatches %d\n",
                nRef, s.featureNames().size(), worst, worstName.c_str(), nanMismatch);
    CHECK(nanMismatch == 0);
    CHECK(worst < 1e-5);
}

TEST(upcoming_equals_historical) {
    auto& s = service();
    FeatureEngineer fe(s.featureNames());
    const auto rows = fe.buildHistorical(s.dataset());
    const auto& all = s.dataset().matches();
    int checked = 0;
    for (size_t i = all.size() - 40; i < all.size(); ++i) {
        MatchQuery q; q.home = all[i].home; q.away = all[i].away; q.date = all[i].date;
        for (int k = 0; k < 3; ++k) q.odds[k] = all[i].odds[k];
        FeatureRow fr; std::string err;
        CHECK(fe.buildUpcoming(s.dataset(), q, fr, err));
        for (size_t f = 0; f < fr.values.size(); ++f) {
            const float a = fr.values[f], b = rows[i].values[f];
            CHECK((std::isnan(a) && std::isnan(b)) || std::fabs(a - b) <= 1e-4f * std::max(1.0f, std::fabs(b)));
        }
        ++checked;
    }
    CHECK(checked == 40);
    FeatureRow fr; std::string err; MatchQuery q; q.home = "Real Madrid"; q.away = "Arsenal"; q.date = all.back().date + 5;
    CHECK(!fe.buildUpcoming(s.dataset(), q, fr, err)); CHECK(!err.empty());
}

TEST(models_match_python) {
    auto& s = service();
    CHECK(s.status().poisson);
    CHECK(OnnxInference::compiledWithOnnx() == s.status().rf || !OnnxInference::compiledWithOnnx());
    CsvTable ref; std::string err; loadCsv(g_root + "/ml_training/fixtures/parity_reference.csv", ref, err);
    PoissonModel pm; CHECK(pm.load(g_paths.poissonJson, err));
    FeatureEngineer fe(s.featureNames());
    const auto rows = fe.buildHistorical(s.dataset());
    const size_t off = rows.size() - ref.rows.size();
    OnnxInference onnx; bool haveRf = OnnxInference::compiledWithOnnx() && onnx.load(g_paths.onnxModel, s.featureNames().size(), err);
    if (OnnxInference::compiledWithOnnx()) CHECK(haveRf);
    std::vector<std::vector<float>> X; for (size_t r = 0; r < ref.rows.size(); ++r) X.push_back(rows[off + r].values);
    std::vector<Probs3> rf; if (haveRf) CHECK(onnx.predictBatch(X, rf, err));
    double maxRf = 0, maxPo = 0, maxLam = 0;
    for (size_t r = 0; r < ref.rows.size(); ++r) {
        auto po = pm.predict(X[r]);
        const double pyPo[3] = {parseDouble(ref.rows[r][ref.col("po_h")]), parseDouble(ref.rows[r][ref.col("po_d")]), parseDouble(ref.rows[r][ref.col("po_a")])};
        const double pyRf[3] = {parseDouble(ref.rows[r][ref.col("rf_h")]), parseDouble(ref.rows[r][ref.col("rf_d")]), parseDouble(ref.rows[r][ref.col("rf_a")])};
        for (int k = 0; k < 3; ++k) {
            maxPo = std::max(maxPo, std::fabs(po.probs[k] - pyPo[k]));
            if (haveRf) maxRf = std::max(maxRf, std::fabs(rf[r][k] - pyRf[k]));
        }
        maxLam = std::max(maxLam, std::fabs(po.lambdaHome - parseDouble(ref.rows[r][ref.col("lam_h")])));
        CHECK_NEAR(po.probs[0] + po.probs[1] + po.probs[2], 1.0, 1e-9);
    }
    std::printf("    Poisson max prob gap %.2e, lambda gap %.2e, RF(ONNX) max prob gap %.2e (%s)\n", maxPo, maxLam, maxRf, haveRf ? "C++ ONNX Runtime" : "ONNX not compiled in");
    CHECK(maxPo < 1e-4); CHECK(maxLam < 1e-4);
    if (haveRf) CHECK(maxRf < 1e-4);
}

TEST(service_end_to_end) {
    auto& s = service();
    AnalysisRequest r; r.home = "Arsenal"; r.away = "Chelsea"; r.kickoffDateIso = "2026-11-07";
    r.hasOdds = true; r.odds.manual = true; r.odds.source = "manual"; r.odds.odds[0] = 1.9; r.odds.odds[1] = 3.6; r.odds.odds[2] = 4.2;
    Analysis a; std::string err;
    CHECK(s.analyse(r, a, err));
    const auto& p = a.reasoning.model;
    CHECK_NEAR(p[0] + p[1] + p[2], 1.0, 1e-6);
    for (double v : p) CHECK(v > 0 && v < 1);
    CHECK(a.reasoning.hasMarket); CHECK_NEAR(a.reasoning.market[0] + a.reasoning.market[1] + a.reasoning.market[2], 1.0, 1e-9);
    CHECK(!a.reasoning.supporting.empty() && !a.reasoning.counter.empty());
    std::printf("    Arsenal v Chelsea: H %.1f%% D %.1f%% A %.1f%% -> %s\n", p[0] * 100, p[1] * 100, p[2] * 100, a.reasoning.headline.c_str());
    r.home = "Nope FC"; CHECK(!s.analyse(r, a, err));
    r.home = "Arsenal"; r.kickoffDateIso = "garbage"; CHECK(!s.analyse(r, a, err));
    r.kickoffDateIso = "2026-11-07"; r.away = "Arsenal"; CHECK(!s.analyse(r, a, err));
}

TEST(backtest_report) {
    auto& s = service();
    auto rep = s.runBacktest();
    CHECK(rep.ok);
    std::printf("    backtest %s..%s, %d matches, RF weight %.2f\n", rep.from.c_str(), rep.to.c_str(), rep.matches, rep.rfWeight);
    for (auto& m : rep.models) std::printf("      %-32s acc %.3f  logloss %.4f  brier %.4f  (n=%d)\n", m.name.c_str(), m.scores.accuracy, m.scores.logLoss, m.scores.brier, m.scores.n);
    for (auto& b : rep.betting) std::printf("      %-60s bets %d  ROI %.1f%% +/- %.1f%% (1 s.e.)  maxDD %.1f\n", b.rule.c_str(), b.bets, std::isnan(b.roi) ? 0.0 : b.roi * 100, std::isnan(b.roiStdErr) ? 0.0 : b.roiStdErr * 100, b.maxDrawdown);
    CHECK(rep.matches == 522);
    CHECK(rep.models.size() >= 4);
    for (auto& m : rep.models) CHECK(std::isfinite(m.scores.logLoss) && m.scores.logLoss > 0.5 && m.scores.logLoss < 1.5);
    int cm = 0; for (auto& r : rep.confusionPlain.m) for (int v : r) cm += v; CHECK(cm == rep.matches);
    CHECK(rep.betting.size() == 2);
    CHECK(rep.betting[1].bets <= rep.betting[0].bets);   // stricter rules never bet more
    CHECK(std::isfinite(rep.quality.modelLogLoss) && std::isfinite(rep.quality.marketLogLoss));
    // goals markets: validated on the same held-out matches
    const auto& g = rep.goals;
    std::printf("    goals markets (n=%d, %d with bookmaker totals): O/U 2.5 logloss model %.4f | bookmaker %.4f | base rate %.4f ; predicted over %.3f vs actual %.3f ; BTTS model %.4f | base %.4f ; predicted %.3f vs actual %.3f\n",
                g.matches, g.marketMatches, g.ouModelLogLoss, g.ouMarketLogLoss, g.ouBaseLogLoss, g.ouMeanPredicted, g.ouActualRate, g.bttsModelLogLoss, g.bttsBaseLogLoss, g.bttsMeanPredicted, g.bttsActualRate);
    CHECK(g.ran() && g.matches == rep.matches && g.marketMatches > 300);
    CHECK(std::isfinite(g.ouModelLogLoss) && std::isfinite(g.ouMarketLogLoss) && std::isfinite(g.ouBaseLogLoss) && std::isfinite(g.bttsModelLogLoss) && std::isfinite(g.bttsBaseLogLoss));
    CHECK(g.ouModelLogLoss > 0.55 && g.ouModelLogLoss < 0.80 && g.bttsModelLogLoss > 0.55 && g.bttsModelLogLoss < 0.80);
    CHECK(std::fabs(g.ouMeanPredicted - g.ouActualRate) < 0.08 && std::fabs(g.bttsMeanPredicted - g.bttsActualRate) < 0.08);   // roughly calibrated on average
    s.setGoalsQuality(g);
    CHECK(s.goalsShrink() <= 0.5 && s.goalsShrink() >= 0.2);
}

// ---------------------------------------------------------------- reasoning
static Evidence baseEvidence() {
    Evidence e; e.home = "A"; e.away = "B"; e.rfAvailable = true;
    e.rf = {0.52, 0.26, 0.22}; e.poisson = {0.50, 0.27, 0.23}; e.blended = {0.51, 0.265, 0.225};
    e.totalFeatures = 58; e.missingFeatures = 0; e.homeMatchesPlayed = 60; e.awayMatchesPlayed = 60;
    e.hasOdds = true; e.odds.manual = true; e.odds.source = "test"; e.odds.odds[0] = 2.30; e.odds.odds[1] = 3.4; e.odds.odds[2] = 3.6;
    e.nowEpoch = 1000000; e.kickoffEpoch = 1000000 + 7200;
    return e;
}

TEST(reasoning_rules) {
    ReasoningEngine eng;
    { auto r = eng.analyse(baseEvidence());
      CHECK(r.decision == Decision::PossibleValue); CHECK(r.candidate == 0); CHECK(r.ev[0] > 0.05); }
    { auto e = baseEvidence(); e.rf = {0.60, 0.20, 0.20}; e.poisson = {0.42, 0.30, 0.28};   // models disagree
      auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); CHECK(!r.withheld.empty()); }
    { auto e = baseEvidence(); e.odds.odds[0] = 1.85; e.odds.odds[1] = 3.3; e.odds.odds[2] = 4.0;     // priced in
      auto r = eng.analyse(e); CHECK(r.decision == Decision::NoRecommendation); }
    { auto e = baseEvidence(); e.odds.manual = false; e.odds.fetchedAtEpoch = e.nowEpoch - 3 * 3600;   // stale
      auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); CHECK(r.oddsStale); }
    { auto e = baseEvidence(); e.odds.manual = false; e.odds.fetchedAtEpoch = e.nowEpoch - 600;        // fresh
      auto r = eng.analyse(e); CHECK(!r.oddsStale); CHECK(r.decision == Decision::PossibleValue); }
    { auto e = baseEvidence(); e.matchStarted = true; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.kickoffEpoch = e.nowEpoch - 60; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.hasOdds = false; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); CHECK(!r.hasMarket); }
    { auto e = baseEvidence(); e.odds.odds[1] = 0.9; auto r = eng.analyse(e); CHECK(!r.hasMarket); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.blended = {0.7, 0.5, 0.4}; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }  // sums to 1.6
    { auto e = baseEvidence(); e.missingFeatures = 20; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.homeMatchesPlayed = 2; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.rfAvailable = false; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.home = e.away; auto r = eng.analyse(e); CHECK(r.decision == Decision::InsufficientEvidence); }
    { auto e = baseEvidence(); e.quality.modelLogLoss = 1.02; e.quality.marketLogLoss = 1.01; e.quality.matches = 522;
      auto r = eng.analyse(e); bool found = false; for (auto& c : r.counter) if (c.find("did not beat") != std::string::npos) found = true; CHECK(found); }
    { auto r = eng.analyse(baseEvidence()); CHECK(r.model[r.mostLikely] >= r.model[1] && r.model[r.mostLikely] >= r.model[2]); }
}

// ---------------------------------------------------------------- tf-idf
TEST(tfidf_search) {
    TfIdfIndex idx;
    idx.build(g_root);
    CHECK(idx.chunkCount() > 50); CHECK(idx.fileCount() > 15);
    auto hits = idx.search("Where is ONNX initialized?", 5);
    CHECK(!hits.empty());
    if (!hits.empty()) std::printf("    'Where is ONNX initialized?' -> %s:%d-%d (score %.3f)\n", hits[0].file.c_str(), hits[0].startLine, hits[0].endLine, hits[0].score);
    bool onnx = false; for (size_t i = 0; i < hits.size() && i < 3; ++i) if (hits[i].file.find("OnnxInference") != std::string::npos) onnx = true;
    CHECK(onnx);
    auto h2 = idx.search("sqlite prepared statement bind", 3); CHECK(!h2.empty() && h2[0].file.find("Database") != std::string::npos);
    auto h3 = idx.search("odds margin overround", 5); CHECK(!h3.empty());
    CHECK(idx.search(std::string("zzz") + "xqv" + "plugh", 3).empty());
    for (auto& h : idx.search("config", 50)) { CHECK(h.file.find("build/") != 0 && h.file.find("venv/") != 0 && h.file.find(".git/") == std::string::npos); }

    // secrets and build folders are never indexed; changes trigger a rebuild
    const fs::path tmp = fs::temp_directory_path() / "sba_idx_test";
    fs::remove_all(tmp); fs::create_directories(tmp / "build"); fs::create_directories(tmp / "src");
    { std::ofstream(tmp / "src/ok.cpp") << "int uniquewidgetfunction() { return 1; }\n"; }
    { std::ofstream(tmp / "secrets.json") << "{\"apiKey\": \"supersecretvalue123\"}\n"; }
    { std::ofstream(tmp / ".env") << "TOKEN=hiddentokenvalue\n"; }
    { std::ofstream(tmp / "build/gen.cpp") << "int generatedjunkfunction() {}\n"; }
    TfIdfIndex t; t.build(tmp.string());
    CHECK(t.fileCount() == 1);
    CHECK(!t.search("uniquewidgetfunction").empty());
    CHECK(t.search("supersecretvalue123").empty()); CHECK(t.search("generatedjunkfunction").empty()); CHECK(t.search("hiddentokenvalue").empty());
    CHECK(!t.refreshIfChanged(tmp.string()));
    { std::ofstream(tmp / "src/new.md") << "# Notes\nthe flux capacitor calibration\n"; }
    CHECK(t.refreshIfChanged(tmp.string())); CHECK(!t.search("flux capacitor").empty());
    fs::remove_all(tmp);
}

// ---------------------------------------------------------------- database
TEST(database_persistence) {
    const fs::path path = fs::temp_directory_path() / "sba_test.db";
    fs::remove(path); fs::remove(path.string() + "-wal"); fs::remove(path.string() + "-shm");
    PredictionRecord r; r.matchKey = "Arsenal|O'Brien'); DROP TABLE predictions;--|2026-11-07";
    r.home = "Arsenal"; r.away = "O'Brien'); DROP TABLE predictions;--"; r.createdIso = "2026-10-30T10:00:00Z"; r.modelVersion = "t";
    r.finalP = {0.5, 0.3, 0.2}; r.poisson = r.finalP; r.hasMarket = true; r.market = {0.45, 0.3, 0.25};
    r.odds[0] = 2.2; r.odds[1] = 3.4; r.odds[2] = 3.9; r.decision = decisionLabel(Decision::PossibleValue); r.candidate = 0; r.edge = 0.05; r.ev = 0.1;
    r.explanation = "line one\nline 'two' \"quoted\"";
    {
        PredictionStore st; CHECK(st.open(path.string()));
        bool created = false; auto id = st.insertPrediction(r, &created); CHECK(id > 0 && created);
        PredictionRecord r2 = r; r2.finalP = {0.1, 0.1, 0.8};
        auto id2 = st.insertPrediction(r2, &created); CHECK(id2 == id && !created);      // original is kept
        auto list = st.all(); CHECK(list.size() == 1); CHECK_NEAR(list[0].finalP[0], 0.5, 1e-12);
        CHECK(list[0].away == r.away); CHECK(list[0].explanation == r.explanation); CHECK(list[0].actual == -1);
        CHECK(!list[0].hasRf);
        CHECK(st.settle(id, 0, "2026-11-08T00:00:00Z"));
        auto s = st.summary(); CHECK(s.total == 1 && s.settled == 1 && s.hypotheticalBets == 1 && s.wins == 1); CHECK_NEAR(s.profit, 1.2, 1e-9);
        Fixture f; f.id = "42"; f.home = "Arsenal FC"; f.away = "Chelsea FC"; f.kickoffIso = "2026-11-07T15:00:00Z"; f.status = "TIMED"; f.homeDataset = "Arsenal"; f.awayDataset = "Chelsea";
        BookOdds b; b.bookmaker = "Bet365"; b.odds[0] = 1.9; b.odds[1] = 3.5; b.odds[2] = 4.2; f.books.push_back(b);
        CHECK(st.saveFixtures({f}));
    }
    {   // reopen: data survives
        PredictionStore st; CHECK(st.open(path.string()));
        CHECK(st.all().size() == 1); auto fx = st.loadFixtures(); CHECK(fx.size() == 1 && fx[0].books.size() == 1 && fx[0].books[0].bookmaker == "Bet365");
        CHECK_NEAR(fx[0].books[0].odds[2], 4.2, 1e-12);
    }
    {   // fixtures settle predictions
        PredictionStore st; st.open(path.string());
        PredictionRecord q = r; q.matchKey = "Arsenal|Chelsea|2026-12-01"; q.away = "Chelsea"; q.candidate = 1;
        st.insertPrediction(q);
        Fixture f; f.status = "FINISHED"; f.homeDataset = "Arsenal"; f.awayDataset = "Chelsea"; f.kickoffIso = "2026-12-01T15:00:00Z"; f.homeGoals = 1; f.awayGoals = 1;
        CHECK(st.settleFromFixtures({f}, "now") == 1);
    }
    {   // corrupted file is reported, not crashed on
        { std::ofstream o(fs::temp_directory_path() / "sba_garbage.db", std::ios::binary); o << "this is definitely not a sqlite database file, just text padding padding padding padding padding"; }
        PredictionStore st; CHECK(!st.open((fs::temp_directory_path() / "sba_garbage.db").string()));
        fs::remove(fs::temp_directory_path() / "sba_garbage.db");
    }
    fs::remove(path); fs::remove(path.string() + "-wal"); fs::remove(path.string() + "-shm");
}

int main(int argc, char** argv) {
    g_paths = locateResources("", g_root);
    if (!g_paths.complete()) { std::printf("Resources not found under %s/ml_training\n", g_root.c_str()); return 2; }
    const std::string only = argc > 1 ? argv[1] : "";
    for (auto& kv : registry()) {
        if (!only.empty() && kv.first != only) continue;
        const int before = g_fail;
        std::printf("[ RUN  ] %s\n", kv.first.c_str());
        try { kv.second(); } catch (const std::exception& e) { ++g_fail; std::printf("    EXCEPTION %s\n", e.what()); }
        std::printf("[ %s ] %s\n", g_fail == before ? " OK " : "FAIL", kv.first.c_str());
    }
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
