#include "Icons.h"

#include <QPainterPath>
#include <cmath>

namespace Icons {
static const double kPi = 3.14159265358979323846;

void paint(QPainter& p, Name n, const QRectF& r, const QColor& color, double stroke) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(r.topLeft());
    p.scale(r.width() / 24.0, r.height() / 24.0);
    QPen pen(color, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    auto L = [&](double x1, double y1, double x2, double y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    auto PL = [&](std::initializer_list<QPointF> pts) { p.drawPolyline(QPolygonF(QList<QPointF>(pts))); };
    auto PG = [&](std::initializer_list<QPointF> pts) { p.drawPolygon(QPolygonF(QList<QPointF>(pts))); };
    auto C = [&](double cx, double cy, double rad) { p.drawEllipse(QPointF(cx, cy), rad, rad); };
    auto RR = [&](double x, double y, double w, double h, double rad) { p.drawRoundedRect(QRectF(x, y, w, h), rad, rad); };
    auto dot = [&](double cx, double cy, double rad) { p.setPen(Qt::NoPen); p.setBrush(color); p.drawEllipse(QPointF(cx, cy), rad, rad); p.setPen(pen); p.setBrush(Qt::NoBrush); };

    switch (n) {
    case Home: PL({{4, 11}, {12, 4}, {20, 11}}); PL({{6, 9.5}, {6, 20}, {18, 20}, {18, 9.5}}); PL({{10, 20}, {10, 14}, {14, 14}, {14, 20}}); break;
    case Dashboard: RR(3, 3, 7.5, 9.5, 2); RR(13.5, 3, 7.5, 5.5, 2); RR(3, 15.5, 7.5, 5.5, 2); RR(13.5, 11.5, 7.5, 9.5, 2); break;
    case Matches: RR(2.5, 5, 19, 14, 2.2); L(12, 5, 12, 19); C(12, 12, 3); PL({{2.5, 9.2}, {5.5, 9.2}, {5.5, 14.8}, {2.5, 14.8}}); PL({{21.5, 9.2}, {18.5, 9.2}, {18.5, 14.8}, {21.5, 14.8}}); break;
    case Analysis: PL({{4, 4}, {4, 20}, {20, 20}}); PL({{7.5, 15}, {11, 10.5}, {14.5, 13.5}, {19.5, 6.5}}); dot(19.5, 6.5, 1.3); break;
    case Performance: RR(4, 12, 4, 8, 1.2); RR(10, 7, 4, 13, 1.2); RR(16, 3, 4, 17, 1.2); break;
    case Search: C(10.8, 10.8, 6.6); L(15.8, 15.8, 21, 21); break;
    case Settings:
        L(4, 7, 7, 7); L(12, 7, 20, 7); C(9.5, 7, 2.5);
        L(4, 12, 12.5, 12); L(17.5, 12, 20, 12); C(15, 12, 2.5);
        L(4, 17, 5.5, 17); L(10.5, 17, 20, 17); C(8, 17, 2.5); break;
    case Table: RR(3, 4, 18, 16, 2.4); L(3, 9.5, 21, 9.5); L(3, 14.7, 21, 14.7); L(9, 9.5, 9, 20); break;
    case Picks: PG({{6.5, 3.5}, {17.5, 3.5}, {17.5, 20.5}, {12, 16.2}, {6.5, 20.5}}); break;
    case Predict: PG({{12, 2.8}, {14.2, 9.8}, {21.2, 12}, {14.2, 14.2}, {12, 21.2}, {9.8, 14.2}, {2.8, 12}, {9.8, 9.8}}); break;
    case Ball: {
        C(12, 12, 9.2);
        QPolygonF pent; for (int i = 0; i < 5; ++i) { const double a = -kPi / 2 + i * 2 * kPi / 5; pent << QPointF(12 + 3.6 * std::cos(a), 12 + 3.6 * std::sin(a)); }
        p.drawPolygon(pent);
        for (int i = 0; i < 5; ++i) { const double a = -kPi / 2 + i * 2 * kPi / 5; L(12 + 3.6 * std::cos(a), 12 + 3.6 * std::sin(a), 12 + 9.1 * std::cos(a), 12 + 9.1 * std::sin(a)); }
        break; }
    case Refresh: {
        QPainterPath path; path.arcMoveTo(QRectF(4, 4, 16, 16), 140); path.arcTo(QRectF(4, 4, 16, 16), 140, -260); p.drawPath(path);
        PL({{19.5, 3.5}, {19.5, 8.5}, {14.5, 8.5}}); break; }
    case Check: PL({{5, 12.5}, {10, 17.5}, {19, 7}}); break;
    case Alert: PG({{12, 3.2}, {21.5, 19.8}, {2.5, 19.8}}); L(12, 9.5, 12, 14); dot(12, 17, 1.1); break;
    case Info: C(12, 12, 9.2); L(12, 11, 12, 16.5); dot(12, 7.9, 1.15); break;
    case ArrowRight: L(4, 12, 20, 12); PL({{14, 6}, {20, 12}, {14, 18}}); break;
    case Shield: PG({{12, 2.8}, {20, 5.8}, {20, 12.4}, {12, 21.2}, {4, 12.4}, {4, 5.8}}); PL({{8.5, 12}, {11, 14.5}, {15.5, 9.5}}); break;
    case Clock: C(12, 12, 9.2); PL({{12, 7}, {12, 12}, {15.5, 14}}); break;
    case Trophy: PG({{7, 3.5}, {17, 3.5}, {17, 9}, {12, 13.5}, {7, 9}}); L(12, 13.5, 12, 17.5); L(8, 20.5, 16, 20.5); L(12, 17.5, 12, 20.5);
        PL({{7, 5.5}, {3.8, 5.5}, {3.8, 8}, {7.2, 10.2}}); PL({{17, 5.5}, {20.2, 5.5}, {20.2, 8}, {16.8, 10.2}}); break;
    case Download: L(12, 3.5, 12, 15); PL({{7, 10.5}, {12, 15.5}, {17, 10.5}}); L(4, 20, 20, 20); break;
    case Folder: PG({{3, 6.5}, {9.5, 6.5}, {11.5, 9}, {21, 9}, {21, 19.5}, {3, 19.5}}); break;
    case Bolt: PG({{13.2, 2.6}, {5, 13.6}, {11.4, 13.6}, {10.6, 21.4}, {19, 10.2}, {12.6, 10.2}}); break;
    case User: C(12, 8, 4); { QPainterPath path; path.moveTo(4.5, 20.5); path.cubicTo(4.5, 16, 8, 13.8, 12, 13.8); path.cubicTo(16, 13.8, 19.5, 16, 19.5, 20.5); p.drawPath(path); } break;
    case Target: C(12, 12, 9.2); C(12, 12, 5.3); dot(12, 12, 1.4); break;
    case Eye: { QPainterPath path; path.moveTo(2, 12); path.cubicTo(5, 6, 8.5, 5, 12, 5); path.cubicTo(15.5, 5, 19, 6, 22, 12); path.cubicTo(19, 18, 15.5, 19, 12, 19); path.cubicTo(8.5, 19, 5, 18, 2, 12); p.drawPath(path); C(12, 12, 3); } break;
    case Lock: RR(5, 11, 14, 9.5, 2.2); PL({{8, 11}, {8, 7.8}, {9.5, 4.8}, {12, 4}, {14.5, 4.8}, {16, 7.8}, {16, 11}}); break;
    case Live: C(12, 12, 2.6); { QPainterPath a; a.arcMoveTo(QRectF(6.5, 6.5, 11, 11), 50); a.arcTo(QRectF(6.5, 6.5, 11, 11), 50, 80); p.drawPath(a); QPainterPath b; b.arcMoveTo(QRectF(6.5, 6.5, 11, 11), 230); b.arcTo(QRectF(6.5, 6.5, 11, 11), 230, 80); p.drawPath(b);
        QPainterPath c; c.arcMoveTo(QRectF(3, 3, 18, 18), 50); c.arcTo(QRectF(3, 3, 18, 18), 50, 80); p.drawPath(c); QPainterPath d; d.arcMoveTo(QRectF(3, 3, 18, 18), 230); d.arcTo(QRectF(3, 3, 18, 18), 230, 80); p.drawPath(d); } break;
    case Calendar: RR(3.5, 5, 17, 15.5, 2.4); L(3.5, 10, 20.5, 10); L(8, 3, 8, 7); L(16, 3, 16, 7); break;
    }
    p.restore();
}

QPixmap pixmap(Name n, const QColor& color, int px, double stroke) {
    const qreal dpr = 2.0;
    QPixmap pm(int(px * dpr), int(px * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paint(p, n, QRectF(0, 0, px, px), color, stroke);
    return pm;
}

QIcon icon(Name n, const QColor& color, int px, double stroke) { return QIcon(pixmap(n, color, px, stroke)); }

}  // namespace Icons
