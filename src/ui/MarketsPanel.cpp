#include "MarketsPanel.h"

#include "design/Components.h"
#include "design/Tokens.h"
#include <QApplication>
#include <QComboBox>
#include <QStyle>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <cmath>

namespace {

enum Col { cLabel = 0, cChance, cFair, cFrom, cOdds, cVerdict, cStake, cCount };
constexpr int kRoleBlurb = Qt::UserRole + 1, kRoleHeader = Qt::UserRole + 2, kRoleProb = Qt::UserRole + 3;

const char* kChipNames[] = {"All", "Result", "Goals", "Both teams to score", "Double chance", "Handicap", "Correct score", "Specials"};
const std::vector<std::vector<std::string>> kChipGroups = {
    {}, {"result", "result_total"}, {"goals", "team_goals", "exact_goals"}, {"btts"}, {"double_chance", "dnb"}, {"handicap"}, {"correct_score"}, {"nil", "odd_even"}};

std::string oppositeKey(const std::string& k) {
    auto swapEnd = [&](const std::string& a, const std::string& b) -> std::string {
        if (k.size() > a.size() && k.compare(k.size() - a.size(), a.size(), a) == 0) return k.substr(0, k.size() - a.size()) + b;
        if (k.size() > b.size() && k.compare(k.size() - b.size(), b.size(), b) == 0) return k.substr(0, k.size() - b.size()) + a;
        return {};
    };
    if (k.rfind("ou:", 0) == 0 || k.rfind("tt:", 0) == 0) return swapEnd(":o", ":u");
    if (k.rfind("btts:", 0) == 0 || k.rfind("cs:h:", 0) == 0 || k.rfind("cs:a:", 0) == 0) return swapEnd(":y", ":n");
    if (k == "oe:odd") return "oe:even";
    if (k == "oe:even") return "oe:odd";
    if (k == "dnb:1") return "dnb:2";
    if (k == "dnb:2") return "dnb:1";
    if (k.rfind("ah:h:", 0) == 0 || k.rfind("ah:a:", 0) == 0) {                 // handicap pair: home +x <-> away -x
        const bool home = k[3] == 'h';
        std::string line = k.substr(5);
        if (line.empty()) return {};
        if (line[0] == '+') line[0] = '-'; else if (line[0] == '-') line[0] = '+';
        return std::string("ah:") + (home ? "a:" : "h:") + line;
    }
    return {};
}

// Probability bar with the percentage next to it.
class ChanceDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        QStyleOptionViewItem o = opt; initStyleOption(&o, idx); o.text.clear();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, p, opt.widget);
        const double v = idx.data(kRoleProb).toDouble();
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(opt.rect).adjusted(8, 0, -8, 0);
        const QRectF track(r.left(), r.center().y() - 4, r.width() - 52, 8);
        p->setPen(Qt::NoPen); p->setBrush(DS::slate100); p->drawRoundedRect(track, 4, 4);
        p->setBrush(v >= 0.5 ? DS::leaf600 : DS::leaf400); p->drawRoundedRect(QRectF(track.left(), track.top(), std::max(4.0, track.width() * std::clamp(v, 0.0, 1.0)), track.height()), 4, 4);
        QFont f = opt.font; f.setBold(true); p->setFont(f); p->setPen(DS::text);
        p->drawText(QRectF(track.right() + 6, r.top(), 46, r.height()), Qt::AlignVCenter | Qt::AlignRight, QString::number(v * 100.0, 'f', v < 0.1 ? 1 : 0) + "%");
        p->restore();
    }
};

