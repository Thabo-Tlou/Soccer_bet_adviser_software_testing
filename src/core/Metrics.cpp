#include "Metrics.h"
#include <algorithm>
#include <cmath>

namespace sba {

int argmax(const Probs3& p) {
    int b = 0;
    for (int i = 1; i < 3; ++i) if (p[i] > p[b]) b = i;
    return b;
}

Scores scoreProbabilities(const std::vector<Probs3>& probs, const std::vector<int>& actual) {
    Scores s;
    s.n = static_cast<int>(std::min(probs.size(), actual.size()));
    if (s.n == 0) return s;
    double ll = 0, br = 0; int hit = 0;
    for (int i = 0; i < s.n; ++i) {
        const Probs3 p = normalised(probs[i]);
        const int a = actual[i];
        ll += -std::log(std::max(p[a], 1e-15));
        for (int k = 0; k < 3; ++k) { const double d = p[k] - (k == a ? 1.0 : 0.0); br += d * d; }
        hit += (argmax(p) == a);
    }
    s.logLoss = ll / s.n; s.brier = br / s.n; s.accuracy = static_cast<double>(hit) / s.n;
    return s;
}

ConfusionMatrix confusion(const std::vector<int>& pred, const std::vector<int>& actual) {
    ConfusionMatrix c;
    const size_t n = std::min(pred.size(), actual.size());
    for (size_t i = 0; i < n; ++i)
        if (actual[i] >= 0 && actual[i] < 3 && pred[i] >= 0 && pred[i] < 3) c.m[actual[i]][pred[i]]++;
    return c;
}

std::vector<CalibrationBin> calibration(const std::vector<Probs3>& probs, const std::vector<int>& actual, int bins) {
    std::vector<double> sumP(bins, 0.0), sumO(bins, 0.0);
    std::vector<int> cnt(bins, 0);
    const size_t n = std::min(probs.size(), actual.size());
    for (size_t i = 0; i < n; ++i)
        for (int k = 0; k < 3; ++k) {
            const double p = std::clamp(probs[i][k], 0.0, 1.0);
            const int b = std::min(bins - 1, static_cast<int>(p * bins));
            sumP[b] += p; sumO[b] += (actual[i] == k ? 1.0 : 0.0); cnt[b]++;
        }
    std::vector<CalibrationBin> out(bins);
    for (int b = 0; b < bins; ++b) {
        out[b].count = cnt[b];
        if (cnt[b]) { out[b].meanPredicted = sumP[b] / cnt[b]; out[b].observed = sumO[b] / cnt[b]; }
    }
    return out;
}

double expectedCalibrationError(const std::vector<CalibrationBin>& bins) {
    double total = 0, e = 0;
    for (const auto& b : bins) total += b.count;
    if (total == 0) return kNaN;
    for (const auto& b : bins) e += (b.count / total) * std::fabs(b.meanPredicted - b.observed);
    return e;
}

}  // namespace sba
