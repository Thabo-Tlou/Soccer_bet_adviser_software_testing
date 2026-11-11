#include "UserOtherPages.h"
#include "ui/Formatting.h"
#include "ui/design/Components.h"
#include "UserTheme.h"
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>
#include <map>

static QLabel* cap(const QString& t) {
    auto* l = new QLabel(t.toUpper());
    l->setStyleSheet("color:#5F6858; font-size:11px; font-weight:600;");
    return l;
}
static QFrame* rule() { auto* f = new QFrame; f->setObjectName("Rule"); return f; }
static QWidget* emptyState(const QString& text) {
    const bool picks = text.startsWith("No saved picks");
    return new EmptyState(picks ? Icons::Picks : Icons::Matches, picks ? "No saved picks yet" : (text.startsWith("Upcoming fixtures") ? "No fixtures to show yet" : "Nothing to show here"), text);
}

// ====================================================================== Predictions

UserPredictionsPage::UserPredictionsPage(UserDataHub* hub, QWidget* parent) : QWidget(parent), m_hub(hub) {
    QVBoxLayout* v = nullptr;
    makeColumn(this, v);

    auto* introBox = new QWidget; auto* ib = new QVBoxLayout(introBox); ib->setContentsMargins(14, 12, 14, 0);
    auto* ai = new AiPanel;
    ai->showIdle("Ask the analyst", "Pick any two teams below, or open an upcoming match.");
    ib->addWidget(ai);
    v->addWidget(introBox);

    auto* form = new QWidget;
    auto* fv = new QVBoxLayout(form); fv->setContentsMargins(16, 12, 16, 12); fv->setSpacing(8);
    fv->addWidget(cap("Pick a match"));
    auto* r1 = new QHBoxLayout; r1->setSpacing(8);
    m_home = new QComboBox; m_away = new QComboBox;
    m_home->setMinimumWidth(150); m_away->setMinimumWidth(150);
    auto* vs = new QLabel("v"); vs->setStyleSheet("color:#5F6858;");
    m_date = new QDateEdit(Fmt::today().addDays(1)); m_date->setCalendarPopup(true); m_date->setDisplayFormat("ddd d MMM yyyy");
    m_date->setMinimumDate(Fmt::today()); m_date->setMinimumWidth(170);
    r1->addWidget(m_home, 1); r1->addWidget(vs); r1->addWidget(m_away, 1); r1->addWidget(m_date);
    fv->addLayout(r1);
    auto* r2 = new QHBoxLayout; r2->setSpacing(8);
    auto* ol = new QLabel("Bookmaker odds (optional)"); ol->setStyleSheet("color:#474F3C; font-size:12px;");
    r2->addWidget(ol);
    const char* ph[3] = {"1", "X", "2"};
    for (int i = 0; i < 3; ++i) { m_odds[i] = new QLineEdit; m_odds[i]->setPlaceholderText(ph[i]); m_odds[i]->setFixedWidth(64); m_odds[i]->setAlignment(Qt::AlignCenter); r2->addWidget(m_odds[i]); }
    r2->addStretch(1);
    auto* go = new QPushButton("Get prediction"); go->setProperty("ubtn", "solid"); go->setCursor(Qt::PointingHandCursor);
    r2->addWidget(go);
    fv->addLayout(r2);
    m_msg = new QLabel; m_msg->setStyleSheet("color:#474F3C; font-size:12px;"); m_msg->hide();
    fv->addWidget(m_msg);
    v->addWidget(form);

    m_filter = new TextTabs(false); m_filter->setTabs({"Upcoming matches", "Value only"});
    v->addWidget(m_filter);
    v->addWidget(rule());
    m_scroll = new QScrollArea; m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame);
    v->addWidget(m_scroll, 1);

    m_debounce.setSingleShot(true); m_debounce.setInterval(60);
    connect(&m_debounce, &QTimer::timeout, this, [this]() { rebuild(); });
    connect(go, &QPushButton::clicked, this, [this]() { submit(); });
    connect(m_filter, &TextTabs::changed, this, [this]() { rebuild(); });
    connect(hub, &UserDataHub::dataChanged, this, [this]() { populateTeams(); m_debounce.start(); });
    connect(hub, &UserDataHub::pickReady, this, [this]() { m_debounce.start(); });
    connect(hub, &UserDataHub::favsChanged, this, [this]() { m_debounce.start(); });
    populateTeams();
    rebuild();
}

