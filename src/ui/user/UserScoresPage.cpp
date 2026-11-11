#include "UserScoresPage.h"
#include "UserTheme.h"
#include "ui/Formatting.h"
#include "ui/design/Components.h"
#include "ui/art/PitchWidget.h"
#include <QHBoxLayout>
#include <QScrollBar>
#include <QVBoxLayout>
#include <map>

UserScoresPage::UserScoresPage(UserDataHub* hub, QWidget* parent) : QWidget(parent), m_hub(hub) {
    QVBoxLayout* v = nullptr;
    makeColumn(this, v);

    m_dates = new DateStrip; v->addWidget(m_dates);

    auto* bar = new QWidget; bar->setFixedHeight(46);
    auto* bl = new QHBoxLayout(bar); bl->setContentsMargins(0, 0, 14, 0); bl->setSpacing(10);
    m_filter = new TextTabs(false); m_filter->setTabs({"All", "Live", "Finished", "Upcoming", "Favourites"});
    m_search = new QLineEdit; m_search->setPlaceholderText("Search team"); m_search->setClearButtonEnabled(true); m_search->setFixedWidth(180);
    bl->addWidget(m_filter); bl->addStretch(1); bl->addWidget(m_search);
    v->addWidget(bar);
    auto* rule = new QFrame; rule->setObjectName("Rule"); v->addWidget(rule);

    m_notice = new QFrame; m_notice->setObjectName("Notice");
    auto* nl = new QHBoxLayout(m_notice); nl->setContentsMargins(16, 7, 12, 7); nl->setSpacing(10);
    m_noticeText = new QLabel; m_noticeText->setStyleSheet("color:#474F3C; font-size:12px;");
    m_noticeConnect = new QPushButton("Connect live data"); m_noticeConnect->setProperty("ubtn", "link"); m_noticeConnect->setCursor(Qt::PointingHandCursor);
    m_noticeDemo = new QPushButton("Preview live demo"); m_noticeDemo->setProperty("ubtn", "green"); m_noticeDemo->setProperty("small", true); m_noticeDemo->setCursor(Qt::PointingHandCursor);
    m_noticeDemo->setStyleSheet("padding:4px 12px; border-radius:8px; font-size:12px;");
    nl->addWidget(m_noticeText, 1); nl->addWidget(m_noticeConnect); nl->addWidget(m_noticeDemo);
    v->addWidget(m_notice);

    m_scroll = new QScrollArea; m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget; m_content = new QVBoxLayout(content); m_content->setContentsMargins(0, 0, 0, 0); m_content->setSpacing(0);
    m_heroBox = new QWidget; auto* hb = new QVBoxLayout(m_heroBox); hb->setContentsMargins(14, 12, 14, 10);
    m_hero = new PitchWidget; m_hero->setFixedHeight(250); m_hero->setClickable(true);
    hb->addWidget(m_hero);
    m_heroBox->hide();
    m_content->addWidget(m_heroBox);
    m_scroll->setWidget(content);
    v->addWidget(m_scroll, 1);
    connect(m_hero, &PitchWidget::clicked, this, [this]() { if (m_hasHero) emit openMatch(m_heroItem); });
    connect(m_noticeConnect, &QPushButton::clicked, this, [this]() { m_hub->controller()->requestSettings(); });
    connect(m_noticeDemo, &QPushButton::clicked, this, [this]() {
        AppSettings st = m_hub->controller()->settings(); st.demoLive = !st.demoLive;
        m_userNavigated = false; m_hub->controller()->applySettings(st);
        m_dates->setDate(m_hub->defaultDate(), true); m_userNavigated = false;
    });

    m_debounce.setSingleShot(true); m_debounce.setInterval(60);
    connect(&m_debounce, &QTimer::timeout, this, [this]() { rebuild(); });

    connect(m_dates, &DateStrip::dateChanged, this, [this]() { m_userNavigated = true; rebuild(); });
    connect(m_filter, &TextTabs::changed, this, [this]() { rebuild(); });
    connect(m_search, &QLineEdit::textChanged, this, [this]() { m_debounce.start(); });
    connect(hub, &UserDataHub::dataChanged, this, [this]() {
        if (!m_userNavigated) m_dates->setDate(m_hub->defaultDate());
        m_debounce.start();
    });
    connect(hub, &UserDataHub::favsChanged, this, [this]() { m_debounce.start(); });
    connect(hub, &UserDataHub::pickReady, this, [this]() { m_debounce.start(); });
    connect(hub->controller()->live(), &LiveDataService::statusChanged, this, [this]() { updateNotice(); });

    m_dates->setDate(hub->defaultDate());
    rebuild();
}

void UserScoresPage::showLatest() {
    const QDate d = m_hub->lastResultDate();
    if (d.isValid()) { m_userNavigated = true; m_dates->setDate(d, true); }
}

void UserScoresPage::updateNotice() {
    const auto fx = m_hub->controller()->live()->fixturesStatus();
    const bool demo = m_hub->demoOn();
    QString t;
    bool offer = false;
    if (demo) {
        t = "Demo mode: a simulated live match is running so you can preview the live screens.";
    } else if (m_hub->hasFixtures() && fx.state == SourceStatus::NotConfigured) {
        t = "Live updates are off. Showing the last saved scores" + (fx.lastEpoch ? " (" + Fmt::ago(fx.lastEpoch) + ")." : QString("."));
        offer = true;
    } else if (fx.state == SourceStatus::NotConfigured || (!m_hub->hasFixtures() && fx.state != SourceStatus::Loading && fx.state != SourceStatus::Fresh)) {
        const QDate last = m_hub->lastResultDate();
        t = last.isValid() ? QString("Showing recorded results up to %1.").arg(QLocale::c().toString(last, "d MMM yyyy")) : QString("Live scores are not connected.");
        if (m_hub->controller()->resultsAdded() > 0) t += " Latest results were downloaded at start-up.";
        else if (m_hub->controller()->resultsMessage().startsWith("Could") || m_hub->controller()->resultsMessage().startsWith("Timed")) t += " Newer results could not be downloaded.";
        offer = true;
    } else if (fx.state == SourceStatus::Error) {
        t = "Live scores are unavailable right now. Showing the last known data.";
    } else if (fx.state == SourceStatus::Stale) {
        t = "Scores may be out of date (last update " + Fmt::ago(fx.lastEpoch) + ").";
    }
    m_notice->setVisible(!t.isEmpty());
    m_noticeText->setText(t);
    m_noticeConnect->setVisible(offer);
    m_noticeDemo->setVisible(offer || demo);
    m_noticeDemo->setText(demo ? "Turn off demo" : "Preview live demo");
}

