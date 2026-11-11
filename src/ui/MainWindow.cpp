#include "MainWindow.h"
#include "DashboardPage.h"
#include "Formatting.h"
#include "MatchesPage.h"
#include "PerformancePage.h"
#include "SearchPage.h"
#include "SettingsPage.h"
#include "Theme.h"
#include <QElapsedTimer>
#include <QKeySequence>
#include <QLineEdit>
#include <QShortcut>
#include <QPainter>
#include "design/Components.h"

static const char* kPageNames[] = {"Dashboard", "Matches", "Match analysis", "Performance", "Knowledge search", "Settings"};

MainWindow::MainWindow(AppController* c, QWidget* parent) : QMainWindow(parent), m_c(c) {
    setWindowTitle("Soccer Bet Adviser - Admin");
    resize(1440, 900); setMinimumSize(1100, 720);
    auto* central = new QWidget; setCentralWidget(central);
    central->setFocusPolicy(Qt::ClickFocus);
    QTimer::singleShot(0, central, [central]() { central->setFocus(); });      // no button starts with a focus ring
    auto* root = new QHBoxLayout(central); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);

    // ---------------- sidebar (brand, grouped navigation, customer-view link)
    auto* side = new QWidget; side->setObjectName("Sidebar"); side->setFixedWidth(240);
    auto* sv = new QVBoxLayout(side); sv->setContentsMargins(14, 16, 14, 14); sv->setSpacing(2);
    {
        auto* brandRow = new QHBoxLayout; brandRow->setContentsMargins(6, 0, 0, 0); brandRow->setSpacing(11);
        auto* mark = new QLabel; mark->setFixedSize(38, 38);
        QPixmap pm(76, 76); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
        { QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); QLinearGradient g(0, 0, 38, 38); g.setColorAt(0, DS::leaf500); g.setColorAt(1, DS::leaf700);
          p.setPen(Qt::NoPen); p.setBrush(g); p.drawRoundedRect(QRectF(0, 0, 38, 38), 11, 11); Icons::paint(p, Icons::Ball, QRectF(8, 8, 22, 22), Qt::white, 1.8); }
        mark->setPixmap(pm); mark->setScaledContents(true);
        auto* names = new QVBoxLayout; names->setSpacing(0);
        auto* t1 = new QLabel("Soccer Bet Adviser"); t1->setStyleSheet("font-size:14px; font-weight:700;");
        auto* t2 = new QLabel("Admin console"); t2->setStyleSheet(QString("font-size:11px; color:%1;").arg(Theme::hex(Theme::dim)));
        names->addWidget(t1); names->addWidget(t2);
        brandRow->addWidget(mark); brandRow->addLayout(names, 1);
        sv->addLayout(brandRow);
        sv->addSpacing(18);
    }
    m_nav = new QButtonGroup(this); m_nav->setExclusive(true);
    struct Item { Icons::Name icon; int page; };
    auto section = [&](const QString& label) {
        auto* l = new QLabel(label.toUpper()); l->setStyleSheet(QString("font-size:10px; font-weight:700; letter-spacing:1px; color:%1; padding:12px 0 4px 10px;").arg(Theme::hex(Theme::dim)));
        sv->addWidget(l);
    };
    auto addNav = [&](Icons::Name icon, int page) {
        auto* b = new SideNavButton(icon, kPageNames[page], QString("%1  (Ctrl+%2)").arg(kPageNames[page]).arg(page + 1));
        m_nav->addButton(b, page); sv->addWidget(b);
    };
    section("Overview");   addNav(Icons::Dashboard, 0); addNav(Icons::Matches, 1);
    section("Analysis");   addNav(Icons::Predict, 2);   addNav(Icons::Performance, 3); addNav(Icons::Search, 4);
    section("System");     addNav(Icons::Settings, 5);
    sv->addStretch(1);
    {
        auto* userView = new QPushButton("  Customer view"); userView->setCursor(Qt::PointingHandCursor);
        userView->setIcon(Icons::icon(Icons::Home, DS::slate700, 18)); userView->setIconSize(QSize(18, 18));
        userView->setToolTip("Switch to the customer view");
        sv->addWidget(userView);
        connect(userView, &QPushButton::clicked, this, [this]() { emit userViewRequested(); });
        auto* ver = new QLabel(QString("Version %1").arg(SBA_VERSION)); ver->setAlignment(Qt::AlignCenter);
        ver->setStyleSheet(QString("font-size:11px; color:%1; padding-top:8px;").arg(Theme::hex(Theme::dim)));
        sv->addWidget(ver);
        m_statusRight = ver;
    }
    root->addWidget(side);

    // ---------------- right column: top bar / pages / status bar
    auto* col = new QVBoxLayout; col->setContentsMargins(0, 0, 0, 0); col->setSpacing(0);
    auto* top = new QWidget; top->setObjectName("TopBar"); top->setFixedHeight(60);
    auto* tl = new QHBoxLayout(top); tl->setContentsMargins(28, 0, 28, 0); tl->setSpacing(10);
    auto* admin = new QLabel("Admin"); admin->setStyleSheet(QString("color:%1;").arg(Theme::hex(Theme::dim)));
    auto* sep = new QLabel("\u203A"); sep->setStyleSheet(QString("color:%1;").arg(Theme::hex(Theme::dim)));
    m_crumb = new QLabel; m_crumb->setStyleSheet("font-weight:600;");
    m_modelChip = new Badge("Model: loading", Badge::Neutral); m_apiChip = new Badge("API: -", Badge::Neutral);
    tl->addWidget(admin); tl->addWidget(sep); tl->addWidget(m_crumb); tl->addStretch();
    tl->addWidget(m_modelChip, 0, Qt::AlignVCenter); tl->addWidget(m_apiChip, 0, Qt::AlignVCenter);
    col->addWidget(top);

    m_stack = new QStackedWidget;
    auto* dash = new DashboardPage(c); auto* matches = new MatchesPage(c); m_analysis = new AnalysisPage(c);
    auto* perf = new PerformancePage(c); auto* search = new SearchPage(c); auto* settings = new SettingsPage(c);
    for (QWidget* w : std::initializer_list<QWidget*>{dash, matches, m_analysis, perf, search, settings}) m_stack->addWidget(w);
    col->addWidget(m_stack, 1);

    auto* sb = new QWidget; sb->setObjectName("StatusBar"); sb->setFixedHeight(28);
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(28, 0, 28, 0);
    m_statusText = new QLabel("Starting...");
    sl->addWidget(m_statusText); sl->addStretch();
    sl->addWidget(new QLabel(QString("Times in %1").arg(Fmt::zoneLabel())));
    col->addWidget(sb);
    root->addLayout(col, 1);

    connect(m_nav, &QButtonGroup::idClicked, this, [this](int id) { goTo(id); });
    for (int i = 0; i < 6; ++i) { auto* s = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), this); connect(s, &QShortcut::activated, this, [this, i]() { goTo(i); }); }
    connect(dash, &DashboardPage::gotoPage, this, &MainWindow::goTo);
    auto open = [this](const sba::Fixture& f) { goTo(2); m_analysis->loadFixture(f); };
    connect(dash, &DashboardPage::openAnalysis, this, open);
    connect(matches, &MatchesPage::openAnalysis, this, open);
    connect(c, &AppController::initialised, this, [this]() { refreshChips(); });
    connect(c, &AppController::settingsChanged, this, [this]() { refreshChips(); });
    connect(c, &AppController::backtestFinished, this, [this]() { refreshChips(); });
    connect(c, &AppController::backtestStarted, this, [this]() { m_statusText->setText("Running held-out backtest..."); });
    connect(c->live(), &LiveDataService::statusChanged, this, [this]() { refreshChips(); });
    goTo(0);
    refreshChips();
}