// Group header rows span the whole table: bold title plus a muted one-line explanation.
class GroupDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        if (!idx.data(kRoleHeader).toBool()) { QStyledItemDelegate::paint(p, opt, idx); return; }
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        p->fillRect(opt.rect, DS::slate50);
        p->setPen(DS::border); p->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());
        QFont f = opt.font; f.setBold(true); f.setPixelSize(14); p->setFont(f); p->setPen(DS::text);
        const QString title = idx.data(Qt::DisplayRole).toString();
        p->drawText(opt.rect.adjusted(12, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, title);
        const int w = QFontMetrics(f).horizontalAdvance(title);
        QFont g = opt.font; g.setPixelSize(12); p->setFont(g); p->setPen(DS::textFaint);
        p->drawText(opt.rect.adjusted(12 + w + 14, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(g).elidedText(idx.data(kRoleBlurb).toString(), Qt::ElideRight, opt.rect.width() - w - 40));
        p->restore();
    }
};

class OddsDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* sb = new QDoubleSpinBox(parent);
        sb->setRange(0.0, 1000.0); sb->setDecimals(2); sb->setSingleStep(0.05); sb->setSpecialValueText("none"); sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sb->setAlignment(Qt::AlignCenter);
        return sb;
    }
    void setEditorData(QWidget* e, const QModelIndex& idx) const override { static_cast<QDoubleSpinBox*>(e)->setValue(idx.data(Qt::EditRole).toDouble()); }
    void setModelData(QWidget* e, QAbstractItemModel* m, const QModelIndex& idx) const override { m->setData(idx, static_cast<QDoubleSpinBox*>(e)->value(), Qt::EditRole); }
    void updateEditorGeometry(QWidget* e, const QStyleOptionViewItem& o, const QModelIndex&) const override { e->setGeometry(o.rect.adjusted(4, 4, -4, -4)); }
};

QTableWidgetItem* cell(const QString& t, Qt::Alignment a = Qt::AlignCenter) {
    auto* it = new QTableWidgetItem(t); it->setTextAlignment(a | Qt::AlignVCenter); it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); return it;
}

}  // namespace

StakeBar::StakeBar(AppController* c, QWidget* parent) : QFrame(parent), m_c(c) {
    setObjectName("StakeBar");
    setStyleSheet(DS::applyTokens("QFrame#StakeBar { background: @slate50; border: 1px solid @border; border-radius: 12px; } QLabel { background: transparent; } "
                                  "QDoubleSpinBox, QComboBox { background: @surface; border: 1px solid @borderStrong; border-radius: 8px; padding: 5px 9px; color: @text; } "
                                  "QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid @primary; }"));
    auto* sb = new QHBoxLayout(this); sb->setContentsMargins(14, 10, 14, 10); sb->setSpacing(10);
    sb->addWidget(new QLabel("<b>Your bankroll</b>"));
    m_bankroll = new QDoubleSpinBox; m_bankroll->setRange(0, 1e9); m_bankroll->setDecimals(2); m_bankroll->setSingleStep(50); m_bankroll->setSpecialValueText("not set");
    m_bankroll->setKeyboardTracking(false); m_bankroll->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_bankroll->setToolTip("Your betting money. Stake advice is a share of this, so it can never put more than the limit at risk.");
    sb->addWidget(m_bankroll);
    m_risk = new QComboBox; m_risk->addItems({"Careful (10% of Kelly)", "Balanced (25% of Kelly)", "Bolder (50% of Kelly)"});
    m_risk->setToolTip("How aggressively to size stakes. Full Kelly is mathematically optimal only if the model is perfect, so smaller is safer.");
    sb->addWidget(m_risk);
    m_limits = new QLabel; m_limits->setWordWrap(true); m_limits->setMinimumWidth(0); m_limits->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_limits->setStyleSheet(QString("color:%1; font-size:12px;").arg(DS::hex(DS::textMuted)));
    sb->addWidget(m_limits, 1);
    m_bankroll->setMinimumWidth(110); m_risk->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); m_risk->setMinimumContentsLength(14);
    connect(m_bankroll, &QDoubleSpinBox::editingFinished, this, [this]() { m_c->setStaking(m_bankroll->value(), m_risk->currentIndex()); });
    connect(m_risk, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { m_c->setStaking(m_bankroll->value(), m_risk->currentIndex()); });
    connect(m_c, &AppController::stakingChanged, this, [this]() { sync(); });
    connect(m_c, &AppController::settingsChanged, this, [this]() { sync(); });
    sync();
}

