#include "UserHomePage.h"

#include "UserTheme.h"
#include "ui/Formatting.h"
#include "ui/art/Crest.h"
#include "ui/art/Icons.h"
#include "ui/art/RobotWidget.h"
#include "ui/design/Components.h"
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QScrollArea>
#include <QVBoxLayout>
#include <cmath>

namespace {

constexpr int kSide = 48;     // horizontal page padding

QLabel* text(const QString& t, int px, bool bold, const QColor& c, bool wrap = false) {
    auto* l = new QLabel(t);
    l->setWordWrap(wrap);
    l->setStyleSheet(QString("font-size:%1px; %2 color:%3; background:transparent;").arg(px).arg(bold ? "font-weight:700;" : "").arg(UT::hex(c)));
    return l;
}

QWidget* iconChip(Icons::Name n, int box = 44, const QColor& bg = DS::leaf50, const QColor& fg = DS::leaf700) {
    auto* l = new QLabel; l->setFixedSize(box, box);
    QPixmap pm(box * 2, box * 2); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
    { QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(bg); p.drawRoundedRect(QRectF(0, 0, box, box), box * 0.28, box * 0.28);
      const double s = box * 0.5; Icons::paint(p, n, QRectF((box - s) / 2, (box - s) / 2, s, s), fg, 1.9); }
    l->setPixmap(pm); l->setScaledContents(true);
    return l;
}

QFrame* card() {
    auto* f = new QFrame; f->setObjectName("HCard");
    applyElevation(f, 18, 2, 18);
    return f;
}

// Deep olive-to-leaf gradient band with soft decorative circles and faint pitch lines.
class HeroBand : public QWidget {
public:
    explicit HeroBand(QWidget* parent = nullptr) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, width(), height());
        g.setColorAt(0, DS::heroFrom); g.setColorAt(0.55, DS::heroMid); g.setColorAt(1, DS::heroTo);
        p.fillRect(rect(), g);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(169, 191, 134, 22)); p.drawEllipse(QPointF(width() * 0.86, -40), 260, 260);
        p.setBrush(QColor(209, 216, 230, 12)); p.drawEllipse(QPointF(width() * 0.97, height() * 0.95), 200, 200);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(209, 216, 230, 22), 2));
        const QPointF c(width() * 0.78, height() * 0.52);                      // faint centre circle + halfway line behind the robot
        p.drawEllipse(c, 190, 190); p.drawLine(QPointF(c.x(), 0), QPointF(c.x(), height()));
        QRadialGradient glow(c, 260); glow.setColorAt(0, QColor(169, 191, 134, 60)); glow.setColorAt(1, QColor(169, 191, 134, 0));
        p.setPen(Qt::NoPen); p.fillRect(rect(), glow);
    }
};

// Top of the table: rank, crest, team, played, points.
class MiniTable : public QWidget {
public:
    explicit MiniTable(QWidget* parent = nullptr) : QWidget(parent) { setMinimumHeight(6 * 46 + 30); connect(&CrestProvider::instance(), &CrestProvider::crestsChanged, this, qOverload<>(&QWidget::update)); }
    void setRows(const std::vector<TableRow>& r) { m_rows = r; update(); }
    QSize sizeHint() const override { return {320, int(std::max<size_t>(m_rows.size(), 6)) * 46}; }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int W = width();
        p.setFont(UT::font(11, true)); p.setPen(UT::muted);
        p.drawText(QRect(W - 118, 0, 44, 24), Qt::AlignCenter, "P");
        p.drawText(QRect(W - 64, 0, 52, 24), Qt::AlignCenter, "PTS");
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const int y = 26 + int(i) * 46;
            p.setPen(UT::line); p.drawLine(0, y + 45, W, y + 45);
            if (i == 0) { p.setPen(Qt::NoPen); p.setBrush(DS::leaf50); p.drawRoundedRect(QRect(-4, y + 2, W + 8, 41), 8, 8); }
            p.setFont(UT::font(13, true)); p.setPen(i < 4 ? UT::accentDark : UT::soft);
            p.drawText(QRect(4, y, 26, 45), Qt::AlignCenter, QString::number(i + 1));
            CrestProvider::instance().paint(p, QRectF(36, y + 8, 29, 29), m_rows[i].team);
            p.setFont(UT::font(14, i == 0)); p.setPen(UT::ink);
            p.drawText(QRect(76, y, W - 76 - 124, 45), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(m_rows[i].team, Qt::ElideRight, W - 76 - 124));
            p.setFont(UT::font(13)); p.setPen(UT::soft);
            p.drawText(QRect(W - 118, y, 44, 45), Qt::AlignCenter, QString::number(m_rows[i].played));
            p.setFont(UT::font(15, true)); p.setPen(UT::ink);
            p.drawText(QRect(W - 64, y, 52, 45), Qt::AlignCenter, QString::number(m_rows[i].points));
        }
    }
