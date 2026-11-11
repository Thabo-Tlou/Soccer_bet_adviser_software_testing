#include "UserWidgets.h"
#include "ui/Formatting.h"
#include "UserTheme.h"
#include "ui/art/AnimClock.h"
#include "ui/art/Crest.h"
#include <QRadialGradient>
#include <QLinearGradient>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>

static QPolygonF starPolygon(const QPointF& c, double r) {
    QPolygonF poly;
    for (int i = 0; i < 10; ++i) {
        const double a = -M_PI / 2 + i * M_PI / 5, rr = (i % 2 == 0) ? r : r * 0.45;
        poly << QPointF(c.x() + rr * std::cos(a), c.y() + rr * std::sin(a));
    }
    return poly;
}

QFrame* makeColumn(QWidget* page, QVBoxLayout*& inner) {
    auto* outer = new QHBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* col = new QFrame; col->setObjectName("Col");                 // fills 100% of the window width
    col->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    inner = new QVBoxLayout(col); inner->setContentsMargins(0, 0, 0, 0); inner->setSpacing(0);
    outer->addWidget(col, 1);
    return col;
}

// ====================================================================== TextTabs

TextTabs::TextTabs(bool dark, QWidget* parent) : QWidget(parent), m_dark(dark) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(dark ? 52 : 44);
}

void TextTabs::setTabs(const QStringList& t) { m_tabs = t; if (m_cur >= t.size()) m_cur = 0; updateGeometry(); update(); }
void TextTabs::setLabel(int i, const QString& t) { if (i >= 0 && i < m_tabs.size() && m_tabs[i] != t) { m_tabs[i] = t; updateGeometry(); update(); } }
void TextTabs::setCurrent(int i, bool notify) {
    if (i < 0 || i >= m_tabs.size() || i == m_cur) return;
    m_cur = i; update();
    if (notify) emit changed(i);
}

QRect TextTabs::tabRect(int i) const {
    QFontMetrics fm(UT::font(13, true));
    int x = m_dark ? 0 : 8;
    for (int k = 0; k < m_tabs.size(); ++k) {
        const int w = fm.horizontalAdvance(m_tabs[k]) + 32;
        if (k == i) return QRect(x, 0, w, height());
        x += w;
    }
    return {};
}

QSize TextTabs::sizeHint() const {
    QFontMetrics fm(UT::font(13, true));
    int w = m_dark ? 0 : 16;
    for (const auto& t : m_tabs) w += fm.horizontalAdvance(t) + 32;
    return {w, height()};
}

int TextTabs::indexAt(int x) const {
    for (int i = 0; i < m_tabs.size(); ++i) if (tabRect(i).contains(x, 1)) return i;
    return -1;
}

void TextTabs::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFontMetrics fm(UT::font(13, true));
    for (int i = 0; i < m_tabs.size(); ++i) {
        const QRect r = tabRect(i);
        const bool act = i == m_cur, hov = i == m_hover;
        QColor c = m_dark ? (act || hov ? UT::white : DS::slate400) : (act ? UT::accentDark : (hov ? UT::black : UT::soft));
        p.setFont(UT::font(13, true));
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter, m_tabs[i]);
        if (act) {
            const int tw = fm.horizontalAdvance(m_tabs[i]);
            p.fillRect(QRect(r.center().x() - tw / 2 - 2, height() - 3, tw + 4, 3), m_dark ? UT::accentOnDark : UT::accent);
        }
    }
}

void TextTabs::mousePressEvent(QMouseEvent* e) {
    const int i = indexAt(e->position().toPoint().x());
    if (i >= 0) setCurrent(i, true);
}
void TextTabs::mouseMoveEvent(QMouseEvent* e) {
    const int i = indexAt(e->position().toPoint().x());
    if (i != m_hover) { m_hover = i; update(); }
}
void TextTabs::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ====================================================================== NavTabs

NavTabs::NavTabs(QWidget* parent) : QWidget(parent) { setMouseTracking(true); setCursor(Qt::PointingHandCursor); setFixedHeight(64); }

void NavTabs::setTabs(const QStringList& labels, const QList<Icons::Name>& icons) { m_labels = labels; m_icons = icons; updateGeometry(); update(); }

void NavTabs::setCurrent(int i, bool notify) {
    if (i < 0 || i >= m_labels.size() || i == m_cur) return;
    m_cur = i; update();
    if (notify) emit changed(i);
}