void StakeBar::sync() {
    const auto& st = m_c->settings();
    QSignalBlocker b1(m_bankroll), b2(m_risk);
    m_bankroll->setPrefix(st.currency + " "); m_bankroll->setValue(st.bankroll); m_risk->setCurrentIndex(st.riskProfile);
    m_limits->setText(QString("At most %1% of it on any one bet.").arg(st.maxStakePct, 0, 'g', 3));
}

MarketsPanel::MarketsPanel(AppController* c, Mode mode, QWidget* parent) : QWidget(parent), m_c(c), m_mode(mode) {
    setStyleSheet(DS::applyTokens(R"(
QTableWidget { background: @surface; border: 1px solid @border; border-radius: 12px; gridline-color: transparent; selection-background-color: @leaf50; selection-color: @text; outline: 0; }
QTableWidget::item { border-bottom: 1px solid @slate100; padding: 2px 8px; }
QHeaderView { background: transparent; }
QHeaderView::section { background: @slate50; color: @textFaint; border: 0; border-bottom: 1px solid @border; padding: 8px 8px; font-size: 11px; font-weight: 700; }
QPushButton[chip="true"] { background: @surface; border: 1px solid @borderStrong; border-radius: 15px; padding: 6px 14px; color: @slate700; font-weight: 600; }
QPushButton[chip="true"]:hover { border-color: @primary; color: @primary; }
QPushButton[chip="true"][active="true"] { background: @primary; border-color: @primary; color: #ffffff; }
QFrame#AdviceBox { background: @surface; border: 1px solid @border; border-radius: 14px; }
QFrame#TrustBox { background: @leaf50; border: 1px solid @leaf100; border-radius: 10px; }
QFrame#StakeBar { background: @slate50; border: 1px solid @border; border-radius: 12px; }
QDoubleSpinBox, QComboBox { background: @surface; border: 1px solid @borderStrong; border-radius: 8px; padding: 5px 9px; color: @text; }
QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid @primary; }
QLabel { background: transparent; }
)"));
    setMinimumWidth(0); setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(12);

    m_empty = new QLabel; m_empty->setAlignment(Qt::AlignCenter); m_empty->setWordWrap(true);
    m_empty->setStyleSheet(QString("color:%1; font-size:14px; padding:36px;").arg(DS::hex(DS::textMuted)));
    root->addWidget(m_empty);

    m_content = new QWidget; auto* v = new QVBoxLayout(m_content); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(12);
    root->addWidget(m_content);

    // match summary
    m_summary = new QLabel; m_summary->setTextFormat(Qt::RichText); m_summary->setWordWrap(true);
    v->addWidget(m_summary);

    v->addWidget(new StakeBar(m_c));

    // how far to trust the goals markets
    auto* trustBox = new QFrame; trustBox->setObjectName("TrustBox");
    auto* tl = new QHBoxLayout(trustBox); tl->setContentsMargins(14, 10, 14, 10);
    m_trust = new QLabel; m_trust->setWordWrap(true); m_trust->setStyleSheet(QString("color:%1; font-size:12px;").arg(DS::hex(DS::leaf800)));
    tl->addWidget(m_trust);
    v->addWidget(trustBox);

    // market filter chips
    auto* chipHost = new QWidget; auto* chips = new QHBoxLayout(chipHost); chips->setContentsMargins(0, 0, 0, 6); chips->setSpacing(8);
    for (size_t i = 0; i < std::size(kChipNames); ++i) {
        auto* b = new QPushButton(kChipNames[i]); b->setProperty("chip", true); b->setProperty("active", i == 0); b->setCursor(Qt::PointingHandCursor);
        m_chips.push_back(b); chips->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i]() {
            m_filter = static_cast<int>(i);
            for (int k = 0; k < m_chips.size(); ++k) { m_chips[k]->setProperty("active", k == m_filter); m_chips[k]->style()->unpolish(m_chips[k]); m_chips[k]->style()->polish(m_chips[k]); }
            rebuild();
        });
    }
    chips->addStretch(1);
    auto* chipScroll = new QScrollArea; chipScroll->setWidget(chipHost); chipScroll->setWidgetResizable(true); chipScroll->setFrameShape(QFrame::NoFrame);
    chipScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); chipScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    chipScroll->setFixedHeight(54); chipScroll->setMinimumWidth(0);
    v->addWidget(chipScroll);

    // the table
    m_table = new QTableWidget(0, cCount);
    m_table->setHorizontalHeaderLabels({"Bet", "Chance", "Fair odds", "Value from", "Your odds", "Verdict", "Stake"});
    m_table->verticalHeader()->hide(); m_table->setShowGrid(false); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_table->setMinimumHeight(360); m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    auto* h = m_table->horizontalHeader(); h->setSectionResizeMode(cLabel, QHeaderView::Stretch); h->setHighlightSections(false);
    for (int k : {cChance, cFair, cFrom, cOdds, cVerdict, cStake}) h->setSectionResizeMode(k, QHeaderView::Fixed);
    m_table->setColumnWidth(cChance, 160); m_table->setColumnWidth(cFair, 72); m_table->setColumnWidth(cFrom, 84); m_table->setColumnWidth(cOdds, 84); m_table->setColumnWidth(cVerdict, 88); m_table->setColumnWidth(cStake, 80);
    m_table->setMinimumWidth(0); m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setItemDelegateForColumn(cChance, new ChanceDelegate(m_table));
    m_table->setItemDelegateForColumn(cLabel, new GroupDelegate(m_table));
    m_table->setItemDelegateForColumn(cOdds, new OddsDelegate(m_table));
    m_table->horizontalHeaderItem(cFair)->setToolTip("The odds at which this bet breaks even according to the model.");
    m_table->horizontalHeaderItem(cFrom)->setToolTip("Bookmaker odds at or above this give the expected value you require (see Settings > Reasoning rules).");
    m_table->horizontalHeaderItem(cOdds)->setToolTip("Double-click and type the bookmaker's decimal odds.");
    v->addWidget(m_table, 1);

    // advice card for the selected bet
    m_adviceBox = new QFrame; m_adviceBox->setObjectName("AdviceBox");
    auto* av = new QVBoxLayout(m_adviceBox); av->setContentsMargins(18, 14, 18, 14); av->setSpacing(4);
    auto* top = new QHBoxLayout;
    m_adviceTitle = new QLabel; m_adviceTitle->setStyleSheet(QString("font-size:16px; font-weight:700; color:%1;").arg(DS::hex(DS::text)));
    top->addWidget(m_adviceTitle, 1);
    m_slipBtn = new QPushButton("Add to bet slip"); m_slipBtn->setProperty("ubtn", "solid"); m_slipBtn->setProperty("primary", true); m_slipBtn->setCursor(Qt::PointingHandCursor);
    m_slipBtn->setVisible(mode == Customer);
    top->addWidget(m_slipBtn);
    av->addLayout(top);
    m_adviceHead = new QLabel; m_adviceHead->setWordWrap(true); m_adviceHead->setStyleSheet("font-size:14px; font-weight:600;");
    m_adviceBody = new QLabel; m_adviceBody->setWordWrap(true); m_adviceBody->setTextFormat(Qt::RichText);
    m_adviceBody->setStyleSheet(QString("font-size:13px; color:%1;").arg(DS::hex(DS::textMuted)));
    av->addWidget(m_adviceHead); av->addWidget(m_adviceBody);
    v->addWidget(m_adviceBox);

    // ---- wiring
    connect(m_table, &QTableWidget::currentCellChanged, this, [this]() { refreshAdvice(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) {
        if (m_building || it->column() != cOdds || it->row() >= static_cast<int>(m_rows.size()) || !m_rows[it->row()].sel) return;
        const double v2 = it->data(Qt::EditRole).toDouble();
        auto& map = m_odds[m_matchKey];
        const std::string& key = m_rows[it->row()].sel->key;
        if (v2 > 1.0) map[QString::fromStdString(key)] = v2; else map.remove(QString::fromStdString(key));
        m_building = true;
        it->setText(v2 > 1.0 ? QString::number(v2, 'f', 2) : QString::fromUtf8("\xE2\x80\x93"));
        m_building = false;
        refreshRow(it->row());
        const std::string opp = oppositeKey(key);                                  // the other side's margin estimate changed too
        for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) if (m_rows[r].sel && m_rows[r].sel->key == opp) refreshRow(r);
        refreshAdvice();
    });
    connect(m_slipBtn, &QPushButton::clicked, this, [this]() {
        const auto* s = selected(); if (!s) return;
        const double odds = typedOdds(s->key);
        if (!(odds > 1.0)) { Toast::show(this, "Type the bookmaker's odds for this bet first.", Toast::Warning); return; }
        sba::SlipLeg leg; leg.matchKey = m_matchKey.toStdString(); leg.matchLabel = m_matchLabel.toStdString(); leg.selectionKey = s->key; leg.selectionLabel = s->label;
        leg.pWin = s->pWin + s->pHalfWin; leg.pLose = s->pLose + s->pHalfLose; leg.pOther = s->pPush + s->pHalfWin + s->pHalfLose; leg.odds = odds;
        emit addToSlip(leg);
    });
    auto syncStake = [this]() { for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) refreshRow(r); refreshAdvice(); };
    connect(m_c, &AppController::stakingChanged, this, syncStake);
    connect(m_c, &AppController::settingsChanged, this, syncStake);
    connect(m_c, &AppController::backtestFinished, this, [this]() { refreshTrustNote(); });
    clearBook("Pick a match to see every betting market.");
}