// Featured match for the hero card: a live match first, then the soonest upcoming one, else the highest-scoring result.
void UserScoresPage::updateHero(const std::vector<MatchItem>& all) {
    const MatchItem* best = nullptr; int bestScore = -1;
    for (const auto& m : all) {
        int score = 0;
        if (m.fx.isLive()) score = 1000 + (m.fx.demo ? 0 : 1);
        else if (m.fx.isUpcoming()) score = 500 - int(std::min<qint64>(400, std::max<qint64>(0, (m.fx.kickoffEpoch - QDateTime::currentSecsSinceEpoch()) / 3600)));
        else if (m.fx.isFinished()) score = 100 + std::max(0, m.fx.homeGoals) + std::max(0, m.fx.awayGoals);
        else continue;
        if (score > bestScore) { bestScore = score; best = &m; }
    }
    if (!best) { m_hasHero = false; m_heroBox->hide(); return; }
    m_heroItem = *best; m_hasHero = true;
    m_hero->setData(pitchDataFor(m_hub, m_heroItem));
    m_heroBox->show();
}

void UserScoresPage::rebuild() {
    updateNotice();
    const QDate day = m_dates->date();
    auto all = m_hub->matchesOn(day);
    const QString q = m_search->text().trimmed().toLower();
    int live = 0, fav = 0;
    for (const auto& m : all) { if (m.fx.isLive()) ++live; if (m_hub->isFav(m.key())) ++fav; }
    m_filter->setLabel(1, live ? QString("Live (%1)").arg(live) : QStringLiteral("Live"));
    m_filter->setLabel(4, fav ? QString("Favourites (%1)").arg(fav) : QStringLiteral("Favourites"));

    for (const auto& m : all) if (m.fx.isUpcoming()) m_hub->requestPick(m);
    updateHero(all);

    std::vector<MatchItem> shown;
    for (const auto& m : all) {
        switch (m_filter->current()) {
            case 1: if (!m.fx.isLive()) continue; break;
            case 2: if (!m.fx.isFinished()) continue; break;
            case 3: if (m.fx.isLive() || m.fx.isFinished()) continue; break;
            case 4: if (!m_hub->isFav(m.key())) continue; break;
            default: break;
        }
        if (!q.isEmpty() && !(m.home() + " " + m.away()).toLower().contains(q)) continue;
        shown.push_back(m);
    }

    auto* list = new QWidget;
    auto* lv = new QVBoxLayout(list); lv->setContentsMargins(0, 0, 0, 0); lv->setSpacing(0);
    if (shown.empty()) {
        const bool ready = m_hub->controller()->ready();
        const QDate last = m_hub->lastResultDate();
        const bool canJump = ready && all.empty() && last.isValid() && last != day;
        QString title, body;
        Icons::Name ic = Icons::Calendar;
        if (!ready) { title = m_hub->controller()->initError().isEmpty() ? "Loading data" : "Data unavailable"; body = m_hub->controller()->initError().isEmpty() ? "Hang on, the models and match history are loading." : m_hub->controller()->initError(); ic = Icons::Clock; }
        else if (all.empty()) { title = "No matches on this day"; body = canJump ? "Pick another date from the strip above, or jump to the latest results." : "Pick another date from the strip above."; }
        else if (m_filter->current() == 4) { title = "No favourites here yet"; body = "Tap the star on any match to follow it and find it here."; ic = Icons::Picks; }
        else { title = "Nothing matches this filter"; body = "Try another filter or clear the search box."; ic = Icons::Search; }
        auto* es = new EmptyState(ic, title, body, canJump ? "Go to latest results" : QString());
        connect(es, &EmptyState::actionClicked, this, [this]() { showLatest(); });
        lv->addWidget(es);
    } else {
        std::map<QString, std::vector<const MatchItem*>> groups;
        std::vector<QString> order;
        for (const auto& m : shown) {
            auto it = groups.find(m.competition());
            if (it == groups.end()) order.push_back(m.competition());
            groups[m.competition()].push_back(&m);
        }
        for (const auto& comp : order) {
            const auto& rows = groups[comp];
            lv->addWidget(new SectionHeader(comp, QString::number(rows.size())));
            for (const MatchItem* m : rows) {
                m_hub->requestPick(*m);
                auto* row = new MatchRow;
                row->setItem(*m, m_hub->pick(*m), m_hub->isFav(m->key()));
                const MatchItem copy = *m;
                connect(row, &MatchRow::clicked, this, [this, copy]() { emit openMatch(copy); });
                connect(row, &MatchRow::favToggled, this, [this, copy]() { m_hub->toggleFav(copy.key()); });
                lv->addWidget(row);
            }
        }
    }
    lv->addStretch(1);
    if (m_list) { m_content->removeWidget(m_list); m_list->deleteLater(); }
    m_content->addWidget(list, 1);
    m_list = list;
}
