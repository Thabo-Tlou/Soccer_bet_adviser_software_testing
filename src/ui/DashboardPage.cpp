#include "DashboardPage.h"
#include "Async.h"
#include "Formatting.h"
#include "Theme.h"
#include <QHeaderView>
#include <QScrollArea>
#include <optional>

DashboardPage::DashboardPage(AppController* c, QWidget* parent) : QWidget(parent), m_c(c) {
    setObjectName("Page");
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->addWidget(scroll);
    auto* page = new QWidget; page->setObjectName("Page"); scroll->setWidget(page);
    auto* v = new QVBoxLayout(page); v->setContentsMargins(28, 24, 28, 24); v->setSpacing(16);

    v->addWidget(makeLabel("Dashboard", "h1"));

    auto* tiles = new QHBoxLayout; tiles->setSpacing(12);
    m_model = new StatTile("Prediction model", Icons::Predict); m_data = new StatTile("Historical data", Icons::Table);
    m_tracked = new StatTile("Tracked predictions", Icons::Picks); m_quality = new StatTile("Held-out log loss", Icons::Target);
    for (auto* t : {m_model, m_data, m_tracked, m_quality}) tiles->addWidget(t, 1);
    v->addLayout(tiles);

    auto* row = new QHBoxLayout; row->setSpacing(12);
    auto* fx = new Card("Fixtures");
    m_fxCard = fx;
    auto* more = makeButton("All matches", false); more->setProperty("flat", true);
    connect(more, &QPushButton::clicked, this, [this]() { emit gotoPage(1); });
    fx->headerRow()->addWidget(more);
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({"Kickoff", "Match", "Home / Draw / Away", "Status"});
    m_table->setItemDelegateForColumn(1, new MatchCellDelegate(m_table));
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setShowGrid(false); m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 150);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setMinimumHeight(330);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int r, int) { if (r < static_cast<int>(m_shown.size()) && !m_shown[r].isFinished()) emit openAnalysis(m_shown[r]); });
    fx->body()->addWidget(m_table);
    row->addWidget(fx, 3);

    auto* side = new QVBoxLayout; side->setSpacing(12);
    auto* st = new Card("System status");
    m_status = new QLabel; m_status->setTextFormat(Qt::RichText); m_status->setWordWrap(true);
    st->body()->addWidget(m_status);
    side->addWidget(st);
    auto* nt = new Card("Model check");
    m_note = new QLabel; m_note->setWordWrap(true); m_note->setTextFormat(Qt::RichText); m_note->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    nt->body()->addWidget(m_note);
    side->addWidget(nt);
    side->addStretch();
    row->addLayout(side, 2);
    v->addLayout(row, 1);

    connect(c, &AppController::initialised, this, [this]() { refresh(); });
    connect(c, &AppController::backtestFinished, this, [this]() { refresh(); });
    connect(c, &AppController::predictionsChanged, this, [this]() { refresh(); });
    connect(c->live(), &LiveDataService::statusChanged, this, [this]() { refreshStatus(); });
    refresh();
}

void DashboardPage::refresh() {
    const auto& st = m_c->service().status();
    if (m_c->ready()) {
        QString m = st.rf && st.poisson ? "RF + Poisson" : (st.rf ? "Random Forest" : "Poisson only");
        m_model->set(m, QString("%1 features, RF weight %2").arg(st.features).arg(m_c->service().rfWeight(), 0, 'f', 2));
        m_data->set(QString("%1 matches").arg(QLocale(QLocale::English).toString(st.matches)), QString("%1 to %2").arg(QString::fromStdString(st.firstDate), QString::fromStdString(st.lastDate)));
    } else {
        m_model->set(m_c->initError().isEmpty() ? "Loading..." : "Unavailable", m_c->initError());
        m_data->set("-", "");
    }
    if (m_c->store().isOpen()) {
        const auto s = m_c->store().summary();
        m_tracked->set(QString::number(s.total), QString("%1 settled with a real result").arg(s.settled));
    } else m_tracked->set("-", "history database unavailable");
    const auto& bt = m_c->backtest();
    if (bt.ok && std::isfinite(bt.quality.modelLogLoss))
        m_quality->set(Fmt::num(bt.quality.modelLogLoss, 4), QString("bookmaker %1 on %2 matches").arg(Fmt::num(bt.quality.marketLogLoss, 4)).arg(bt.quality.matches));
    else m_quality->set(m_c->backtestRunning() ? "Running..." : "-", "lower is better");

    QString verdict = "Backtest pending.";
    if (bt.ok && bt.quality.modelLogLoss >= bt.quality.marketLogLoss)
        verdict = QString("The model did <b>not</b> beat bookmaker probabilities on held-out matches (%1 vs %2). Treat apparent edges with suspicion.")
                      .arg(Fmt::num(bt.quality.modelLogLoss, 4), Fmt::num(bt.quality.marketLogLoss, 4));
    else if (bt.ok)
        verdict = QString("Model %1 vs bookmaker %2 on held-out matches: a small, unproven advantage.").arg(Fmt::num(bt.quality.modelLogLoss, 4), Fmt::num(bt.quality.marketLogLoss, 4));
    m_note->setText(QString("<span style='color:%1'>%2</span>").arg(Theme::hex(Theme::muted), verdict));
    refreshStatus();
    refreshFixtures();
}

