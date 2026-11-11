#pragma once
// Poisson goal model (imputer + scaler + two log-linear regressors) exported by
// ml_training/export_cpp_assets.py. Gives scoreline-based H/D/A probabilities.
#include <string>
#include <vector>
#include "Types.h"

namespace sba {

struct PoissonOutput {
    Probs3 probs{};
    double lambdaHome = 0, lambdaAway = 0;
};

class PoissonModel {
public:
    bool load(const std::string& jsonPath, std::string& error);
    bool isLoaded() const { return m_loaded; }
    const std::vector<std::string>& featureNames() const { return m_names; }
    PoissonOutput predict(const std::vector<float>& features) const;

private:
    bool m_loaded = false;
    std::vector<std::string> m_names;
    std::vector<double> m_median, m_mean, m_scale, m_homeCoef, m_awayCoef;
    double m_homeIntercept = 0, m_awayIntercept = 0, m_drawInflation = 0, m_clipLo = 0.05, m_clipHi = 6.0;
    int m_maxGoals = 10;
};

}  // namespace sba
