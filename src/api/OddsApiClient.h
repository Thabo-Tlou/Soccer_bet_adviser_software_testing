#pragma once
// The Odds API v4 (head-to-head decimal odds from several bookmakers).
#include <vector>
#include "HttpClient.h"
#include "core/Fixture.h"

struct OddsEvent {
    std::string home, away;
    qint64 commenceEpoch = 0;
    std::vector<sba::BookOdds> books;
};

struct OddsResult {
    bool ok = false;
    std::vector<OddsEvent> events;
    QString error;
    bool stale = false;
    qint64 fetchedEpoch = 0;
    int remaining = -1;         // credits left, when reported
};

class OddsApiClient {
public:
    explicit OddsApiClient(HttpClient* http) : m_http(http) {}
    void fetchOdds(const QString& apiKey, const QString& sportKey, const QString& regions,
                   std::function<void(const OddsResult&)> cb, bool force = false);
    static OddsResult parse(const QByteArray& json);
private:
    HttpClient* m_http;
};
