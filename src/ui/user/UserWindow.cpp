#include "UserWindow.h"
#include "ui/Formatting.h"
#include "UserTheme.h"
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include "ui/art/Icons.h"
#include "ui/MarketsPanel.h"
#include "ui/design/Components.h"
#include <cmath>
static const double kPiLocal = 3.14159265358979323846;
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

UserWindow::UserWindow(AppController* c, QWidget* parent) : QMainWindow(parent), m_c(c) {
    setWindowTitle("Soccer Bet Adviser");
    resize(1360, 860);
    setMinimumSize(900, 640);
    m_hub = new UserDataHub(c, this);

    auto* root = new QWidget; root->setObjectName("UserRoot");
    setCentralWidget(root);
    root->setFocusPolicy(Qt::ClickFocus);
    QTimer::singleShot(0, root, [root]() { root->setFocus(); });
    auto* rv = new QVBoxLayout(root); rv->setContentsMargins(0, 0, 0, 0); rv->setSpacing(0);

    // ---------------------------------------------------------------- header: brand, navigation, status, admin
    auto* header = new QFrame; header->setObjectName("UHeader"); header->setFixedHeight(64);
    header->setStyleSheet(QString("QFrame#UHeader { background: %1; border-bottom: 1px solid %2; }").arg(UT::hex(UT::white), UT::hex(UT::line)));
    auto* hl = new QHBoxLayout(header); hl->setContentsMargins(24, 0, 24, 0); hl->setSpacing(28);
    auto* mark = new QLabel; mark->setFixedSize(38, 38);
    {
        QPixmap pm(76, 76); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
        QPainter pp(&pm); pp.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, 38, 38); g.setColorAt(0, DS::leaf500); g.setColorAt(1, DS::leaf800);
        pp.setPen(Qt::NoPen); pp.setBrush(g); pp.drawRoundedRect(QRectF(0, 0, 38, 38), 11, 11);
        Icons::paint(pp, Icons::Ball, QRectF(8, 8, 22, 22), Qt::white, 1.8);
        mark->setPixmap(pm); mark->setScaledContents(true);
    }
    auto* brand = new QLabel(QString("Soccer Bet <span style='color:%1'>Adviser</span>").arg(UT::hex(UT::accent)));
    brand->setStyleSheet(QString("font-size:18px; font-weight:800; color:%1; background:transparent;").arg(UT::hex(UT::ink)));
    auto* brandBox = new QWidget; auto* bb = new QHBoxLayout(brandBox); bb->setContentsMargins(0, 0, 0, 0); bb->setSpacing(11); bb->addWidget(mark); bb->addWidget(brand);
    m_nav = new NavTabs;
    const QStringList navLabels{"Home", "Scores", "Predictions", "Table", "My picks", "Bet slip"};
    const QList<Icons::Name> navIcons{Icons::Home, Icons::Live, Icons::Predict, Icons::Table, Icons::Picks, Icons::Target};
    m_nav->setTabs(navLabels, navIcons);
    auto* admin = new QPushButton("Admin"); admin->setProperty("ubtn", "darkghost"); admin->setCursor(Qt::PointingHandCursor);
    admin->setIcon(Icons::icon(Icons::Shield, DS::slate700, 18)); admin->setIconSize(QSize(18, 18));
    admin->setToolTip("Open the admin panel");
    m_status = new QLabel; m_status->setTextFormat(Qt::RichText); m_status->setFixedHeight(32); m_status->setAlignment(Qt::AlignCenter);
    hl->addWidget(brandBox); hl->addWidget(m_nav); hl->addStretch(1); hl->addWidget(m_status); hl->addWidget(admin);
    rv->addWidget(header);

    // ---------------------------------------------------------------- pages
    m_stack = new QStackedWidget;
    m_home = new UserHomePage(m_hub);
    m_scores = new UserScoresPage(m_hub);
    m_predictions = new UserPredictionsPage(m_hub);
    m_table = new UserTablePage(m_hub);
    m_picks = new UserPicksPage(m_hub);
    m_slip = new UserSlipPage(m_hub);
    m_match = new UserMatchPage(m_hub);
    for (QWidget* w : std::initializer_list<QWidget*>{m_home, m_scores, m_predictions, m_table, m_picks, m_slip, m_match}) m_stack->addWidget(w);
    rv->addWidget(m_stack, 1);

    // ---------------------------------------------------------------- footer
    auto* foot = new QLabel(QString("Estimates only, not financial advice  \u00B7  18+  \u00B7  Please gamble responsibly  \u00B7  v%1  \u00B7  Times in %2").arg(SBA_VERSION, Fmt::zoneLabel()));
    foot->setAlignment(Qt::AlignCenter); foot->setFixedHeight(30);
    foot->setStyleSheet(QString("color:%1; font-size:11px; background:%2; border-top:1px solid %3;").arg(UT::hex(UT::muted), UT::hex(UT::white), UT::hex(UT::line)));
    rv->addWidget(foot);

    auto refreshStatus = [this]() {
        const auto fx = m_c->live()->fixturesStatus();
        QColor dot = DS::slate400; QString txt = "Recorded results";
        if (m_c->settings().demoLive) { dot = DS::warning600; txt = "Demo mode"; }
        else if (fx.state == SourceStatus::Fresh) { dot = DS::success600; txt = "Live data"; }
        else if (fx.state == SourceStatus::Loading) { dot = DS::info600; txt = "Updating"; }
        else if (fx.state == SourceStatus::Stale || fx.state == SourceStatus::Error) { dot = DS::warning600; txt = "Data may be old"; }
        m_status->setText(QString("<span style='color:%1'>\u25CF</span>&nbsp; %2").arg(UT::hex(dot), txt));
        m_status->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600; background:%2; border:1px solid %3; border-radius:14px; padding:6px 13px;").arg(UT::hex(UT::soft), UT::hex(DS::slate50), UT::hex(UT::line)));
    };
    connect(m_c->live(), &LiveDataService::statusChanged, this, refreshStatus);
    connect(m_c, &AppController::settingsChanged, this, refreshStatus);
    refreshStatus();

    connect(m_nav, &NavTabs::changed, this, [this](int i) { goTab(i); });
    connect(m_slip, &UserSlipPage::browseRequested, this, [this]() { m_nav->setCurrent(Scores); goTab(Scores); });
    connect(m_slip, &UserSlipPage::countChanged, this, [this, navLabels, navIcons](int n) {
        QStringList l = navLabels; if (n > 0) l[Slip] = QString("Bet slip (%1)").arg(n);
        m_nav->setTabs(l, navIcons);
    });
    connect(m_match, &UserMatchPage::slipLegAdded, this, [this](bool replaced) {
        Toast::show(this, replaced ? "Replaced your earlier selection for this match on the bet slip." : "Added to your bet slip.", Toast::Success);
    });
    emit m_slip->countChanged(static_cast<int>(m_c->slipLegs().size()));
    connect(admin, &QPushButton::clicked, this, [this]() { emit adminRequested(); });
    connect(m_home, &UserHomePage::gotoTab, this, [this](int i) { m_nav->setCurrent(i); goTab(i); });
    connect(m_home, &UserHomePage::openMatch, this, [this](const MatchItem& m) { openMatch(m); });
    connect(m_scores, &UserScoresPage::openMatch, this, [this](const MatchItem& m) { openMatch(m); });
    connect(m_predictions, &UserPredictionsPage::openMatch, this, [this](const MatchItem& m) { openMatch(m); });
    connect(m_match, &UserMatchPage::backRequested, this, [this]() { m_stack->setCurrentIndex(m_prev); });
    for (int i = 0; i < 6; ++i) {
        auto* s = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), this);
        connect(s, &QShortcut::activated, this, [this, i]() { m_nav->setCurrent(i); goTab(i); });
    }
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this]() { if (m_stack->currentWidget() == m_match) m_stack->setCurrentIndex(m_prev); });
}

