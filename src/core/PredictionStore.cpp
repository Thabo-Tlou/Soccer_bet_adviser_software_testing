#include "PredictionStore.h"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace sba {
namespace {
constexpr int kSchemaVersion = 1;

PredictionRecord readRecord(Statement& s) {
    PredictionRecord r; int c = 0;
    r.id = s.integer(c++); r.matchKey = s.text(c++); r.home = s.text(c++); r.away = s.text(c++);
    r.kickoffIso = s.text(c++); r.createdIso = s.text(c++); r.modelVersion = s.text(c++);
    r.rf = {s.real(c), s.real(c + 1), s.real(c + 2)}; c += 3; r.hasRf = !std::isnan(r.rf[0]);
    r.poisson = {s.real(c), s.real(c + 1), s.real(c + 2)}; c += 3;
    r.finalP = {s.real(c), s.real(c + 1), s.real(c + 2)}; c += 3;
    r.market = {s.real(c), s.real(c + 1), s.real(c + 2)}; c += 3; r.hasMarket = !std::isnan(r.market[0]);
    for (int k = 0; k < 3; ++k) r.odds[k] = s.real(c++);
    r.oddsSource = s.text(c++); r.oddsFetchedAt = s.integer(c++);
    r.decision = s.text(c++); r.candidate = s.isNull(c) ? -1 : static_cast<int>(s.integer(c)); ++c;
    r.edge = s.real(c++); r.ev = s.real(c++); r.explanation = s.text(c++);
    r.actual = s.isNull(c) ? -1 : static_cast<int>(s.integer(c)); ++c;
    r.settledIso = s.text(c++);
    return r;
}

const char* kSelect =
    "SELECT id, match_key, home, away, kickoff_iso, created_iso, model_version,"
    " rf_h, rf_d, rf_a, po_h, po_d, po_a, fin_h, fin_d, fin_a, mkt_h, mkt_d, mkt_a,"
    " odds_h, odds_d, odds_a, odds_source, odds_fetched, decision, candidate, edge, ev, explanation,"
    " actual, settled_iso FROM predictions ";
}  // namespace

bool PredictionStore::open(const std::string& path) {
    if (!m_db.open(path)) return false;
    return migrate();
}

bool PredictionStore::migrate() {
    if (m_db.userVersion() >= kSchemaVersion) return true;
    const bool ok = m_db.exec(
        "CREATE TABLE IF NOT EXISTS predictions ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, match_key TEXT NOT NULL UNIQUE,"
        " home TEXT NOT NULL, away TEXT NOT NULL, kickoff_iso TEXT, created_iso TEXT NOT NULL, model_version TEXT,"
        " rf_h REAL, rf_d REAL, rf_a REAL, po_h REAL, po_d REAL, po_a REAL,"
        " fin_h REAL NOT NULL, fin_d REAL NOT NULL, fin_a REAL NOT NULL,"
        " mkt_h REAL, mkt_d REAL, mkt_a REAL, odds_h REAL, odds_d REAL, odds_a REAL,"
        " odds_source TEXT, odds_fetched INTEGER, decision TEXT, candidate INTEGER, edge REAL, ev REAL,"
        " explanation TEXT, actual INTEGER, settled_iso TEXT);"
        "CREATE TABLE IF NOT EXISTS fixtures ("
        " id TEXT PRIMARY KEY, competition TEXT, kickoff_iso TEXT, home TEXT, away TEXT, home_ds TEXT, away_ds TEXT,"
        " status TEXT, hg INTEGER, ag INTEGER, minute INTEGER, kickoff_epoch INTEGER, updated_epoch INTEGER,"
        " odds_epoch INTEGER, source TEXT, books_json TEXT);"
        "CREATE INDEX IF NOT EXISTS idx_pred_open ON predictions(actual);");
    if (ok) m_db.setUserVersion(kSchemaVersion);
    return ok;
}

std::int64_t PredictionStore::insertPrediction(const PredictionRecord& r, bool* created) {
    if (created) *created = false;
    {
        Statement q(m_db, "SELECT id FROM predictions WHERE match_key = ?1");
        q.bind(1, r.matchKey);
        if (q.step()) return q.integer(0);   // keep the original prediction
    }
    Statement s(m_db,
        "INSERT INTO predictions (match_key, home, away, kickoff_iso, created_iso, model_version,"
        " rf_h, rf_d, rf_a, po_h, po_d, po_a, fin_h, fin_d, fin_a, mkt_h, mkt_d, mkt_a, odds_h, odds_d, odds_a,"
        " odds_source, odds_fetched, decision, candidate, edge, ev, explanation)"
        " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,?28)");
    int i = 1;
    s.bind(i++, r.matchKey).bind(i++, r.home).bind(i++, r.away).bind(i++, r.kickoffIso).bind(i++, r.createdIso).bind(i++, r.modelVersion);
    for (int k = 0; k < 3; ++k) s.bind(i++, r.hasRf ? r.rf[k] : kNaN);
    for (int k = 0; k < 3; ++k) s.bind(i++, r.poisson[k]);
    for (int k = 0; k < 3; ++k) s.bind(i++, r.finalP[k]);
    for (int k = 0; k < 3; ++k) s.bind(i++, r.hasMarket ? r.market[k] : kNaN);
    for (int k = 0; k < 3; ++k) s.bind(i++, r.odds[k]);
    s.bind(i++, r.oddsSource).bind(i++, r.oddsFetchedAt).bind(i++, r.decision);
    if (r.candidate >= 0) s.bind(i++, r.candidate); else s.bindNull(i++);
    s.bind(i++, r.edge).bind(i++, r.ev).bind(i++, r.explanation);
    if (!s.run()) return 0;
    if (created) *created = true;
    return m_db.lastInsertId();
}

