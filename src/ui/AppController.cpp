#include "AppController.h"
#include "Async.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include "art/AnimClock.h"
#include "art/Crest.h"
#include "Formatting.h"
#include <QProcess>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

AppController::AppController(QObject* parent) : QObject(parent) {
    m_settings = AppSettings::load();
    if (qEnvironmentVariableIntValue("SBA_DEMO") == 1) m_settings.demoLive = true;     // preview aid, not saved
    Fmt::setZone(m_settings.timeZone);
    loadSlip();
    m_live = new LiveDataService(&m_store, this);
    connect(m_live, &LiveDataService::fixturesChanged, this, &AppController::predictionsChanged);
    // Backend -> UI: crest URLs that arrive with fixtures are handed to the crest provider (downloaded once, then cached).
    connect(m_live, &LiveDataService::fixturesChanged, this, [this]() {
        for (const auto& f : m_live->fixtures()) {
            if (f.demo) continue;
            CrestProvider& cp = CrestProvider::instance();
            const QString h = QString::fromStdString(f.homeDataset.empty() ? f.home : f.homeDataset);
            const QString a = QString::fromStdString(f.awayDataset.empty() ? f.away : f.awayDataset);
            cp.registerUrl(h, QString::fromStdString(f.homeCrest));
            cp.registerUrl(a, QString::fromStdString(f.awayCrest));
        }
    });
    // Club logos dropped into any of these folders override the generated crests.
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QStringList dirs{QCoreApplication::applicationDirPath() + "/logos", QString::fromUtf8(SBA_SOURCE_DIR) + "/logos", appData + "/logos"};
    QDir().mkpath(appData + "/logos");
    CrestProvider::instance().setLogoDirs(dirs);
    AnimClock::instance().setEnabled(!m_settings.reduceMotion);
}

std::vector<sba::Fixture> AppController::recordedResults(int n) const {
    std::vector<sba::Fixture> out;
    if (!m_ready) return out;
    const auto& ms = m_service.dataset().matches();
    for (auto it = ms.rbegin(); it != ms.rend() && static_cast<int>(out.size()) < n; ++it) {
        sba::Fixture f;
        f.id = "dataset"; f.competition = "Premier League";
        f.home = f.homeDataset = it->home; f.away = f.awayDataset = it->away;
        f.status = "FINISHED"; f.homeGoals = it->fthg; f.awayGoals = it->ftag;
        f.kickoffEpoch = Fmt::ukEpoch(it->date, it->timeMin);
        f.source = "dataset";
        out.push_back(std::move(f));
    }
    return out;
}

void AppController::start() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_dbPath = dataDir + "/soccer_bet_adviser.db";
    if (!m_store.open(m_dbPath.toStdString()))
        m_initError = "Prediction history database could not be opened: " + QString::fromStdString(m_store.error());

    // Project folder searched by the knowledge base: the source tree if present, else the installed docs.
    const QString src = QString::fromUtf8(SBA_SOURCE_DIR);
    m_indexRoot = QDir(src).exists() ? src : QCoreApplication::applicationDirPath() + "/docs";

    // Fetch the newest results first (short timeout, falls back to the saved copy), then load everything.
    m_updater = new ResultsUpdater(this);
    connect(m_updater, &ResultsUpdater::finished, this, [this](bool, const QString& msg) { m_resultsMsg = msg; beginInit(); });
    if (m_settings.autoUpdateResults && qEnvironmentVariableIntValue("SBA_NO_UPDATE") != 1) m_updater->fetch();
    else beginInit();
}

void AppController::beginInit() {
    if (m_initStarted) return;
    m_initStarted = true;
    const std::string exeDir = QCoreApplication::applicationDirPath().toStdString();
    runAsync(this, [this, exeDir]() {
        m_paths = sba::locateResources(exeDir, SBA_SOURCE_DIR);
        for (const QString& f : ResultsUpdater::cachedFiles()) m_paths.updateCsvs.push_back(f.toStdString());
        return m_service.initialise(m_paths);
    }, [this](bool ok) {
        m_ready = ok;
        if (ok) {
            m_service.setRfWeight(m_settings.rfWeight);
            m_service.setRiskConfig(m_settings.risk);
            m_live->setSettings(m_settings);
            m_live->setDatasetTeams(m_service.dataset().teams());
            m_live->start();
            runBacktest();
        } else {
            m_live->setSettings(m_settings);
            const auto& msgs = m_service.status().messages;
            m_initError = msgs.empty() ? "Initialisation failed" : QString::fromStdString(msgs.front());
        }
        emit initialised(ok);
        rebuildIndex();
    });
}