static int navTabWidth(const QString& label) { return 14 + 18 + 8 + QFontMetrics(UT::font(14, true)).horizontalAdvance(label) + 16; }

QSize NavTabs::sizeHint() const {
    int w = 0; for (const QString& l : m_labels) w += navTabWidth(l) + 4;
    return {w, 64};
}

QRect NavTabs::tabRect(int i) const {
    int x = 0;
    for (int k = 0; k < i; ++k) x += navTabWidth(m_labels[k]) + 4;
    return QRect(x, 11, navTabWidth(m_labels[i]), height() - 22);
}

int NavTabs::indexAt(const QPoint& p) const { for (int i = 0; i < m_labels.size(); ++i) if (tabRect(i).contains(p)) return i; return -1; }

void NavTabs::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < m_labels.size(); ++i) {
        const QRect r = tabRect(i);
        const bool act = i == m_cur, hov = i == m_hover;
        if (act) { p.setPen(Qt::NoPen); p.setBrush(UT::accentSoft); p.drawRoundedRect(r, 10, 10); }
        else if (hov) { p.setPen(Qt::NoPen); p.setBrush(DS::slate100); p.drawRoundedRect(r, 10, 10); }
        const QColor c = act ? DS::leaf800 : (hov ? UT::ink : UT::soft);
        if (i < m_icons.size()) Icons::paint(p, m_icons[i], QRectF(r.left() + 14, r.center().y() - 9, 18, 18), c, act ? 2.0 : 1.8);
        p.setFont(UT::font(14, true)); p.setPen(c);
        p.drawText(QRect(r.left() + 14 + 18 + 8, r.top(), r.width() - 40, r.height()), Qt::AlignVCenter | Qt::AlignLeft, m_labels[i]);
    }
}

void NavTabs::mousePressEvent(QMouseEvent* e) { const int i = indexAt(e->position().toPoint()); if (i >= 0) setCurrent(i, true); }
void NavTabs::mouseMoveEvent(QMouseEvent* e) { const int i = indexAt(e->position().toPoint()); if (i != m_hover) { m_hover = i; update(); } }
void NavTabs::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ====================================================================== DateStrip

DateStrip::DateStrip(QWidget* parent) : QWidget(parent), m_sel(Fmt::today()) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(58);
}

void DateStrip::setDate(const QDate& d, bool notify) {
    if (!d.isValid() || d == m_sel) return;
    m_sel = d; update();
    if (notify) emit dateChanged(d);
}

int DateStrip::cellAt(const QPoint& pt) const {
    const int arrow = 44, W = width();
    if (pt.x() < arrow) return -1;
    if (pt.x() >= W - arrow) return 7;
    const double cw = double(W - 2 * arrow) / 7.0;
    return std::min(6, int((pt.x() - arrow) / cw));
}

void DateStrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int arrow = 44, W = width(), H = height();
    const double cw = double(W - 2 * arrow) / 7.0;
    p.fillRect(rect(), UT::white);
    p.setPen(UT::line); p.drawLine(0, H - 1, W, H - 1);

    // arrows
    for (int side = 0; side < 2; ++side) {
        const bool hov = m_hover == (side == 0 ? -1 : 7);
        p.setPen(QPen(hov ? UT::black : UT::muted, 1.8, Qt::SolidLine, Qt::RoundCap));
        const double cx = side == 0 ? arrow / 2.0 : W - arrow / 2.0, cy = H / 2.0, d = side == 0 ? -1 : 1;
        p.drawLine(QPointF(cx - 3 * d, cy - 6), QPointF(cx + 3 * d, cy));
        p.drawLine(QPointF(cx + 3 * d, cy), QPointF(cx - 3 * d, cy + 6));
    }
    const QDate today = Fmt::today();
    for (int i = 0; i < 7; ++i) {
        const QDate d = m_sel.addDays(i - 3);
        const QRectF cell(arrow + i * cw, 0, cw, H - 1);
        const bool sel = i == 3;
        const QRectF pillR = cell.adjusted(8, 6, -8, -7);
        if (sel) { p.setPen(Qt::NoPen); p.setBrush(UT::accent); p.drawRoundedRect(pillR, 12, 12); }
        else if (i == m_hover) { p.setPen(Qt::NoPen); p.setBrush(UT::hover); p.drawRoundedRect(pillR, 12, 12); }
        const QString top = d == today ? QStringLiteral("TODAY") : QLocale::c().toString(d, "ddd").toUpper();
        const QString bottom = QLocale::c().toString(d, "d MMM").toUpper();
        p.setFont(UT::font(10, true));
        p.setPen(sel ? QColor(255, 255, 255, 190) : (d == today ? UT::accent : UT::muted));
        p.drawText(QRectF(cell.x(), 12, cell.width(), 14), Qt::AlignCenter, top);
        p.setFont(UT::font(13, sel));
        p.setPen(sel ? UT::white : UT::soft);
        p.drawText(QRectF(cell.x(), 27, cell.width(), 18), Qt::AlignCenter, bottom);
    }
}

