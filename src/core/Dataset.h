#pragma once
#include <map>
#include <string>
#include <vector>
#include "Types.h"
#include "Csv.h"

namespace sba {

// One finished historical match, cleaned exactly like train_model.py does.
struct HistMatch {
    int date = 0;                 // days since epoch
    std::string home, away;
    int fthg = 0, ftag = 0;
    char ftr = 'D';               // H / D / A
    int season = 0;               // season start year
    int timeMin = -1;             // kickoff time as minutes after midnight (UK local), -1 = unknown
    double stat[2][3] = {{kNaN, kNaN, kNaN}, {kNaN, kNaN, kNaN}};  // [home|away][shots, on target, corners]
    double odds[3] = {kNaN, kNaN, kNaN};                            // Bet365 H/D/A (decimal)
    double ou[2] = {kNaN, kNaN};                                    // over / under 2.5 goals (decimal): market average, else Bet365
    int target() const { return ftr == 'H' ? 0 : (ftr == 'D' ? 1 : 2); }
};

class Dataset {
public:
    // Loads and cleans the CSV. On failure returns false and sets error().
    bool load(const std::string& csvPath);

    // Adds matches from another results CSV (e.g. a fresh football-data.co.uk download) that are not
    // already present. Returns how many were added, or -1 if the file could not be read.
    // Call only while no other thread is reading the dataset (i.e. during start-up).
    int mergeCsv(const std::string& csvPath);

    const std::vector<HistMatch>& matches() const { return m_matches; }
    const std::vector<std::string>& teams() const { return m_teams; }   // sorted
    int seasonStart(int season, int fallback) const;
    int firstDate() const { return m_matches.empty() ? 0 : m_matches.front().date; }
    int lastDate() const { return m_matches.empty() ? 0 : m_matches.back().date; }
    int droppedRows() const { return m_dropped; }
    const std::string& error() const { return m_error; }

    // Date index where the chronological 15 % test window starts (same rule as training).
    int testStartDate() const;

private:
    static bool parseTable(const CsvTable& t, std::vector<HistMatch>& rows, int& dropped, std::string& error);
    void rebuildIndexes();
    std::vector<HistMatch> m_matches;
    std::vector<std::string> m_teams;
    std::map<int, int> m_seasonStart;
    int m_dropped = 0;
    std::string m_error;
};

}  // namespace sba
