#include "FeatureEngineer.h"
#include "DateUtil.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace sba {
namespace {

// Constants mirror train_model.py
constexpr double kInitialElo = 1500.0, kPromotedElo = 1430.0, kSeasonRegression = 0.25;
constexpr double kEloK = 24.0, kHomeAdvantage = 60.0;
constexpr int kStats = 3;  // shots, shots on target, corners
const char* kStatNames[kStats] = {"shots", "shots_on_target", "corners"};

struct Rec {
    int date = 0;
    double gf = 0, ga = 0, pts = 0;
    double sf[kStats], sa[kStats];
};

struct Summary {
    double gf = 1.4, ga = 1.4, points = 1.3, gd = 0.0, wins = 0.40, draws = 0.25, wpoints = 1.3, low = 0.5;
    double sf[kStats] = {kNaN, kNaN, kNaN}, sa[kStats] = {kNaN, kNaN, kNaN};
};

double nanMean(const std::vector<const Rec*>& v, bool forSide, int k) {
    double s = 0; int n = 0;
    for (const Rec* r : v) {
        const double x = forSide ? r->sf[k] : r->sa[k];
        if (std::isfinite(x)) { s += x; ++n; }
    }
    return n ? s / n : kNaN;
}

Summary summarize(const std::vector<Rec>& hist, size_t last) {
    Summary s;
    if (hist.empty()) return s;
    const size_t from = hist.size() > last ? hist.size() - last : 0;
    std::vector<const Rec*> v;
    for (size_t i = from; i < hist.size(); ++i) v.push_back(&hist[i]);
    const double n = static_cast<double>(v.size());
    double gf = 0, ga = 0, pts = 0, wins = 0, draws = 0, low = 0, wsum = 0, wpts = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        const Rec& r = *v[i];
        gf += r.gf; ga += r.ga; pts += r.pts;
        wins += (r.pts == 3.0); draws += (r.pts == 1.0); low += ((r.gf + r.ga) <= 2.0);
        const double w = static_cast<double>(i + 1);
        wsum += w; wpts += w * r.pts;
    }
    s.gf = gf / n; s.ga = ga / n; s.points = pts / n; s.gd = (gf - ga) / n;
    s.wins = wins / n; s.draws = draws / n; s.wpoints = wpts / wsum; s.low = low / n;
    for (int k = 0; k < kStats; ++k) { s.sf[k] = nanMean(v, true, k); s.sa[k] = nanMean(v, false, k); }
    return s;
}

void schedule(const std::vector<Rec>& hist, int date, double& rest, double& busy) {
    if (hist.empty()) { rest = 7.0; busy = 0.0; return; }
    rest = std::min(30.0, static_cast<double>(date - hist.back().date));
    int b = 0;
    const size_t from = hist.size() > 8 ? hist.size() - 8 : 0;
    for (size_t i = hist.size(); i-- > from;) {
        if (date - hist[i].date <= 14) ++b; else break;
    }
    busy = b;
}

class Tracker {
public:
    double rating(const std::string& team) {
        auto it = m_elo.find(team);
        if (it == m_elo.end()) it = m_elo.emplace(team, m_newTeamElo).first;
        return it->second;
    }

    void startSeason(int season) {
        if (m_hasSeason && season != m_season) {
            for (auto& kv : m_elo) kv.second = kInitialElo + (1 - kSeasonRegression) * (kv.second - kInitialElo);
            m_newTeamElo = kPromotedElo;
        }
        m_season = season; m_hasSeason = true;
    }