void DateStrip::mousePressEvent(QMouseEvent* e) {
    const int c = cellAt(e->position().toPoint());
    if (c == -1) setDate(m_sel.addDays(-1), true);
    else if (c == 7) setDate(m_sel.addDays(1), true);
    else if (c >= 0) setDate(m_sel.addDays(c - 3), true);
}
void DateStrip::mouseMoveEvent(QMouseEvent* e) {
    const int c = cellAt(e->position().toPoint());
    if (c != m_hover) { m_hover = c; update(); }
}
void DateStrip::leaveEvent(QEvent*) { m_hover = -2; update(); }

// ====================================================================== SectionHeader

SectionHeader::SectionHeader(const QString& title, const QString& right, QWidget* parent) : QWidget(parent), m_title(title), m_right(right) {
    setFixedHeight(34);
}

void SectionHeader::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), UT::band);
    p.setPen(UT::line);
    p.drawLine(0, 0, width(), 0);
    p.drawLine(0, height() - 1, width(), height() - 1);
    p.setFont(UT::font(13, true));
    p.setPen(UT::black);
    p.drawText(QRect(16, 0, width() - 100, height()), Qt::AlignVCenter | Qt::AlignLeft, m_title);
    if (!m_right.isEmpty()) {
        p.setFont(UT::font(12));
        p.setPen(UT::muted);
        p.drawText(QRect(0, 0, width() - 16, height()), Qt::AlignVCenter | Qt::AlignRight, m_right);
    }
}

// ====================================================================== MatchRow

MatchRow::MatchRow(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(62);
    connect(&AnimClock::instance(), &AnimClock::tick, this, [this]() { if (m_item.fx.isLive() && isVisible()) update(); });
    connect(&CrestProvider::instance(), &CrestProvider::crestsChanged, this, qOverload<>(&QWidget::update));
}

QString MatchRow::statusText(const MatchItem& m) {
    const auto& f = m.fx;
    if (f.isLive()) return f.status == "PAUSED" ? QStringLiteral("HT") : (f.minute >= 0 ? QString("%1'").arg(f.minute) : QStringLiteral("LIVE"));
    if (f.isFinished()) return QStringLiteral("FT");
    if (f.status == "POSTPONED") return QStringLiteral("PP");
    if (f.status == "CANCELLED") return QStringLiteral("CAN");
    if (f.status == "SUSPENDED") return QStringLiteral("SUSP");
    if (f.kickoffEpoch > 0 && !m.fromDataset) return Fmt::local(f.kickoffEpoch).toString("HH:mm");
    return QStringLiteral("-");
}

void MatchRow::setItem(const MatchItem& m, const PickInfo* pick, bool fav) {
    m_item = m; m_fav = fav;
    m_hasPick = pick && pick->ok && pick->a;
    m_value = m_hasPick && pick->a->reasoning.decision == sba::Decision::PossibleValue;
    m_best = m_hasPick ? pick->best : 0;
    m_p = m_hasPick ? pick->p : 0;
    update();
}

