#pragma once
// Probability-quality metrics used by backtests and prospective tracking.
#include <array>
#include <vector>
#include "Types.h"

namespace sba {

struct ConfusionMatrix { int m[3][3] = {{0,0,0},{0,0,0},{0,0,0}}; };  // [actual][predicted]

struct CalibrationBin {
    double meanPredicted = 0, observed = 0;
    int count = 0;
};

struct Scores {
    int n = 0;
    double accuracy = kNaN, logLoss = kNaN, brier = kNaN;
};

Scores scoreProbabilities(const std::vector<Probs3>& probs, const std::vector<int>& actual);
ConfusionMatrix confusion(const std::vector<int>& predicted, const std::vector<int>& actual);

// Pools all three outcome probabilities into `bins` equal-width confidence bins.
std::vector<CalibrationBin> calibration(const std::vector<Probs3>& probs, const std::vector<int>& actual, int bins = 10);
double expectedCalibrationError(const std::vector<CalibrationBin>& bins);

int argmax(const Probs3& p);

}  // namespace sba
