#include "AnalysisPage.h"
#include "Formatting.h"
#include "Theme.h"
#include "core/DateUtil.h"
#include "art/Crest.h"
#include "design/Components.h"
#include "MarketsPanel.h"
#include <QTimer>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>
#include <cmath>

static QDoubleSpinBox* oddsBox() {
    auto* b = new QDoubleSpinBox; b->setRange(0.0, 1000.0); b->setDecimals(2); b->setSingleStep(0.05);
    b->setSpecialValueText("none"); b->setValue(0.0); b->setAlignment(Qt::AlignCenter);
    return b;
}

AnalysisPage::AnalysisPage(AppController* c, QWidget* parent) : QWidget(parent), m_c(c) {
    setObjectName("Page");
    auto* root = new QVBoxLayout(this); root->setContentsMargins(28, 24, 28, 24); root->setSpacing(14);
    root->addWidget(makeLabel("Match analysis", "h1"));
    m_banner = new Banner; root->addWidget(m_banner);

    auto* split = new QSplitter(Qt::Horizontal); split->setChildrenCollapsible(false);

    // ---------------- left: form
    auto* form = new Card("Match");
    form->setMinimumWidth(300); form->setMaximumWidth(380);
    auto* fl = form->body();
    fl->addWidget(makeLabel("HOME TEAM", "caption")); m_home = new QComboBox; fl->addWidget(m_home);
    fl->addWidget(makeLabel("AWAY TEAM", "caption")); m_away = new QComboBox; fl->addWidget(m_away);
    fl->addWidget(makeLabel("KICKOFF DATE", "caption"));
    m_date = new QDateEdit(Fmt::today().addDays(1)); m_date->setCalendarPopup(true); m_date->setDisplayFormat("yyyy-MM-dd"); fl->addWidget(m_date);
    fl->addSpacing(4);
    fl->addWidget(makeLabel("DECIMAL ODDS (OPTIONAL)", "caption"));
    auto* orow = new QHBoxLayout; m_o1 = oddsBox(); m_oX = oddsBox(); m_o2 = oddsBox();
    for (auto* b : {m_o1, m_oX, m_o2}) orow->addWidget(b);
    fl->addLayout(orow);
    auto* lab = new QHBoxLayout;
    for (const char* t : {"Home", "Draw", "Away"}) { auto* l = makeLabel(t, "dim"); l->setAlignment(Qt::AlignCenter); lab->addWidget(l); }
    fl->addLayout(lab);
    m_oddsInfo = makeLabel("", "dim"); m_oddsInfo->setWordWrap(true); m_oddsInfo->hide();
    fl->addWidget(m_oddsInfo);
    fl->addSpacing(6);
    m_run = makeButton("Analyse match", true); fl->addWidget(m_run);
    m_save = makeButton("Save prediction to history"); m_save->setEnabled(false); fl->addWidget(m_save);
    fl->addStretch();
    split->addWidget(form);

    // ---------------- right: results
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    m_results = new QWidget; m_results->setObjectName("Page");
    auto* rv = new QVBoxLayout(m_results); rv->setContentsMargins(0, 0, 8, 0); rv->setSpacing(12);

    auto* stageRow = new QHBoxLayout; stageRow->setSpacing(12);
    m_stage = new PitchWidget; m_stage->setMinimumHeight(268);
    stageRow->addWidget(m_stage, 3);
    auto* ai = new Card; ai->setMinimumWidth(240); ai->setMaximumWidth(280);
    ai->setElevated(false);                                  // it animates, so keep it flat
    ai->setStyleSheet(QString("QFrame#Card { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2); border: 1px solid %3; border-radius: 14px; }")
                          .arg(DS::hex(DS::stadiumTop), DS::hex(DS::paletteTurtleSkin), DS::hex(DS::slate800)));
    m_robot = new RobotWidget; m_robot->setMinimumHeight(206); m_robot->setCaption("Ready");
    ai->body()->setSpacing(4);
    ai->body()->addWidget(m_robot, 1);
    m_badge = new Badge("Ready", Badge::Neutral);
    ai->body()->addWidget(m_badge, 0, Qt::AlignHCenter);
    m_headline = makeLabel("", "muted"); m_headline->setWordWrap(true); m_headline->setAlignment(Qt::AlignCenter);
    m_headline->setStyleSheet(QString("color:%1; background:transparent;").arg(DS::hex(DS::leaf200)));
    ai->body()->addWidget(m_headline);
    stageRow->addWidget(ai, 1);
    rv->addLayout(stageRow);

    auto* prob = new Card("Probabilities");
    prob->headerRow()->addWidget(makeLabel("bar: model   |   tick: market", "dim"));
    m_bars[0] = new OutcomeBar("Home win", Theme::homeColor);
    m_bars[1] = new OutcomeBar("Draw", Theme::drawColor);
    m_bars[2] = new OutcomeBar("Away win", Theme::awayColor);
    for (auto* b : m_bars) prob->body()->addWidget(b);
    auto* mrow = new QHBoxLayout; mrow->setSpacing(10);
    const char* mt[5] = {"BOOKMAKER MARGIN", "VALUE CANDIDATE", "MODEL EDGE", "EXPECTED VALUE", "MODEL AGREEMENT"};
    for (int i = 0; i < 5; ++i) {
        auto* box = new QVBoxLayout; box->setSpacing(1);
        box->addWidget(makeLabel(mt[i], "caption"));
        m_metrics[i] = makeLabel("-", "h2"); box->addWidget(m_metrics[i]);
        mrow->addLayout(box, 1);
    }
    prob->body()->addSpacing(6); prob->body()->addWidget(makeDivider()); prob->body()->addSpacing(2);
    prob->body()->addLayout(mrow);
    rv->addWidget(prob);

    auto* mk = new Card("Betting markets");
    mk->setElevated(false);
    m_markets = new MarketsPanel(m_c, MarketsPanel::Admin);
    mk->body()->addWidget(m_markets);
    rv->addWidget(mk);

    auto* cmp = new Card("Model comparison");
    m_compare = new QTableWidget(0, 5);
    m_compare->setHorizontalHeaderLabels({"Source", "Home", "Draw", "Away", "Notes"});
    m_compare->verticalHeader()->hide(); m_compare->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_compare->setShowGrid(false); m_compare->setSelectionMode(QAbstractItemView::NoSelection); m_compare->setAlternatingRowColors(true);
    m_compare->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_compare->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_compare->setFixedHeight(190);
    cmp->body()->addWidget(m_compare);
    rv->addWidget(cmp);

    auto* ex = new Card("Reasoning");
    m_explain = new QTextBrowser; m_explain->setFrameShape(QFrame::NoFrame); m_explain->setMinimumHeight(420);
    m_explain->setStyleSheet("QTextBrowser { background: transparent; border: 0; padding: 0; }");
    ex->body()->addWidget(m_explain);
    rv->addWidget(ex);
    rv->addStretch();
    scroll->setWidget(m_results);
    split->addWidget(scroll);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    connect(m_home, &QComboBox::currentTextChanged, this, [this]() { updateStage(); });
    connect(m_away, &QComboBox::currentTextChanged, this, [this]() { updateStage(); });
    connect(m_date, &QDateEdit::dateChanged, this, [this]() { updateStage(); });
    connect(m_run, &QPushButton::clicked, this, [this]() { runAnalysis(); });
    connect(m_save, &QPushButton::clicked, this, [this]() { saveCurrent(); });
    for (auto* b : {m_o1, m_oX, m_o2}) connect(b, &QDoubleSpinBox::valueChanged, this, [this]() { m_apiOdds = false; m_oddsInfo->setText("Manual odds: make sure they are current."); m_oddsInfo->show(); });
    connect(c, &AppController::initialised, this, [this]() { populateTeams(); });
    populateTeams();
}

