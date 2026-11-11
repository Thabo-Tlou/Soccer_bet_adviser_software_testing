#include "SettingsPage.h"
#include "Theme.h"
#include <QComboBox>
#include "design/Components.h"
#include <QDateTime>
#include <QTimeZone>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QStandardPaths>
#include <QUrl>
#include <QScrollArea>

static QLineEdit* secretField() { auto* e = new QLineEdit; e->setEchoMode(QLineEdit::Password); e->setPlaceholderText("not set"); return e; }

SettingsPage::SettingsPage(AppController* c, QWidget* parent) : QWidget(parent), m_c(c) {
    setObjectName("Page");
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto* page = new QWidget; page->setObjectName("Page"); scroll->setWidget(page);
    auto* v = new QVBoxLayout(page); v->setContentsMargins(28, 24, 28, 24); v->setSpacing(14);
    v->addWidget(makeLabel("Settings", "h1"));
    m_banner = new Banner; v->addWidget(m_banner);

    auto form = [](Card* card) { auto* f = new QFormLayout; f->setLabelAlignment(Qt::AlignLeft); f->setHorizontalSpacing(18); f->setVerticalSpacing(10); card->body()->addLayout(f); return f; };

    auto* api = new Card("Data providers");
    auto* f1 = form(api);
    m_fdKey = secretField(); m_oddsKey = secretField();
    m_comp = new QLineEdit; m_sport = new QLineEdit; m_regions = new QLineEdit;
    f1->addRow("football-data.org key", m_fdKey);
    f1->addRow("The Odds API key", m_oddsKey);
    f1->addRow("Competition code", m_comp);
    f1->addRow("Odds sport key", m_sport);
    f1->addRow("Odds regions", m_regions);
    auto* n = makeLabel("Environment variables FOOTBALL_DATA_API_KEY and ODDS_API_KEY take priority over keys saved here.", "dim");
    n->setWordWrap(true); api->body()->addWidget(n);
    auto* test = makeButton("Test football-data.org connection"); api->body()->addWidget(test, 0, Qt::AlignLeft);
    v->addWidget(api);

    auto* look = new Card("Look & feel");
    auto* f4 = form(look);
    m_demo = new QCheckBox("Show a simulated live match (demo)"); m_motion = new QCheckBox("Reduce motion");
    m_zone = new QComboBox; m_zone->setEditable(true); m_zone->setMaximumWidth(340);
    for (const char* z : {"Africa/Maseru", "Africa/Johannesburg", "Europe/London", "Europe/Paris", "Africa/Lagos", "Africa/Nairobi", "America/New_York", "Asia/Dubai", "Asia/Kolkata", "UTC"}) m_zone->addItem(z);
    m_zoneInfo = makeLabel("", "dim");
    f4->addRow("Time zone", m_zone); f4->addRow("", m_zoneInfo);
    f4->addRow("", m_demo); f4->addRow("", m_motion);
    connect(m_zone, &QComboBox::currentTextChanged, this, [this](const QString& id) {
        QTimeZone z(id.toUtf8());
        if (!z.isValid()) { m_zoneInfo->setText("Unknown time zone"); return; }
        const int a = z.offsetFromUtc(QDateTime::currentDateTimeUtc()) / 60;
        m_zoneInfo->setText(QString("Kickoff times show as UTC%1%2:%3").arg(a < 0 ? "-" : "+").arg(std::abs(a) / 60).arg(std::abs(a) % 60, 2, 10, QChar('0')));
    });
    const QString logoDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logos";
    auto* ln = makeLabel("Real club logos download automatically once football-data.org is connected. To use your own, put PNG or SVG files named after the club (for example Man City.png) in the logos folder.", "dim");
    ln->setWordWrap(true); look->body()->addWidget(ln);
    auto* openLogos = makeButton("Open logos folder"); look->body()->addWidget(openLogos, 0, Qt::AlignLeft);
    connect(openLogos, &QPushButton::clicked, this, [logoDir]() { QDir().mkpath(logoDir); QDesktopServices::openUrl(QUrl::fromLocalFile(logoDir)); });
    v->addWidget(look);


    auto* rf = new Card("Refresh");
    auto* f2 = form(rf);
    m_auto = new QCheckBox("Automatic refresh"); m_live = new QSpinBox; m_fix = new QSpinBox; m_odds = new QSpinBox;
    m_live->setRange(30, 600); m_live->setSuffix(" s"); m_fix->setRange(1, 120); m_fix->setSuffix(" min"); m_odds->setRange(5, 360); m_odds->setSuffix(" min");
    f2->addRow("", m_auto); f2->addRow("Live scores (only while a match is live)", m_live);
    f2->addRow("Fixtures", m_fix); f2->addRow("Odds (watch your monthly quota)", m_odds);
    v->addWidget(rf);

    auto* rk = new Card("Reasoning rules");
    auto* f3 = form(rk);
    m_rf = new QDoubleSpinBox; m_rf->setRange(0, 1); m_rf->setSingleStep(0.1); m_rf->setDecimals(2);
    m_ev = new QDoubleSpinBox; m_ev->setRange(0, 1); m_ev->setSingleStep(0.01); m_ev->setDecimals(3);
    m_edge = new QDoubleSpinBox; m_edge->setRange(0, 1); m_edge->setSingleStep(0.01); m_edge->setDecimals(3);
    m_dis = new QDoubleSpinBox; m_dis->setRange(0.01, 1); m_dis->setSingleStep(0.01); m_dis->setDecimals(3);
    m_shrink = new QDoubleSpinBox; m_shrink->setRange(0, 1); m_shrink->setSingleStep(0.1); m_shrink->setDecimals(2);
    m_age = new QSpinBox; m_age->setRange(1, 1440); m_age->setSuffix(" min");
    m_minMatches = new QSpinBox; m_minMatches->setRange(1, 50);
    f3->addRow("Random Forest weight in blend (0 = Poisson only)", m_rf);
    f3->addRow("Minimum expected value per unit", m_ev);
    f3->addRow("Minimum model edge over market (probability)", m_edge);
    f3->addRow("Maximum RF/Poisson disagreement", m_dis);
    f3->addRow("Shrink model toward market (1 = no shrink)", m_shrink);
    f3->addRow("Odds count as stale after", m_age);
    f3->addRow("Minimum matches of history per team", m_minMatches);
    v->addWidget(rk);

    auto* stk = new Card("Bankroll and staking");
    auto* f6 = form(stk);
    m_bank = new QDoubleSpinBox; m_bank->setRange(0, 1e9); m_bank->setDecimals(2); m_bank->setSingleStep(50); m_bank->setSpecialValueText("not set"); m_bank->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_cur = new QLineEdit; m_cur->setMaxLength(4); m_cur->setMaximumWidth(90); m_cur->setPlaceholderText("M");
    m_riskProf = new QComboBox; m_riskProf->addItems({"Careful (10% of Kelly)", "Balanced (25% of Kelly)", "Bolder (50% of Kelly)"});
    m_maxPct = new QDoubleSpinBox; m_maxPct->setRange(0.5, 25); m_maxPct->setDecimals(1); m_maxPct->setSuffix(" % of bankroll"); m_maxPct->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_minStake = new QDoubleSpinBox; m_minStake->setRange(0, 1e6); m_minStake->setDecimals(2); m_minStake->setSpecialValueText("no minimum"); m_minStake->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_round = new QDoubleSpinBox; m_round->setRange(0, 1000); m_round->setDecimals(2); m_round->setSpecialValueText("no rounding"); m_round->setButtonSymbols(QAbstractSpinBox::NoButtons);
    f6->addRow("Bankroll", m_bank); f6->addRow("Currency symbol", m_cur); f6->addRow("Risk profile", m_riskProf);
    f6->addRow("Maximum stake per bet", m_maxPct); f6->addRow("Minimum stake", m_minStake); f6->addRow("Round stakes down to", m_round);
    auto* sn = makeLabel("Stake advice uses a fraction of the Kelly criterion on a cautious version of the model's probability, never more than your maximum per bet. Only bet money you can afford to lose.", "dim");
    sn->setWordWrap(true); stk->body()->addWidget(sn);
    v->addWidget(stk);

    auto* data = new Card("Match data");
    auto* f5 = form(data);
    m_autoResults = new QCheckBox("Download the latest results when the app starts");
    f5->addRow("", m_autoResults);
    m_resultsInfo = makeLabel("", "dim"); m_resultsInfo->setWordWrap(true);
    data->body()->addWidget(m_resultsInfo);
    auto* updateNow = makeButton("Update results now (restarts the app)"); data->body()->addWidget(updateNow, 0, Qt::AlignLeft);
    connect(updateNow, &QPushButton::clicked, this, [this, updateNow]() { updateNow->setEnabled(false); updateNow->setText("Downloading..."); m_c->updateResultsAndRestart(); });
    connect(m_c, &AppController::resultsUpdateFailed, this, [this, updateNow](const QString& msg) { updateNow->setEnabled(true); updateNow->setText("Update results now (restarts the app)"); m_resultsInfo->setText(msg); });
    v->addWidget(data);

    auto* env = new Card("Environment");
    m_info = makeLabel("", "mono"); m_info->setWordWrap(true); m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    env->body()->addWidget(m_info);
    v->addWidget(env);

    for (QWidget* w : std::initializer_list<QWidget*>{m_live, m_fix, m_odds, m_rf, m_ev, m_edge, m_dis, m_shrink, m_age, m_minMatches, m_comp, m_sport, m_regions}) w->setMaximumWidth(260);
    for (QWidget* w : std::initializer_list<QWidget*>{m_fdKey, m_oddsKey}) w->setMaximumWidth(520);
    auto* save = makeButton("Save settings", true);
    v->addWidget(save, 0, Qt::AlignLeft);
    v->addStretch();

    connect(save, &QPushButton::clicked, this, [this]() { m_c->applySettings(collect()); Toast::show(this, "Settings saved.", Toast::Success); load(); });
    connect(test, &QPushButton::clicked, this, [this, test]() {
        m_c->applySettings(collect()); test->setEnabled(false);
        m_c->live()->testFootballConnection([this, test](bool ok, const QString& msg) {
            test->setEnabled(true); m_banner->show(ok ? Banner::Success : Banner::Error, "football-data.org: " + msg);
        });
    });
    load();
}

