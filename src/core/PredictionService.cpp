#include "PredictionService.h"
#include "DateUtil.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace sba {

bool PredictionService::initialise(const ResourcePaths& paths) {
    m_status = ServiceStatus{};
    if (!paths.complete()) {
        m_status.messages.push_back("Required data/model files were not found. Expected ml_training/data and ml_training/models next to the app (or set SBA_RESOURCES).");
        return false;
    }
    // decision config (feature order + draw-aware weights)
    try {
        std::ifstream in(paths.decisionConfig);
        nlohmann::json j = nlohmann::json::parse(in);
        m_featureNames = j.at("feature_names").get<std::vector<std::string>>();
        auto w = j.at("decision_weights_home_draw_away").get<std::vector<double>>();
        if (w.size() == 3) m_decisionWeights = {w[0], w[1], w[2]};
        m_modelVersion = "rf+poisson/" + std::to_string(m_featureNames.size()) + "f";
    } catch (const std::exception& e) {
        m_status.messages.push_back(std::string("Decision config unreadable: ") + e.what());
        return false;
    }
    m_status.features = static_cast<int>(m_featureNames.size());

    if (!m_dataset.load(paths.dataCsv)) {
        m_status.messages.push_back("Dataset error: " + m_dataset.error());
        return false;
    }
    m_status.dataset = true;
    for (const auto& extra : paths.updateCsvs) {            // newer results downloaded by the app
        const int n = m_dataset.mergeCsv(extra);
        if (n > 0) m_status.updatedRows += n;
    }
    m_status.matches = static_cast<int>(m_dataset.matches().size());
    m_status.firstDate = isoFromSerial(m_dataset.firstDate());
    m_status.lastDate = isoFromSerial(m_dataset.lastDate());
    if (m_dataset.droppedRows() > 0)
        m_status.messages.push_back(std::to_string(m_dataset.droppedRows()) + " dataset rows were dropped during cleaning.");

    try { m_features = std::make_unique<FeatureEngineer>(m_featureNames); }
    catch (const std::exception& e) { m_status.messages.push_back(e.what()); return false; }

    std::string err;
    if (!paths.poissonJson.empty() && m_poisson.load(paths.poissonJson, err)) {
        if (m_poisson.featureNames() == m_featureNames) m_status.poisson = true;
        else m_status.messages.push_back("Poisson model features do not match the decision config - re-run export_cpp_assets.py.");
    } else m_status.messages.push_back(err.empty() ? "Poisson model file missing - run ml_training/export_cpp_assets.py." : err);

    if (!paths.onnxModel.empty()) {
        if (m_onnx.load(paths.onnxModel, m_featureNames.size(), err)) m_status.rf = true;
        else m_status.messages.push_back("Random Forest unavailable: " + err);
    } else m_status.messages.push_back("Random Forest ONNX file missing.");

    if (!m_status.poisson && !m_status.rf) { m_status.messages.push_back("No prediction model could be loaded."); return false; }
    return true;
}

