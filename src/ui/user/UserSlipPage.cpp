#include "UserSlipPage.h"

#include "UserTheme.h"
#include "ui/MarketsPanel.h"
#include "ui/design/Components.h"
#include <QHBoxLayout>
#include <QLocale>
#include <QScrollArea>
#include <cmath>

namespace {
QColor verdictColor(sba::Verdict v) {
    switch (v) { case sba::Verdict::Value: return DS::success700; case sba::Verdict::Marginal: return DS::warning700; case sba::Verdict::NoValue: return DS::slate700; default: return DS::textMuted; }
}
}

UserSlipPage::UserSlipPage(UserDataHub* hub, QWidget* parent) : QWidget(parent), m_hub(hub), m_c(hub->controller()) {
    QVBoxLayout* inner = nullptr;
    makeColumn(this, inner);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    inner->addWidget(scroll);
    auto* content = new QWidget; scroll->setWidget(content);
    auto* root = new QHBoxLayout(content); root->setContentsMargins(28, 22, 28, 28); root->setSpacing(24);

    // ---------------------------------------------------------------- left: the selections
    auto* left = new QVBoxLayout; left->setSpacing(12);
    auto* head = new QHBoxLayout;
    m_title = new QLabel("Bet slip"); m_title->setStyleSheet(QString("font-size:24px; font-weight:800; color:%1;").arg(UT::hex(UT::ink)));
    m_count = new QLabel; m_count->setStyleSheet(QString("font-size:12px; font-weight:700; color:%1; background:%2; border-radius:11px; padding:3px 10px;").arg(UT::hex(DS::leaf800), UT::hex(DS::leaf100)));
    m_clear = new QPushButton("Clear all"); m_clear->setProperty("ubtn", "link"); m_clear->setCursor(Qt::PointingHandCursor);
    head->addWidget(m_title); head->addWidget(m_count); head->addStretch(1); head->addWidget(m_clear);
    left->addLayout(head);
    left->addWidget(new StakeBar(m_c));
    m_legsHost = new QWidget; m_legsLayout = new QVBoxLayout(m_legsHost); m_legsLayout->setContentsMargins(0, 0, 0, 0); m_legsLayout->setSpacing(10);
    left->addWidget(m_legsHost);
    left->addStretch(1);
    root->addLayout(left, 3);

    // ---------------------------------------------------------------- right: the summary
    m_summaryCard = new QFrame; m_summaryCard->setObjectName("HCard"); m_summaryCard->setMinimumWidth(380); m_summaryCard->setMaximumWidth(460);
    m_summaryCard->setStyleSheet(DS::applyTokens("QFrame#HCard { background: @surface; border: 1px solid @border; border-radius: 16px; } QLabel { background: transparent; }"));
    applyElevation(m_summaryCard, 18, 2, 18);
    auto* sv = new QVBoxLayout(m_summaryCard); sv->setContentsMargins(22, 20, 22, 22); sv->setSpacing(10);
    auto* st = new QLabel("Your stake"); st->setStyleSheet(QString("font-size:16px; font-weight:700; color:%1;").arg(UT::hex(UT::ink)));
    sv->addWidget(st);
    auto* sr = new QHBoxLayout;
    m_stake = new QDoubleSpinBox; m_stake->setRange(0, 1e9); m_stake->setDecimals(2); m_stake->setSingleStep(5); m_stake->setPrefix(m_c->settings().currency + " "); m_stake->setKeyboardTracking(false); m_stake->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_stake->setMinimumHeight(40); m_stake->setStyleSheet("font-size:16px; font-weight:700;");
    m_use = new QPushButton("Use suggested"); m_use->setProperty("ubtn", "small"); m_use->setCursor(Qt::PointingHandCursor);
    sr->addWidget(m_stake, 1); sr->addWidget(m_use);
    sv->addLayout(sr);
    m_suggest = new QLabel; m_suggest->setWordWrap(true); m_suggest->setStyleSheet(QString("font-size:12px; color:%1;").arg(UT::hex(UT::soft)));
    sv->addWidget(m_suggest);
    m_singles = new QCheckBox("Treat as separate single bets (stake on each)"); m_singles->setCursor(Qt::PointingHandCursor);
    sv->addWidget(m_singles);
    auto* rule = new QFrame; rule->setObjectName("Rule"); sv->addWidget(rule);
    m_rows = new QLabel; m_rows->setTextFormat(Qt::RichText); m_rows->setStyleSheet("font-size:13px;");
    sv->addWidget(m_rows);
    m_headline = new QLabel; m_headline->setWordWrap(true); m_headline->setStyleSheet("font-size:14px; font-weight:700;");
    sv->addWidget(m_headline);
    m_warn = new QLabel; m_warn->setWordWrap(true); m_warn->setTextFormat(Qt::RichText); m_warn->setStyleSheet(QString("font-size:12px; color:%1;").arg(UT::hex(UT::soft)));
    sv->addWidget(m_warn);
    auto* rg = new QLabel("Estimates only, not financial advice. 18+. Never bet money you cannot afford to lose."); rg->setWordWrap(true);
    rg->setStyleSheet(QString("font-size:11px; color:%1;").arg(UT::hex(UT::muted)));
    sv->addSpacing(4); sv->addWidget(rg);
    sv->addStretch(1);
    root->addWidget(m_summaryCard, 2, Qt::AlignTop);

    connect(m_clear, &QPushButton::clicked, this, [this]() { m_c->clearSlip(); });
    connect(m_stake, &QDoubleSpinBox::editingFinished, this, [this]() { m_c->setSlipStake(m_stake->value()); });
    connect(m_singles, &QCheckBox::toggled, this, [this](bool on) { m_c->setSlipAsSingles(on); });
    connect(m_c, &AppController::slipChanged, this, [this]() { refresh(); });
    connect(m_c, &AppController::stakingChanged, this, [this]() { refreshSummary(); });
    connect(m_c, &AppController::settingsChanged, this, [this]() { refresh(); });
    refresh();
}