void MatchRow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = width(), H = height();
    const auto& f = m_item.fx;
    const bool live = f.isLive(), fin = f.isFinished();
    const double t = AnimClock::instance().seconds();
    p.fillRect(rect(), live ? QColor("#FDF5F3") : (m_hover ? UT::hover : UT::white));
    p.setPen(UT::line); p.drawLine(16, H - 1, W - 16, H - 1);
    if (live) p.fillRect(0, 0, 3, H, UT::live);

    const int statusW = 76, starW = 44, pickW = 176;
    // status (live rows get a pulsing dot)
    p.setFont(UT::font(12, live));
    p.setPen(live ? UT::live : (fin ? UT::muted : UT::black));
    if (live) {
        const double pulse = 0.5 + 0.5 * std::sin(t * 5.0);
        p.setPen(Qt::NoPen); p.setBrush(QColor(200, 55, 45, int(60 + 60 * pulse))); p.drawEllipse(QPointF(20, H / 2.0), 7 + 2 * pulse, 7 + 2 * pulse);
        p.setBrush(UT::live); p.drawEllipse(QPointF(20, H / 2.0), 3.6, 3.6);
        p.setPen(UT::live);
        p.drawText(QRect(30, 0, statusW - 30, H), Qt::AlignVCenter | Qt::AlignLeft, statusText(m_item));
    } else {
        p.drawText(QRect(0, 0, statusW, H), Qt::AlignCenter, statusText(m_item));
    }

    const int midL = statusW, midR = std::max(midL + 300, W - pickW - starW), cx = (midL + midR) / 2, boxW = 74;
    // score pill
    const QRectF sp(cx - boxW / 2.0, (H - 30) / 2.0, boxW, 30);
    if (m_item.hasScore() && (live || fin)) {
        p.setPen(Qt::NoPen); p.setBrush(live ? UT::live : DS::slate100); p.drawRoundedRect(sp, 9, 9);
        p.setFont(UT::font(15, true)); p.setPen(live ? UT::white : UT::black);
        p.drawText(sp, Qt::AlignCenter, QString("%1 - %2").arg(f.homeGoals).arg(f.awayGoals));
    } else {
        p.setFont(UT::font(13, true)); p.setPen(DS::slate400);
        p.drawText(sp, Qt::AlignCenter, QStringLiteral("vs"));
    }
    // crests + names
    const int cs = 30;
    const QRectF hc(cx - boxW / 2.0 - 12 - cs, (H - cs) / 2.0, cs, cs), ac(cx + boxW / 2.0 + 12, (H - cs) / 2.0, cs, cs);
    CrestProvider::instance().paint(p, hc, m_item.home());
    CrestProvider::instance().paint(p, ac, m_item.away());
    p.setFont(UT::font(14, true));
    p.setPen(UT::black);
    QFontMetrics fm(p.font());
    const QRect hr(midL + 4, 0, int(hc.left()) - 10 - (midL + 4), H), ar(int(ac.right()) + 10, 0, midR - 4 - (int(ac.right()) + 10), H);
    p.drawText(hr, Qt::AlignVCenter | Qt::AlignRight, fm.elidedText(m_item.home(), Qt::ElideRight, hr.width()));
    p.drawText(ar, Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(m_item.away(), Qt::ElideRight, ar.width()));

    // pick
    int x = W - starW;
    if (m_hasPick) {
        p.setFont(UT::font(13, true));
        p.setPen(UT::black);
        p.drawText(QRect(x - 46, 0, 42, H), Qt::AlignVCenter | Qt::AlignRight, pctText(m_p));
        const QRect chip(x - 46 - 30, (H - 24) / 2, 24, 24);
        p.setPen(Qt::NoPen); p.setBrush(UT::accent);
        p.drawRoundedRect(chip, 8, 8);
        p.setPen(UT::white); p.setFont(UT::font(13, true));
        p.drawText(chip, Qt::AlignCenter, outcomeSymbol(m_best));
        if (m_value) {
            const QRect tag(chip.left() - 8 - 52, (H - 22) / 2, 52, 22);
            p.setPen(QPen(UT::success, 1.2)); p.setBrush(DS::success50);
            p.drawRoundedRect(tag, 11, 11);
            p.setFont(UT::font(10, true)); p.setPen(UT::success);
            p.drawText(tag, Qt::AlignCenter, QStringLiteral("VALUE"));
        }
    }
    // star
    const QPointF sc(W - starW / 2.0 - 2, H / 2.0);
    p.setBrush(m_fav ? QBrush(UT::accent) : Qt::NoBrush);
    p.setPen(QPen(m_fav ? UT::accent : (m_overStar ? UT::soft : DS::slate400), 1.4));
    p.drawPolygon(starPolygon(sc, 8.5));
}