QString MarketsPanel::money(double v) const {
    const bool whole = std::fabs(v - std::round(v)) < 0.005;
    return m_c->settings().currency + " " + QLocale(QLocale::English).toString(v, 'f', whole ? 0 : 2);
}

double MarketsPanel::shrinkFor(const sba::Selection& s) const {
    const bool oneXTwo = s.group == "result" || s.group == "double_chance" || s.group == "dnb";
    return oneXTwo ? m_c->settings().risk.shrinkTowardMarket : m_c->service().goalsShrink();
}

double MarketsPanel::typedOdds(const std::string& key) const {
    const auto it = m_odds.constFind(m_matchKey);
    if (it == m_odds.constEnd()) return sba::kNaN;
    const auto jt = it->constFind(QString::fromStdString(key));
    return jt == it->constEnd() ? sba::kNaN : *jt;
}

double MarketsPanel::oppositeOdds(const std::string& key) const {
    const std::string o = oppositeKey(key);
    return o.empty() ? sba::kNaN : typedOdds(o);
}

sba::BetAdvice MarketsPanel::adviceFor(const sba::Selection& s) const {
    return sba::adviseBet(s, typedOdds(s.key), oppositeOdds(s.key), m_c->stakeSettings(), m_c->settings().risk, shrinkFor(s));
}