void AnalysisPage::populateTeams() {
    const bool keepH = m_home->count() > 0;
    if (keepH || !m_c->ready()) { if (!m_c->ready()) m_banner->show(Banner::Info, "Loading models and historical data..."); if (keepH) return; }
    if (!m_c->ready()) return;
    m_banner->show(Banner::Info, "");
    for (const auto& t : m_c->service().dataset().teams()) {
        const QString n = QString::fromStdString(t);
        m_home->addItem(CrestProvider::instance().icon(n, 20), n); m_away->addItem(CrestProvider::instance().icon(n, 20), n);
    }
    if (m_away->count() > 1) m_away->setCurrentIndex(1);
    m_home->setEnabled(true);
    updateStage();
}

// Keeps the stadium graphic in step with the form (crests appear as soon as a team is chosen).
void AnalysisPage::updateStage() {
    PitchData d;
    d.home = m_home->currentText(); d.away = m_away->currentText(); d.subtitle = "Premier League";
    d.kind = PitchData::Upcoming;
    d.whenText = QLocale::c().toString(m_date->date(), "ddd d MMM").toUpper();
    if (m_hasLast && QString::fromStdString(m_last.evidence.home) == d.home && QString::fromStdString(m_last.evidence.away) == d.away) {
        d.hasProbs = true; d.pHome = m_last.reasoning.model[0]; d.pDraw = m_last.reasoning.model[1]; d.pAway = m_last.reasoning.model[2];
    }
    m_stage->setData(d);
}

