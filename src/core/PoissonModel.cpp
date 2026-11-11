#include "PoissonModel.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace sba {

bool PoissonModel::load(const std::string& path, std::string& error) {
    m_loaded = false;
    try {
        std::ifstream in(path);
        if (!in) { error = "Cannot open Poisson model: " + path; return false; }
        nlohmann::json j = nlohmann::json::parse(in);
        m_names = j.at("feature_names").get<std::vector<std::string>>();
        m_median = j.at("imputer_median").get<std::vector<double>>();
        m_mean = j.at("scaler_mean").get<std::vector<double>>();
        m_scale = j.at("scaler_scale").get<std::vector<double>>();
        m_homeCoef = j.at("home_coef").get<std::vector<double>>();
        m_awayCoef = j.at("away_coef").get<std::vector<double>>();
        m_homeIntercept = j.at("home_intercept").get<double>();
        m_awayIntercept = j.at("away_intercept").get<double>();
        m_drawInflation = j.at("draw_inflation").get<double>();
        m_maxGoals = j.value("max_goals", 10);
        auto clip = j.at("lambda_clip").get<std::vector<double>>();
        m_clipLo = clip.at(0); m_clipHi = clip.at(1);
        const size_t n = m_names.size();
        if (m_median.size() != n || m_mean.size() != n || m_scale.size() != n ||
            m_homeCoef.size() != n || m_awayCoef.size() != n) {
            error = "Poisson model JSON has inconsistent vector sizes";
            return false;
        }
    } catch (const std::exception& e) {
        error = std::string("Poisson model JSON error: ") + e.what();
        return false;
    }
    m_loaded = true;
    return true;
}

PoissonOutput PoissonModel::predict(const std::vector<float>& x) const {
    PoissonOutput out;
    const size_t n = m_names.size();
    if (!m_loaded || x.size() != n) return out;
    double zh = m_homeIntercept, za = m_awayIntercept;
    for (size_t i = 0; i < n; ++i) {
        double v = std::isnan(x[i]) ? m_median[i] : static_cast<double>(x[i]);
        v = (v - m_mean[i]) / (m_scale[i] == 0.0 ? 1.0 : m_scale[i]);
        zh += m_homeCoef[i] * v;
        za += m_awayCoef[i] * v;
    }
    out.lambdaHome = std::clamp(std::exp(zh), m_clipLo, m_clipHi);
    out.lambdaAway = std::clamp(std::exp(za), m_clipLo, m_clipHi);

    auto pmf = [&](double lam) {
        std::vector<double> p(m_maxGoals + 1);
        for (int k = 0; k <= m_maxGoals; ++k) p[k] = std::exp(-lam + k * std::log(lam) - std::lgamma(k + 1.0));
        return p;
    };
    const auto ph = pmf(out.lambdaHome), pa = pmf(out.lambdaAway);
    double home = 0, draw = 0, away = 0;
    for (int i = 0; i <= m_maxGoals; ++i)
        for (int j = 0; j <= m_maxGoals; ++j) {
            const double v = ph[i] * pa[j];
            if (i > j) home += v; else if (i == j) draw += v; else away += v;
        }
    draw *= (1.0 + m_drawInflation);
    out.probs = normalised({home, draw, away});
    return out;
}

}  // namespace sba
