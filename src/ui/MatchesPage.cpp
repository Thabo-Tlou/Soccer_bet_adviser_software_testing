#include "MatchesPage.h"
#include "Formatting.h"
#include "Theme.h"
#include <QHeaderView>
#include <QSplitter>
#include <cmath>

static QTableWidgetItem* cell(const QString& t, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter, const QColor& fg = QColor()) {
    auto* i = new QTableWidgetItem(t);
    i->setTextAlignment(a);
    if (fg.isValid()) i->setForeground(fg);
    return i;
}

MatchesPage::MatchesPage(AppController* c, QWidget* parent) : QWidget(parent), m_c(c) {
    setObjectName("Page");
    auto* v = new QVBoxLayout(this); v->setContentsMargins(28, 24, 28, 24); v->setSpacing(14);
    v->addWidget(makeLabel("Matches", "h1"));

    m_banner = new Banner;
    v->addWidget(m_banner);

    auto* bar = new Card;
    auto* br = new QHBoxLayout; br->setSpacing(10);
    m_statusFilter = new QComboBox; m_statusFilter->addItems({"All statuses", "Live", "Upcoming", "Finished"});
    m_teamFilter = new QLineEdit; m_teamFilter->setPlaceholderText("Filter by team"); m_teamFilter->setClearButtonEnabled(true);
    m_dateOn = new QCheckBox("Date range");
    m_from = new QDateEdit(Fmt::today().addDays(-3)); m_to = new QDateEdit(Fmt::today().addDays(14));
    for (auto* d : {m_from, m_to}) { d->setCalendarPopup(true); d->setDisplayFormat("d MMM yy"); d->setEnabled(false); }
    m_auto = new QCheckBox("Auto-refresh"); m_auto->setChecked(c->settings().autoRefresh);
    m_updated = makeLabel("", "dim");
    auto* refresh = makeButton("Refresh now", true);
    br->addWidget(m_statusFilter); br->addWidget(m_teamFilter, 1); br->addWidget(m_dateOn); br->addWidget(m_from); br->addWidget(m_to);
    br->addSpacing(8); br->addWidget(m_auto); br->addWidget(refresh);
    bar->body()->addLayout(br);
    bar->body()->addWidget(m_updated);
    v->addWidget(bar);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setChildrenCollapsible(false);
    m_table = new QTableWidget(0, 8);
    m_table->setHorizontalHeaderLabels({"Status", "Kickoff", "Match", "Score", "1", "X", "2", "Updated"});
    m_table->setItemDelegateForColumn(2, new MatchCellDelegate(m_table));
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(false); m_table->setAlternatingRowColors(true);
    auto* h = m_table->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::ResizeToContents);
    h->setSectionResizeMode(2, QHeaderView::Stretch);
    split->addWidget(m_table);

    auto* detail = new Card;
    m_vs = new TeamVs; detail->body()->addWidget(m_vs);
    m_detailTitle = makeLabel("Select a match", "dim"); m_detailTitle->setAlignment(Qt::AlignCenter);
    detail->body()->addWidget(m_detailTitle);
    detail->body()->addWidget(makeLabel("ODDS", "caption"));
    m_books = new QTableWidget(0, 5);
    m_books->setHorizontalHeaderLabels({"Bookmaker", "1", "X", "2", "Updated"});
    m_books->verticalHeader()->hide(); m_books->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_books->setShowGrid(false); m_books->setSelectionMode(QAbstractItemView::NoSelection);
    m_books->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_books->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    detail->body()->addWidget(m_books, 1);
    m_detailNote = makeLabel("", "dim"); m_detailNote->setWordWrap(true);
    detail->body()->addWidget(m_detailNote);
    m_analyse = makeButton("Analyse this match", true);
    m_analyse->setEnabled(false);
    detail->body()->addWidget(m_analyse);
    detail->setMinimumWidth(340);
    split->addWidget(detail);
    split->setStretchFactor(0, 3); split->setStretchFactor(1, 1);
    v->addWidget(split, 1);

    auto rebuildSlot = [this]() { rebuild(); };
    connect(m_statusFilter, &QComboBox::currentIndexChanged, this, rebuildSlot);
    connect(m_teamFilter, &QLineEdit::textChanged, this, rebuildSlot);
    connect(m_dateOn, &QCheckBox::toggled, this, [this](bool on) { m_from->setEnabled(on); m_to->setEnabled(on); rebuild(); });
    connect(m_from, &QDateEdit::dateChanged, this, rebuildSlot);
    connect(m_to, &QDateEdit::dateChanged, this, rebuildSlot);
    connect(m_auto, &QCheckBox::toggled, this, [this](bool on) { auto s = m_c->settings(); s.autoRefresh = on; m_c->applySettings(s); });
    connect(refresh, &QPushButton::clicked, this, [this]() { m_c->live()->refreshAll(true); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { showDetail(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int r, int) { if (r >= 0 && r < static_cast<int>(m_rows.size())) emit openAnalysis(m_rows[r]); });
    connect(m_analyse, &QPushButton::clicked, this, [this]() { const int r = m_table->currentRow(); if (r >= 0 && r < static_cast<int>(m_rows.size())) emit openAnalysis(m_rows[r]); });
    connect(c->live(), &LiveDataService::fixturesChanged, this, [this]() { rebuild(); });
    connect(c->live(), &LiveDataService::statusChanged, this, [this]() { updateStatus(); });
    connect(c, &AppController::settingsChanged, this, [this]() { m_auto->setChecked(m_c->settings().autoRefresh); });
    rebuild();
    updateStatus();
}

void MatchesPage::updateStatus() {
    const auto fx = m_c->live()->fixturesStatus(), od = m_c->live()->oddsStatus();
    const int maxAge = std::max(5, m_c->settings().fixturesRefreshMin * 3);
    QString line = QString("Fixtures %1  \u00B7  Odds %2")
        .arg(fx.lastEpoch ? Fmt::ago(fx.lastEpoch) : "no data", od.lastEpoch ? Fmt::ago(od.lastEpoch) : "no data");
    if (fx.remaining >= 0) line += QString("  \u00B7  %1 requests left").arg(fx.remaining);
    m_updated->setText(line);

    if (m_c->settings().demoLive) m_banner->show(Banner::Warning, "Demo mode is on: the live match below is simulated.");
    else if (fx.state == SourceStatus::NotConfigured)
        m_banner->show(Banner::Info, m_recorded ? "Live data is not connected, so recorded results are shown. Add a football-data.org key in Settings."
                                                : "Live data is not connected. Add a football-data.org key in Settings.");
    else if (fx.state == SourceStatus::Error) m_banner->show(Banner::Error, "Fixtures unavailable: " + fx.message);
    else if (fx.state == SourceStatus::Stale || m_c->live()->isStale(fx, maxAge)) m_banner->show(Banner::Warning, "Fixture data is stale (last update " + Fmt::ago(fx.lastEpoch) + ").");
    else if (od.state == SourceStatus::Error) m_banner->show(Banner::Warning, "Odds unavailable: " + od.message);
    else m_banner->show(Banner::Info, "");
}

void MatchesPage::rebuild() {
    m_rows.clear();
    const QString q = m_teamFilter->text().trimmed().toLower();
    const int sf = m_statusFilter->currentIndex();
    const qint64 from = m_from->date().startOfDay().toSecsSinceEpoch();
    const qint64 to = m_to->date().addDays(1).startOfDay().toSecsSinceEpoch();
    std::vector<sba::Fixture> source = m_c->live()->fixtures();
    m_recorded = source.empty();
    if (m_recorded) source = m_c->recordedResults(40);       // nothing live: browse the latest recorded results
    for (const auto& f : source) {
        if (sf == 1 && !f.isLive()) continue;
        if (sf == 2 && !f.isUpcoming()) continue;
        if (sf == 3 && !f.isFinished()) continue;
        if (m_dateOn->isChecked() && (f.kickoffEpoch < from || f.kickoffEpoch >= to)) continue;
        if (!q.isEmpty()) {
            const QString hay = QString::fromStdString(f.home + " " + f.away + " " + f.competition).toLower();
            if (!hay.contains(q)) continue;
        }
        m_rows.push_back(f);
    }
    std::sort(m_rows.begin(), m_rows.end(), [](const auto& a, const auto& b) {
        const int ra = a.isLive() ? 0 : (a.isUpcoming() ? 1 : 2), rb = b.isLive() ? 0 : (b.isUpcoming() ? 1 : 2);
        if (ra != rb) return ra < rb;
        return ra == 2 ? a.kickoffEpoch > b.kickoffEpoch : a.kickoffEpoch < b.kickoffEpoch;
    });
    m_table->setRowCount(static_cast<int>(m_rows.size()));
    const int maxAge = std::max(5, m_c->settings().fixturesRefreshMin * 3);
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) {
        const auto& f = m_rows[r];
        QString st = QString::fromStdString(f.status); QColor stc = Theme::muted;
        if (f.isLive()) { st = f.minute >= 0 ? QString("LIVE %1'").arg(f.minute) : "LIVE"; if (f.demo) st += " (demo)"; stc = Theme::live; }
        else if (f.isUpcoming()) st = "Upcoming";
        else if (f.isFinished()) st = "Full time";
        else if (st == "POSTPONED" || st == "CANCELLED" || st == "SUSPENDED") stc = Theme::danger;
        m_table->setItem(r, 0, cell(st, Qt::AlignLeft | Qt::AlignVCenter, stc));
        m_table->setItem(r, 1, cell(Fmt::kickoff(f.kickoffEpoch)));
        const QString home = QString::fromStdString(f.homeDataset.empty() ? f.home : f.homeDataset), away = QString::fromStdString(f.awayDataset.empty() ? f.away : f.awayDataset);
        auto* mi = cell(QString()); mi->setData(Qt::UserRole, QStringList{home, away}); m_table->setItem(r, 2, mi);
        const bool hasScore = f.homeGoals >= 0 && f.awayGoals >= 0;
        auto* sc = cell(hasScore ? QString("%1 - %2").arg(f.homeGoals).arg(f.awayGoals) : "vs", Qt::AlignCenter, hasScore ? Theme::text : Theme::dim);
        if (hasScore) { QFont bf = sc->font(); bf.setBold(true); sc->setFont(bf); }
        m_table->setItem(r, 3, sc);
        double best[3]; std::string names[3];
        const bool have = f.bestOdds(best, names);
        const bool oddsStale = have && (f.oddsFetchedEpoch <= 0 || QDateTime::currentSecsSinceEpoch() - f.oddsFetchedEpoch > m_c->settings().risk.maxOddsAgeMinutes * 60LL);
        for (int k = 0; k < 3; ++k) {
            auto* it = cell(have ? Fmt::num(best[k]) : "-", Qt::AlignCenter, oddsStale ? Theme::warning : Theme::text);
            if (have) it->setToolTip(QString("Best price: %1%2").arg(QString::fromStdString(names[k]), oddsStale ? "\nSTALE - refresh before relying on it" : ""));
            m_table->setItem(r, 4 + k, it);
        }
        const bool old = f.updatedEpoch > 0 && QDateTime::currentSecsSinceEpoch() - f.updatedEpoch > maxAge * 60LL;
        m_table->setItem(r, 7, cell(f.updatedEpoch > 0 ? Fmt::ago(f.updatedEpoch) : "-", Qt::AlignLeft | Qt::AlignVCenter, old ? Theme::warning : Theme::dim));
        m_table->setRowHeight(r, 44);
    }
    if (m_rows.empty()) m_table->setRowCount(0);
    showDetail();
    updateStatus();
}