void MainWindow::goTo(int i) {
    m_stack->setCurrentIndex(i);
    if (auto* b = m_nav->button(i)) b->setChecked(true);
    m_crumb->setText(kPageNames[i]);
}

void MainWindow::refreshChips() {
    const auto& st = m_c->service().status();
    if (!m_c->ready()) m_modelChip->set(m_c->initError().isEmpty() ? "Model: loading" : "Model: unavailable", m_c->initError().isEmpty() ? Badge::Neutral : Badge::Danger);
    else if (st.rf && st.poisson) m_modelChip->set("Model: RF + Poisson", Badge::Success);
    else m_modelChip->set(st.rf ? "Model: RF only" : "Model: Poisson only", Badge::Warning);

    const auto fx = m_c->live()->fixturesStatus();
    switch (fx.state) {
        case SourceStatus::Fresh: m_apiChip->set("Live data", Badge::Success); break;
        case SourceStatus::Stale: m_apiChip->set("Data stale", Badge::Warning); break;
        case SourceStatus::Error: m_apiChip->set("Data error", Badge::Danger); break;
        case SourceStatus::Loading: m_apiChip->set("Refreshing", Badge::Info); break;
        case SourceStatus::NotConfigured: m_apiChip->set("Live data off", Badge::Neutral); break;
        default: m_apiChip->set("Idle", Badge::Neutral); break;
    }
    if (m_c->settings().demoLive) m_apiChip->set("Demo mode", Badge::Warning);
    if (m_c->ready())
        m_statusText->setText(QString("%1 matches  \u00B7  %2 to %3")
                                  .arg(st.matches).arg(QString::fromStdString(st.firstDate), QString::fromStdString(st.lastDate)));
    else m_statusText->setText(m_c->initError().isEmpty() ? "Loading models and data..." : m_c->initError());
}