    std::map<std::string, double> features(const std::string& home, const std::string& away, int date,
                                           int seasonStartDate, const double odds[3]) {
        static const std::vector<Rec> kEmpty;
        const auto& hh = hist(home, false);
        const auto& ah = hist(away, false);
        const auto& hv = venue(home, 'H');
        const auto& av = venue(away, 'A');

        const Summary h5 = summarize(hh, 5), a5 = summarize(ah, 5), h10 = summarize(hh, 10), a10 = summarize(ah, 10);
        const Summary hvs = summarize(hv, 10), avs = summarize(av, 10);
        double homeRest, homeBusy, awayRest, awayBusy;
        schedule(hh, date, homeRest, homeBusy);
        schedule(ah, date, awayRest, awayBusy);

        const double homeElo = rating(home), awayElo = rating(away);
        const double diff = homeElo + kHomeAdvantage - awayElo;
        const double expHome = 1.0 / (1.0 + std::pow(10.0, -diff / 400.0));
        const double days = std::max(0, date - seasonStartDate);

        std::map<std::string, double> f;
        f["elo_difference"] = diff;
        f["elo_expected_home"] = expHome;
        f["home_elo"] = homeElo;
        f["away_elo"] = awayElo;
        f["elo_absolute_difference"] = std::fabs(diff);
        f["elo_closeness"] = std::exp(-std::pow(diff / 150.0, 2.0));
        f["home_form_points_5"] = h5.points;
        f["away_form_points_5"] = a5.points;
        f["home_form_points_10"] = h10.points;
        f["away_form_points_10"] = a10.points;
        f["home_weighted_points_5"] = h5.wpoints;
        f["away_weighted_points_5"] = a5.wpoints;
        f["form_points_gap_5"] = std::fabs(h5.points - a5.points);
        f["home_goals_for_5"] = h5.gf;
        f["home_goals_against_5"] = h5.ga;
        f["away_goals_for_5"] = a5.gf;
        f["away_goals_against_5"] = a5.ga;
        f["home_goal_difference_5"] = h5.gd;
        f["away_goal_difference_5"] = a5.gd;
        f["home_win_rate_5"] = h5.wins;
        f["away_win_rate_5"] = a5.wins;
        f["home_draw_rate_5"] = h5.draws;
        f["away_draw_rate_5"] = a5.draws;
        f["home_draw_rate_10"] = h10.draws;
        f["away_draw_rate_10"] = a10.draws;
        f["home_home_goals_for"] = hvs.gf;
        f["home_home_goals_against"] = hvs.ga;
        f["home_home_points"] = hvs.points;
        f["away_away_goals_for"] = avs.gf;
        f["away_away_goals_against"] = avs.ga;
        f["away_away_points"] = avs.points;
        f["home_rest_days"] = homeRest;
        f["away_rest_days"] = awayRest;
        f["home_matches_last_14_days"] = homeBusy;
        f["away_matches_last_14_days"] = awayBusy;
        f["days_into_season"] = days;
        f["combined_goals_5"] = h5.gf + a5.gf;
        f["combined_goals_10"] = h10.gf + a10.gf;
        f["combined_draw_rate_5"] = (h5.draws + a5.draws) / 2.0;
        f["combined_draw_rate_10"] = (h10.draws + a10.draws) / 2.0;
        f["combined_low_scoring_5"] = (h5.low + a5.low) / 2.0;
        f["expected_total_goals"] = (h5.gf + a5.ga) / 2.0 + (a5.gf + h5.ga) / 2.0;
        for (int k = 0; k < kStats; ++k) {
            const std::string s = kStatNames[k];
            f["home_" + s + "_for_5"] = h5.sf[k];
            f["home_" + s + "_against_5"] = h5.sa[k];
            f["away_" + s + "_for_5"] = a5.sf[k];
            f["away_" + s + "_against_5"] = a5.sa[k];
        }
        double m[3] = {kNaN, kNaN, kNaN};
        if (!marketProbabilities(odds, m)) m[0] = m[1] = m[2] = kNaN;
        f["market_home_probability"] = m[0];
        f["market_draw_probability"] = m[1];
        f["market_away_probability"] = m[2];
        f["market_closeness"] = 1.0 - std::fabs(m[0] - m[2]);
        return f;
    }

    void updateElo(const std::vector<const HistMatch*>& day) {
        std::map<std::string, double> changes;
        for (const HistMatch* m : day) {
            const double hr = rating(m->home), ar = rating(m->away);
            const double expected = 1.0 / (1.0 + std::pow(10.0, -((hr + kHomeAdvantage) - ar) / 400.0));
            const double score = m->ftr == 'H' ? 1.0 : (m->ftr == 'D' ? 0.5 : 0.0);
            const double margin = std::fabs(static_cast<double>(m->fthg) - m->ftag);
            const double mult = margin > 0 ? std::log1p(margin) * 2.2 / (std::fabs(hr - ar) * 0.001 + 2.2) : 1.0;
            const double delta = kEloK * mult * (score - expected);
            changes[m->home] += delta;
            changes[m->away] -= delta;
        }
        for (const auto& kv : changes) m_elo[kv.first] += kv.second;
    }

    void record(const HistMatch& m) {
        double hp, ap;
        if (m.fthg > m.ftag) { hp = 3; ap = 0; } else if (m.fthg < m.ftag) { hp = 0; ap = 3; } else { hp = ap = 1; }
        Rec h, a;
        h.date = a.date = m.date;
        h.gf = m.fthg; h.ga = m.ftag; h.pts = hp;
        a.gf = m.ftag; a.ga = m.fthg; a.pts = ap;
        for (int k = 0; k < kStats; ++k) {
            h.sf[k] = m.stat[0][k]; h.sa[k] = m.stat[1][k];
            a.sf[k] = m.stat[1][k]; a.sa[k] = m.stat[0][k];
        }
        m_hist[m.home].push_back(h);
        m_hist[m.away].push_back(a);
        m_venue[m.home + "|H"].push_back(h);
        m_venue[m.away + "|A"].push_back(a);
    }