const sba::Selection* MarketsPanel::selected() const {
    const int r = m_table->currentRow();
    return (r >= 0 && r < static_cast<int>(m_rows.size())) ? m_rows[r].sel : nullptr;
}

void MarketsPanel::clearBook(const QString& message) {
    m_book = sba::MarketBook(); m_rows.clear(); m_matchKey.clear();
    m_empty->setText(message); m_empty->show(); m_content->hide();
}

void MarketsPanel::setBook(const sba::MarketBook& book, const QString& matchKey, const QString& matchLabel) {
    if (!book.ok) { clearBook("No betting markets for this match: the goals model has no forecast for it."); return; }
    m_book = book; m_matchKey = matchKey; m_matchLabel = matchLabel;
    m_empty->hide(); m_content->show();
    refreshSummary(); refreshTrustNote(); rebuild();
}

void MarketsPanel::presetOdds(const std::vector<std::pair<std::string, double>>& odds) {
    bool any = false;
    for (const auto& o : odds) if (o.second > 1.0 && m_book.find(o.first)) { m_odds[m_matchKey][QString::fromStdString(o.first)] = o.second; any = true; }
    if (any && m_book.ok) rebuild();
}

void MarketsPanel::devFill(const std::pair<const char*, double>* picks, size_t n) {
    for (size_t i = 0; i < n; ++i) if (const auto* s = m_book.find(picks[i].first); s && std::isfinite(s->fairOdds)) m_odds[m_matchKey][QString::fromStdString(s->key)] = std::round(s->fairOdds * picks[i].second * 100.0) / 100.0;
    rebuild();
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) if (m_rows[r].sel && m_rows[r].sel->key == picks[0].first) { m_table->setCurrentCell(r, cOdds); break; }
    refreshAdvice();
}