bool PredictionService::analyse(const AnalysisRequest& req, Analysis& out, std::string& error) const {
    if (!m_features || !m_status.dataset) { error = "Prediction service is not initialised"; return false; }
    if (!m_status.poisson && !m_status.rf) { error = "No model loaded"; return false; }
    MatchQuery q; q.home = req.home; q.away = req.away;
    if (!parseIsoDate(req.kickoffDateIso, q.date)) { error = "Invalid kickoff date (use YYYY-MM-DD)"; return false; }
    const bool oddsOk = req.hasOdds && req.odds.complete();
    if (oddsOk) for (int i = 0; i < 3; ++i) q.odds[i] = req.odds.odds[i];

    if (!m_features->buildUpcoming(m_dataset, q, out.features, error)) return false;

    Evidence& e = out.evidence;
    e = Evidence{};
    e.home = req.home; e.away = req.away; e.kickoffIso = req.kickoffIso.empty() ? req.kickoffDateIso : req.kickoffIso;
    e.kickoffEpoch = req.kickoffEpoch; e.nowEpoch = req.nowEpoch; e.matchStarted = req.matchStarted;
    e.features = out.features.byName;
    e.missingFeatures = out.features.missing; e.totalFeatures = static_cast<int>(out.features.values.size());
    e.homeMatchesPlayed = out.features.homeMatchesPlayed; e.awayMatchesPlayed = out.features.awayMatchesPlayed;
    e.hasOdds = oddsOk; e.odds = req.odds; e.quality = m_quality;

    if (m_status.poisson) {
        out.poisson = m_poisson.predict(out.features.values);
        e.poisson = out.poisson.probs; e.lambdaHome = out.poisson.lambdaHome; e.lambdaAway = out.poisson.lambdaAway;
    }
    if (m_status.rf) {
        std::string oe;
        Probs3 rf;
        if (m_onnx.predict(out.features.values, rf, oe)) { e.rf = rf; e.rfAvailable = true; }
        else { error = oe; return false; }
    }
    if (!e.rfAvailable) e.blended = e.poisson;
    else if (!m_status.poisson) e.blended = e.rf;
    else {
        const double w = m_rfWeight;
        Probs3 b;
        for (int i = 0; i < 3; ++i) b[i] = w * e.rf[i] + (1 - w) * e.poisson[i];
        e.blended = normalised(b);
    }
    e.rfWeight = e.rfAvailable ? m_rfWeight : 0.0;
    out.reasoning = m_engine.analyse(e);
    out.markets = buildMarketBook(req.home, req.away, out.poisson.lambdaHome, out.poisson.lambdaAway, e.blended);
    return true;
}

double PredictionService::goalsShrink() const {
    const double base = m_engine.config().shrinkTowardMarket;
    if (!m_goals.ran()) return std::min(base, 0.3);                       // not validated yet: lean on the market
    if (m_goals.marketMatches == 0) return std::min(base, 0.35);          // no bookmaker totals to compare with
    return m_goals.modelBeatsMarket() ? base : std::min(base, 0.25);      // the model did not beat the market on goals: trust it less
}

