#pragma once
// Orchestrates dataset -> features -> models -> reasoning. Qt-free and unit-tested.
#include <memory>
#include <string>
#include <vector>
#include "Dataset.h"
#include "FeatureEngineer.h"
#include "Metrics.h"
#include "OnnxInference.h"
#include "PoissonModel.h"
#include "Markets.h"
#include "ReasoningEngine.h"
#include "Resources.h"

namespace sba {

struct AnalysisRequest {
    std::string home, away;               // dataset team names
    std::string kickoffDateIso;           // YYYY-MM-DD
    std::string kickoffIso;               // full ISO (display / storage), optional
    std::int64_t kickoffEpoch = 0, nowEpoch = 0;
    bool matchStarted = false;
    bool hasOdds = false;
    OddsQuote odds;
};

struct Analysis {
    FeatureRow features;
    PoissonOutput poisson;
    Evidence evidence;
    ReasoningResult reasoning;
    MarketBook markets;                  // every betting market priced from the same scoreline grid
};

struct ModelScoreLine { std::string name; Scores scores; };

struct BettingSummary {
    std::string rule;
    int bets = 0, wins = 0;
    double staked = 0, profit = 0, roi = kNaN, roiStdErr = kNaN, maxDrawdown = 0, finalBankroll = 100.0;
};

struct GoalsQuality {                 // held-out check of the goals markets (NaN = not run yet)
    int matches = 0;                  // held-out matches with a goals forecast
    int marketMatches = 0;            // of those, matches that also have over/under 2.5 bookmaker odds
    double ouBaseRate = kNaN, bttsBaseRate = kNaN;                       // league base rates before the test window
    double ouMeanPredicted = kNaN, ouActualRate = kNaN;                  // calibration of over 2.5 on the held-out window
    double bttsMeanPredicted = kNaN, bttsActualRate = kNaN;
    double ouModelLogLoss = kNaN, ouMarketLogLoss = kNaN, ouBaseLogLoss = kNaN;   // all three on the same matches (those with market odds)
    double bttsModelLogLoss = kNaN, bttsBaseLogLoss = kNaN;
    bool ran() const { return matches > 0; }
    bool modelBeatsMarket() const { return marketMatches > 0 && ouModelLogLoss < ouMarketLogLoss; }
};

struct BacktestReport {
    bool ok = false;
    std::string error;
    int matches = 0;
    std::string from, to;
    double rfWeight = 0;
    std::vector<ModelScoreLine> models;       // sorted by log loss
    ConfusionMatrix confusionPlain, confusionDrawAware;
    std::vector<CalibrationBin> calibration;  // blended model
    double ece = kNaN;
    std::vector<BettingSummary> betting;
    ModelQuality quality;                      // blended model vs bookmaker
    GoalsQuality goals;                        // over/under 2.5 and BTTS
};

struct ServiceStatus {
    bool dataset = false, poisson = false, rf = false;
    int matches = 0, features = 0;
    int updatedRows = 0;                       // matches merged in from downloaded result files
    std::string firstDate, lastDate;
    std::vector<std::string> messages;         // problems / info for the status panel
};

class PredictionService {
public:
    bool initialise(const ResourcePaths& paths);          // never throws; see status()
    const ServiceStatus& status() const { return m_status; }
    const Dataset& dataset() const { return m_dataset; }
    const std::vector<std::string>& featureNames() const { return m_featureNames; }
    std::string modelVersion() const { return m_modelVersion; }

    void setRiskConfig(const RiskConfig& c) { m_engine.setConfig(c); }
    const RiskConfig& riskConfig() const { return m_engine.config(); }
    void setRfWeight(double w) { m_rfWeight = w < 0 ? 0 : (w > 1 ? 1 : w); }
    double rfWeight() const { return m_status.rf ? m_rfWeight : 0.0; }
    void setModelQuality(const ModelQuality& q) { m_quality = q; }
    void setGoalsQuality(const GoalsQuality& g) { m_goals = g; }
    const GoalsQuality& goalsQuality() const { return m_goals; }
    // How much the goals markets are trusted against the bookmaker (0..1). Lower until the held-out check shows the model keeps up.
    double goalsShrink() const;

    bool analyse(const AnalysisRequest& req, Analysis& out, std::string& error) const;

    // Held-out evaluation on the chronological test window (matches the training split).
    BacktestReport runBacktest() const;

private:
    Dataset m_dataset;
    std::unique_ptr<FeatureEngineer> m_features;
    PoissonModel m_poisson;
    OnnxInference m_onnx;
    ReasoningEngine m_engine;
    ServiceStatus m_status;
    std::vector<std::string> m_featureNames;
    Probs3 m_decisionWeights{1.0, 1.0, 1.0};
    double m_rfWeight = 0.5;
    ModelQuality m_quality;
    GoalsQuality m_goals;
    std::string m_modelVersion;
};

}  // namespace sba
