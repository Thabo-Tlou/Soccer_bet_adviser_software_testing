#pragma once
// Calendar helpers (days since 1970-01-01) so feature code needs no time zones.
#include <cstdio>
#include <string>

namespace sba {

inline int daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

inline void civilFromDays(int z, int& y, int& m, int& d) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y += (m <= 2);
}

// Accepts "YYYY-MM-DD" optionally followed by 'T...' / ' ...'.
inline bool parseIsoDate(const std::string& s, int& serial) {
    int y = 0, m = 0, d = 0;
    if (s.size() < 10 || std::sscanf(s.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1800 || y > 2200) return false;
    serial = daysFromCivil(y, m, d);
    return true;
}

// Accepts ISO dates and the "dd/mm/yyyy" / "dd/mm/yy" style used by football-data.co.uk downloads.
inline bool parseFlexibleDate(const std::string& s, int& serial) {
    if (parseIsoDate(s, serial)) return true;
    int d = 0, m = 0, y = 0;
    if (std::sscanf(s.c_str(), "%2d/%2d/%4d", &d, &m, &y) != 3) return false;
    if (y < 100) y += 2000;
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1800 || y > 2200) return false;
    serial = daysFromCivil(y, m, d);
    return true;
}

inline std::string isoFromSerial(int serial) {
    int y, m, d;
    civilFromDays(serial, y, m, d);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
    return buf;
}

// August starts a season (matches the training script's fallback rule).
inline int seasonStartYearFor(int serial) {
    int y, m, d;
    civilFromDays(serial, y, m, d);
    return m >= 8 ? y : y - 1;
}

}  // namespace sba
