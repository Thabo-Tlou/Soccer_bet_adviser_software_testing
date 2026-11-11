#include "Fixture.h"
#include <cmath>

namespace sba {

bool Fixture::bestOdds(double out[3], std::string names[3]) const {
    bool any = false;
    for (int i = 0; i < 3; ++i) out[i] = kNaN;
    for (const auto& b : books)
        for (int i = 0; i < 3; ++i)
            if (std::isfinite(b.odds[i]) && b.odds[i] > 1.0 && (std::isnan(out[i]) || b.odds[i] > out[i])) {
                out[i] = b.odds[i];
                if (names) names[i] = b.bookmaker;
                any = true;
            }
    return any && std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}

}  // namespace sba
