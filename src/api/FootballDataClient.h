#pragma once
// football-data.org v4 (free tier: fixtures, scores, results for covered competitions).
#include <vector>
#include "HttpClient.h"
#include "core/Fixture.h"

struct FixturesResult {
    bool ok = false;
    std::vector<sba::Fixture> fixtures;
    QString error;
    bool stale = false;
    qint64 fetchedEpoch = 0;
    int remaining = -1;
};

class FootballDataClient {
public:
    explicit FootballDataClient(HttpClient* http) : m_http(http) {}
    void fetchMatches(const QString& apiKey, const QString& competition, const QString& dateFrom, const QString& dateTo,
                      std::function<void(const FixturesResult&)> cb, bool force = false);
    static FixturesResult parse(const QByteArray& json);   // exposed for tests
private:
    HttpClient* m_http;
};
