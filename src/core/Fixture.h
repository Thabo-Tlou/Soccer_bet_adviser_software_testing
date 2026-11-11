#pragma once
// A fixture as shown on the Matches screen (from API or cache).
#include <cstdint>
#include <string>
#include <vector>
#include "ReasoningEngine.h"

namespace sba {

struct BookOdds {
    std::string bookmaker;
    double odds[3] = {kNaN, kNaN, kNaN};   // Home / Draw / Away
    std::int64_t updatedEpoch = 0;
};

struct Fixture {
    std::string id, competition, kickoffIso;     // ISO-8601 UTC
    std::string home, away;                      // names as given by the provider
    std::string homeDataset, awayDataset;        // mapped to the training dataset ("" = unknown team)
    std::string status;                          // SCHEDULED, TIMED, IN_PLAY, PAUSED, FINISHED, POSTPONED...
    int homeGoals = -1, awayGoals = -1;          // -1 = unknown
    int minute = -1;                             // -1 = not provided by the API
    std::int64_t kickoffEpoch = 0;
    std::int64_t updatedEpoch = 0;               // when the match data was last fetched
    std::vector<BookOdds> books;                 // odds comparison (may be empty)
    std::int64_t oddsFetchedEpoch = 0;
    std::string source;                          // e.g. "football-data.org"
    std::string homeCrest, awayCrest;            // crest image URLs from the provider (may be empty)
    bool demo = false;                           // simulated preview match - never stored, never predicted

    bool isLive() const { return status == "IN_PLAY" || status == "PAUSED" || status == "LIVE"; }
    bool isFinished() const { return status == "FINISHED" || status == "AWARDED"; }
    bool isUpcoming() const { return status == "SCHEDULED" || status == "TIMED"; }

    // Best price per outcome across bookmakers (NaN when none).
    bool bestOdds(double out[3], std::string names[3] = nullptr) const;
};

}  // namespace sba