private:
    std::vector<TableRow> m_rows;
};

}  // namespace

UserHomePage::UserHomePage(UserDataHub* hub, QWidget* parent) : QWidget(parent), m_hub(hub) {
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* content = new QWidget; auto* v = new QVBoxLayout(content); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(0);
    scroll->setWidget(content);

    // ------------------------------------------------------------------------------------------ hero
    auto* hero = new HeroBand; hero->setMinimumHeight(400);
    auto* hl = new QHBoxLayout(hero); hl->setContentsMargins(kSide + 8, 40, kSide, 40); hl->setSpacing(24);
    auto* left = new QVBoxLayout; left->setSpacing(16); left->addStretch(1);
    auto* badge = new QLabel("  AI-POWERED MATCH ANALYSIS  ");
    badge->setStyleSheet("font-size:11px; font-weight:700; letter-spacing:1px; color:#CBD9B3; background:rgba(255,255,255,0.10); border:1px solid rgba(255,255,255,0.22); border-radius:13px; padding:5px 6px;");
    left->addWidget(badge, 0, Qt::AlignLeft);
    auto* title = text("Football predictions\nyou can actually check.", 46, true, Qt::white, true);
    title->setStyleSheet("font-size:46px; font-weight:800; color:#ffffff; background:transparent;");
    left->addWidget(title);
    auto* sub = text("See the probability of every result, compare it with bookmaker prices and understand why. A goals model and a random forest work side by side, and every number is shown openly.", 16, false, DS::paletteVioletWater, true);
    sub->setMaximumWidth(600);
    left->addWidget(sub);
    auto* ctas = new QHBoxLayout; ctas->setSpacing(12);
    auto* b1 = new QPushButton("Today's matches"); b1->setProperty("ubtn", "hero"); b1->setCursor(Qt::PointingHandCursor);
    b1->setIcon(Icons::icon(Icons::ArrowRight, DS::leaf800, 18)); b1->setLayoutDirection(Qt::RightToLeft);
    auto* b2 = new QPushButton("Get a prediction"); b2->setProperty("ubtn", "heroGhost"); b2->setCursor(Qt::PointingHandCursor);
    ctas->addWidget(b1); ctas->addWidget(b2); ctas->addStretch(1);
    left->addSpacing(6); left->addLayout(ctas);
    left->addWidget(text("Free to use  \u00B7  No account  \u00B7  Estimates only, never guarantees", 12, false, DS::slate400));
    left->addStretch(1);
    hl->addLayout(left, 3);
    auto* robot = new RobotWidget; robot->setMinimumSize(300, 300); robot->setMaximumWidth(420); robot->setAccent(DS::leaf300);
    hl->addWidget(robot, 2);
    v->addWidget(hero);

    // ------------------------------------------------------------------------------------------ stats
    auto* statsBox = new QWidget; auto* sl = new QHBoxLayout(statsBox); sl->setContentsMargins(kSide, 28, kSide, 4); sl->setSpacing(16);
    auto stat = [&](Icons::Name ic, QLabel*& value, QLabel*& subLabel, const QString& caption) {
        auto* c = card(); auto* h = new QHBoxLayout(c); h->setContentsMargins(20, 18, 20, 18); h->setSpacing(14);
        h->addWidget(iconChip(ic));
        auto* col = new QVBoxLayout; col->setSpacing(1);
        value = text("-", 26, true, UT::ink); subLabel = text("", 12, false, UT::soft);
        col->addWidget(text(caption.toUpper(), 10, true, UT::muted)); col->addWidget(value); col->addWidget(subLabel);
        h->addLayout(col, 1);
        sl->addWidget(c, 1);
    };
    stat(Icons::Table, m_statMatches, m_statMatchesSub, "Matches analysed");
    stat(Icons::Trophy, m_statClubs, m_statClubsSub, "Clubs tracked");
    stat(Icons::Calendar, m_statDate, m_statDateSub, "Results up to");
    stat(Icons::Predict, m_statModels, m_statModelsSub, "Models blended");
    v->addWidget(statsBox);

    // ------------------------------------------------------------------------------------------ featured + snapshot
    auto* featBox = new QWidget; auto* fl = new QHBoxLayout(featBox); fl->setContentsMargins(kSide, 28, kSide, 4); fl->setSpacing(20);
    auto* fleft = new QVBoxLayout; fleft->setSpacing(10);
    auto* fh = new QHBoxLayout;
    fh->addWidget(text("Featured match", 20, true, UT::ink));
    m_featKicker = new QLabel; m_featKicker->setStyleSheet(QString("font-size:11px; font-weight:700; color:%1; background:%2; border-radius:11px; padding:3px 10px;").arg(UT::hex(DS::leaf800), UT::hex(DS::leaf100)));
    fh->addWidget(m_featKicker); fh->addStretch(1);
    auto* allScores = new QPushButton("All scores"); allScores->setProperty("ubtn", "link"); allScores->setCursor(Qt::PointingHandCursor);
    fh->addWidget(allScores);
    fleft->addLayout(fh);
    m_pitch = new PitchWidget; m_pitch->setMinimumHeight(372); m_pitch->setClickable(true);
    m_featBox = m_pitch;
    fleft->addWidget(m_pitch, 1);
    fl->addLayout(fleft, 3);

    auto* snap = card(); auto* sv = new QVBoxLayout(snap); sv->setContentsMargins(20, 18, 20, 14); sv->setSpacing(6);
    auto* sh = new QHBoxLayout; sh->addWidget(text("League snapshot", 17, true, UT::ink)); sh->addStretch(1);
    auto* fullTable = new QPushButton("Full table"); fullTable->setProperty("ubtn", "link"); fullTable->setCursor(Qt::PointingHandCursor); sh->addWidget(fullTable);
    sv->addLayout(sh);
    m_mini = new MiniTable; sv->addWidget(m_mini, 1);
    fl->addWidget(snap, 2);
    v->addWidget(featBox);

    // ------------------------------------------------------------------------------------------ how it works
    auto* howBox = new QWidget; auto* hv = new QVBoxLayout(howBox); hv->setContentsMargins(kSide, 36, kSide, 4); hv->setSpacing(14);
    hv->addWidget(text("How it works", 20, true, UT::ink));
    auto* steps = new QHBoxLayout; steps->setSpacing(16);
    auto step = [&](int n, Icons::Name ic, const QString& title2, const QString& body) {
        auto* c = card(); auto* cl = new QVBoxLayout(c); cl->setContentsMargins(22, 20, 22, 22); cl->setSpacing(10);
        auto* top = new QHBoxLayout; top->addWidget(iconChip(ic, 46)); top->addStretch(1);
        top->addWidget(text(QString("0%1").arg(n), 26, true, DS::slate300));
        cl->addLayout(top);
        cl->addWidget(text(title2, 16, true, UT::ink));
        cl->addWidget(text(body, 13, false, UT::soft, true));
        cl->addStretch(1);
        steps->addWidget(c, 1);
    };
    step(1, Icons::Matches, "Pick a match", "Browse live scores and upcoming fixtures, or choose any two clubs yourself.");
    step(2, Icons::Predict, "The analyst runs the numbers", "Recent form, goals scored and conceded, and a random forest trained on years of results.");
    step(3, Icons::Target, "Compare with the market", "Probabilities are checked against bookmaker odds, so you see where value exists and where it does not.");
    hv->addLayout(steps);
    v->addWidget(howBox);

    // ------------------------------------------------------------------------------------------ honest by design
    auto* trustBox = new QWidget; auto* tv = new QVBoxLayout(trustBox); tv->setContentsMargins(kSide, 32, kSide, 4);
    auto* trust = new QFrame; trust->setObjectName("Trust");
    trust->setStyleSheet(QString("QFrame#Trust { border-radius:18px; background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2); } QLabel { background:transparent; }").arg(UT::hex(DS::heroFrom), UT::hex(DS::heroMid)));
    auto* trl = new QHBoxLayout(trust); trl->setContentsMargins(32, 26, 32, 26); trl->setSpacing(28);
    trl->addWidget(iconChip(Icons::Shield, 56, QColor(255, 255, 255, 30), DS::leaf200));
    auto* tcol = new QVBoxLayout; tcol->setSpacing(6);
    tcol->addWidget(text("Honest by design", 20, true, Qt::white));
    m_trustText = text("", 14, false, DS::paletteVioletWater, true);
    tcol->addWidget(m_trustText);
    trl->addLayout(tcol, 3);
    m_trustMetric = text("", 13, true, DS::leaf200);
    m_trustMetric->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    trl->addWidget(m_trustMetric, 1);
    tv->addWidget(trust);
    v->addWidget(trustBox);

    // ------------------------------------------------------------------------------------------ closing call to action
    auto* endBox = new QWidget; auto* ev = new QVBoxLayout(endBox); ev->setContentsMargins(kSide, 44, kSide, 40); ev->setSpacing(12);
    auto* e1 = text("Ready when you are.", 26, true, UT::ink); e1->setAlignment(Qt::AlignCenter);
    auto* e2 = text("Open today's scores or ask the analyst about any fixture.", 14, false, UT::soft); e2->setAlignment(Qt::AlignCenter);
    ev->addWidget(e1); ev->addWidget(e2);
    auto* er = new QHBoxLayout; er->addStretch(1);
    auto* c1 = new QPushButton("Browse scores"); c1->setProperty("ubtn", "solid"); c1->setCursor(Qt::PointingHandCursor);
    auto* c2 = new QPushButton("Ask the analyst"); c2->setCursor(Qt::PointingHandCursor);
    er->addWidget(c1); er->addSpacing(8); er->addWidget(c2); er->addStretch(1);
    ev->addLayout(er);
    auto* disc = text("Estimates only, not financial advice. 18+. Please gamble responsibly.", 12, false, UT::muted); disc->setAlignment(Qt::AlignCenter);
    ev->addSpacing(10); ev->addWidget(disc);
    v->addWidget(endBox);
    v->addStretch(1);

    // ------------------------------------------------------------------------------------------ wiring
    connect(b1, &QPushButton::clicked, this, [this]() { emit gotoTab(1); });
    connect(b2, &QPushButton::clicked, this, [this]() { emit gotoTab(2); });
    connect(allScores, &QPushButton::clicked, this, [this]() { emit gotoTab(1); });
    connect(fullTable, &QPushButton::clicked, this, [this]() { emit gotoTab(3); });
    connect(c1, &QPushButton::clicked, this, [this]() { emit gotoTab(1); });
    connect(c2, &QPushButton::clicked, this, [this]() { emit gotoTab(2); });
    connect(m_pitch, &PitchWidget::clicked, this, [this]() { if (m_hasFeat) emit openMatch(m_feat); });
    connect(hub, &UserDataHub::dataChanged, this, [this]() { refresh(); });
    connect(hub, &UserDataHub::pickReady, this, [this]() { if (m_hasFeat) m_pitch->setData(pitchDataFor(m_hub, m_feat)); });
    connect(hub->controller(), &AppController::initialised, this, [this]() { refresh(); });
    connect(hub->controller(), &AppController::backtestFinished, this, [this]() { refresh(); });
    refresh();
}