void AnalysisPage::loadFixture(const sba::Fixture& f) {
    if (!m_c->ready()) return;
    m_fixture = f; m_hasFixture = true;
    const int hi = m_home->findText(QString::fromStdString(f.homeDataset)), ai = m_away->findText(QString::fromStdString(f.awayDataset));
    if (hi < 0 || ai < 0) { m_banner->show(Banner::Warning, "This fixture contains a team that is not in the historical dataset."); return; }
    m_home->setCurrentIndex(hi); m_away->setCurrentIndex(ai);
    m_date->setDate(QDateTime::fromSecsSinceEpoch(f.kickoffEpoch).toUTC().date());
    double best[3];
    for (auto* b : {m_o1, m_oX, m_o2}) b->blockSignals(true);
    if (f.bestOdds(best)) {
        m_o1->setValue(best[0]); m_oX->setValue(best[1]); m_o2->setValue(best[2]); m_apiOdds = true;
        m_oddsInfo->setText(QString("Best of %1 bookmakers, fetched %2.").arg(f.books.size()).arg(Fmt::ago(f.oddsFetchedEpoch))); m_oddsInfo->show();
    } else {
        m_o1->setValue(0); m_oX->setValue(0); m_o2->setValue(0); m_apiOdds = false;
        m_oddsInfo->hide();
    }
    for (auto* b : {m_o1, m_oX, m_o2}) b->blockSignals(false);
    runAnalysis();
}

bool AnalysisPage::buildRequest(sba::AnalysisRequest& req) {
    req.home = m_home->currentText().toStdString();
    req.away = m_away->currentText().toStdString();
    req.kickoffDateIso = m_date->date().toString("yyyy-MM-dd").toStdString();
    req.nowEpoch = QDateTime::currentSecsSinceEpoch();
    const bool sameFixture = m_hasFixture && m_fixture.homeDataset == req.home && m_fixture.awayDataset == req.away;
    if (sameFixture) {
        req.kickoffEpoch = m_fixture.kickoffEpoch; req.kickoffIso = m_fixture.kickoffIso;
        req.matchStarted = m_fixture.isLive() || m_fixture.isFinished();
    } else {
        // manual match: kickoff time unknown, so only the date is checked (a past date cannot be "pre-match")
        const qint64 end = m_date->date().addDays(1).startOfDay().toSecsSinceEpoch();
        req.matchStarted = end <= req.nowEpoch;
    }
    const double o[3] = {m_o1->value(), m_oX->value(), m_o2->value()};
    const bool any = o[0] >= 1.01 || o[1] >= 1.01 || o[2] >= 1.01;
    if (any) {
        req.hasOdds = true;
        for (int i = 0; i < 3; ++i) req.odds.odds[i] = o[i] >= 1.01 ? o[i] : sba::kNaN;
        req.odds.manual = !(m_apiOdds && sameFixture);
        req.odds.source = req.odds.manual ? "manual entry" : "The Odds API (best price)";
        req.odds.fetchedAtEpoch = req.odds.manual ? 0 : m_fixture.oddsFetchedEpoch;
        if (!req.odds.complete()) { m_banner->show(Banner::Warning, "Enter all three odds (home, draw, away) or leave all empty."); return false; }
    }
    return true;
}

