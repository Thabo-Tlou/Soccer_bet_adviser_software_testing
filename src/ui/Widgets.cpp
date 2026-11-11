#include "Widgets.h"
#include "Theme.h"
#include "art/Crest.h"
#include "design/Components.h"
#include <QApplication>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------- helpers
QLabel* makeLabel(const QString& text, const char* role, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setProperty("role", role);
    l->setWordWrap(false);
    return l;
}

QPushButton* makeButton(const QString& text, bool primary, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    if (primary) b->setProperty("primary", true);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QFrame* makeDivider(QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setObjectName("Divider");
    f->setFixedHeight(1);
    return f;
}

// ---------------------------------------------------------------- Card
Card::Card(const QString& title, QWidget* parent) : QFrame(parent) {
    setObjectName("Card");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 18, 20, 20);
    outer->setSpacing(12);
    m_header = new QHBoxLayout;
    m_title = makeLabel(title, "h2");
    m_header->addWidget(m_title);
    m_header->addStretch();
    outer->addLayout(m_header);
    m_body = new QVBoxLayout;
    m_body->setSpacing(8);
    outer->addLayout(m_body, 1);
    m_title->setVisible(!title.isEmpty());
    applyElevation(this);
}

void Card::setElevated(bool on) {
    if (!on) setGraphicsEffect(nullptr);
    else if (!graphicsEffect()) applyElevation(this);
}

void Card::setTitle(const QString& t) { m_title->setText(t); m_title->setVisible(!t.isEmpty()); }

// ---------------------------------------------------------------- Badge
Badge::Badge(const QString& text, Kind k, QWidget* parent) : QLabel(text, parent) {
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setFixedHeight(22);
    setKind(k);
}

void Badge::setKind(Kind k) {
    QColor fg = DS::slate600, bg = DS::slate100;
    switch (k) {
        case Success: fg = DS::success700; bg = DS::success50; break;
        case Warning: fg = DS::warning700; bg = DS::warning50; break;
        case Danger:  fg = DS::danger700;  bg = DS::danger50;  break;
        case Info:    fg = DS::leaf700;  bg = DS::leaf50;  break;
        default: break;
    }
    setStyleSheet(QString("QLabel { color: %1; background: %2; border: 1px solid %3; border-radius: 11px; padding: 2px 10px; font-size: 8.5pt; font-weight: 600; }")
                      .arg(DS::hex(fg), DS::hex(bg), DS::hex(bg.darker(108))));
}

// ---------------------------------------------------------------- StatTile
StatTile::StatTile(const QString& caption, Icons::Name icon, QWidget* parent) : Card(QString(), parent) {
    body()->setSpacing(4);
    auto* top = new QHBoxLayout; top->setSpacing(8);
    top->addWidget(makeLabel(caption.toUpper(), "caption"), 1);
    auto* chip = new QLabel; chip->setFixedSize(34, 34);
    QPixmap pm(68, 68); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
    { QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(DS::leaf50); p.drawRoundedRect(QRectF(0, 0, 34, 34), 10, 10); Icons::paint(p, icon, QRectF(8, 8, 18, 18), DS::leaf600, 1.9); }
    chip->setPixmap(pm); chip->setScaledContents(true);
    top->addWidget(chip, 0, Qt::AlignTop);
    body()->addLayout(top);
    m_value = makeLabel("-", "kpi");
    m_sub = makeLabel("", "dim");
    body()->addWidget(m_value);
    body()->addWidget(m_sub);
}

void StatTile::set(const QString& value, const QString& sub) { m_value->setText(value); m_sub->setText(sub); }

// ---------------------------------------------------------------- Banner
Banner::Banner(QWidget* parent) : QFrame(parent) {
    setObjectName("Banner");
    auto* l = new QHBoxLayout(this);
    l->setContentsMargins(12, 8, 12, 8);
    l->setSpacing(10);
    m_dot = new QLabel(QString::fromUtf8("\xE2\x97\x8F"));
    m_text = new QLabel;
    m_text->setWordWrap(true);
    l->addWidget(m_dot, 0, Qt::AlignTop);
    l->addWidget(m_text, 1);
    hide();
}

void Banner::show(Kind k, const QString& text) {
    QColor fg = DS::leaf800, bg = DS::leaf50, bd = DS::leaf200, dot = DS::leaf600;
    switch (k) {
        case Warning: fg = DS::warning700; bg = DS::warning50; bd = QColor("#FDE68A"); dot = DS::warning600; break;
        case Error:   fg = DS::danger700;  bg = DS::danger50;  bd = QColor("#FECDD3"); dot = DS::danger600; break;
        case Success: fg = DS::success700; bg = DS::success50; bd = QColor("#A7F3D0"); dot = DS::success600; break;
        default: break;
    }
    setStyleSheet(QString("QFrame#Banner { background: %1; border: 1px solid %2; border-radius: 10px; } QLabel { background: transparent; color: %3; }")
                      .arg(DS::hex(bg), DS::hex(bd), DS::hex(fg)));
    m_dot->setStyleSheet(QString("color: %1; font-size: 11pt;").arg(DS::hex(dot)));
    m_text->setText(text);
    setVisible(!text.isEmpty());
}