void UserWindow::goTab(int i) {
    m_stack->setCurrentIndex(i);
    if (i == Picks) m_picks->refresh();
}

void UserWindow::openMatch(const MatchItem& m) {
    if (m_stack->currentWidget() != m_match) m_prev = m_stack->currentIndex();
    m_match->showMatch(m);
    m_stack->setCurrentWidget(m_match);
}

void UserWindow::captureScreens(const QString& dir) {
    QDir().mkpath(dir);
    auto wait = [](int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) QApplication::processEvents(QEventLoop::AllEvents, 40); };
    auto shot = [this, dir, &wait](const QString& n) { wait(250); grab().save(dir + "/" + n + ".png"); };
    shot("home");
    { const QSize old = size(); resize(old.width(), 2300); shot("home_full"); resize(old); }      // whole Home page in one picture
    m_nav->setCurrent(Scores); m_stack->setCurrentIndex(Scores); shot("scores");
    for (const auto& m : m_hub->matchesOn(Fmt::today())) if (m.fx.isLive()) { m_match->showMatch(m); m_stack->setCurrentWidget(m_match); shot("match_live"); m_stack->setCurrentIndex(Scores); break; }
    // first finished match of the shown day, then an upcoming one
    auto day = m_hub->matchesOn(m_hub->defaultDate());
    if (!day.empty()) {
        m_match->showMatch(day.front()); m_stack->setCurrentWidget(m_match); shot("match_finished_h2h");
    }
    MatchItem up;
    for (const auto& m : m_hub->upcoming(14)) { up = m; break; }
    if (up.fx.home.empty()) {
        up.manual = true; up.day = Fmt::today().addDays(1);
        up.fx.home = up.fx.homeDataset = "Arsenal"; up.fx.away = up.fx.awayDataset = "Chelsea";
        up.fx.competition = "Premier League"; up.fx.status = "TIMED";
        sba::BookOdds b; b.bookmaker = "Your odds"; b.odds[0] = 1.95; b.odds[1] = 3.6; b.odds[2] = 4.1; b.updatedEpoch = QDateTime::currentSecsSinceEpoch();
        up.fx.books.push_back(b);
    }
    m_match->showMatch(up); m_stack->setCurrentWidget(m_match);
    wait(2500); shot("match_prediction");
    m_match->showTab("Markets"); wait(800); m_match->devFillOdds(); wait(500);
    { const QSize old = size(); resize(old.width(), 1500); wait(300); shot("match_markets"); resize(old); wait(300); }
    if (auto* mk = m_match->findChild<MarketsPanel*>()) { Q_UNUSED(mk); }
    {   // put two selections from different matches on the slip, then show it
        m_c->clearSlip();
        sba::SlipLeg a; a.matchKey = "m1"; a.matchLabel = "Arsenal v Chelsea"; a.selectionKey = "ou:2.5:o"; a.selectionLabel = "Over 2.5 goals"; a.pWin = 0.54; a.pLose = 0.46; a.odds = 2.05;
        sba::SlipLeg b; b.matchKey = "m2"; b.matchLabel = "Liverpool v Brighton"; b.selectionKey = "btts:y"; b.selectionLabel = "Both teams to score: Yes"; b.pWin = 0.55; b.pLose = 0.45; b.odds = 1.95;
        m_c->addSlipLeg(a); m_c->addSlipLeg(b); m_c->setSlipStake(20);
        m_nav->setCurrent(Slip); m_stack->setCurrentIndex(Slip); wait(600); shot("bet_slip");
        m_c->clearSlip(); m_c->setSlipStake(0);
        wait(300); shot("bet_slip_empty");
    }
    m_stack->setCurrentWidget(m_match);
    m_nav->setCurrent(Predictions); m_stack->setCurrentIndex(Predictions); wait(1500); shot("predictions");
    m_nav->setCurrent(Table); m_stack->setCurrentIndex(Table); shot("table");
    m_nav->setCurrent(Picks); goTab(Picks); shot("picks");
}
