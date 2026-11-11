#pragma once
// C++ port of the pre-match feature pipeline in ml_training/train_model.py.
// Every feature is computed from matches played strictly BEFORE the target date.
#include <map>
#include <string>
#include <vector>
#include "Dataset.h"

namespace sba {

struct MatchQuery {
    std::string home, away;
    int date = 0;                       // kickoff date (days since epoch)
    double odds[3] = {kNaN, kNaN, kNaN}; // optional decimal odds H/D/A (market features)
};

struct FeatureRow {
    std::vector<float> values;                 // in model order (NaN = missing)
    std::map<std::string, double> byName;      // all computed features, for explanations
    int missing = 0;                           // NaN count among model inputs
    int homeMatchesPlayed = 0;
    int awayMatchesPlayed = 0;
};

class FeatureEngineer {
public:
    // `modelFeatureNames` fixes the order/length of the vector sent to the model
    // (read from models/epl_decision_config.json).
    explicit FeatureEngineer(std::vector<std::string> modelFeatureNames);

    const std::vector<std::string>& featureNames() const { return m_names; }

    // One row per historical match, aligned with ds.matches() (for backtests / parity tests).
    std::vector<FeatureRow> buildHistorical(const Dataset& ds) const;

    // Features for a match that has not been played yet. Uses every dataset
    // match dated before q.date. Returns false (with reason) for unknown teams.
    bool buildUpcoming(const Dataset& ds, const MatchQuery& q, FeatureRow& out, std::string& error) const;

private:
    std::vector<std::string> m_names;
};

// Removes the bookmaker margin (proportional method, same as training). False if odds invalid.
bool marketProbabilities(const double odds[3], double out[3]);

}  // namespace sba