// ---------------------------------------------------------------- ProbBar
ProbBar::ProbBar(QWidget* parent) : QWidget(parent) { setMinimumHeight(24); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); }

void ProbBar::setProbs(const sba::Probs3& p, bool valid) { m_p = p; m_valid = valid; update(); }
void ProbBar::setMarket(const sba::Probs3& m, bool valid) { m_m = m; m_hasMarket = valid; update(); }

void ProbBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, m_hasMarket ? -6.5 : -0.5);
    QPainterPath clip; clip.addRoundedRect(r, 5, 5);
    if (!m_valid) {
        p.setPen(Theme::border); p.setBrush(Theme::bg3); p.drawPath(clip);
        p.setPen(Theme::dim); p.drawText(r, Qt::AlignCenter, QString::fromUtf8("\xE2\x80\x93"));
        return;
    }
    p.setClipPath(clip);
    const QColor cols[3] = {Theme::homeColor, Theme::drawColor, Theme::awayColor};
    double x = r.left();
    p.setFont(Theme::uiFont(8, QFont::DemiBold));
    for (int i = 0; i < 3; ++i) {
        const double w = r.width() * m_p[i];
        p.fillRect(QRectF(x, r.top(), w, r.height()), cols[i]);
        if (w > 34) {
            p.setPen(Qt::white);
            p.drawText(QRectF(x, r.top(), w, r.height()), Qt::AlignCenter, QString("%1%").arg(m_p[i] * 100, 0, 'f', 0));
        }
        x += w;
    }
    p.setClipping(false);
    if (m_hasMarket) {
        p.setPen(QPen(DS::slate900, 2));
        double c = 0;
        for (int i = 0; i < 2; ++i) {
            c += m_m[i];
            const double mx = r.left() + r.width() * c;
            p.drawLine(QPointF(mx, r.bottom() + 1), QPointF(mx, r.bottom() + 5));
        }
    }
}

// ---------------------------------------------------------------- OutcomeBar
OutcomeBar::OutcomeBar(const QString& label, const QColor& color, QWidget* parent) : QWidget(parent), m_label(label), m_color(color) {
    setMinimumHeight(34);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void OutcomeBar::setValues(double model, double market) { m_model = model; m_market = market; update(); }

void OutcomeBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(Theme::uiFont(9, QFont::DemiBold));
    const int labelW = 78;
    p.setPen(Theme::muted);
    p.drawText(QRect(0, 0, labelW, height()), Qt::AlignVCenter | Qt::AlignLeft, m_label);
    const QRectF track(labelW, height() / 2.0 - 7, width() - labelW - 60, 14);
    p.setPen(Qt::NoPen); p.setBrush(Theme::bg3); p.drawRoundedRect(track, 7, 7);
    const double v = std::clamp(m_model, 0.0, 1.0);
    if (v > 0) { p.setBrush(m_color); p.drawRoundedRect(QRectF(track.left(), track.top(), std::max(14.0, track.width() * v), track.height()), 7, 7); }
    if (std::isfinite(m_market)) {
        const double mx = track.left() + track.width() * std::clamp(m_market, 0.0, 1.0);
        p.setPen(QPen(Theme::text, 2)); p.drawLine(QPointF(mx, track.top() - 4), QPointF(mx, track.bottom() + 4));
    }
    p.setPen(Theme::text);
    p.drawText(QRect(width() - 56, 0, 56, height()), Qt::AlignVCenter | Qt::AlignRight, QString("%1%").arg(m_model * 100, 0, 'f', 1));
}

// ---------------------------------------------------------------- CalibrationChart
CalibrationChart::CalibrationChart(QWidget* parent) : QWidget(parent) { setMinimumSize(300, 260); }
void CalibrationChart::setBins(const std::vector<sba::CalibrationBin>& b) { m_bins = b; update(); }

void CalibrationChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot(44, 12, width() - 60, height() - 46);
    p.setPen(Theme::border);
    for (int i = 0; i <= 5; ++i) {
        const double t = i / 5.0;
        p.drawLine(QPointF(plot.left(), plot.bottom() - plot.height() * t), QPointF(plot.right(), plot.bottom() - plot.height() * t));
        p.drawLine(QPointF(plot.left() + plot.width() * t, plot.top()), QPointF(plot.left() + plot.width() * t, plot.bottom()));
    }
    p.setFont(Theme::uiFont(8)); p.setPen(Theme::dim);
    for (int i = 0; i <= 5; ++i) {
        const double t = i / 5.0;
        p.drawText(QRectF(0, plot.bottom() - plot.height() * t - 8, 38, 16), Qt::AlignRight | Qt::AlignVCenter, QString("%1%").arg(i * 20));
        p.drawText(QRectF(plot.left() + plot.width() * t - 20, plot.bottom() + 4, 40, 14), Qt::AlignCenter, QString("%1%").arg(i * 20));
    }
    p.drawText(QRectF(plot.left(), height() - 18, plot.width(), 16), Qt::AlignCenter, "predicted probability");
    p.setPen(QPen(Theme::dim, 1, Qt::DashLine));
    p.drawLine(plot.bottomLeft(), plot.topRight());   // perfect calibration

    int maxCount = 1;
    for (const auto& b : m_bins) maxCount = std::max(maxCount, b.count);
    QPolygonF line;
    for (const auto& b : m_bins) {
        if (b.count < 5) continue;
        line << QPointF(plot.left() + plot.width() * b.meanPredicted, plot.bottom() - plot.height() * b.observed);
    }
    p.setPen(QPen(Theme::primary, 2)); p.drawPolyline(line);
    for (const auto& b : m_bins) {
        if (b.count < 5) continue;
        const QPointF c(plot.left() + plot.width() * b.meanPredicted, plot.bottom() - plot.height() * b.observed);
        const double r = 3.5 + 6.0 * std::sqrt(static_cast<double>(b.count) / maxCount);
        p.setPen(QPen(Theme::bg2, 1.5)); p.setBrush(Theme::tint); p.drawEllipse(c, r, r);
    }
    p.save(); p.translate(12, plot.center().y()); p.rotate(-90); p.setPen(Theme::dim);
    p.drawText(QRectF(-80, -10, 160, 14), Qt::AlignCenter, "observed frequency"); p.restore();
}

// ---------------------------------------------------------------- ConfusionWidget
ConfusionWidget::ConfusionWidget(QWidget* parent) : QWidget(parent) { setMinimumSize(280, 220); }
void ConfusionWidget::setMatrix(const sba::ConfusionMatrix& m) { m_m = m; update(); }

void ConfusionWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const char* names[3] = {"Home", "Draw", "Away"};
    const double left = 96, top = 28, cw = (width() - left - 8) / 3.0, ch = (height() - top - 26) / 3.0;
    p.setFont(Theme::uiFont(8, QFont::DemiBold)); p.setPen(Theme::muted);
    for (int i = 0; i < 3; ++i) {
        p.drawText(QRectF(left + i * cw, 4, cw, 20), Qt::AlignCenter, QString("pred %1").arg(names[i]));
        p.drawText(QRectF(0, top + i * ch, left - 6, ch), Qt::AlignRight | Qt::AlignVCenter, QString("actual %1").arg(names[i]));
    }
    for (int a = 0; a < 3; ++a) {
        int rowSum = 0; for (int q = 0; q < 3; ++q) rowSum += m_m.m[a][q];
        for (int q = 0; q < 3; ++q) {
            const double share = rowSum ? static_cast<double>(m_m.m[a][q]) / rowSum : 0.0;
            QColor c = (a == q) ? Theme::primary : Theme::danger;
            c.setAlphaF(0.10 + 0.75 * share);
            const QRectF cell(left + q * cw + 2, top + a * ch + 2, cw - 4, ch - 4);
            p.setPen(Qt::NoPen); p.setBrush(c); p.drawRoundedRect(cell, 6, 6);
            p.setPen(Theme::text); p.setFont(Theme::uiFont(11, QFont::DemiBold));
            p.drawText(cell, Qt::AlignCenter, QString::number(m_m.m[a][q]));
            p.setFont(Theme::uiFont(8)); p.setPen(Theme::muted);
            p.drawText(cell.adjusted(0, 0, 0, -4), Qt::AlignHCenter | Qt::AlignBottom, QString("%1%").arg(share * 100, 0, 'f', 0));
        }
    }
}

// ---------------------------------------------------------------- SideNavButton
SideNavButton::SideNavButton(Icons::Name icon, const QString& label, const QString& tip, QWidget* parent) : QPushButton(parent), m_icon(icon), m_label(label) {
    setCheckable(true);
    setToolTip(tip);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(42);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFlat(true);
    setFocusPolicy(Qt::NoFocus);               // reachable with Ctrl+1..6, so no stray focus ring after a click
}

void SideNavButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool on = isChecked(), hov = underMouse();
    const QRectF pill = QRectF(rect()).adjusted(0, 1, 0, -1);
    if (on) { p.setPen(Qt::NoPen); p.setBrush(DS::leaf50); p.drawRoundedRect(pill, 10, 10); }
    else if (hov) { p.setPen(Qt::NoPen); p.setBrush(DS::slate100); p.drawRoundedRect(pill, 10, 10); }
    if (hasFocus() && !on) { p.setPen(QPen(DS::focusRing, 1.5)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(pill.adjusted(0.75, 0.75, -0.75, -0.75), 10, 10); }
    const QColor c = on ? DS::leaf700 : (hov ? DS::slate900 : DS::slate600);
    Icons::paint(p, m_icon, QRectF(14, (height() - 20) / 2.0, 20, 20), c, on ? 2.0 : 1.8);
    QFont f = font(); f.setPixelSize(14); f.setWeight(on ? QFont::DemiBold : QFont::Medium); p.setFont(f);
    p.setPen(c);
    p.drawText(QRectF(46, 0, width() - 56, height()), Qt::AlignVCenter | Qt::AlignLeft, m_label);
    if (on) { p.setPen(Qt::NoPen); p.setBrush(DS::leaf600); p.drawRoundedRect(QRectF(0, height() / 2.0 - 9, 3.5, 18), 2, 2); }
}

// ---------------------------------------------------------------- MatchCellDelegate / TeamVs
void MatchCellDelegate::paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const {
    QStyleOptionViewItem o = option;
    initStyleOption(&o, idx);
    const QStringList teams = idx.data(Qt::UserRole).toStringList();
    if (teams.size() < 2) { QStyledItemDelegate::paint(p, option, idx); return; }
    o.text.clear(); o.icon = QIcon();
    const QWidget* w = option.widget;
    (w ? w->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &o, p, w);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRect r = option.rect;
    const int cs = 24, gap = 8;
    int x = r.left() + 8;
    QFont f = option.font; p->setFont(f);
    const QFontMetrics fm(f);
    const bool sel = option.state & QStyle::State_Selected;
    const QColor fg = sel ? Theme::tint : Theme::text;
    const int avail = r.width() - 16;
    const int vsW = fm.horizontalAdvance("vs") + 16;
    const int space = std::max(60, avail - 2 * (cs + gap) - vsW);
    const int w0 = fm.horizontalAdvance(teams[0]) + 2, w1 = fm.horizontalAdvance(teams[1]) + 2;
    const int n0 = std::min(w0, std::max(space / 2, space - w1)), n1 = std::min(w1, space - n0);
    for (int k = 0; k < 2; ++k) {
        const int nameW = k == 0 ? n0 : n1;
        CrestProvider::instance().paint(*p, QRectF(x, r.center().y() - cs / 2.0, cs, cs), teams[k], false);
        x += cs + gap;
        p->setPen(fg);
        p->drawText(QRect(x, r.top(), nameW, r.height()), Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(teams[k], Qt::ElideRight, nameW));
        x += nameW;
        if (k == 0) { p->setPen(Theme::dim); p->drawText(QRect(x, r.top(), vsW, r.height()), Qt::AlignCenter, "vs"); x += vsW; }
    }
    p->restore();
}

QSize MatchCellDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    QSize s = QStyledItemDelegate::sizeHint(opt, idx);
    s.setHeight(std::max(s.height(), 40)); s.setWidth(std::max(s.width(), 360));
    return s;
}

TeamVs::TeamVs(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(96);
    connect(&CrestProvider::instance(), &CrestProvider::crestsChanged, this, qOverload<>(&QWidget::update));
}
void TeamVs::setTeams(const QString& h, const QString& a) { m_home = h; m_away = a; update(); }
void TeamVs::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double W = width(), cs = 50;
    if (m_home.isEmpty() && m_away.isEmpty()) return;
    for (int k = 0; k < 2; ++k) {
        const double cx = k == 0 ? W * 0.22 : W * 0.78;
        const QString team = k == 0 ? m_home : m_away;
        CrestProvider::instance().paint(p, QRectF(cx - cs / 2, 8, cs, cs), team, false);
        p.setFont(Theme::uiFont(9, QFont::DemiBold)); p.setPen(Theme::text);
        p.drawText(QRectF(cx - W * 0.22, 62, W * 0.44, 28), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, team);
    }
    p.setFont(Theme::uiFont(10, QFont::DemiBold)); p.setPen(Theme::dim);
    p.drawText(QRectF(W / 2 - 20, 8, 40, cs), Qt::AlignCenter, "vs");
}