void SettingsPage::load() {
    const auto& s = m_c->settings();
    m_fdKey->setText(s.footballDataKey); m_oddsKey->setText(s.oddsApiKey);
    m_comp->setText(s.competitionCode); m_sport->setText(s.oddsSportKey); m_regions->setText(s.oddsRegions);
    m_auto->setChecked(s.autoRefresh); m_live->setValue(s.liveRefreshSec); m_fix->setValue(s.fixturesRefreshMin); m_odds->setValue(s.oddsRefreshMin);
    m_demo->setChecked(s.demoLive); m_motion->setChecked(s.reduceMotion);
    m_bank->setValue(s.bankroll); m_cur->setText(s.currency); m_riskProf->setCurrentIndex(s.riskProfile);
    m_maxPct->setValue(s.maxStakePct); m_minStake->setValue(s.minStake); m_round->setValue(s.stakeRound);
    m_zone->setCurrentText(s.timeZone); emit m_zone->currentTextChanged(m_zone->currentText()); m_autoResults->setChecked(s.autoUpdateResults);
    {
        const auto& st = m_c->service().status();
        QString t = m_c->ready() ? QString("History runs to %1 (%2 matches).").arg(QString::fromStdString(st.lastDate)).arg(st.matches) : QString("Loading...");
        if (st.updatedRows > 0) t += QString(" %1 newer matches were added from the latest download.").arg(st.updatedRows);
        if (!m_c->resultsMessage().isEmpty()) t += " " + m_c->resultsMessage();
        m_resultsInfo->setText(t);
    }
    m_rf->setValue(s.rfWeight); m_ev->setValue(s.risk.minExpectedValue); m_edge->setValue(s.risk.minEdge); m_dis->setValue(s.risk.maxDisagreement);
    m_shrink->setValue(s.risk.shrinkTowardMarket); m_age->setValue(s.risk.maxOddsAgeMinutes); m_minMatches->setValue(s.risk.minMatchesPlayed);
    m_info->setText(QString("football-data key: %1%2\nOdds API key:      %3%4\nResources:         %5\nHistory database:  %6\nKnowledge index:   %7")
                        .arg(AppSettings::mask(s.effectiveFootballKey()), qEnvironmentVariableIsSet("FOOTBALL_DATA_API_KEY") ? " (from environment)" : "",
                             AppSettings::mask(s.effectiveOddsKey()), qEnvironmentVariableIsSet("ODDS_API_KEY") ? " (from environment)" : "",
                             m_c->resourcesRoot().isEmpty() ? "not found" : m_c->resourcesRoot(), m_c->dbPath(), m_c->indexRoot()));
}

