#pragma once
// Fetches fixtures (football-data.org) and odds (The Odds API), merges them, caches the result
// and reports honest per-source status (fresh / stale / unavailable).
#include <QObject>
#include <QTimer>
#include <vector>
#include "AppSettings.h"
#include "FootballDataClient.h"
#include "HttpClient.h"
#include "OddsApiClient.h"
#include "core/PredictionStore.h"

struct SourceStatus {
    enum State { NotConfigured, Idle, Loading, Fresh, Stale, Error };
    QString name;
    State state = Idle;
    QString message;
    qint64 lastEpoch = 0;       // when data was last successfully fetched
    int remaining = -1;         // remaining requests/credits if the provider reports them
};

class LiveDataService : public QObject {
    Q_OBJECT
public:
    LiveDataService(sba::PredictionStore* store, QObject* parent = nullptr);

    void setSettings(const AppSettings& s);
    void setDatasetTeams(const std::vector<std::string>& teams) { m_teams = teams; remap(); }
    void start();                                    // loads cache, starts timers, first refresh
    void refreshAll(bool force = false);
    void refreshFixtures(bool force = false);
    void refreshOdds(bool force = false);
    // Finished matches of the last days (for settling stored predictions).
    void fetchRecentResults(std::function<void(const std::vector<sba::Fixture>&, const QString& error)> cb);
    void testFootballConnection(std::function<void(bool ok, const QString& message)> cb);

    // Real fixtures plus (only when demo mode is on) one clearly flagged simulated live match.
    const std::vector<sba::Fixture>& fixtures() const { return m_view; }
    SourceStatus fixturesStatus() const { return m_fixStatus; }
    SourceStatus oddsStatus() const { return m_oddsStatus; }
    bool isStale(const SourceStatus& s, int maxAgeMin) const;

signals:
    void fixturesChanged();
    void statusChanged();
    void demoTick();                 // the simulated match advanced (separate so heavy pages need not rebuild)

private:
    void applyOdds();
    void remap();
    void emitAndStore();
    void rebuildView();
    void tickDemo();

    sba::PredictionStore* m_store;
    HttpClient m_http;
    FootballDataClient m_fd{&m_http};
    OddsApiClient m_odds{&m_http};
    AppSettings m_settings;
    std::vector<sba::Fixture> m_fixtures;     // real data only (this is what gets cached)
    std::vector<sba::Fixture> m_view;         // m_fixtures + demo match
    sba::Fixture m_demo;
    int m_demoStep = 0;
    std::vector<OddsEvent> m_oddsEvents;
    std::vector<std::string> m_teams;
    SourceStatus m_fixStatus, m_oddsStatus;
    qint64 m_oddsFetchedEpoch = 0;
    QTimer m_fixTimer, m_oddsTimer, m_liveTimer, m_demoTimer;
};