void DashboardPage::refreshStatus() {
    auto dot = [](const QColor& c) { return QString("<span style='color:%1'>&#9679;</span> ").arg(Theme::hex(c)); };
    const auto& st = m_c->service().status();
    QString h;
    h += dot(st.dataset ? Theme::success : Theme::danger) + (st.dataset ? "Match history" : "Match history missing") + "<br>";
    h += dot(st.poisson ? Theme::success : Theme::warning) + (st.poisson ? "Poisson model" : "Poisson model unavailable") + "<br>";
    h += dot(st.rf ? Theme::success : Theme::warning) + (st.rf ? "Random Forest" : "Random Forest unavailable") + "<br>";
    h += dot(m_c->store().isOpen() ? Theme::success : Theme::danger) + (m_c->store().isOpen() ? "Prediction history" : "History database unavailable") + "<br>";
    auto src = [&](const SourceStatus& s) {
        QColor c = Theme::dim; QString t = s.message;
        if (s.state == SourceStatus::NotConfigured) t = "not connected";
        switch (s.state) {
            case SourceStatus::Fresh: c = Theme::primary; t = "live, " + Fmt::ago(s.lastEpoch); break;
            case SourceStatus::Stale: c = Theme::warning; t = "stale, last data " + Fmt::ago(s.lastEpoch); break;
            case SourceStatus::Error: c = Theme::danger; break;
            case SourceStatus::Loading: c = Theme::muted; t = "refreshing..."; break;
            default: break;
        }
        return dot(c) + Fmt::html(s.name) + ": " + Fmt::html(t) + "<br>";
    };
    h += src(m_c->live()->fixturesStatus());
    h += src(m_c->live()->oddsStatus());
    for (const auto& m : st.messages) h += dot(Theme::warning) + Fmt::html(QString::fromStdString(m)) + "<br>";
    m_status->setText(h);
}

void DashboardPage::refreshFixtures() {
    m_shown.clear();
    const auto now = QDateTime::currentSecsSinceEpoch();
    for (const auto& f : m_c->live()->fixtures())
        if ((f.isUpcoming() || f.isLive()) && f.kickoffEpoch > now - 3 * 3600) m_shown.push_back(f);
    std::sort(m_shown.begin(), m_shown.end(), [](const auto& a, const auto& b) { return a.kickoffEpoch < b.kickoffEpoch; });
    if (m_shown.size() > 8) m_shown.resize(8);
    const bool recorded = m_shown.empty();
    if (recorded) m_shown = m_c->recordedResults(8);            // no live feed: show the latest recorded results instead
    m_fxCard->setTitle(recorded ? "Recent results" : "Fixtures");
    m_table->horizontalHeaderItem(2)->setText(recorded ? "Score" : "Home / Draw / Away");
    m_table->clearSpans();
    m_table->setRowCount(static_cast<int>(m_shown.size()));
    const int gen = ++m_generation;
    for (int r = 0; r < static_cast<int>(m_shown.size()); ++r) {
        const auto& f = m_shown[r];
        const QString home = QString::fromStdString(f.homeDataset.empty() ? f.home : f.homeDataset), away = QString::fromStdString(f.awayDataset.empty() ? f.away : f.awayDataset);
        m_table->setItem(r, 0, new QTableWidgetItem(Fmt::kickoff(f.kickoffEpoch)));
        auto* mi = new QTableWidgetItem; mi->setData(Qt::UserRole, QStringList{home, away}); m_table->setItem(r, 1, mi);
        QString st = f.isLive() ? (f.demo ? "LIVE (demo)" : "LIVE") : (f.isFinished() ? "Full time" : "Upcoming");
        auto* si = new QTableWidgetItem(st); if (f.isLive()) si->setForeground(Theme::live); m_table->setItem(r, 3, si);
        if (recorded) {
            auto* sc = new QTableWidgetItem(QString("%1 - %2").arg(f.homeGoals).arg(f.awayGoals)); sc->setTextAlignment(Qt::AlignCenter);
            QFont bf = sc->font(); bf.setBold(true); sc->setFont(bf); m_table->setItem(r, 2, sc);
        } else {
            auto* bar = new ProbBar; bar->setProbs({}, false);
            m_table->setCellWidget(r, 2, bar);
        }
        m_table->setRowHeight(r, 44);
    }
    if (m_shown.empty()) {
        m_table->setRowCount(1);
        auto* it = new QTableWidgetItem("No fixtures yet. Connect football-data.org in Settings to load them.");
        it->setForeground(Theme::muted); m_table->setSpan(0, 0, 1, 4); m_table->setItem(0, 0, it); m_table->setRowHeight(0, 48);
        return;
    }
    if (recorded || !m_c->ready()) return;
    // Model probabilities for each fixture are computed off the UI thread.
    auto fixtures = m_shown;
    auto* svc = &m_c->service();
    runAsync(this, [fixtures, svc]() {
        std::vector<std::optional<sba::Probs3>> out;
        for (const auto& f : fixtures) {
            if (f.homeDataset.empty() || f.awayDataset.empty() || f.isLive()) { out.push_back(std::nullopt); continue; }
            sba::AnalysisRequest req; req.home = f.homeDataset; req.away = f.awayDataset;
            req.kickoffDateIso = f.kickoffIso.substr(0, 10); req.kickoffEpoch = f.kickoffEpoch; req.nowEpoch = QDateTime::currentSecsSinceEpoch();
            sba::Analysis a; std::string err;
            if (svc->analyse(req, a, err)) out.push_back(a.reasoning.model); else out.push_back(std::nullopt);
        }
        return out;
    }, [this, gen](const std::vector<std::optional<sba::Probs3>>& probs) {
        if (gen != m_generation) return;   // table was rebuilt meanwhile
        for (size_t r = 0; r < probs.size() && static_cast<int>(r) < m_table->rowCount(); ++r)
            if (auto* bar = qobject_cast<ProbBar*>(m_table->cellWidget(static_cast<int>(r), 2))) {
                if (probs[r]) bar->setProbs(*probs[r], true); else bar->setProbs({}, false);
            }
    });
}