void MarketsPanel::refreshSummary() {
    auto pct = [this](const char* key) { const auto* s = m_book.find(key); return s ? QString::number(s->pWin * 100, 'f', 0) + "%" : QString("-"); };
    QString score = "-";
    if (!m_book.topScores.empty()) score = QString("%1-%2 (%3%)").arg(m_book.topScores[0].home).arg(m_book.topScores[0].away).arg(m_book.topScores[0].p * 100, 0, 'f', 0);
    auto chip = [](const QString& k, const QString& v) {
        return QString("<span style='background:%1; color:%2; border-radius:10px; padding:3px 10px;'>&nbsp;%3 <b>%4</b>&nbsp;</span>&nbsp;&nbsp;").arg(DS::hex(DS::leaf50), DS::hex(DS::leaf800), k, v);
    };
    m_summary->setText(QString("<span style='font-size:14px;'><b>Expected goals</b> &nbsp;%1 %2 &ndash; %3 %4 &nbsp;&middot;&nbsp; total <b>%5</b></span><br><br>")
                           .arg(m_book.home.c_str()).arg(m_book.lambdaHome, 0, 'f', 2).arg(m_book.lambdaAway, 0, 'f', 2).arg(m_book.away.c_str()).arg(m_book.expectedTotal, 0, 'f', 2)
                       + chip("Over 2.5", pct("ou:2.5:o")) + chip("Under 2.5", pct("ou:2.5:u")) + chip("Both score", pct("btts:y")) + chip("Most likely score", score));
}

void MarketsPanel::refreshTrustNote() {
    const auto& g = m_c->service().goalsQuality();
    QString t;
    if (!g.ran()) t = "<b>How far to trust this:</b> the goals-market check on past matches is still running. Until it finishes, value flags are treated with extra caution.";
    else {
        const double diff = g.ouModelLogLoss - g.ouMarketLogLoss;
        QString verdict = g.marketMatches == 0 ? "there are no bookmaker totals to compare with, so value flags are treated cautiously"
                          : (diff < -0.01 ? "slightly better than the bookmaker's"
                          : (diff <= 0.01 ? "level with the bookmaker's, not better, so any edge shown here is small and uncertain"
                          : "behind the bookmaker's, so value flags on goals markets are treated with extra caution"));
        t = QString("<b>How far to trust this:</b> on %1 past matches the model had not seen, its over/under 2.5 accuracy (log loss %2) was %3 (%4). Odds-based edges on any market are estimates, not promises.")
                .arg(g.matches).arg(g.ouModelLogLoss, 0, 'f', 3).arg(verdict).arg(g.marketMatches ? QString("bookmaker %1").arg(g.ouMarketLogLoss, 0, 'f', 3) : QString("league average %1").arg(g.ouBaseLogLoss, 0, 'f', 3));
    }
    m_trust->setText(t);
}