std::vector<PredictionRecord> PredictionStore::all() {
    std::vector<PredictionRecord> out;
    Statement s(m_db, std::string(kSelect) + "ORDER BY created_iso DESC, id DESC");
    while (s.step()) out.push_back(readRecord(s));
    return out;
}

bool PredictionStore::settle(std::int64_t id, int actual, const std::string& iso) {
    if (actual < 0 || actual > 2) return false;
    Statement s(m_db, "UPDATE predictions SET actual = ?1, settled_iso = ?2 WHERE id = ?3 AND actual IS NULL");
    s.bind(1, actual).bind(2, iso).bind(3, id);
    return s.run();
}

int PredictionStore::settleFromFixtures(const std::vector<Fixture>& finished, const std::string& iso) {
    int n = 0;
    for (const auto& f : finished) {
        if (!f.isFinished() || f.homeGoals < 0 || f.awayGoals < 0 || f.homeDataset.empty() || f.awayDataset.empty()) continue;
        const std::string key = f.homeDataset + "|" + f.awayDataset + "|" + f.kickoffIso.substr(0, 10);
        const int actual = f.homeGoals > f.awayGoals ? 0 : (f.homeGoals == f.awayGoals ? 1 : 2);
        Statement s(m_db, "UPDATE predictions SET actual = ?1, settled_iso = ?2 WHERE match_key = ?3 AND actual IS NULL");
        s.bind(1, actual).bind(2, iso).bind(3, key);
        if (s.run()) { Statement c(m_db, "SELECT changes()"); if (c.step()) n += static_cast<int>(c.integer(0)); }
    }
    return n;
}

ProspectiveSummary PredictionStore::summary() {
    ProspectiveSummary out;
    std::vector<Probs3> mp, kp; std::vector<int> ma, ka;
    for (const auto& r : all()) {
        ++out.total;
        if (r.actual < 0) continue;
        ++out.settled;
        mp.push_back(r.finalP); ma.push_back(r.actual);
        if (r.hasMarket) { kp.push_back(r.market); ka.push_back(r.actual); }
        if (r.decision == decisionLabel(Decision::PossibleValue) && r.candidate >= 0 && std::isfinite(r.odds[r.candidate])) {
            ++out.hypotheticalBets;
            const bool win = (r.candidate == r.actual);
            out.wins += win;
            out.profit += win ? r.odds[r.candidate] - 1.0 : -1.0;
        }
    }
    out.model = scoreProbabilities(mp, ma);
    out.market = scoreProbabilities(kp, ka);
    if (out.hypotheticalBets > 0) out.roi = out.profit / out.hypotheticalBets;
    return out;
}

bool PredictionStore::saveFixtures(const std::vector<Fixture>& list) {
    if (!m_db.exec("BEGIN")) return false;
    for (const auto& f : list) {
        nlohmann::json books = nlohmann::json::array();
        for (const auto& b : f.books)
            books.push_back({{"n", b.bookmaker}, {"h", b.odds[0]}, {"d", b.odds[1]}, {"a", b.odds[2]}, {"t", b.updatedEpoch}});
        Statement s(m_db,
            "INSERT OR REPLACE INTO fixtures (id, competition, kickoff_iso, home, away, home_ds, away_ds, status, hg, ag, minute,"
            " kickoff_epoch, updated_epoch, odds_epoch, source, books_json) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16)");
        s.bind(1, f.id).bind(2, f.competition).bind(3, f.kickoffIso).bind(4, f.home).bind(5, f.away).bind(6, f.homeDataset)
         .bind(7, f.awayDataset).bind(8, f.status).bind(9, f.homeGoals).bind(10, f.awayGoals).bind(11, f.minute)
         .bind(12, f.kickoffEpoch).bind(13, f.updatedEpoch).bind(14, f.oddsFetchedEpoch).bind(15, f.source).bind(16, books.dump());
        if (!s.run()) { m_db.exec("ROLLBACK"); return false; }
    }
    return m_db.exec("COMMIT");
}

std::vector<Fixture> PredictionStore::loadFixtures() {
    std::vector<Fixture> out;
    Statement s(m_db, "SELECT id, competition, kickoff_iso, home, away, home_ds, away_ds, status, hg, ag, minute, kickoff_epoch,"
                      " updated_epoch, odds_epoch, source, books_json FROM fixtures ORDER BY kickoff_iso");
    while (s.step()) {
        Fixture f; int c = 0;
        f.id = s.text(c++); f.competition = s.text(c++); f.kickoffIso = s.text(c++); f.home = s.text(c++); f.away = s.text(c++);
        f.homeDataset = s.text(c++); f.awayDataset = s.text(c++); f.status = s.text(c++);
        f.homeGoals = static_cast<int>(s.integer(c++)); f.awayGoals = static_cast<int>(s.integer(c++)); f.minute = static_cast<int>(s.integer(c++));
        f.kickoffEpoch = s.integer(c++); f.updatedEpoch = s.integer(c++); f.oddsFetchedEpoch = s.integer(c++); f.source = s.text(c++);
        try {
            auto j = nlohmann::json::parse(s.text(c++), nullptr, false);
            if (j.is_array())
                for (const auto& b : j) {
                    BookOdds bo; bo.bookmaker = b.value("n", "");
                    bo.odds[0] = b.value("h", kNaN); bo.odds[1] = b.value("d", kNaN); bo.odds[2] = b.value("a", kNaN);
                    bo.updatedEpoch = b.value("t", static_cast<std::int64_t>(0));
                    f.books.push_back(bo);
                }
        } catch (...) {}
        out.push_back(std::move(f));
    }
    return out;
}

}  // namespace sba