void MatchRow::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (e->position().x() >= width() - 44) emit favToggled(); else emit clicked();
}
void MatchRow::mouseMoveEvent(QMouseEvent* e) {
    const bool s = e->position().x() >= width() - 44;
    if (s != m_overStar) { m_overStar = s; update(); }
}
void MatchRow::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void MatchRow::leaveEvent(QEvent*) { m_hover = false; m_overStar = false; update(); }

// ====================================================================== ProbRows

ProbRows::ProbRows(QWidget* parent) : QWidget(parent) { setMinimumHeight(150); }

void ProbRows::set(const QString labels[3], const sba::Probs3& model, bool hasMarket, const sba::Probs3& market) {
    for (int i = 0; i < 3; ++i) m_labels[i] = labels[i];
    m_model = model; m_market = market; m_hasMarket = hasMarket;
    update();
}

void ProbRows::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = width(), labelW = 170, rightW = m_hasMarket ? 150 : 70, barL = labelW + 8, barR = W - rightW;
    int best = 0;
    for (int i = 1; i < 3; ++i) if (m_model[i] > m_model[best]) best = i;
    p.setFont(UT::font(10, true)); p.setPen(UT::muted);
    p.drawText(QRect(W - 112, 0, 60, 20), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("MODEL"));
    if (m_hasMarket) p.drawText(QRect(W - 54, 0, 54, 20), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("MARKET"));
    for (int i = 0; i < 3; ++i) {
        const int y = 24 + i * 40;
        p.setFont(UT::font(14, i == best)); p.setPen(UT::black);
        QFontMetrics fm(p.font());
        p.drawText(QRect(0, y, labelW, 32), Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(m_labels[i], Qt::ElideRight, labelW));
        const QRectF track(barL, y + 11, std::max(10, barR - barL), 10);
        p.setPen(Qt::NoPen); p.setBrush(QColor("#ececec")); p.drawRoundedRect(track, 5, 5);
        p.setBrush(i == best ? UT::accent : DS::slate300);
        p.drawRoundedRect(QRectF(track.x(), track.y(), std::max(6.0, track.width() * m_model[i]), track.height()), 5, 5);
        p.setFont(UT::font(15, true)); p.setPen(UT::black);
        p.drawText(QRect(W - 112, y, 60, 32), Qt::AlignVCenter | Qt::AlignRight, pctText(m_model[i]));
        if (m_hasMarket) {
            p.setFont(UT::font(13)); p.setPen(UT::muted);
            p.drawText(QRect(W - 54, y, 54, 32), Qt::AlignVCenter | Qt::AlignRight, pctText(m_market[i]));
        }
    }
}

// ====================================================================== FormChips

FormChips::FormChips(QWidget* parent) : QWidget(parent) { setFixedHeight(26); }
void FormChips::setForm(const QString& f) { m_form = f; updateGeometry(); update(); }
QSize FormChips::sizeHint() const { return {std::max(1, int(m_form.size())) * 28, 26}; }

void FormChips::paintChips(QPainter& p, const QString& form, int x, int y, int size, int gap) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(UT::font(std::max(9, size * 5 / 9), true));
    for (int i = 0; i < form.size(); ++i) {
        const QRect r(x + i * (size + gap), y, size, size);
        const QChar c = form[i];
        p.setPen(Qt::NoPen);
        p.setBrush(c == 'W' ? UT::success : (c == 'D' ? DS::slate400 : UT::danger));
        p.drawRoundedRect(r, 3, 3);
        p.setPen(UT::white);
        p.drawText(r, Qt::AlignCenter, QString(c));
    }
    p.restore();
}

void FormChips::paintEvent(QPaintEvent*) {
    QPainter p(this);
    paintChips(p, m_form, 0, 1, 24, 4);
}

// ====================================================================== team badge

void paintTeamBadge(QPainter& p, const QPointF& c, double r, const QString& team, bool dark) {
    CrestProvider::instance().paint(p, QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), team, dark);
}

// ====================================================================== StandingsWidget

StandingsWidget::StandingsWidget(QWidget* parent) : QWidget(parent) {
    connect(&CrestProvider::instance(), &CrestProvider::crestsChanged, this, qOverload<>(&QWidget::update));
}

void StandingsWidget::setRows(const std::vector<TableRow>& r) {
    m_rows = r;
    setFixedHeight(32 + 42 * static_cast<int>(std::max<size_t>(1, r.size())));
    update();
}

void StandingsWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = width();
    const int formW = 5 * 16 + 4 * 3, colW = 36, gdW = 48, ptsW = 52;
    const int xForm = W - 16 - formW, xPts = xForm - 14 - ptsW, xGd = xPts - gdW, xL = xGd - colW, xD = xL - colW, xW = xD - colW, xP = xW - colW;
    p.fillRect(rect(), UT::white);
    // header
    p.fillRect(0, 0, W, 32, UT::band);
    p.setPen(UT::line); p.drawLine(0, 31, W, 31);
    p.setFont(UT::font(11, true)); p.setPen(UT::muted);
    p.drawText(QRect(0, 0, 48, 32), Qt::AlignCenter, "#");
    p.drawText(QRect(54, 0, 200, 32), Qt::AlignVCenter | Qt::AlignLeft, "TEAM");
    const struct { int x, w; const char* t; } heads[] = {{xP, colW, "P"}, {xW, colW, "W"}, {xD, colW, "D"}, {xL, colW, "L"}, {xGd, gdW, "GD"}, {xPts, ptsW, "PTS"}};
    for (const auto& h : heads) p.drawText(QRect(h.x, 0, h.w, 32), Qt::AlignCenter, h.t);
    p.drawText(QRect(xForm, 0, formW, 32), Qt::AlignCenter, "FORM");

    const int n = static_cast<int>(m_rows.size());
    for (int i = 0; i < n; ++i) {
        const auto& r = m_rows[static_cast<size_t>(i)];
        const int y = 32 + i * 42;
        p.setPen(UT::line); p.drawLine(0, y + 41, W, y + 41);
        if (i < 4) p.fillRect(0, y, 4, 41, UT::accent);
        else if (n >= 12 && i >= n - 3) p.fillRect(0, y, 4, 41, UT::danger);
        p.setFont(UT::font(13)); p.setPen(UT::black);
        p.drawText(QRect(0, y, 48, 41), Qt::AlignCenter, QString::number(i + 1));
        CrestProvider::instance().paint(p, QRectF(52, y + 6, 29, 29), r.team);
        p.setFont(UT::font(14, i < 4));
        QFontMetrics fm(p.font());
        p.drawText(QRect(90, y, xP - 100, 41), Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(r.team, Qt::ElideRight, xP - 100));
        p.setFont(UT::font(13)); p.setPen(UT::black);
        const QString cells[] = {QString::number(r.played), QString::number(r.won), QString::number(r.drawn), QString::number(r.lost),
                                 (r.gd() > 0 ? "+" : "") + QString::number(r.gd())};
        const int xs[] = {xP, xW, xD, xL, xGd}, ws[] = {colW, colW, colW, colW, gdW};
        for (int k = 0; k < 5; ++k) p.drawText(QRect(xs[k], y, ws[k], 41), Qt::AlignCenter, cells[k]);
        p.setFont(UT::font(14, true));
        p.drawText(QRect(xPts, y, ptsW, 41), Qt::AlignCenter, QString::number(r.points));
        FormChips::paintChips(p, r.form, xForm + (formW - (int(r.form.size()) * 16 + std::max(0, int(r.form.size()) - 1) * 3)), y + 13, 16, 3);
    }
}

// ====================================================================== PickRowWidget

PickRowWidget::PickRowWidget(QWidget* parent) : QWidget(parent) { setFixedHeight(64); }

void PickRowWidget::set(const QString& date, const QString& teams, const QString& pick, State s, const QString& result) {
    m_date = date; m_teams = teams; m_pick = pick; m_state = s; m_result = result;
    update();
}

void PickRowWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = width(), H = height();
    p.fillRect(rect(), UT::white);
    p.setPen(UT::line); p.drawLine(0, H - 1, W, H - 1);
    p.setFont(UT::font(12)); p.setPen(UT::muted);
    p.drawText(QRect(16, 0, 70, H), Qt::AlignVCenter | Qt::AlignLeft, m_date);
    const int chipW = 78, textR = W - chipW - 28;
    int tx = 92;
    if (!m_home.isEmpty()) {
        CrestProvider::instance().paint(p, QRectF(92, (H - 30) / 2.0, 30, 30), m_home);
        CrestProvider::instance().paint(p, QRectF(110, (H - 30) / 2.0, 30, 30), m_away);
        tx = 154;
    }
    p.setFont(UT::font(14, true)); p.setPen(UT::black);
    QFontMetrics fm(p.font());
    p.drawText(QRect(tx, 8, textR - tx, 20), Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(m_teams, Qt::ElideRight, textR - tx));
    p.setFont(UT::font(12)); p.setPen(UT::soft);
    QFontMetrics fm2(p.font());
    p.drawText(QRect(tx, 29, textR - tx, 18), Qt::AlignVCenter | Qt::AlignLeft, fm2.elidedText(m_pick, Qt::ElideRight, textR - tx));
    // state chip
    const QRect chip(W - 16 - chipW, (H - 24) / 2, chipW, 24);
    p.setFont(UT::font(11, true));
    if (m_state == Pending) { p.setPen(QPen(DS::slate300, 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(chip, 12, 12); p.setPen(UT::muted); }
    else { p.setPen(Qt::NoPen); p.setBrush(m_state == Won ? UT::success : UT::danger); p.drawRoundedRect(chip, 12, 12); p.setPen(UT::white); }
    p.drawText(chip, Qt::AlignCenter, m_state == Pending ? "PENDING" : (m_state == Won ? "CORRECT" : "MISSED"));
    if (!m_result.isEmpty()) {
        p.setFont(UT::font(12)); p.setPen(UT::muted);
        p.drawText(QRect(chip.left() - 150, 0, 140, H), Qt::AlignVCenter | Qt::AlignRight, m_result);
    }
}


// ====================================================================== pitch data + AI panel

PitchData pitchDataFor(UserDataHub* hub, const MatchItem& m) {
    PitchData d;
    const auto& f = m.fx;
    d.home = m.home(); d.away = m.away(); d.subtitle = m.competition(); d.demo = f.demo;
    if (f.isLive()) {
        d.kind = PitchData::Live; d.homeGoals = std::max(0, f.homeGoals); d.awayGoals = std::max(0, f.awayGoals);
        d.minute = f.minute; d.halfTime = f.status == "PAUSED";
    } else if (f.isFinished()) {
        d.kind = PitchData::Finished; d.homeGoals = f.homeGoals; d.awayGoals = f.awayGoals;
        if (const sba::HistMatch* h = hub->findHist(m)) {
            if (std::isfinite(h->stat[0][0]) && std::isfinite(h->stat[1][0]) && std::isfinite(h->stat[0][1]) && std::isfinite(h->stat[1][1])) {
                d.hasStats = true;
                d.shotsH = int(h->stat[0][0]); d.shotsA = int(h->stat[1][0]); d.sotH = int(h->stat[0][1]); d.sotA = int(h->stat[1][1]);
                if (std::isfinite(h->stat[0][2])) d.cornersH = int(h->stat[0][2]);
                if (std::isfinite(h->stat[1][2])) d.cornersA = int(h->stat[1][2]);
            }
        }
    } else {
        d.kind = PitchData::Upcoming;
        if (m.manual || f.kickoffEpoch <= 0) d.whenText = QLocale::c().toString(m.day, "ddd d MMM").toUpper();
        else {
            const QDateTime k = Fmt::local(f.kickoffEpoch);
            d.whenText = (k.date() == Fmt::today() ? QStringLiteral("TODAY ") : QLocale::c().toString(k.date(), "ddd d MMM ").toUpper()) + k.toString("HH:mm");
        }
        if (const PickInfo* pi = hub->pick(m); pi && pi->ok && pi->a) {
            const auto& p = pi->a->reasoning.model;
            d.hasProbs = true; d.pHome = p[0]; d.pDraw = p[1]; d.pAway = p[2];
        }
    }
    return d;
}

AiPanel::AiPanel(QWidget* parent) : QFrame(parent) {
    setMinimumHeight(200); setMaximumHeight(224);
    auto* h = new QHBoxLayout(this); h->setContentsMargins(8, 6, 22, 6); h->setSpacing(6);
    m_robot = new RobotWidget; m_robot->setFixedWidth(210);
    h->addWidget(m_robot);
    auto* col = new QVBoxLayout; col->setSpacing(4); col->setContentsMargins(0, 14, 0, 14);
    m_kicker = new QLabel; m_kicker->setStyleSheet("color:#A9BF86; font-size:11px; font-weight:700; letter-spacing:1px; background:transparent;");
    m_head = new QLabel; m_head->setWordWrap(true); m_head->setStyleSheet("color:#ffffff; font-size:24px; font-weight:700; background:transparent;");
    m_pct = new QLabel; m_pct->setStyleSheet("color:#CBD9B3; font-size:13px; font-weight:700; background:transparent;");
    m_note = new QLabel; m_note->setWordWrap(true); m_note->setStyleSheet("color:#aebbb4; font-size:13px; background:transparent;");
    col->addStretch(1); col->addWidget(m_kicker); col->addWidget(m_head); col->addWidget(m_pct); col->addWidget(m_note); col->addStretch(1);
    h->addLayout(col, 1);
    showIdle("AI analyst", "");
}

void AiPanel::setBreakdown(const QStringList& names, const std::array<double, 3>& p, int best) {
    m_bnames = names; m_bp = p; m_bbest = best; m_hasBreak = true; update();
}

void AiPanel::showIdle(const QString& headline, const QString& note) {
    m_hasBreak = false;
    m_robot->setState(RobotWidget::Idle); m_robot->setCaption(QString());
    m_kicker->setText("AI ANALYST"); m_head->setText(headline); m_pct->hide(); m_note->setText(note); m_note->setVisible(!note.isEmpty());
}
void AiPanel::showThinking(const QString& caption, const QString& note) {
    m_hasBreak = false;
    m_robot->setAccent(QColor("#A9BF86")); m_robot->setState(RobotWidget::Thinking); m_robot->setCaption(caption);
    m_kicker->setText("AI ANALYST"); m_head->setText("Crunching the numbers"); m_pct->hide(); m_note->setText(note); m_note->setVisible(!note.isEmpty());
}
void AiPanel::showResult(const QString& kicker, const QString& headline, const QString& pct, const QString& note, const QColor& accent) {
    m_robot->setAccent(accent); m_robot->setState(RobotWidget::Done); m_robot->setCaption(QString());
    m_kicker->setText(kicker.toUpper());
    m_kicker->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px; background:transparent;").arg(accent.lighter(130).name()));
    m_head->setText(headline);
    m_pct->setText(pct); m_pct->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700; background:transparent;").arg(accent.lighter(125).name())); m_pct->setVisible(!pct.isEmpty());
    m_note->setText(note); m_note->setVisible(!note.isEmpty());
}

void AiPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path; path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
    QLinearGradient g(0, 0, width(), height()); g.setColorAt(0, DS::stadiumTop); g.setColorAt(1, DS::paletteTurtleSkin);
    p.fillPath(path, g);
    p.setClipPath(path);
    p.setPen(Qt::NoPen);                                    // faint dot grid for a "tech" feel
    for (int x = 12; x < width(); x += 22) for (int y = 12; y < height(); y += 22) { p.setBrush(QColor(169, 191, 134, 20)); p.drawEllipse(QPointF(x, y), 1.1, 1.1); }
    p.setClipping(false);
    if (m_hasBreak && width() > 780) {                       // outcome probabilities, right-aligned
        const int colW = 128; const int x0 = width() - 22 - 3 * colW;
        for (int i = 0; i < 3; ++i) {
            const QRectF cell(x0 + i * colW, height() / 2.0 - 52, colW - 10, 104);
            const bool best = i == m_bbest;
            p.setPen(best ? QPen(DS::leaf300, 1.2) : QPen(QColor(255, 255, 255, 36), 1));
            p.setBrush(best ? QColor(169, 191, 134, 40) : QColor(255, 255, 255, 12));
            p.drawRoundedRect(cell, 14, 14);
            p.setFont(UT::font(30, true)); p.setPen(Qt::white);
            p.drawText(QRectF(cell.x(), cell.y() + 12, cell.width(), 48), Qt::AlignCenter, QString::number(int(std::round(m_bp[i] * 100))) + "%");
            p.setFont(UT::font(11, true)); p.setPen(best ? DS::leaf200 : DS::slate400);
            p.drawText(QRectF(cell.x() + 6, cell.y() + 62, cell.width() - 12, 34), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_bnames[i].toUpper());
        }
    }
    p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(169, 191, 134, 70), 1)); p.drawPath(path);
}