void UserPredictionsPage::populateTeams() {
    if (m_home->count() > 0 || !m_hub->controller()->ready()) return;
    for (const auto& t : m_hub->controller()->service().dataset().teams()) {
        m_home->addItem(QString::fromStdString(t)); m_away->addItem(QString::fromStdString(t));
    }
    if (m_away->count() > 1) m_away->setCurrentIndex(1);
}

void UserPredictionsPage::submit() {
    auto warn = [this](const QString& t) { m_msg->setText(t); m_msg->show(); };
    if (!m_hub->controller()->ready()) { warn("The models are still loading."); return; }
    if (m_home->currentText().isEmpty() || m_home->currentText() == m_away->currentText()) { warn("Choose two different teams."); return; }
    MatchItem it;
    it.manual = true;
    it.day = m_date->date();
    auto& f = it.fx;
    f.home = f.homeDataset = m_home->currentText().toStdString();
    f.away = f.awayDataset = m_away->currentText().toStdString();
    f.competition = "Premier League";
    f.status = "TIMED";
    int filled = 0; double o[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        QString t = m_odds[i]->text().trimmed(); t.replace(',', '.');
        if (t.isEmpty()) continue;
        bool ok = false; const double d = t.toDouble(&ok);
        if (!ok || d < 1.01) { warn("Odds must be decimal numbers such as 2.10."); return; }
        o[i] = d; ++filled;
    }
    if (filled != 0 && filled != 3) { warn("Enter all three odds (1, X, 2) or leave them empty."); return; }
    if (filled == 3) {
        sba::BookOdds b; b.bookmaker = "Your odds"; for (int i = 0; i < 3; ++i) b.odds[i] = o[i];
        b.updatedEpoch = QDateTime::currentSecsSinceEpoch();
        f.books.push_back(b);
    }
    m_msg->hide();
    emit openMatch(it);
}

void UserPredictionsPage::rebuild() {
    auto items = m_hub->upcoming(14);
    auto* list = new QWidget;
    auto* lv = new QVBoxLayout(list); lv->setContentsMargins(0, 0, 0, 0); lv->setSpacing(0);
    int shown = 0;
    std::map<QDate, std::vector<const MatchItem*>> byDay;
    for (const auto& m : items) {
        m_hub->requestPick(m);
        if (m_filter->current() == 1) {
            const PickInfo* p = m_hub->pick(m);
            if (!p || !p->ok || !p->a || p->a->reasoning.decision != sba::Decision::PossibleValue) continue;
        }
        byDay[m.day].push_back(&m);
    }
    for (const auto& kv : byDay) {
        lv->addWidget(new SectionHeader(QLocale::c().toString(kv.first, "dddd d MMMM"), QString::number(kv.second.size())));
        for (const MatchItem* m : kv.second) {
            auto* row = new MatchRow;
            row->setItem(*m, m_hub->pick(*m), m_hub->isFav(m->key()));
            const MatchItem copy = *m;
            connect(row, &MatchRow::clicked, this, [this, copy]() { emit openMatch(copy); });
            connect(row, &MatchRow::favToggled, this, [this, copy]() { m_hub->toggleFav(copy.key()); });
            lv->addWidget(row);
            ++shown;
        }
    }
    if (!shown) {
        if (!m_hub->hasFixtures()) lv->addWidget(emptyState("Upcoming fixtures appear here once live data is connected. You can still get a prediction for any two teams above."));
        else lv->addWidget(emptyState(m_filter->current() == 1 ? "No upcoming match shows clear value right now. That is normal: most matches don't." : "No upcoming matches in the next two weeks."));
    }
    lv->addStretch(1);
    m_scroll->setWidget(list);
}