void UserHomePage::refresh() {
    auto* c = m_hub->controller();
    if (!c->ready()) {
        m_statMatches->setText(c->initError().isEmpty() ? "..." : "-"); m_statMatchesSub->setText(c->initError().isEmpty() ? "Loading data" : "Unavailable");
        m_trustText->setText("The models are loading. Everything here comes straight from them.");
        return;
    }
    const auto& st = c->service().status();
    const auto& ds = c->service().dataset();
    m_statMatches->setText(QLocale(QLocale::English).toString(st.matches)); m_statMatchesSub->setText("since " + QLocale::c().toString(UserDataHub::dateOf(ds.firstDate()), "MMM yyyy"));
    m_statClubs->setText(QString::number(ds.teams().size())); m_statClubsSub->setText("Premier League and more");
    const QDate last = m_hub->lastResultDate();
    m_statDate->setText(last.isValid() ? QLocale::c().toString(last, "d MMM") : "-"); m_statDateSub->setText(last.isValid() ? QString::number(last.year()) : "");
    m_statModels->setText(st.rf && st.poisson ? "2" : "1"); m_statModelsSub->setText(st.rf && st.poisson ? "Random Forest + Poisson" : (st.rf ? "Random Forest" : "Poisson"));

    // featured match
    MatchItem f;
    m_hasFeat = m_hub->featured(f);
    if (m_hasFeat) {
        m_feat = f;
        if (f.fx.isUpcoming()) m_hub->requestPick(f);
        m_featKicker->setText(f.fx.isLive() ? "LIVE NOW" : (f.fx.isUpcoming() ? "NEXT UP" : "LATEST RESULT"));
        m_pitch->setData(pitchDataFor(m_hub, f));
    } else {
        PitchData d; d.home = "Arsenal"; d.away = "Chelsea"; d.kind = PitchData::Upcoming; d.whenText = "VS"; d.subtitle = "Premier League";
        m_featKicker->setText("PREVIEW"); m_pitch->setData(d);
    }

    // league snapshot
    const auto seasons = m_hub->seasons();
    if (!seasons.empty()) {
        auto rows = m_hub->table(seasons.front());
        if (rows.size() > 6) rows.resize(6);
        static_cast<MiniTable*>(m_mini)->setRows(rows);
    }

    // honest by design
    const auto& bt = c->backtest();
    if (bt.ok && std::isfinite(bt.quality.modelLogLoss)) {
        const bool beats = bt.quality.modelLogLoss < bt.quality.marketLogLoss;
        m_trustText->setText(beats ? "On matches it had never seen, the model scored slightly better than bookmaker odds. We still show both numbers, because a small edge is not a guarantee."
                                   : "On matches it had never seen, the model did not beat bookmaker odds. We show that openly, so you can judge every prediction for what it is: an estimate.");
        m_trustMetric->setText(QString("Model %1\nBookmakers %2\nlog loss, lower is better").arg(Fmt::num(bt.quality.modelLogLoss, 4), Fmt::num(bt.quality.marketLogLoss, 4)));
    } else {
        m_trustText->setText("We compare our model with bookmaker odds on matches it has never seen, and we show you the result, good or bad.");
        m_trustMetric->setText(c->backtestRunning() ? "Running the check..." : "");
    }
}