void MarketsPanel::rebuild() {
    m_building = true;
    const int keepRow = m_table->currentRow();
    const std::string keepKey = (keepRow >= 0 && keepRow < static_cast<int>(m_rows.size()) && m_rows[keepRow].sel) ? m_rows[keepRow].sel->key : std::string();
    m_rows.clear(); m_table->clearSpans(); m_table->setRowCount(0);
    const auto& allow = kChipGroups[static_cast<size_t>(m_filter)];
    for (const auto& g : m_book.groups) {
        if (!allow.empty() && std::find(allow.begin(), allow.end(), g.id) == allow.end()) continue;
        Row hr; hr.header = true; hr.title = QString::fromStdString(g.title); hr.blurb = QString::fromStdString(g.blurb); m_rows.push_back(hr);
        for (const auto& s : g.items) { Row r; r.sel = &s; m_rows.push_back(r); }
    }
    m_table->setRowCount(static_cast<int>(m_rows.size()));
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) {
        const Row& row = m_rows[r];
        if (row.header) {
            auto* it = new QTableWidgetItem(row.title); it->setData(kRoleHeader, true); it->setData(kRoleBlurb, row.blurb); it->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(r, cLabel, it); m_table->setSpan(r, 0, 1, cCount); m_table->setRowHeight(r, 38);
            continue;
        }
        const sba::Selection& s = *row.sel;
        m_table->setItem(r, cLabel, cell(QString::fromStdString(s.label), Qt::AlignLeft));
        auto* ch = cell(""); ch->setData(kRoleProb, s.pWin + s.pHalfWin);
        ch->setToolTip(s.hasPush() ? QString("Win %1%  ·  money back / half result %2%  ·  lose %3%").arg(s.pWin * 100, 0, 'f', 1).arg((s.pPush + s.pHalfWin + s.pHalfLose) * 100, 0, 'f', 1).arg(s.pLose * 100, 0, 'f', 1) : QString("Model chance this bet wins: %1%").arg(s.pWin * 100, 0, 'f', 1));
        m_table->setItem(r, cChance, ch);
        m_table->setItem(r, cFair, cell(std::isfinite(s.fairOdds) ? QString::number(s.fairOdds, 'f', 2) : QString::fromUtf8("\xE2\x80\x93")));
        const double from = s.oddsForEv(m_c->settings().risk.minExpectedValue);
        auto* fr = cell(std::isfinite(from) && from < 1000 ? QString::fromUtf8("\xE2\x89\xA5 ") + QString::number(from, 'f', 2) : QString::fromUtf8("\xE2\x80\x93"));
        fr->setForeground(DS::textMuted); m_table->setItem(r, cFrom, fr);
        auto* od = cell(QString::fromUtf8("\xE2\x80\x93")); od->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        const double typed = typedOdds(s.key);
        od->setData(Qt::EditRole, std::isfinite(typed) ? typed : 0.0); od->setText(std::isfinite(typed) ? QString::number(typed, 'f', 2) : QString::fromUtf8("\xE2\x80\x93"));
        od->setBackground(DS::slate50); m_table->setItem(r, cOdds, od);
        m_table->setItem(r, cVerdict, cell("")); m_table->setItem(r, cStake, cell(""));
        m_table->setRowHeight(r, 38);
    }
    m_building = false;
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) refreshRow(r);
    int target = -1;
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) if (m_rows[r].sel && (keepKey.empty() ? target < 0 : m_rows[r].sel->key == keepKey)) { target = r; break; }
    if (target >= 0) m_table->setCurrentCell(target, cOdds);
    refreshAdvice();
}

void MarketsPanel::refreshRow(int r) {
    if (r < 0 || r >= static_cast<int>(m_rows.size()) || !m_rows[r].sel) return;
    const sba::BetAdvice a = adviceFor(*m_rows[r].sel);
    m_building = true;
    auto* v = m_table->item(r, cVerdict); auto* st = m_table->item(r, cStake);
    QColor fg = DS::textFaint, bg = Qt::transparent;
    switch (a.verdict) {
        case sba::Verdict::Value: fg = DS::success700; bg = DS::success50; break;
        case sba::Verdict::Marginal: fg = DS::warning700; bg = DS::warning50; break;
        case sba::Verdict::Invalid: fg = DS::danger700; break;
        default: break;
    }
    v->setText(a.verdict == sba::Verdict::NoOdds ? QString() : QString(sba::verdictName(a.verdict)));
    v->setForeground(fg); v->setBackground(bg == Qt::transparent ? QBrush() : QBrush(bg));
    QFont f = v->font(); f.setBold(a.verdict == sba::Verdict::Value); v->setFont(f);
    QString stake = QString::fromUtf8("\xE2\x80\x93");
    if (a.verdict == sba::Verdict::Value) stake = a.stake > 0 ? money(a.stake) : (a.stakeFraction > 0 && m_c->settings().bankroll <= 0 ? QString("%1%").arg(a.stakeFraction * 100, 0, 'f', 1) : stake);
    st->setText(stake); st->setForeground(a.verdict == sba::Verdict::Value ? DS::text : DS::textFaint);
    m_building = false;
}

