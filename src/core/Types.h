#pragma once
// Shared small types used across the Qt-free core library.
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace sba {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

enum Outcome : int { Home = 0, Draw = 1, Away = 2 };

using Probs3 = std::array<double, 3>;

inline const char* outcomeName(int o) {
    static const char* n[] = {"Home Win", "Draw", "Away Win"};
    return (o >= 0 && o < 3) ? n[o] : "None";
}

inline Probs3 normalised(Probs3 p) {
    double s = 0.0;
    for (double& v : p) { if (!(v > 1e-12)) v = 1e-12; s += v; }
    for (double& v : p) v /= s;
    return p;
}

}  // namespace sba