QString UserSlipPage::money(double v) const {
    const bool whole = std::fabs(v - std::round(v)) < 0.005;
    return m_c->settings().currency + " " + QLocale(QLocale::English).toString(v, 'f', whole ? 0 : 2);
}

double UserSlipPage::shrink() const { return std::min(m_c->settings().risk.shrinkTowardMarket, m_c->service().goalsShrink()); }

void UserSlipPage::refresh() {
    const auto& legs = m_c->slipLegs();
    QString sig;
    for (const auto& l : legs) sig += QString::fromStdString(l.matchKey + "|" + l.selectionKey) + QString::number(l.odds, 'f', 2) + ";";
    if (sig != m_sig) { m_sig = sig; rebuildLegs(); }
    m_stake->setPrefix(m_c->settings().currency + " ");
    { QSignalBlocker b(m_stake); m_stake->setValue(m_c->slipStake()); }
    { QSignalBlocker b(m_singles); m_singles->setChecked(m_c->slipAsSingles()); }
    refreshSummary();
    emit countChanged(static_cast<int>(legs.size()));
}

void UserSlipPage::rebuildLegs() {
    while (QLayoutItem* it = m_legsLayout->takeAt(0)) { if (it->widget()) { it->widget()->hide(); it->widget()->deleteLater(); } delete it; }
    const auto& legs = m_c->slipLegs();
    m_count->setText(QString("%1 selection%2").arg(legs.size()).arg(legs.size() == 1 ? "" : "s"));
    m_count->setVisible(!legs.empty()); m_clear->setVisible(!legs.empty());
    if (legs.empty()) {
        auto* es = new EmptyState(Icons::Picks, "Your bet slip is empty", "Open a match, go to its Markets tab, type the bookmaker's odds for the bet you like, then press \"Add to bet slip\".", "Browse matches");
        connect(es, &EmptyState::actionClicked, this, &UserSlipPage::browseRequested);
        auto* card = new QFrame; card->setObjectName("HCard"); card->setStyleSheet(DS::applyTokens("QFrame#HCard { background: @surface; border: 1px solid @border; border-radius: 16px; }"));
        auto* cl = new QVBoxLayout(card); cl->addWidget(es);
        m_legsLayout->addWidget(card);
        return;
    }
    for (int i = 0; i < static_cast<int>(legs.size()); ++i) {
        const auto& l = legs[i];
        auto* card = new QFrame; card->setObjectName("HCard");
        card->setStyleSheet(DS::applyTokens("QFrame#HCard { background: @surface; border: 1px solid @border; border-radius: 14px; } QLabel { background: transparent; } "
                                            "QDoubleSpinBox { background: @surface; border: 1px solid @borderStrong; border-radius: 8px; padding: 6px 9px; font-weight: 700; } QDoubleSpinBox:focus { border: 1px solid @primary; }"));
        auto* h = new QHBoxLayout(card); h->setContentsMargins(18, 14, 14, 14); h->setSpacing(14);
        auto* col = new QVBoxLayout; col->setSpacing(2);
        auto* m = new QLabel(QString::fromStdString(l.matchLabel)); m->setStyleSheet(QString("font-size:12px; color:%1;").arg(UT::hex(UT::muted)));
        auto* s = new QLabel(QString::fromStdString(l.selectionLabel)); s->setStyleSheet(QString("font-size:15px; font-weight:700; color:%1;").arg(UT::hex(UT::ink)));
        auto* p = new QLabel(QString("Model chance %1%").arg(l.pWin * 100, 0, 'f', 0)); p->setStyleSheet(QString("font-size:12px; color:%1;").arg(UT::hex(UT::soft)));
        col->addWidget(m); col->addWidget(s); col->addWidget(p);
        h->addLayout(col, 1);
        // single-bet verdict chip
        const auto single = sba::evaluateSlip({l}, 1.0, m_c->stakeSettings(), m_c->settings().risk, shrink());
        auto* chip = new QLabel(single.valid ? QString(sba::verdictName(single.verdict)) : QString("Check odds"));
        const QColor vc = verdictColor(single.valid ? single.verdict : sba::Verdict::Invalid);
        chip->setStyleSheet(QString("font-size:12px; font-weight:700; color:%1; background:%2; border-radius:11px; padding:3px 10px;").arg(UT::hex(vc), UT::hex(single.verdict == sba::Verdict::Value ? DS::success50 : (single.verdict == sba::Verdict::Marginal ? DS::warning50 : DS::slate100))));
        chip->setFixedHeight(26); chip->setAlignment(Qt::AlignCenter);
        h->addWidget(chip, 0, Qt::AlignVCenter);
        auto* odds = new QDoubleSpinBox; odds->setRange(1.01, 1000); odds->setDecimals(2); odds->setSingleStep(0.05); odds->setValue(std::isfinite(l.odds) ? l.odds : 2.0); odds->setFixedWidth(92); odds->setKeyboardTracking(false); odds->setButtonSymbols(QAbstractSpinBox::NoButtons); odds->setAlignment(Qt::AlignCenter);
        odds->setToolTip("Decimal odds. Press Enter to update.");
        connect(odds, &QDoubleSpinBox::editingFinished, this, [this, i, odds]() { m_c->setSlipOdds(i, odds->value()); });
        h->addWidget(odds);
        auto* rm = new QPushButton; rm->setIcon(Icons::icon(Icons::Alert, DS::slate500, 16)); rm->setText("Remove"); rm->setProperty("ubtn", "link"); rm->setCursor(Qt::PointingHandCursor);
        rm->setStyleSheet(QString("color:%1;").arg(UT::hex(DS::slate500)));
        connect(rm, &QPushButton::clicked, this, [this, i]() { m_c->removeSlipLeg(i); });
        h->addWidget(rm);
        m_legsLayout->addWidget(card);
    }
}