AppSettings SettingsPage::collect() const {
    AppSettings s = m_c->settings();
    s.footballDataKey = m_fdKey->text().trimmed(); s.oddsApiKey = m_oddsKey->text().trimmed();
    s.competitionCode = m_comp->text().trimmed().toUpper(); s.oddsSportKey = m_sport->text().trimmed(); s.oddsRegions = m_regions->text().trimmed();
    s.demoLive = m_demo->isChecked(); s.reduceMotion = m_motion->isChecked();
    s.bankroll = m_bank->value(); s.currency = m_cur->text().trimmed().isEmpty() ? QStringLiteral("M") : m_cur->text().trimmed(); s.riskProfile = m_riskProf->currentIndex();
    s.maxStakePct = m_maxPct->value(); s.minStake = m_minStake->value(); s.stakeRound = m_round->value();
    s.timeZone = QTimeZone(m_zone->currentText().toUtf8()).isValid() ? m_zone->currentText() : QStringLiteral("Africa/Maseru");
    s.autoUpdateResults = m_autoResults->isChecked();
    s.autoRefresh = m_auto->isChecked(); s.liveRefreshSec = m_live->value(); s.fixturesRefreshMin = m_fix->value(); s.oddsRefreshMin = m_odds->value();
    s.rfWeight = m_rf->value(); s.risk.minExpectedValue = m_ev->value(); s.risk.minEdge = m_edge->value(); s.risk.maxDisagreement = m_dis->value();
    s.risk.shrinkTowardMarket = m_shrink->value(); s.risk.maxOddsAgeMinutes = m_age->value(); s.risk.minMatchesPlayed = m_minMatches->value();
    return s;
}