void AnalysisPage::runAnalysis() {
    if (!m_c->ready()) { m_banner->show(Banner::Warning, "Models are not loaded: " + m_c->initError()); return; }
    sba::AnalysisRequest req;
    if (!buildRequest(req)) return;
    if (req.home == req.away) { m_banner->show(Banner::Warning, "Choose two different teams."); return; }
    m_banner->show(Banner::Info, "");
    m_run->setEnabled(false); m_run->setText("Analysing...");
    m_lastDateIso = req.kickoffDateIso;
    m_robot->setAccent(DS::leaf300); m_robot->setState(RobotWidget::Thinking); m_robot->setCaption("Analysing");
    m_markets->clearBook("Pricing every market...");
    m_badge->set("Working", Badge::Info); m_headline->setText("");
    m_runTimer.start();
    m_c->analyse(req, [this](bool ok, const sba::Analysis& a, const QString& err) {
        // Hold the "thinking" animation for a moment so the hand-over to the verdict is readable.
        const int wait = std::max(0, 1100 - int(m_runTimer.elapsed()));
        QTimer::singleShot(wait, this, [this, ok, a, err]() {
            m_run->setEnabled(true); m_run->setText("Analyse match");
            if (!ok) { m_robot->setState(RobotWidget::Idle); m_robot->setCaption("Ready"); m_badge->set("Failed", Badge::Danger); m_banner->show(Banner::Error, "Analysis failed: " + err); m_save->setEnabled(false); return; }
            m_last = a; m_hasLast = true;
            showResult(a);
        });
    });
}

void AnalysisPage::devFillMarketOdds() {          // screenshots / demos only
    const std::pair<const char*, double> picks[] = {{"ou:2.5:o", 1.14}, {"btts:y", 1.0}, {"dc:1X", 1.07}, {"ah:h:-0.5", 1.04}, {"btts:n", 1.0}, {"ou:2.5:u", 0.80}};
    m_markets->devFill(picks, std::size(picks));
}

void AnalysisPage::showResult(const sba::Analysis& a) {
    const auto& r = a.reasoning; const auto& e = a.evidence;
    {   // every betting market for this match; odds already typed in the form carry over to the 1X2 rows
        const QString key = QString("%1|%2|%3").arg(QString::fromStdString(e.home), QString::fromStdString(e.away), QString::fromStdString(m_lastDateIso));
        m_markets->setBook(a.markets, key, QString::fromStdString(e.home + " v " + e.away));
        m_markets->presetOdds({{"r:1", m_o1->value()}, {"r:X", m_oX->value()}, {"r:2", m_o2->value()}});
    }
    QColor accent = DS::leaf300;
    switch (r.decision) {
        case sba::Decision::PossibleValue: m_badge->set("Possible value", Badge::Success); accent = DS::leaf300; break;
        case sba::Decision::InsufficientEvidence: m_badge->set("Insufficient evidence", Badge::Warning); accent = DS::slate400; break;
        default: m_badge->set("No recommendation", Badge::Neutral); break;
    }
    QString hl = QString::fromStdString(r.headline);
    if (hl.compare("No recommendation", Qt::CaseInsensitive) == 0) hl.clear();
    m_headline->setText(hl);
    m_robot->setAccent(accent); m_robot->setState(RobotWidget::Done); m_robot->setCaption("Analysis complete");
    updateStage();
    for (int i = 0; i < 3; ++i) m_bars[i]->setValues(r.model[i], r.hasMarket ? r.market[i] : std::nan(""));
    m_metrics[0]->setText(r.hasMarket ? Fmt::pct(r.overround) : "-");
    m_metrics[1]->setText(r.hasMarket && r.candidate >= 0 ? QString::fromStdString(sba::outcomeName(r.candidate)) : "-");
    m_metrics[2]->setText(r.hasMarket && r.candidate >= 0 ? Fmt::signedPct(r.edge[r.candidate]) + " pts" : "-");
    m_metrics[3]->setText(r.hasMarket && r.candidate >= 0 ? Fmt::signedPct(r.ev[r.candidate]) : "-");
    m_metrics[4]->setText(e.rfAvailable ? QString("%1 pts apart").arg(r.disagreement * 100, 0, 'f', 1) : "single model");

    struct Row { QString name; sba::Probs3 p; bool ok; QString note; };
    std::vector<Row> rows;
    rows.push_back({"Blend (used for decisions)", e.blended, true, e.rfAvailable ? QString("RF weight %1").arg(e.rfWeight, 0, 'f', 2) : "Poisson only"});
    if (e.rfAvailable) rows.push_back({"Random Forest (ONNX)", e.rf, true, "500 trees, 58 features"});
    rows.push_back({"Poisson goals model", e.poisson, true, std::isfinite(e.lambdaHome) ? QString("expected goals %1 - %2").arg(e.lambdaHome, 0, 'f', 2).arg(e.lambdaAway, 0, 'f', 2) : ""});
    rows.push_back({"Market (margin removed)", r.market, r.hasMarket, r.hasMarket ? QString::fromStdString(e.odds.source) : "no odds supplied"});
    m_compare->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        auto mk = [](const QString& t, Qt::Alignment al = Qt::AlignCenter, const QColor& c = QColor()) { auto* it = new QTableWidgetItem(t); it->setTextAlignment(al | Qt::AlignVCenter); if (c.isValid()) it->setForeground(c); return it; };
        m_compare->setItem(i, 0, mk(rows[i].name, Qt::AlignLeft));
        for (int k = 0; k < 3; ++k) m_compare->setItem(i, 1 + k, mk(rows[i].ok ? Fmt::pct(rows[i].p[k]) : "-"));
        m_compare->setItem(i, 4, mk(rows[i].note, Qt::AlignLeft, Theme::dim));
        m_compare->setRowHeight(i, 34);
    }

    auto list = [](const std::vector<std::string>& v, const QString& colour) {
        QString h = "<ul style='margin-top:2px; margin-bottom:6px; padding-left:18px'>";
        for (const auto& s : v) h += QString("<li style='margin-bottom:4px; color:%1'>%2</li>").arg(colour, Fmt::html(QString::fromStdString(s)));
        return h + "</ul>";
    };
    auto head = [](const QString& t) { return QString("<p style='margin:10px 0 2px 0; color:%1; font-weight:600; letter-spacing:1px; font-size:8pt'>%2</p>").arg(Theme::hex(Theme::dim), t.toUpper()); };
    QString html = QString("<div style='color:%1; font-size:10pt'>").arg(Theme::hex(Theme::text));
    if (!r.withheld.empty()) html += head("Why no recommendation") + list(r.withheld, Theme::hex(Theme::text));
    html += head("Supporting factors") + list(r.supporting, Theme::hex(Theme::text));
    html += head("Counter-arguments") + list(r.counter, Theme::hex(Theme::muted));
    html += head("Evidence checks") + "<table cellspacing='0' cellpadding='2'>";
    for (const auto& c : r.checks) {
        const QString line = QString::fromStdString(c);
        const QString tag = line.left(4);
        QColor col = tag == "PASS" ? Theme::success : (tag == "FAIL" ? Theme::danger : (tag == "WARN" ? Theme::warning : Theme::dim));
        html += QString("<tr><td style='color:%1; font-family:Consolas,monospace; padding-right:10px'>%2</td><td style='color:%3'>%4</td></tr>")
                    .arg(Theme::hex(col), Fmt::html(tag), Theme::hex(Theme::muted), Fmt::html(line.mid(6)));
    }
    html += "</table></div>";
    m_explain->setHtml(html);
    m_save->setEnabled(true);
}