// ---------------------------------------------------------------------------------------------- bet slip
void AppController::saveSlip() {
    QJsonArray arr;
    for (const auto& l : m_slip) {
        QJsonObject o;
        o["matchKey"] = QString::fromStdString(l.matchKey); o["matchLabel"] = QString::fromStdString(l.matchLabel);
        o["selKey"] = QString::fromStdString(l.selectionKey); o["selLabel"] = QString::fromStdString(l.selectionLabel);
        o["pWin"] = l.pWin; o["pLose"] = l.pLose; o["pOther"] = l.pOther; o["odds"] = l.odds;
        arr.append(o);
    }
    QSettings q;
    q.setValue("slip/legs", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    q.setValue("slip/stake", m_slipStake); q.setValue("slip/singles", m_slipSingles);
}

void AppController::loadSlip() {
    QSettings q;
    const auto doc = QJsonDocument::fromJson(q.value("slip/legs").toString().toUtf8());
    m_slip.clear();
    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        sba::SlipLeg l;
        l.matchKey = o["matchKey"].toString().toStdString(); l.matchLabel = o["matchLabel"].toString().toStdString();
        l.selectionKey = o["selKey"].toString().toStdString(); l.selectionLabel = o["selLabel"].toString().toStdString();
        l.pWin = o["pWin"].toDouble(); l.pLose = o["pLose"].toDouble(); l.pOther = o["pOther"].toDouble(); l.odds = o["odds"].toDouble(sba::kNaN);
        if (!l.matchKey.empty() && l.pWin > 0) m_slip.push_back(l);
    }
    m_slipStake = std::max(0.0, q.value("slip/stake", 0.0).toDouble());
    m_slipSingles = q.value("slip/singles", false).toBool();
}

bool AppController::addSlipLeg(const sba::SlipLeg& leg) {
    bool replaced = false;
    for (auto& l : m_slip) if (l.matchKey == leg.matchKey) { l = leg; replaced = true; break; }
    if (!replaced) m_slip.push_back(leg);
    saveSlip(); emit slipChanged();
    return replaced;
}
void AppController::removeSlipLeg(int i) { if (i >= 0 && i < static_cast<int>(m_slip.size())) { m_slip.erase(m_slip.begin() + i); saveSlip(); emit slipChanged(); } }
void AppController::setSlipOdds(int i, double odds) { if (i >= 0 && i < static_cast<int>(m_slip.size())) { m_slip[i].odds = odds; saveSlip(); emit slipChanged(); } }
void AppController::setSlipStake(double s) { m_slipStake = std::max(0.0, s); saveSlip(); emit slipChanged(); }
void AppController::setSlipAsSingles(bool on) { m_slipSingles = on; saveSlip(); emit slipChanged(); }
void AppController::clearSlip() { m_slip.clear(); saveSlip(); emit slipChanged(); }

void AppController::updateResultsAndRestart() {
    auto* u = new ResultsUpdater(this);
    connect(u, &ResultsUpdater::finished, this, [this, u](bool ok, const QString& msg) {
        u->deleteLater();
        m_resultsMsg = msg;
        if (ok) { QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1)); QCoreApplication::quit(); }
        else emit resultsUpdateFailed(msg);
    });
    u->fetch();
}

void AppController::applySettings(const AppSettings& s) {
    m_settings = s;
    m_settings.save();
    Fmt::setZone(s.timeZone);
    m_service.setRfWeight(s.rfWeight);
    m_service.setRiskConfig(s.risk);
    m_live->setSettings(s);
    AnimClock::instance().setEnabled(!s.reduceMotion);
    emit settingsChanged();
}

void AppController::analyse(const sba::AnalysisRequest& req, std::function<void(bool, const sba::Analysis&, const QString&)> done) {
    if (!m_ready) { done(false, {}, "Models are not loaded yet."); return; }
    struct Out { bool ok; sba::Analysis a; QString err; };
    runAsync(this, [this, req]() {
        Out o; std::string err;
        o.ok = m_service.analyse(req, o.a, err);
        o.err = QString::fromStdString(err);
        return o;
    }, [done](const Out& o) { done(o.ok, o.a, o.err); });
}

void AppController::runBacktest() {
    if (!m_ready || m_backtestRunning) return;
    m_backtestRunning = true;
    emit backtestStarted();
    runAsync(this, [this]() { return m_service.runBacktest(); }, [this](const sba::BacktestReport& r) {
        m_backtestRunning = false;
        m_backtest = r;
        if (r.ok) { m_service.setModelQuality(r.quality); m_service.setGoalsQuality(r.goals); }
        emit backtestFinished();
    });
}

void AppController::rebuildIndex() {
    if (m_indexBuilding) return;
    m_indexBuilding = true;
    const std::string root = m_indexRoot.toStdString();
    runAsync(this, [root]() {
        auto idx = std::make_shared<sba::TfIdfIndex>();
        idx->build(root);
        return idx;
    }, [this](std::shared_ptr<sba::TfIdfIndex> idx) {
        m_index = idx;
        m_indexBuilding = false;
        emit indexChanged();
    });
}