    int played(const std::string& team) const {
        auto it = m_hist.find(team);
        return it == m_hist.end() ? 0 : static_cast<int>(it->second.size());
    }

private:
    const std::vector<Rec>& hist(const std::string& t, bool) const {
        static const std::vector<Rec> empty;
        auto it = m_hist.find(t);
        return it == m_hist.end() ? empty : it->second;
    }
    const std::vector<Rec>& venue(const std::string& t, char side) const {
        static const std::vector<Rec> empty;
        auto it = m_venue.find(t + "|" + side);
        return it == m_venue.end() ? empty : it->second;
    }

    std::unordered_map<std::string, std::vector<Rec>> m_hist, m_venue;
    std::map<std::string, double> m_elo;
    double m_newTeamElo = kInitialElo;
    int m_season = 0;
    bool m_hasSeason = false;
};

}  // namespace

bool marketProbabilities(const double odds[3], double out[3]) {
    for (int i = 0; i < 3; ++i) if (!std::isfinite(odds[i]) || odds[i] <= 1.0) return false;
    double s = 0;
    for (int i = 0; i < 3; ++i) { out[i] = 1.0 / odds[i]; s += out[i]; }
    for (int i = 0; i < 3; ++i) out[i] /= s;
    return true;
}

FeatureEngineer::FeatureEngineer(std::vector<std::string> names) : m_names(std::move(names)) {}

static void fillRow(const std::vector<std::string>& names, std::map<std::string, double>&& f, FeatureRow& row) {
    row.values.clear(); row.missing = 0;
    for (const auto& n : names) {
        auto it = f.find(n);
        if (it == f.end()) throw std::runtime_error("Feature '" + n + "' is not produced by FeatureEngineer");
        row.values.push_back(static_cast<float>(it->second));
        if (std::isnan(it->second)) ++row.missing;
    }
    row.byName = std::move(f);
}

std::vector<FeatureRow> FeatureEngineer::buildHistorical(const Dataset& ds) const {
    std::vector<FeatureRow> rows;
    const auto& all = ds.matches();
    rows.reserve(all.size());
    Tracker tracker;
    size_t i = 0;
    while (i < all.size()) {
        size_t j = i;
        while (j < all.size() && all[j].date == all[i].date) ++j;
        tracker.startSeason(all[i].season);
        std::vector<const HistMatch*> day;
        for (size_t k = i; k < j; ++k) {
            const HistMatch& m = all[k];
            FeatureRow row;
            row.homeMatchesPlayed = tracker.played(m.home);
            row.awayMatchesPlayed = tracker.played(m.away);
            fillRow(m_names, tracker.features(m.home, m.away, m.date, ds.seasonStart(m.season, m.date), m.odds), row);
            day.push_back(&m);
            rows.push_back(std::move(row));
        }
        tracker.updateElo(day);
        for (const HistMatch* m : day) tracker.record(*m);
        i = j;
    }
    return rows;
}

bool FeatureEngineer::buildUpcoming(const Dataset& ds, const MatchQuery& q, FeatureRow& out, std::string& error) const {
    const auto& teams = ds.teams();
    if (!std::binary_search(teams.begin(), teams.end(), q.home)) { error = "Unknown team (no history): " + q.home; return false; }
    if (!std::binary_search(teams.begin(), teams.end(), q.away)) { error = "Unknown team (no history): " + q.away; return false; }
    if (q.home == q.away) { error = "Home and away team must differ"; return false; }

    const auto& all = ds.matches();
    Tracker tracker;
    size_t i = 0;
    while (i < all.size() && all[i].date < q.date) {
        size_t j = i;
        while (j < all.size() && all[j].date == all[i].date && all[j].date < q.date) ++j;
        tracker.startSeason(all[i].season);
        std::vector<const HistMatch*> day;
        for (size_t k = i; k < j; ++k) day.push_back(&all[k]);
        tracker.updateElo(day);
        for (const HistMatch* m : day) tracker.record(*m);
        i = j;
    }
    const int season = seasonStartYearFor(q.date);
    tracker.startSeason(season);
    out = FeatureRow{};
    out.homeMatchesPlayed = tracker.played(q.home);
    out.awayMatchesPlayed = tracker.played(q.away);
    fillRow(m_names, tracker.features(q.home, q.away, q.date, ds.seasonStart(season, q.date), q.odds), out);
    return true;
}

}  // namespace sba