void MatchesPage::showDetail() {
    const int r = m_table->currentRow();
    m_books->setRowCount(0);
    if (r < 0 || r >= static_cast<int>(m_rows.size())) { m_vs->setTeams("", ""); m_detailTitle->setText("Select a match"); m_detailNote->setText(""); m_analyse->setEnabled(false); return; }
    const auto& f = m_rows[r];
    m_vs->setTeams(QString::fromStdString(f.homeDataset.empty() ? f.home : f.homeDataset), QString::fromStdString(f.awayDataset.empty() ? f.away : f.awayDataset));
    m_detailTitle->setText(f.homeGoals >= 0 && f.awayGoals >= 0 ? QString("%1 - %2").arg(f.homeGoals).arg(f.awayGoals) : Fmt::kickoff(f.kickoffEpoch));
    m_analyse->setEnabled(!f.homeDataset.empty() && !f.awayDataset.empty() && m_c->ready() && !f.isFinished());
    QString note;
    if (f.homeDataset.empty() || f.awayDataset.empty()) note = "One team is not in the historical data, so no model analysis is available.";
    if (f.books.empty()) {
        if (note.isEmpty()) note = f.isFinished() ? "Odds are not kept for finished matches." : "No bookmaker odds for this fixture.";
        m_detailNote->setText(note); return;
    }
    double best[3]; f.bestOdds(best);
    m_books->setRowCount(static_cast<int>(f.books.size()));
    for (int i = 0; i < static_cast<int>(f.books.size()); ++i) {
        const auto& b = f.books[i];
        m_books->setItem(i, 0, cell(QString::fromStdString(b.bookmaker)));
        for (int k = 0; k < 3; ++k) {
            const bool isBest = std::fabs(b.odds[k] - best[k]) < 1e-9;
            m_books->setItem(i, 1 + k, cell(Fmt::num(b.odds[k]), Qt::AlignCenter, isBest ? Theme::success : Theme::text));
            if (isBest) { auto f2 = m_books->item(i, 1 + k)->font(); f2.setBold(true); m_books->item(i, 1 + k)->setFont(f2); }
        }
        m_books->setItem(i, 4, cell(Fmt::ago(b.updatedEpoch), Qt::AlignLeft | Qt::AlignVCenter, Theme::dim));
        m_books->setRowHeight(i, 30);
    }
    m_detailNote->setText(note + QString("Fetched %1. Green marks the best price.").arg(Fmt::ago(f.oddsFetchedEpoch)));
}
