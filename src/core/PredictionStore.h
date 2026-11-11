#pragma once
// Phase 8 - persistent prediction history and cached fixtures (SQLite).
#include <string>
#include <vector>
#include "Database.h"
#include "Fixture.h"
#include "Metrics.h"
#include "Types.h"

namespace sba {

struct PredictionRecord {
    std::int64_t id = 0;
    std::string matchKey;                 // "home|away|YYYY-MM-DD"
    std::string home, away, kickoffIso, createdIso, modelVersion;
    bool hasRf = false;
    Probs3 rf{}, poisson{}, finalP{}, market{};
    bool hasMarket = false;
    double odds[3] = {kNaN, kNaN, kNaN};
    std::string oddsSource;
    std::int64_t oddsFetchedAt = 0;
    std::string decision;                 // decisionLabel()
    int candidate = -1;
    double edge = kNaN, ev = kNaN;
    std::string explanation;
    int actual = -1;                      // -1 = not settled yet
    std::string settledIso;
};

struct ProspectiveSummary {
    int total = 0, settled = 0;
    Scores model, market;
    int hypotheticalBets = 0, wins = 0;
    double profit = 0, roi = kNaN;
};

class PredictionStore {
public:
    bool open(const std::string& path);
    const std::string& error() const { return m_db.lastError(); }
    bool isOpen() const { return m_db.isOpen(); }

    // Stores the ORIGINAL prediction; an existing prediction for the same match is never overwritten.
    // Returns the id (new or existing); `created` tells which.
    std::int64_t insertPrediction(const PredictionRecord& r, bool* created = nullptr);
    std::vector<PredictionRecord> all();
    bool settle(std::int64_t id, int actual, const std::string& settledIso);
    // Settles every open prediction whose match (teams + kickoff date) appears in `results`.
    int settleFromFixtures(const std::vector<Fixture>& finished, const std::string& settledIso);
    ProspectiveSummary summary();

    bool saveFixtures(const std::vector<Fixture>& f);
    std::vector<Fixture> loadFixtures();

private:
    bool migrate();
    Database m_db;
};

}  // namespace sba