void AnalysisPage::saveCurrent() {
    if (!m_hasLast) return;
    const auto& r = m_last.reasoning; const auto& e = m_last.evidence;
    sba::PredictionRecord rec;
    rec.home = e.home; rec.away = e.away; rec.kickoffIso = e.kickoffIso;
    rec.matchKey = e.home + "|" + e.away + "|" + m_lastDateIso;
    rec.createdIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    rec.modelVersion = m_c->service().modelVersion();
    rec.hasRf = e.rfAvailable; rec.rf = e.rf; rec.poisson = e.poisson; rec.finalP = e.blended;
    rec.hasMarket = r.hasMarket; rec.market = r.market;
    if (e.hasOdds) { for (int i = 0; i < 3; ++i) rec.odds[i] = e.odds.odds[i]; rec.oddsSource = e.odds.source; rec.oddsFetchedAt = e.odds.fetchedAtEpoch; }
    rec.decision = sba::decisionLabel(r.decision); rec.candidate = r.candidate;
    if (r.candidate >= 0 && r.hasMarket) { rec.edge = r.edge[r.candidate]; rec.ev = r.ev[r.candidate]; }
    rec.explanation = r.headline;
    bool created = false;
    const auto id = m_c->store().insertPrediction(rec, &created);
    if (id <= 0) m_banner->show(Banner::Error, "Could not save: " + QString::fromStdString(m_c->store().error()));
    else if (created) { Toast::show(this, "Prediction saved. It will be scored once the real result is known.", Toast::Success); m_c->notifyPredictionsChanged(); }
    else m_banner->show(Banner::Info, "A prediction for this match already exists - the original was kept unchanged.");
}