// ====================================================================== Table

UserTablePage::UserTablePage(UserDataHub* hub, QWidget* parent) : QWidget(parent), m_hub(hub) {
    QVBoxLayout* v = nullptr;
    makeColumn(this, v);

    auto* head = new QWidget;
    auto* hl = new QHBoxLayout(head); hl->setContentsMargins(16, 14, 16, 12);
    auto* tv = new QVBoxLayout; tv->setSpacing(2);
    auto* title = new QLabel("Premier League"); title->setStyleSheet("font-size:22px; font-weight:700;");
    m_sub = new QLabel; m_sub->setStyleSheet("color:#5F6858; font-size:12px;");
    tv->addWidget(title); tv->addWidget(m_sub);
    m_season = new QComboBox; m_season->setMinimumWidth(110);
    hl->addLayout(tv, 1); hl->addWidget(m_season, 0, Qt::AlignVCenter);
    v->addWidget(head);
    v->addWidget(rule());

    m_scroll = new QScrollArea; m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget; auto* bv = new QVBoxLayout(body); bv->setContentsMargins(0, 0, 0, 0); bv->setSpacing(0);
    m_table = new StandingsWidget; bv->addWidget(m_table);
    auto* legend = new QLabel("<span style='color:#4f6a30'>&#9632;</span>&nbsp; Top four &nbsp;&nbsp;&nbsp; <span style='color:#B3402A'>&#9632;</span>&nbsp; Bottom three");
    legend->setContentsMargins(16, 12, 16, 16); legend->setStyleSheet("color:#474F3C; font-size:12px;");
    bv->addWidget(legend); bv->addStretch(1);
    m_scroll->setWidget(body);
    v->addWidget(m_scroll, 1);

    connect(m_season, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    connect(hub, &UserDataHub::dataChanged, this, [this]() { populate(); });
    populate();
}

void UserTablePage::populate() {
    if (m_season->count() > 0 || !m_hub->controller()->ready()) return;
    m_season->blockSignals(true);
    for (int s : m_hub->seasons()) m_season->addItem(UserDataHub::seasonLabel(s), s);
    m_season->blockSignals(false);
    rebuild();
}

void UserTablePage::rebuild() {
    if (m_season->count() == 0) { m_sub->setText("Loading..."); m_table->setRows({}); return; }
    const int season = m_season->currentData().toInt();
    auto rows = m_hub->table(season);
    int played = 0; for (const auto& r : rows) played = std::max(played, r.played);
    played = static_cast<int>(rows.size() ? [&]() { int t = 0; for (const auto& r : rows) t += r.played; return t / 2; }() : 0);
    m_sub->setText(QString("Season %1  |  %2 matches played").arg(UserDataHub::seasonLabel(season)).arg(played));
    m_table->setRows(rows);
}

// ====================================================================== My picks

UserPicksPage::UserPicksPage(UserDataHub* hub, QWidget* parent) : QWidget(parent), m_hub(hub) {
    QVBoxLayout* v = nullptr;
    makeColumn(this, v);

    auto* head = new QWidget;
    auto* hl = new QHBoxLayout(head); hl->setContentsMargins(16, 14, 16, 12);
    auto* tv = new QVBoxLayout; tv->setSpacing(2);
    auto* title = new QLabel("My picks"); title->setStyleSheet("font-size:22px; font-weight:700;");
    m_summary = new QLabel; m_summary->setStyleSheet("color:#474F3C; font-size:12px;");
    tv->addWidget(title); tv->addWidget(m_summary);
    m_update = new QPushButton("Update results"); m_update->setCursor(Qt::PointingHandCursor);
    hl->addLayout(tv, 1); hl->addWidget(m_update, 0, Qt::AlignVCenter);
    v->addWidget(head);
    m_note = new QLabel; m_note->setContentsMargins(16, 6, 16, 6); m_note->setStyleSheet("color:#474F3C; background:#f6f6f6; font-size:12px;"); m_note->hide();
    v->addWidget(m_note);
    v->addWidget(rule());

    m_scroll = new QScrollArea; m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame);
    v->addWidget(m_scroll, 1);

    connect(m_update, &QPushButton::clicked, this, [this]() { updateResults(); });
    connect(hub->controller(), &AppController::predictionsChanged, this, [this]() { refresh(); });
    refresh();
}