void UserSlipPage::refreshSummary() {
    const auto& legs = m_c->slipLegs();
    const bool many = legs.size() > 1;
    m_singles->setVisible(many);
    const bool singles = many && m_c->slipAsSingles();
    m_use->hide(); m_warn->clear(); m_suggest->clear();
    if (legs.empty()) { m_rows->setText("Add selections to see your return and advice."); m_headline->clear(); return; }

    const auto cfg = m_c->stakeSettings(); const auto& risk = m_c->settings().risk;
    const double stake = m_c->slipStake();
    auto row = [](const QString& k, const QString& v, bool strong = false) {
        return QString("<tr><td style='color:%1; padding:3px 0;'>%2</td><td align='right' style='%3'>%4</td></tr>").arg(DS::hex(DS::textMuted), k, strong ? "font-weight:700; font-size:15px;" : "font-weight:600;", v);
    };
    QString html = "<table width='100%'>";
    sba::Verdict verdict = sba::Verdict::NoOdds; QString headline; QStringList warns; double suggested = 0;

    if (!singles) {
        const auto r = sba::evaluateSlip(legs, stake, cfg, risk, shrink());
        if (!r.valid) { m_rows->setText(""); m_headline->setStyleSheet(QString("font-size:14px; font-weight:700; color:%1;").arg(DS::hex(DS::danger700))); m_headline->setText(QString::fromStdString(r.problem)); return; }
        html += row(many ? QString("Accumulator of %1").arg(r.legs) : "Single bet", "");
        html += row("Total odds", QString::number(r.combinedOdds, 'f', 2), true);
        html += row(many ? "Chance all win" : "Model chance", QString("%1%").arg(r.combinedProb * 100, 0, 'f', many ? 1 : 0));
        html += row("Stake", money(r.stake));
        html += row("Potential return", money(r.returnIfWin), true);
        html += row("Potential profit", money(r.profitIfWin));
        html += row("Expected profit (model)", (r.evMoney >= 0 ? "+" : "") + money(r.evMoney));
        verdict = r.verdict; suggested = r.suggestedStake;
        for (const auto& w : r.warnings) warns << QString::fromStdString(w);
        headline = verdict == sba::Verdict::Value ? QString("Value: the model expects a profit of %1% per unit staked.").arg(r.evPerUnit * 100, 0, 'f', 1)
                 : (verdict == sba::Verdict::Marginal ? QString("Marginal: a small edge, within model error (%1%).").arg(r.evPerUnit * 100, 0, 'f', 1)
                 : QString("No value: you would lose about %1% per unit staked on average.").arg(-r.evPerUnit * 100, 0, 'f', 1));
    } else {
        double totalStake = 0, ret = 0, ev = 0; int value = 0;
        for (const auto& l : legs) {
            const auto r = sba::evaluateSlip({l}, stake, cfg, risk, shrink());
            if (!r.valid) { m_rows->setText(""); m_headline->setText(QString::fromStdString(r.problem)); return; }
            totalStake += r.stake; ret += r.returnIfWin; ev += r.evMoney; value += r.verdict == sba::Verdict::Value; suggested += r.suggestedStake / std::max<size_t>(1, 1);
            for (const auto& w : r.warnings) if (!warns.contains(QString::fromStdString(w))) warns << QString::fromStdString(w);
        }
        html += row(QString("%1 separate single bets").arg(legs.size()), "");
        html += row("Stake on each", money(stake));
        html += row("Total stake", money(totalStake), true);
        html += row("Return if every bet wins", money(ret));
        html += row("Profit if every bet wins", money(ret - totalStake));
        html += row("Expected profit (model)", (ev >= 0 ? "+" : "") + money(ev));
        verdict = value == static_cast<int>(legs.size()) ? sba::Verdict::Value : (value > 0 ? sba::Verdict::Marginal : sba::Verdict::NoValue);
        headline = QString("%1 of %2 selections have value at these odds.").arg(value).arg(legs.size());
        suggested = suggested / static_cast<double>(legs.size());
        if (cfg.bankroll > 0 && totalStake > cfg.maxStakeFraction * cfg.bankroll * legs.size() + 1e-9) warns << "Your stake on each bet is above your limit per bet.";
    }
    html += "</table>";
    m_rows->setText(html);
    m_headline->setStyleSheet(QString("font-size:14px; font-weight:700; color:%1;").arg(DS::hex(verdictColor(verdict))));
    m_headline->setText(headline);
    if (cfg.bankroll > 0 && suggested > 0) {
        m_suggest->setText(QString("Suggested stake%1: <b>%2</b> (based on your bankroll and risk setting).").arg(singles ? " on each bet" : "").arg(money(suggested)));
        m_suggest->setTextFormat(Qt::RichText);
        m_use->show();
        disconnect(m_use, nullptr, this, nullptr);
        connect(m_use, &QPushButton::clicked, this, [this, suggested]() { m_c->setSlipStake(suggested); });
    } else if (cfg.bankroll <= 0) {
        m_suggest->setText("Set your bankroll to get a suggested stake.");
    } else {
        m_suggest->setText(verdict == sba::Verdict::Value ? "The suggested stake is below your minimum stake." : "No stake advised: this does not look like value.");
    }
    QString w; for (const QString& x : warns) w += "&bull;&nbsp; " + x.toHtmlEscaped() + "<br>";
    m_warn->setTextFormat(Qt::RichText); m_warn->setText(w);
}