#include <QApplication>
#include <QDir>
#include <QTimer>
void MainWindow::captureScreens(const QString& dir) {
    QDir().mkpath(dir);
    const char* names[] = {"dashboard", "matches", "analysis", "performance", "search", "settings"};
    auto shot = [this, dir](const QString& n) { QApplication::processEvents(); grab().save(dir + "/" + n + ".png"); };
    for (int i = 0; i < 6; ++i) {
        goTo(i);
        if (i == 2) {
            sba::Fixture f; f.homeDataset = "Arsenal"; f.awayDataset = "Chelsea"; f.home = "Arsenal"; f.away = "Chelsea";
            f.kickoffEpoch = QDateTime::currentSecsSinceEpoch() + 2 * 86400; f.status = "TIMED"; f.oddsFetchedEpoch = QDateTime::currentSecsSinceEpoch() - 300;
            sba::BookOdds b; b.bookmaker = "Bet365"; b.odds[0] = 1.95; b.odds[1] = 3.60; b.odds[2] = 4.10; f.books.push_back(b);
            m_analysis->loadFixture(f);
            QElapsedTimer t; t.start();
            while (t.elapsed() < 2500) { QApplication::processEvents(QEventLoop::AllEvents, 50); }
            // the markets card lives below the fold: capture the page tall, with a few odds typed in so verdicts and stakes show
            m_c->setStaking(1000, 1);
            m_analysis->devFillMarketOdds();
            const QSize old = size(); resize(old.width(), 2700);
            QElapsedTimer t2; t2.start(); while (t2.elapsed() < 600) { QApplication::processEvents(QEventLoop::AllEvents, 50); }
            shot("analysis_markets"); resize(old);
            QElapsedTimer t3; t3.start(); while (t3.elapsed() < 400) { QApplication::processEvents(QEventLoop::AllEvents, 50); }
        }
        if (i == 4) {
            auto* page = qobject_cast<QWidget*>(m_stack->widget(4));
            if (auto* le = page->findChild<QLineEdit*>()) { le->setText("Where is ONNX initialized?"); emit le->returnPressed(); }
        }
        shot(names[i]);
    }
}