void UserPicksPage::updateResults() {
    m_update->setEnabled(false);
    m_note->hide();
    int n = m_hub->settleFromData();
    m_hub->controller()->live()->fetchRecentResults([this, n](const std::vector<sba::Fixture>& list, const QString& err) {
        m_update->setEnabled(true);
        int total = n;
        if (!list.empty()) total += m_hub->controller()->store().settleFromFixtures(list, QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString());
        m_note->setText(total ? QString("%1 pick(s) updated with the final result.").arg(total)
                              : (err.isEmpty() || err.startsWith("No football-data") ? QString("No new results for your picks yet.") : QString("No new results. ") + err));
        m_note->show();
        refresh();
    });
}

void UserPicksPage::refresh() {
    m_hub->settleFromData();
    auto recs = m_hub->controller()->store().all();
    std::reverse(recs.begin(), recs.end());
    int settled = 0, correct = 0;
    auto* list = new QWidget;
    auto* lv = new QVBoxLayout(list); lv->setContentsMargins(0, 0, 0, 0); lv->setSpacing(0);
    for (const auto& r : recs) {
        int best = 0;
        for (int i = 1; i < 3; ++i) if (r.finalP[static_cast<size_t>(i)] > r.finalP[static_cast<size_t>(best)]) best = i;
        const QString home = QString::fromStdString(r.home), away = QString::fromStdString(r.away);
        const QDate d = r.matchKey.size() >= 10 ? QDate::fromString(QString::fromStdString(r.matchKey.substr(r.matchKey.size() - 10)), Qt::ISODate) : QDate();
        QString pick = (best == 0 ? home + " to win" : (best == 1 ? QStringLiteral("Draw") : away + " to win")) + "  |  " + pctText(r.finalP[static_cast<size_t>(best)]);
        if (r.decision == sba::decisionLabel(sba::Decision::PossibleValue)) pick += "  |  value flagged";
        auto state = PickRowWidget::Pending; QString result;
        if (r.actual >= 0) {
            ++settled; state = (r.actual == best) ? PickRowWidget::Won : PickRowWidget::Lost;
            if (state == PickRowWidget::Won) ++correct;
            result = r.actual == 0 ? "Home win" : (r.actual == 1 ? "Draw" : "Away win");
        }
        auto* row = new PickRowWidget;
        row->set(d.isValid() ? QLocale::c().toString(d, "d MMM yy") : QString("-"), home + "  v  " + away, pick, state, result);
        row->setTeams(home, away);
        lv->addWidget(row);
    }
    if (recs.empty()) lv->addWidget(emptyState("No saved picks yet. Open a match, then press \"Save to My picks\" on the Prediction tab. Your original prediction is kept so you can judge it fairly later."));
    lv->addStretch(1);
    m_scroll->setWidget(list);
    if (recs.empty()) m_summary->setText("Nothing saved yet");
    else if (settled == 0) m_summary->setText(QString("%1 saved, none played yet").arg(recs.size()));
    else m_summary->setText(QString("%1 saved  |  %2 played  |  %3 of %2 correct").arg(recs.size()).arg(settled).arg(correct));
}
