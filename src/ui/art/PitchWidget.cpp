#include "PitchWidget.h"

#include "AnimClock.h"
#include "Crest.h"
#include "../design/Tokens.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {
double colourDistance(const QColor& a, const QColor& b) {
    const double dr = a.redF() - b.redF(), dg = a.greenF() - b.greenF(), db = a.blueF() - b.blueF();
    return std::sqrt(dr * dr + dg * dg + db * db);
}
// Colours that stay visible on grass; near-white / near-black kits are nudged.
QColor teamTint(const QString& team, bool second, const QColor& avoid) {
    ClubStyle s = CrestProvider::styleFor(team);
    QColor c = s.primary;
    const double lum = 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF();
    if (lum > 0.85 || lum < 0.1) c = s.secondary;
    if (second && colourDistance(c, avoid) < 0.35) c = (lum > 0.5) ? QColor("#1D4ED8") : QColor("#F59E0B");
    return c;
}
void pill(QPainter& p, const QRectF& r, const QColor& bg) {
    p.setPen(Qt::NoPen); p.setBrush(bg); p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
}
}

PitchWidget::PitchWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    connect(&AnimClock::instance(), &AnimClock::tick, this, [this] { if (isVisible()) update(); });
    connect(&CrestProvider::instance(), &CrestProvider::crestsChanged, this, qOverload<>(&QWidget::update));
}

void PitchWidget::setClickable(bool on) { m_clickable = on; setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor); }

void PitchWidget::setData(const PitchData& d) {
    if (d == m_d) return;
    const bool probsChanged = d.hasProbs && (!m_d.hasProbs || d.pHome != m_d.pHome || d.pDraw != m_d.pDraw || d.pAway != m_d.pAway || d.home != m_d.home);
    if (probsChanged) { m_fromH = 1 / 3.0; m_fromD = 1 / 3.0; m_changeT = AnimClock::instance().seconds(); }
    m_d = d;
    update();
}

void PitchWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (m_clickable && e->button() == Qt::LeftButton && rect().contains(e->pos())) emit clicked();
    QWidget::mouseReleaseEvent(e);
}

void PitchWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const double t = AnimClock::instance().seconds();
    const QRectF full = QRectF(rect());
    QPainterPath clip; clip.addRoundedRect(full, 14, 14);
    p.setClipPath(clip);

    // ---- stadium background ------------------------------------------------------------------
    QLinearGradient bg(0, 0, 0, full.height());
    bg.setColorAt(0, DS::stadiumTop); bg.setColorAt(1, DS::stadiumBottom);
    p.fillRect(full, bg);

    const QColor cHome = teamTint(m_d.home, false, QColor());
    const QColor cAway = teamTint(m_d.away, true, cHome);

    // pitch rectangle (flexible aspect so the banner always fills well)
    const double sideW = std::clamp((full.width() - full.height() * 1.8) / 2.0, 0.0, 230.0);
    const bool sidePanels = sideW >= 92;
    const double padY = std::max(14.0, full.height() * 0.09);
    QRectF pr(full.left() + sideW + (sidePanels ? 0 : 14), full.top() + padY, full.width() - 2 * sideW - (sidePanels ? 0 : 28), full.height() - 2 * padY);
    pr.setHeight(std::min(pr.height(), pr.width() / 1.35));
    pr.moveTop(full.center().y() - pr.height() / 2);
    const double sx = pr.width() / 105.0, sy = pr.height() / 68.0;

    // crowd: twinkling dots in both clubs' colours behind the pitch
    {
        p.setPen(Qt::NoPen);
        quint32 seed = qHash(m_d.home + m_d.away) | 1u;
        auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / double(1 << 24); };
        const int n = int(full.width() * full.height() / 420.0);
        const bool live = m_d.kind == PitchData::Live;
        for (int i = 0; i < n; ++i) {
            const double x = full.left() + rnd() * full.width(), y = full.top() + rnd() * full.height(), ph = rnd() * 6.28;
            if (pr.adjusted(-6, -6, 6, 6).contains(QPointF(x, y))) continue;
            const bool leftHalf = x < full.center().x();
            QColor c = leftHalf ? cHome : cAway;
            const double tw = live ? 0.5 + 0.5 * std::sin(t * 2.4 + ph) : 0.55 + 0.25 * std::sin(t * 0.8 + ph);
            c.setAlpha(int(28 + 70 * tw));
            p.setBrush(c);
            p.drawEllipse(QPointF(x, y), 1.5, 1.5);
        }
    }

    // ---- grass ----------------------------------------------------------------------------------
    QPainterPath pitchPath; pitchPath.addRoundedRect(pr, 8, 8);
    p.save();
    p.setClipPath(pitchPath);
    const int bands = 14;
    for (int i = 0; i < bands; ++i) {
        QRectF b(pr.left() + pr.width() * i / bands, pr.top(), pr.width() / bands + 1, pr.height());
        p.fillRect(b, i % 2 ? DS::leaf600 : QColor("#58783A"));
    }
    QRadialGradient vig(pr.center(), pr.width() * 0.62);
    vig.setColorAt(0, QColor(255, 255, 255, 22)); vig.setColorAt(1, QColor(0, 0, 0, 120));
    p.fillRect(pr, vig);

    // markings
    QPen line(QColor(255, 255, 255, 200), std::max(1.3, sy * 0.28));
    line.setCapStyle(Qt::RoundCap);
    p.setPen(line); p.setBrush(Qt::NoBrush);
    p.drawRect(pr);
    p.drawLine(QPointF(pr.center().x(), pr.top()), QPointF(pr.center().x(), pr.bottom()));
    p.drawEllipse(pr.center(), 9.15 * sx, 9.15 * sy);
    p.setBrush(QColor(255, 255, 255, 200)); p.drawEllipse(pr.center(), 0.5 * sx + 1, 0.5 * sy + 1); p.setBrush(Qt::NoBrush);
    for (int side = 0; side < 2; ++side) {
        const double dir = side == 0 ? 1 : -1;
        const double gx = side == 0 ? pr.left() : pr.right();
        auto box = [&](double depth, double width) {
            p.drawRect(QRectF(side == 0 ? gx : gx - depth * sx, pr.center().y() - width / 2 * sy, depth * sx, width * sy));
        };
        box(16.5, 40.32); box(5.5, 18.32);
        const QPointF spot(gx + dir * 11 * sx, pr.center().y());
        p.setBrush(QColor(255, 255, 255, 200)); p.drawEllipse(spot, 0.45 * sx + 1, 0.45 * sy + 1); p.setBrush(Qt::NoBrush);
        const QRectF arc(spot.x() - 9.15 * sx, spot.y() - 9.15 * sy, 18.3 * sx, 18.3 * sy);
        const int span = int(106.2 * 16);
        p.drawArc(arc, (side == 0 ? int(-53.1 * 16) : int((180 - 53.1) * 16)), span);
        p.setPen(QPen(QColor(255, 255, 255, 230), line.widthF() * 1.6));
        p.drawRect(QRectF(side == 0 ? gx - 2 * sx : gx, pr.center().y() - 3.66 * sy, 2 * sx, 7.32 * sy));   // goal
        p.setPen(line);
    }
    p.restore();

    // ---- state overlays -------------------------------------------------------------------------
    p.save();
    p.setClipPath(pitchPath);
    const QFont base = font();
    auto setPx = [&](bool bold, double px) { QFont f = base; f.setBold(bold); f.setPixelSize(std::max(8, int(px))); p.setFont(f); };

    if (m_d.kind == PitchData::Upcoming && m_d.hasProbs) {
        const double a = easeOutCubic((t - m_changeT) / 0.9);
        const double ph = m_fromH + (m_d.pHome - m_fromH) * a, pd = m_fromD + (m_d.pDraw - m_fromD) * a, pa = 1 - ph - pd;
        const double x1 = pr.left() + pr.width() * ph, x2 = x1 + pr.width() * pd;
        QColor ch = DS::steel600; ch.setAlpha(175); QColor ca = DS::paletteNightmare; ca.setAlpha(150);
        p.fillRect(QRectF(pr.left(), pr.top(), x1 - pr.left(), pr.height()), ch);
        p.fillRect(QRectF(x1, pr.top(), x2 - x1, pr.height()), QColor(255, 255, 255, 28));
        p.fillRect(QRectF(x2, pr.top(), pr.right() - x2, pr.height()), ca);
        p.setPen(QPen(QColor(255, 255, 255, 190), 1.4, Qt::DashLine));
        p.drawLine(QPointF(x1, pr.top()), QPointF(x1, pr.bottom()));
        p.drawLine(QPointF(x2, pr.top()), QPointF(x2, pr.bottom()));
        auto zoneLabel = [&](double x0, double x1b, double v, const QString& cap) {
            if (x1b - x0 < 54) return;
            const QPointF c((x0 + x1b) / 2, pr.center().y() + pr.height() * 0.2);
            setPx(true, pr.height() * 0.17); p.setPen(Qt::white);
            p.drawText(QRectF(c.x() - 70, c.y() - pr.height() * 0.1, 140, pr.height() * 0.2), Qt::AlignCenter, QString::number(int(std::round(v * 100))) + "%");
            setPx(false, pr.height() * 0.07); p.setPen(QColor(255, 255, 255, 190));
            p.drawText(QRectF(c.x() - 70, c.y() + pr.height() * 0.08, 140, pr.height() * 0.1), Qt::AlignCenter, cap);
        };
        zoneLabel(pr.left(), x1, ph, "HOME WIN"); zoneLabel(x1, x2, pd, "DRAW"); zoneLabel(x2, pr.right(), pa, "AWAY WIN");
    }

    if (m_d.kind == PitchData::Live) {      // expanding pulse from the centre spot
        const double ph = std::fmod(t, 2.6) / 2.6;
        QColor pc(255, 255, 255, int(120 * (1 - ph)));
        p.setPen(QPen(pc, 2)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(pr.center(), (9.15 + 30 * ph) * sx, (9.15 + 30 * ph) * sy);
    }

    if (m_d.kind == PitchData::Finished && m_d.hasStats) {
        auto cluster = [&](int shots, int sot, const QColor& c, bool rightSide) {
            const int cols = 5; const double step = std::max(10.0, sy * 2.9), r = step * 0.38;
            const int rows = (std::max(shots, 1) + cols - 1) / cols;
            const double w = (cols - 1) * step, h = (rows - 1) * step;
            const double cx = rightSide ? pr.right() - pr.width() * 0.17 : pr.left() + pr.width() * 0.17;
            const double cy = pr.center().y() - pr.height() * 0.12;
            for (int i = 0; i < shots; ++i) {
                const double x = cx - w / 2 + (i % cols) * step, y = cy - h / 2 + (i / cols) * step;
                if (i < sot) { p.setPen(QPen(QColor(0, 0, 0, 170), 1.4)); p.setBrush(c.lighter(115)); }
                else         { p.setPen(QPen(QColor(255, 255, 255, 200), 1.3)); p.setBrush(QColor(0, 0, 0, 70)); }
                p.drawEllipse(QPointF(x, y), r, r);
            }
            setPx(true, std::max(9.5, pr.height() * 0.07));
            const QString txt = QString("%1 shots · %2 on target").arg(shots).arg(sot);
            const double tw = QFontMetricsF(p.font()).horizontalAdvance(txt) + 18;
            const QRectF lab(cx - tw / 2, cy + h / 2 + r + 7, tw, std::max(18.0, pr.height() * 0.1));
            pill(p, lab, QColor(4, 10, 7, 170)); p.setPen(Qt::white); p.drawText(lab, Qt::AlignCenter, txt);
        };
        cluster(m_d.shotsH, m_d.sotH, cHome, false);    // each team's shots sit on its own side, next to its crest
        cluster(m_d.shotsA, m_d.sotA, cAway, true);
    }
    p.restore();

    // light sweep across the grass
    {
        const double ph = std::fmod(t / 8.0, 1.0);
        const double x = pr.left() - pr.width() * 0.3 + ph * pr.width() * 1.6;
        QLinearGradient sw(x, pr.top(), x + pr.width() * 0.18, pr.bottom());
        sw.setColorAt(0, QColor(255, 255, 255, 0)); sw.setColorAt(0.5, QColor(255, 255, 255, 20)); sw.setColorAt(1, QColor(255, 255, 255, 0));
        p.save(); p.setClipPath(pitchPath); p.fillRect(pr, sw); p.restore();
    }

    // ---- crests + names --------------------------------------------------------------------------
    CrestProvider& cp = CrestProvider::instance();
    auto nameText = [&](const QString& n, const QRectF& r, Qt::Alignment al) {
        setPx(true, std::clamp(full.height() * 0.075, 11.0, 17.0)); p.setPen(Qt::white); p.drawText(r, al | Qt::AlignVCenter | Qt::TextWordWrap, n);
    };
    if (sidePanels) {
        const double cs = std::min(sideW * 0.58, full.height() * 0.42);
        for (int s = 0; s < 2; ++s) {
            const QString team = s == 0 ? m_d.home : m_d.away;
            const double cx = s == 0 ? full.left() + sideW / 2 : full.right() - sideW / 2;
            const double bob = std::sin(t * 1.3 + s * 1.7) * 2.0;
            cp.paint(p, QRectF(cx - cs / 2, full.center().y() - cs / 2 - 16 + bob, cs, cs), team, true);
            nameText(team, QRectF(cx - sideW / 2 + 8, full.center().y() + cs / 2 - 8, sideW - 16, 40), Qt::AlignHCenter | Qt::AlignTop);
        }
    } else {
        const double cs = std::clamp(pr.height() * 0.3, 26.0, 52.0);
        cp.paint(p, QRectF(pr.left() + 10, pr.center().y() - cs / 2, cs, cs), m_d.home, true);
        cp.paint(p, QRectF(pr.right() - 10 - cs, pr.center().y() - cs / 2, cs, cs), m_d.away, true);
    }

    // ---- scoreboard pill --------------------------------------------------------------------------
    const bool hasScore = m_d.homeGoals >= 0 && m_d.awayGoals >= 0 && m_d.kind != PitchData::Upcoming;
    const double ph = std::clamp(full.height() * 0.2, 38.0, 58.0);
    const QString sbText = hasScore ? QString("%1 – %2").arg(m_d.homeGoals).arg(m_d.awayGoals) : (m_d.whenText.isEmpty() ? QStringLiteral("VS") : m_d.whenText);
    setPx(true, hasScore ? ph * 0.56 : ph * 0.4);
    const double sbW = std::max(hasScore ? 120.0 : 110.0, QFontMetricsF(p.font()).horizontalAdvance(sbText) + 44);
    const QRectF sb(full.center().x() - sbW / 2, pr.top() + 10, sbW, ph);
    pill(p, sb, QColor(6, 10, 8, 178));
    p.setPen(QPen(QColor(255, 255, 255, 40), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(sb, ph / 2, ph / 2);
    p.setPen(hasScore ? QColor(Qt::white) : QColor(255, 255, 255, 230));
    p.drawText(sb, Qt::AlignCenter, sbText);

    // status chip under the scoreboard
    {
        QString txt; QColor bgc(255, 255, 255, 40), fg = Qt::white;
        if (m_d.kind == PitchData::Live) {
            txt = m_d.halfTime ? "HALF TIME" : (m_d.minute >= 0 ? QString("LIVE  %1'").arg(m_d.minute) : QString("LIVE"));
            bgc = QColor(220, 38, 38, 235);
        } else if (m_d.kind == PitchData::Finished) txt = "FULL TIME";
        if (m_d.kind == PitchData::Upcoming && m_d.whenText.isEmpty()) txt.clear();
        if (!txt.isEmpty()) {
            setPx(true, 10.5);
            const double w = QFontMetricsF(p.font()).horizontalAdvance(txt) + 26;
            const QRectF chip(full.center().x() - w / 2, sb.bottom() + 6, w, 20);
            pill(p, chip, bgc);
            if (m_d.kind == PitchData::Live) {
                const double pulse = 0.55 + 0.45 * std::sin(t * 5.0);
                p.setBrush(QColor(255, 255, 255, int(120 + 135 * pulse))); p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(chip.left() + 11, chip.center().y()), 3.2, 3.2);
            }
            p.setPen(fg); p.drawText(chip.adjusted(m_d.kind == PitchData::Live ? 10 : 0, 0, 0, 0), Qt::AlignCenter, txt);
        }
    }

    // live minute tracker along the bottom touchline
    if (m_d.kind == PitchData::Live && m_d.minute >= 0) {
        const QRectF tr(pr.left() + 18, pr.bottom() - 16, pr.width() - 36, 4);
        p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 110)); p.drawRoundedRect(tr, 2, 2);
        const double f = std::clamp(m_d.minute / 90.0, 0.0, 1.0);
        QLinearGradient g(tr.left(), 0, tr.right(), 0); g.setColorAt(0, DS::leaf400); g.setColorAt(1, DS::leaf300);
        p.setBrush(g); p.drawRoundedRect(QRectF(tr.left(), tr.top(), tr.width() * f, tr.height()), 2, 2);
        p.setBrush(QColor(255, 255, 255, 140)); p.drawRect(QRectF(tr.left() + tr.width() * 0.5 - 0.5, tr.top() - 3, 1, tr.height() + 6));
        const double pulse = 0.5 + 0.5 * std::sin(t * 4.0);
        const QPointF head(tr.left() + tr.width() * f, tr.center().y());
        p.setBrush(QColor(255, 255, 255, int(40 + 50 * pulse))); p.drawEllipse(head, 8 + 3 * pulse, 8 + 3 * pulse);
        p.setBrush(Qt::white); p.drawEllipse(head, 4, 4);
    }

    // corner chips
    if (!m_d.subtitle.isEmpty()) {
        setPx(true, 10.5);
        const double w = QFontMetricsF(p.font()).horizontalAdvance(m_d.subtitle.toUpper()) + 22;
        const QRectF chip(full.left() + 14, full.top() + 12, w, 22);
        pill(p, chip, QColor(255, 255, 255, 26)); p.setPen(QColor(255, 255, 255, 215)); p.drawText(chip, Qt::AlignCenter, m_d.subtitle.toUpper());
    }
    if (m_d.demo) {
        setPx(true, 10.5);
        const QRectF chip(full.right() - 72, full.top() + 12, 58, 22);
        pill(p, chip, QColor("#F59E0B")); p.setPen(QColor("#1A1100")); p.drawText(chip, Qt::AlignCenter, "DEMO");
    }

    // edge highlight
    p.setClipping(false); p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawRoundedRect(full.adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);
}