BacktestReport PredictionService::runBacktest() const {
    BacktestReport rep;
    if (!m_features || !m_status.dataset) { rep.error = "Prediction service is not initialised"; return rep; }
    const auto& all = m_dataset.matches();
    const int testStart = m_dataset.testStartDate();
    const auto rows = m_features->buildHistorical(m_dataset);

    std::vector<size_t> idx;
    for (size_t i = 0; i < all.size(); ++i) if (all[i].date >= testStart) idx.push_back(i);
    if (idx.size() < 30) { rep.error = "Too few held-out matches for a backtest"; return rep; }

    std::vector<std::vector<float>> X;
    for (size_t i : idx) X.push_back(rows[i].values);
    std::vector<Probs3> rf;
    std::string err;
    if (m_status.rf && !m_onnx.predictBatch(X, rf, err)) { rep.error = err; return rep; }

    // class frequencies before the test window (naive baseline)
    double cls[3] = {0, 0, 0}; int nTrain = 0;
    for (const auto& m : all) if (m.date < testStart) { cls[m.target()]++; ++nTrain; }

    const double w = rfWeight();
    std::vector<int> actual;
    std::vector<Probs3> pois, blend, elo, market, prior;
    std::vector<int> marketActual; std::vector<Probs3> blendOnMarket;
    for (size_t n = 0; n < idx.size(); ++n) {
        const HistMatch& m = all[idx[n]];
        actual.push_back(m.target());
        Probs3 p = m_status.poisson ? m_poisson.predict(X[n]).probs : rf[n];
        pois.push_back(p);
        Probs3 b = p;
        if (m_status.rf) {
            if (!m_status.poisson) b = rf[n];
            else for (int k = 0; k < 3; ++k) b[k] = w * rf[n][k] + (1 - w) * p[k];
        }
        blend.push_back(normalised(b));
        prior.push_back({cls[0] / nTrain, cls[1] / nTrain, cls[2] / nTrain});
        const double eh = rows[idx[n]].byName.at("elo_expected_home");
        const double dr = std::clamp(cls[1] / nTrain, 0.15, 0.35);
        elo.push_back({(1 - dr) * eh, dr, (1 - dr) * (1 - eh)});
        double mk[3];
        if (marketProbabilities(m.odds, mk)) { market.push_back({mk[0], mk[1], mk[2]}); marketActual.push_back(m.target()); blendOnMarket.push_back(blend.back()); }
    }

    // ---- goals markets (over/under 2.5, both teams to score) on the same held-out matches
    if (m_status.poisson) {
        double ov = 0, bt = 0; int nT = 0;
        for (const auto& m : all) if (m.date < testStart) { ov += (m.fthg + m.ftag > 2); bt += (m.fthg > 0 && m.ftag > 0); ++nT; }
        if (nT > 0) {
            GoalsQuality g;
            g.ouBaseRate = ov / nT; g.bttsBaseRate = bt / nT;
            auto ll = [](double p, bool y) { p = std::clamp(p, 1e-6, 1.0 - 1e-6); return -(y ? std::log(p) : std::log(1.0 - p)); };
            double sOu = 0, sOuM = 0, sOuB = 0, sBt = 0, sBtB = 0, predOu = 0, actOu = 0, predBt = 0, actBt = 0, sOuAll = 0, sOuBAll = 0;
            int n = 0, nm = 0;
            for (size_t k = 0; k < idx.size(); ++k) {
                const HistMatch& m = all[idx[k]];
                const PoissonOutput po = m_poisson.predict(X[k]);
                if (!(po.lambdaHome > 0) || !(po.lambdaAway > 0)) continue;
                ScoreGrid grid = ScoreGrid::fromLambdas(po.lambdaHome, po.lambdaAway);
                grid.rescaleToResult(blend[k]);
                double pOver = 0, pBtts = 0;
                for (int i = 0; i < ScoreGrid::N; ++i) for (int j = 0; j < ScoreGrid::N; ++j) { if (i + j > 2) pOver += grid.at(i, j); if (i > 0 && j > 0) pBtts += grid.at(i, j); }
                const bool yOver = m.fthg + m.ftag > 2, yBtts = m.fthg > 0 && m.ftag > 0;
                ++n; predOu += pOver; actOu += yOver; predBt += pBtts; actBt += yBtts;
                sBt += ll(pBtts, yBtts); sBtB += ll(g.bttsBaseRate, yBtts);
                sOuAll += ll(pOver, yOver); sOuBAll += ll(g.ouBaseRate, yOver);
                if (std::isfinite(m.ou[0]) && std::isfinite(m.ou[1])) {                 // bookmaker's over probability with the margin removed
                    const double io = 1.0 / m.ou[0], iu = 1.0 / m.ou[1], pm = io / (io + iu);
                    ++nm; sOu += ll(pOver, yOver); sOuM += ll(pm, yOver); sOuB += ll(g.ouBaseRate, yOver);
                }
            }
            if (n > 0) {
                g.matches = n; g.marketMatches = nm;
                g.ouMeanPredicted = predOu / n; g.ouActualRate = actOu / n; g.bttsMeanPredicted = predBt / n; g.bttsActualRate = actBt / n;
                g.bttsModelLogLoss = sBt / n; g.bttsBaseLogLoss = sBtB / n;
                if (nm > 0) { g.ouModelLogLoss = sOu / nm; g.ouMarketLogLoss = sOuM / nm; g.ouBaseLogLoss = sOuB / nm; }
                else { g.ouModelLogLoss = sOuAll / n; g.ouBaseLogLoss = sOuBAll / n; }
            }
            rep.goals = g;
        }
    }

    auto add = [&](const char* name, const std::vector<Probs3>& p, const std::vector<int>& a) {
        rep.models.push_back({name, scoreProbabilities(p, a)});
    };
    if (m_status.rf) add("Random Forest (ONNX)", rf, actual);
    if (m_status.poisson) add("Poisson goals model", pois, actual);
    if (m_status.rf && m_status.poisson) add("Blend (used by the app)", blend, actual);
    add("Elo baseline", elo, actual);
    add("Always league base rates", prior, actual);
    if (!market.empty()) add("Bookmaker (margin removed)", market, marketActual);
    std::sort(rep.models.begin(), rep.models.end(), [](const auto& a, const auto& b) { return a.scores.logLoss < b.scores.logLoss; });

    std::vector<int> plain, drawAware;
    for (const auto& p : blend) {
        plain.push_back(argmax(p));
        Probs3 q{p[0] * m_decisionWeights[0], p[1] * m_decisionWeights[1], p[2] * m_decisionWeights[2]};
        drawAware.push_back(argmax(q));
    }
    rep.confusionPlain = confusion(plain, actual);
    rep.confusionDrawAware = confusion(drawAware, actual);
    rep.calibration = calibration(blend, actual, 10);
    rep.ece = expectedCalibrationError(rep.calibration);

    rep.quality.matches = static_cast<int>(marketActual.size());
    rep.quality.modelLogLoss = scoreProbabilities(blendOnMarket, marketActual).logLoss;
    rep.quality.marketLogLoss = scoreProbabilities(market, marketActual).logLoss;

    // Betting simulations (flat 1-unit stakes, odds = Bet365 closing-day prices in the dataset).
    auto simulate = [&](const char* rule, bool fullRules) {
        BettingSummary s; s.rule = rule;
        double bank = 100.0, peak = 100.0;
        std::vector<double> profits;
        for (size_t n = 0; n < idx.size(); ++n) {
            const HistMatch& m = all[idx[n]];
            OddsQuote oq; oq.manual = true; oq.source = "Bet365";
            for (int k = 0; k < 3; ++k) oq.odds[k] = m.odds[k];
            if (!oq.complete()) continue;
            int pick = -1;
            if (!fullRules) {
                double best = -1e9;
                for (int k = 0; k < 3; ++k) { const double ev = blend[n][k] * m.odds[k] - 1.0; if (ev > best) { best = ev; pick = k; } }
                if (best < m_engine.config().minExpectedValue) continue;
            } else {
                Evidence e;
                e.home = m.home; e.away = m.away; e.rfAvailable = m_status.rf;
                if (m_status.rf) e.rf = rf[n];
                e.poisson = pois[n]; e.blended = blend[n];
                e.missingFeatures = rows[idx[n]].missing; e.totalFeatures = static_cast<int>(X[n].size());
                e.homeMatchesPlayed = rows[idx[n]].homeMatchesPlayed; e.awayMatchesPlayed = rows[idx[n]].awayMatchesPlayed;
                e.hasOdds = true; e.odds = oq; e.features = rows[idx[n]].byName;
                const auto r = m_engine.analyse(e);
                if (r.decision != Decision::PossibleValue) continue;
                pick = r.candidate;
            }
            const bool win = pick == m.target();
            const double profit = win ? m.odds[pick] - 1.0 : -1.0;
            bank += profit; peak = std::max(peak, bank);
            s.maxDrawdown = std::max(s.maxDrawdown, peak - bank);
            s.profit += profit; s.staked += 1.0; ++s.bets; s.wins += win; profits.push_back(profit);
        }
        s.finalBankroll = bank;
        if (s.bets) s.roi = s.profit / s.staked;
        if (s.bets > 1) {   // standard error of the mean profit per unit staked
            const double mean = s.profit / s.bets; double ss = 0;
            for (double v : profits) ss += (v - mean) * (v - mean);
            s.roiStdErr = std::sqrt(ss / (s.bets - 1)) / std::sqrt(static_cast<double>(s.bets));
        }
        return s;
    };
    rep.betting.push_back(simulate("EV rule (as in training backtest)", false));
    rep.betting.push_back(simulate("Full reasoning rules (edge, shrinkage, agreement, data checks)", true));

    rep.matches = static_cast<int>(idx.size());
    rep.from = isoFromSerial(all[idx.front()].date);
    rep.to = isoFromSerial(all[idx.back()].date);
    rep.rfWeight = w;
    rep.ok = true;
    return rep;
}

}  // namespace sba
