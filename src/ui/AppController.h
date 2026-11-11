#pragma once
#include <algorithm>
// Owns the long-lived services and exposes asynchronous operations to the pages.
#include <QObject>
#include <functional>
#include <memory>
#include "api/AppSettings.h"
#include "api/LiveDataService.h"
#include "core/PredictionService.h"
#include "core/PredictionStore.h"
#include "core/TfIdfIndex.h"
#include "core/Fixture.h"
#include "api/ResultsUpdater.h"
#include <vector>

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);

    void start();                                   // loads data/models asynchronously
    bool ready() const { return m_ready; }
    QString initError() const { return m_initError; }

    sba::PredictionService& service() { return m_service; }
    sba::PredictionStore& store() { return m_store; }
    LiveDataService* live() { return m_live; }
    const AppSettings& settings() const { return m_settings; }
    void applySettings(const AppSettings& s);

    void analyse(const sba::AnalysisRequest& req, std::function<void(bool, const sba::Analysis&, const QString&)> done);
    void runBacktest();
    bool backtestRunning() const { return m_backtestRunning; }
    const sba::BacktestReport& backtest() const { return m_backtest; }

    void rebuildIndex();
    bool indexBuilding() const { return m_indexBuilding; }
    std::shared_ptr<sba::TfIdfIndex> index() const { return m_index; }
    QString indexRoot() const { return m_indexRoot; }
    QString dbPath() const { return m_dbPath; }
    QString resourcesRoot() const { return QString::fromStdString(m_paths.root); }
    void notifyPredictionsChanged() { emit predictionsChanged(); }

    // The newest results from the bundled dataset (finished matches, newest first). Both the admin and the
    // customer screens fall back to this when no live feed is configured, so no screen is ever empty.
    std::vector<sba::Fixture> recordedResults(int n) const;
    // Opens the admin Settings page (used by the "Connect live data" buttons on the customer side).
    void requestSettings() { emit settingsRequested(); }
    // Downloads the newest results and restarts the app so the models and tables use them.
    void updateResultsAndRestart();
    // Changes only the bankroll / risk profile (no full settings round-trip, so predictions are not recomputed).
    void setStaking(double bankroll, int riskProfile) {
        m_settings.bankroll = std::max(0.0, bankroll); m_settings.riskProfile = std::clamp(riskProfile, 0, 2);
        m_settings.save();
        emit stakingChanged();
    }
    sba::StakeSettings stakeSettings() const {
        sba::StakeSettings s;
        s.bankroll = m_settings.bankroll; s.kellyFraction = m_settings.kellyFraction(); s.maxStakeFraction = m_settings.maxStakePct / 100.0;
        s.minStake = m_settings.minStake; s.roundTo = m_settings.stakeRound;
        return s;
    }
    // ---- bet slip (kept between sessions) ----
    const std::vector<sba::SlipLeg>& slipLegs() const { return m_slip; }
    double slipStake() const { return m_slipStake; }
    bool slipAsSingles() const { return m_slipSingles; }
    bool addSlipLeg(const sba::SlipLeg& leg);                 // returns true if it replaced an earlier selection for the same match
    void removeSlipLeg(int index);
    void setSlipOdds(int index, double odds);
    void setSlipStake(double stake);
    void setSlipAsSingles(bool on);
    void clearSlip();
    QString resultsMessage() const { return m_resultsMsg; }      // outcome of the start-up update
    int resultsAdded() const { return m_service.status().updatedRows; }

signals:
    void initialised(bool ok);
    void backtestStarted();
    void backtestFinished();
    void indexChanged();
    void settingsChanged();
    void predictionsChanged();
    void settingsRequested();
    void stakingChanged();
    void slipChanged();
    void resultsUpdateFailed(const QString& message);

private:
    sba::PredictionService m_service;
    sba::PredictionStore m_store;
    LiveDataService* m_live = nullptr;
    AppSettings m_settings;
    sba::ResourcePaths m_paths;
    sba::BacktestReport m_backtest;
    std::shared_ptr<sba::TfIdfIndex> m_index = std::make_shared<sba::TfIdfIndex>();
    QString m_indexRoot, m_dbPath, m_initError, m_resultsMsg;
    ResultsUpdater* m_updater = nullptr;
    bool m_initStarted = false;
    void beginInit();
    void saveSlip();
    void loadSlip();
    std::vector<sba::SlipLeg> m_slip;
    double m_slipStake = 0;
    bool m_slipSingles = false;
    bool m_ready = false, m_backtestRunning = false, m_indexBuilding = false;
};