void MarketsPanel::refreshAdvice() {
    const sba::Selection* s = selected();
    if (!s) {
        m_adviceTitle->setText("Pick a bet in the table"); m_adviceHead->setText("Select a row, then double-click \"Your odds\" and type what the bookmaker offers."); m_adviceBody->setText("");
        m_slipBtn->setEnabled(false); return;
    }
    const sba::BetAdvice a = adviceFor(*s);
    m_adviceTitle->setText(QString::fromStdString(s->label));
    QColor c = DS::textMuted;
    if (a.verdict == sba::Verdict::Value) c = DS::success700; else if (a.verdict == sba::Verdict::Marginal) c = DS::warning700; else if (a.verdict == sba::Verdict::NoValue) c = DS::slate700; else if (a.verdict == sba::Verdict::Invalid) c = DS::danger700;
    m_adviceHead->setStyleSheet(QString("font-size:14px; font-weight:600; color:%1;").arg(DS::hex(c)));
    QString head;
    QStringList lines;
    const double win = (s->pWin + s->pHalfWin) * 100, lose = (s->pLose + s->pHalfLose) * 100;
    if (a.verdict == sba::Verdict::NoOdds) {
        head = QString("Model chance %1%. Back it at odds of %2 or better to have value.").arg(win, 0, 'f', 0).arg(std::isfinite(a.valueFromOdds) && a.valueFromOdds < 1000 ? QString::number(a.valueFromOdds, 'f', 2) : QString("-"));
        lines << "Type the bookmaker's decimal odds in the <b>Your odds</b> column to get a verdict and a suggested stake.";
    } else {
        head = QString::fromStdString(a.headline);
        for (const auto& n : a.notes) lines << QString::fromStdString(n).toHtmlEscaped();
        if (a.verdict == sba::Verdict::Value) {
            const auto cfg = m_c->stakeSettings();
            const QString basis = QString("%1% of full Kelly%2").arg(cfg.kellyFraction * 100, 0, 'f', 0).arg(a.kellyApplied > cfg.maxStakeFraction ? ", capped at your limit per bet" : "");
            if (a.stake > 0) lines << QString("<b>Suggested stake: %1</b> (%2% of your bankroll; %3). If it wins you receive %4 (profit %5); if it loses you lose %6.")
                                          .arg(money(a.stake)).arg(a.stakeFraction * 100, 0, 'f', 1).arg(basis).arg(money(a.returnIfWin)).arg(money(a.profitIfWin)).arg(money(a.stake));
            else if (m_c->settings().bankroll <= 0) lines << QString("<b>Suggested stake: %1% of your bankroll</b> (%2). Set your bankroll above to see the amount in money.").arg(a.stakeFraction * 100, 0, 'f', 1).arg(basis);
        } else if (m_c->settings().bankroll > 0) lines << "<b>Suggested stake: none.</b> Skipping a bet like this is the right call.";
        lines << QString("This bet loses about %1% of the time, even when it is good value.").arg(lose, 0, 'f', 0);
    }
    m_adviceHead->setText(head);
    m_adviceBody->setText(lines.join("<br>"));
    m_slipBtn->setEnabled(std::isfinite(typedOdds(s->key)) && typedOdds(s->key) > 1.0);
}
