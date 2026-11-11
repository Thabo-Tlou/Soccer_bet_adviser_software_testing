#include "RobotWidget.h"

#include "AnimClock.h"

#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

namespace {
const double kPi = 3.14159265358979323846;
RVec add(RVec a, RVec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
RVec sub(RVec a, RVec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
RVec cross(RVec a, RVec b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double dot(RVec a, RVec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
RVec norm(RVec a) { const double l = std::sqrt(dot(a, a)); return l < 1e-9 ? RVec{0, 0, 1} : RVec{a.x / l, a.y / l, a.z / l}; }
RVec rotX(RVec v, double a) { const double c = std::cos(a), s = std::sin(a); return {v.x, v.y * c - v.z * s, v.y * s + v.z * c}; }
RVec rotY(RVec v, double a) { const double c = std::cos(a), s = std::sin(a); return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c}; }
RVec rotZ(RVec v, double a) { const double c = std::cos(a), s = std::sin(a); return {v.x * c - v.y * s, v.x * s + v.y * c, v.z}; }
double spow(double v, double e) { return (v < 0 ? -1.0 : 1.0) * std::pow(std::fabs(v), e); }

struct Pose { double rx = 0, ry = 0, rz = 0; RVec off; };
RVec applyPose(RVec v, RVec pivot, const Pose& p) {
    RVec l = sub(v, pivot);
    l = rotZ(rotY(rotX(l, p.rx), p.ry), p.rz);
    return add(add(l, pivot), p.off);
}
RVec rotateNormal(RVec n, const Pose& p) { return rotZ(rotY(rotX(n, p.rx), p.ry), p.rz); }
QColor shade(const QColor& c, double k, double spec) {
    auto f = [&](int v) { return std::clamp(int(v * k + 255 * spec), 0, 255); };
    return QColor(f(c.red()), f(c.green()), f(c.blue()), c.alpha());
}
}

RobotWidget::RobotWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    build();
    connect(&AnimClock::instance(), &AnimClock::tick, this, [this] { if (isVisible()) update(); });
}

void RobotWidget::setState(State s) {
    if (s == m_state) return;
    m_state = s; m_stateT = AnimClock::instance().seconds(); update();
}

int RobotWidget::addPart(RVec pivot, int id) { Part p; p.pivot = pivot; p.id = id; m_parts.push_back(p); return m_parts.size() - 1; }

void RobotWidget::addPoly(int part, const QVector<RVec>& pts, RVec outward, const QColor& col, bool emissive, double glow) {
    Face f; f.v = pts; f.base = col; f.emissive = emissive; f.glow = glow;
    RVec n{0, 0, 1};
    if (pts.size() >= 3) n = norm(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])));
    if (dot(n, outward) < 0) n = {-n.x, -n.y, -n.z};
    f.n = n;
    m_parts[part].faces.push_back(f);
}

void RobotWidget::addBox(int part, RVec c, RVec s, const QColor& col) {
    const double hx = s.x / 2, hy = s.y / 2, hz = s.z / 2;
    auto P = [&](double x, double y, double z) { return RVec{c.x + x * hx, c.y + y * hy, c.z + z * hz}; };
    struct Q { RVec a, b, c2, d, out; };
    const Q qs[] = {
        {P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), {0, 0, 1}},   {P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), {0, 0, -1}},
        {P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), {0, 1, 0}},   {P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), {0, -1, 0}},
        {P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), {1, 0, 0}},   {P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), {-1, 0, 0}},
    };
    for (const Q& q : qs) addPoly(part, {q.a, q.b, q.c2, q.d}, q.out, col, false);
}

// Superellipsoid: squarish = 1 gives a sphere, ~0.45 a soft rounded box.
void RobotWidget::addSphere(int part, RVec c, RVec r, const QColor& col, int lon, int lat, double sq) {
    auto pt = [&](int i, int j) {
        const double u = -kPi / 2 + kPi * j / lat, v = 2 * kPi * i / lon;
        const double cu = spow(std::cos(u), sq), su = spow(std::sin(u), sq), cv = spow(std::cos(v), sq), sv = spow(std::sin(v), sq);
        return RVec{c.x + r.x * cu * cv, c.y + r.y * su, c.z + r.z * cu * sv};
    };
    for (int j = 0; j < lat; ++j)
        for (int i = 0; i < lon; ++i) {
            QVector<RVec> q{pt(i, j), pt(i + 1, j), pt(i + 1, j + 1), pt(i, j + 1)};
            RVec cen{(q[0].x + q[2].x) / 2, (q[0].y + q[2].y) / 2, (q[0].z + q[2].z) / 2};
            addPoly(part, q, sub(cen, c), col, false);
        }
}

void RobotWidget::addCylinderY(int part, RVec c, double r0, double r1, double h, const QColor& col, int seg) {
    for (int i = 0; i < seg; ++i) {
        const double a0 = 2 * kPi * i / seg, a1 = 2 * kPi * (i + 1) / seg;
        RVec b0{c.x + r0 * std::cos(a0), c.y - h / 2, c.z + r0 * std::sin(a0)}, b1{c.x + r0 * std::cos(a1), c.y - h / 2, c.z + r0 * std::sin(a1)};
        RVec t1{c.x + r1 * std::cos(a1), c.y + h / 2, c.z + r1 * std::sin(a1)}, t0{c.x + r1 * std::cos(a0), c.y + h / 2, c.z + r1 * std::sin(a0)};
        RVec mid{(b0.x + t1.x) / 2, c.y, (b0.z + t1.z) / 2};
        addPoly(part, {b0, b1, t1, t0}, sub(mid, c), col, false);
        addPoly(part, {{c.x, c.y + h / 2, c.z}, t0, t1}, {0, 1, 0}, col, false);
        addPoly(part, {{c.x, c.y - h / 2, c.z}, b1, b0}, {0, -1, 0}, col, false);
    }
}

void RobotWidget::addCylinderX(int part, RVec c, double r, double h, const QColor& col, int seg) {
    for (int i = 0; i < seg; ++i) {
        const double a0 = 2 * kPi * i / seg, a1 = 2 * kPi * (i + 1) / seg;
        RVec l0{c.x - h / 2, c.y + r * std::cos(a0), c.z + r * std::sin(a0)}, l1{c.x - h / 2, c.y + r * std::cos(a1), c.z + r * std::sin(a1)};
        RVec r1{c.x + h / 2, l1.y, l1.z}, r0{c.x + h / 2, l0.y, l0.z};
        RVec mid{c.x, (l0.y + l1.y) / 2, (l0.z + l1.z) / 2};
        addPoly(part, {l0, l1, r1, r0}, sub(mid, c), col, false);
        addPoly(part, {{c.x + h / 2, c.y, c.z}, r0, r1}, {1, 0, 0}, col, false);
        addPoly(part, {{c.x - h / 2, c.y, c.z}, l1, l0}, {-1, 0, 0}, col, false);
    }
}

void RobotWidget::build() {
    const QColor shell("#E8EEF3"), shellDark("#B9C4CE"), dark("#1A2430"), mid("#7E8B98");
    // --- body (torso, neck, hover base) ---
    const int body = addPart({0, 0, 0}, Body);
    addSphere(body, {0, -0.62, 0}, {0.50, 0.46, 0.36}, shell, 16, 10, 0.55);
    addPoly(body, {{-0.28, -0.8, 0.375}, {0.28, -0.8, 0.375}, {0.28, -0.44, 0.375}, {-0.28, -0.44, 0.375}}, {0, 0, 1}, dark, false);
    addCylinderY(body, {0, -0.12, 0}, 0.15, 0.17, 0.2, mid, 10);
    addCylinderY(body, {0, -1.16, 0}, 0.30, 0.12, 0.36, shellDark, 12);
    // --- head ---
    const int head = addPart({0, -0.08, 0}, Head);
    addSphere(head, {0, 0.58, 0}, {0.56, 0.44, 0.42}, shell, 18, 12, 0.42);
    addCylinderX(head, {-0.58, 0.58, 0}, 0.17, 0.1, shellDark, 12);
    addCylinderX(head, {0.58, 0.58, 0}, 0.17, 0.1, shellDark, 12);
    addCylinderX(head, {-0.64, 0.58, 0}, 0.1, 0.06, dark, 10);
    addCylinderX(head, {0.64, 0.58, 0}, 0.1, 0.06, dark, 10);
    // face screen (rounded rectangle)
    {
        QVector<RVec> pts; const double w = 0.44, h = 0.30, r = 0.12; const double zf = 0.405;
        const int cs = 6;
        const double cx[4] = {w - r, -(w - r), -(w - r), w - r}, cy[4] = {h - r, h - r, -(h - r), -(h - r)};
        for (int k = 0; k < 4; ++k)
            for (int i = 0; i <= cs; ++i) {
                const double a = kPi / 2 * (k + double(i) / cs);
                pts.push_back({cx[k] + r * std::cos(a), 0.58 + cy[k] + r * std::sin(a), zf});
            }
        addPoly(head, pts, {0, 0, 1}, QColor("#0A1218"), false);
    }
    // --- antenna ---
    const int ant = addPart({0.25, 1.0, 0}, Antenna);
    addCylinderY(ant, {0.25, 1.1, 0}, 0.025, 0.025, 0.22, mid, 6);
    addSphere(ant, {0.25, 1.24, 0}, {0.065, 0.065, 0.065}, QColor("#FFFFFF"), 8, 6);
    // --- arms ---
    for (int s = 0; s < 2; ++s) {
        const double sg = s == 0 ? -1 : 1;
        const int arm = addPart({sg * 0.62, -0.38, 0}, s == 0 ? ArmL : ArmR);
        addSphere(arm, {sg * 0.62, -0.38, 0}, {0.15, 0.15, 0.15}, shellDark, 10, 7);
        addCylinderY(arm, {sg * 0.66, -0.6, 0.02}, 0.085, 0.07, 0.34, shell, 10);
        addSphere(arm, {sg * 0.67, -0.82, 0.05}, {0.11, 0.11, 0.11}, mid, 10, 7);
        addSphere(arm, {sg * 0.67, -0.95, 0.1}, {0.12, 0.12, 0.12}, shell, 10, 7);
    }
    // --- eyes (rebuilt every frame, see paintEvent) ---
    addPart({0, 0, 0}, Eyes);
}

void RobotWidget::mouseMoveEvent(QMouseEvent* e) {
    m_hover = true;
    m_lookTarget = QPointF((e->position().x() / width() - 0.5) * 2, (e->position().y() / height() - 0.45) * 2);
    if (m_drag) m_userYaw = m_dragStartYaw + (e->position().x() - m_dragStartX) / width() * 4.0;
}
void RobotWidget::mousePressEvent(QMouseEvent* e) { m_drag = true; m_dragStartX = e->position().x(); m_dragStartYaw = m_userYaw; setCursor(Qt::ClosedHandCursor); }
void RobotWidget::mouseReleaseEvent(QMouseEvent*) { m_drag = false; setCursor(Qt::ArrowCursor); }
void RobotWidget::leaveEvent(QEvent*) { m_hover = false; m_drag = false; m_lookTarget = {0, 0}; }

void RobotWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const double t = AnimClock::instance().seconds();
    const double st = t - m_stateT;
    const double W = width(), H = height();
    const bool capOn = !m_caption.isEmpty();
    const double areaH = H - (capOn ? 26 : 0);
    const double unit = std::min(W * 0.34, areaH * 0.335);
    const double cx = W / 2, cy = areaH * 0.5 + 0.02 * unit;
    const double camZ = 7.0;

    // smooth look-at
    if (!m_hover) { m_lookTarget = QPointF(std::sin(t * 0.55) * 0.55, std::sin(t * 0.37 + 1.0) * 0.18); }
    m_look += (m_lookTarget - m_look) * 0.12;
    if (!m_drag) m_userYaw *= 0.94;

    // ---- pose -------------------------------------------------------------------------------------
    const double speed = m_state == Thinking ? 2.4 : 1.0;
    double bodyYaw = m_userYaw + std::sin(t * 0.5) * 0.18 + m_look.x() * 0.22;
    double bob = std::sin(t * 1.7 * (m_state == Thinking ? 1.4 : 1.0)) * 0.045;
    Pose pBody; pBody.off = {0, bob, 0};
    Pose pHead; pHead.ry = m_look.x() * 0.55 + (m_state == Thinking ? std::sin(t * 2.2) * 0.35 : 0); pHead.rx = m_look.y() * 0.28;
    if (m_state == Done) { const double n = std::clamp(st / 1.4, 0.0, 1.0); pHead.rx += std::sin(n * kPi * 3) * 0.2 * (1 - n); }
    Pose pAL, pAR;
    pAL.rz = pAR.rz = 0;
    pAL.rx = std::sin(t * 1.7) * 0.07; pAR.rx = -std::sin(t * 1.7) * 0.07;
    pAL.rz = -0.06; pAR.rz = 0.06;
    if (m_state == Thinking) {              // arms raised, "typing" on a hologram
        pAL.rx = -1.15 + std::sin(t * 7) * 0.12; pAR.rx = -1.15 + std::cos(t * 7.5) * 0.12; pAL.rz = 0.25; pAR.rz = -0.25;
    } else if (m_state == Done) {           // one arm waves a quick confirmation
        const double n = std::clamp(st / 2.4, 0.0, 1.0), k = std::sin(n * kPi);
        pAR.rz = -0.15 - 2.2 * k; pAR.rx = -0.2 * k + std::sin(t * 9) * 0.18 * k;
    }
    Pose pAnt; pAnt.rz = std::sin(t * 2.5 * speed) * 0.12; pAnt.rx = std::cos(t * 2.1 * speed) * 0.08;
    Pose none;

    // ---- gather faces ---------------------------------------------------------------------------
    struct Item { QPolygonF poly; double depth; QColor col; bool emissive; double glow; QPointF centre; };
    QVector<Item> items; items.reserve(700);
    const RVec light = norm({-0.45, 0.75, 0.7});
    auto project = [&](RVec v) {
        const double k = camZ / (camZ - v.z);
        return QPointF(cx + v.x * unit * k, cy - v.y * unit * k);
    };
    auto world = [&](RVec v, const Pose* pp, RVec pivot) {
        if (pp) v = applyPose(v, pivot, *pp);
        v = rotY(v, bodyYaw);
        v = add(v, {0, bob, 0});
        return v;
    };
    auto pushFace = [&](const QVector<RVec>& vs, RVec n, const QColor& col, bool emissive, double glow, const Pose* pp, RVec pivot, const Pose* headPose = nullptr, RVec headPivot = {}) {
        QPolygonF poly; double dz = 0; RVec cen{0, 0, 0};
        for (const RVec& v0 : vs) {
            RVec v = v0;
            if (headPose) v = applyPose(v, headPivot, *headPose);        // eyes follow the head
            RVec w = world(v, pp, pivot);
            poly << project(w); dz += w.z; cen = add(cen, w);
        }
        dz /= vs.size();
        RVec nn = n;
        if (headPose) nn = rotateNormal(nn, *headPose);
        if (pp) nn = rotateNormal(nn, *pp);
        nn = rotY(nn, bodyYaw);
        if (nn.z <= -0.02 && !emissive) return;
        if (nn.z <= 0.02 && emissive) return;
        const double lam = std::max(0.0, dot(nn, light));
        const double rim = std::pow(std::max(0.0, 1.0 - std::fabs(nn.z)), 3) * 0.12;
        const double spec = std::pow(std::max(0.0, dot(nn, norm({-0.25, 0.55, 1.0}))), 28) * 0.28;
        QColor c = emissive ? col : shade(col, 0.42 + 0.62 * lam, spec + rim);
        const double n2 = double(vs.size());
        items.push_back({poly, dz + (emissive ? 0.02 : 0), c, emissive, glow, QPointF(poly.boundingRect().center())});
        (void)n2;
    };

    for (const Part& part : m_parts) {
        if (part.id == Eyes) continue;
        const Pose* pp = &none;
        switch (part.id) {
        case Body: pp = &none; break;
        case Head: pp = &pHead; break;
        case ArmL: pp = &pAL; break;
        case ArmR: pp = &pAR; break;
        case Antenna: pp = &pAnt; break;
        }
        // the antenna rides on the head
        const bool onHead = part.id == Antenna;
        for (const Face& f : part.faces) {
            if (onHead) {
                QVector<RVec> vs; for (const RVec& v : f.v) vs.push_back(applyPose(applyPose(v, part.pivot, pAnt), {0, -0.08, 0}, pHead));
                RVec n = rotateNormal(rotateNormal(f.n, pAnt), pHead);
                pushFace(vs, n, f.base, f.emissive, f.glow, nullptr, {});
            } else {
                pushFace(f.v, f.n, f.base, f.emissive, f.glow, pp, part.pivot);
            }
        }
    }

    // --- emissive details: eyes, chest core, antenna tip ---------------------------------------
    const QColor acc = m_accent;
    const RVec headPivot{0, -0.08, 0};
    const double blink = (std::fmod(t, 4.2) > 4.0) ? 0.1 : 1.0;
    {
        const double zf = 0.41, ey = 0.585;
        auto rr = [&](double x0, double y0, double w, double h) {
            QVector<RVec> v; const int n = 10; const double r = std::min(w, h) * 0.5;
            for (int k = 0; k < 4; ++k) {
                const double ccx = (k == 0 || k == 3) ? w / 2 - r : -(w / 2 - r), ccy = (k < 2) ? h / 2 - r : -(h / 2 - r);
                for (int i = 0; i <= n; ++i) { const double a = kPi / 2 * (k + double(i) / n); v.push_back({x0 + ccx + r * std::cos(a), y0 + ccy + r * std::sin(a), zf}); }
            }
            return v;
        };
        if (m_state == Thinking) {
            const double sweep = std::sin(st * 3.2);
            pushFace(rr(-0.22, ey, 0.1, 0.1), {0, 0, 1}, acc.darker(140), true, 0.5, nullptr, {}, &pHead, headPivot);
            pushFace(rr(0.22, ey, 0.1, 0.1), {0, 0, 1}, acc.darker(140), true, 0.5, nullptr, {}, &pHead, headPivot);
            pushFace(rr(sweep * 0.3, ey, 0.12, 0.2), {0, 0, 1}, acc.lighter(130), true, 1.0, nullptr, {}, &pHead, headPivot);
        } else if (m_state == Done) {
            for (int s = -1; s <= 1; s += 2) {
                const double x0 = s * 0.2, w = 0.14, h = 0.09, th = 0.05;
                QVector<RVec> chev{{x0 - w, ey - h, zf}, {x0, ey + h, zf}, {x0 + w, ey - h, zf}, {x0 + w - th * 1.2, ey - h, zf}, {x0, ey + h - th * 1.5, zf}, {x0 - w + th * 1.2, ey - h, zf}};
                pushFace(chev, {0, 0, 1}, acc.lighter(125), true, 1.0, nullptr, {}, &pHead, headPivot);
            }
        } else {
            const double e = 0.2 + 0.0, hh = 0.22 * blink;
            pushFace(rr(-e, ey, 0.13, std::max(0.03, hh)), {0, 0, 1}, acc.lighter(125), true, 1.0, nullptr, {}, &pHead, headPivot);
            pushFace(rr(e, ey, 0.13, std::max(0.03, hh)), {0, 0, 1}, acc.lighter(125), true, 1.0, nullptr, {}, &pHead, headPivot);
        }
        // chest core
        const double pulse = m_state == Thinking ? 0.6 + 0.4 * std::sin(t * 8) : (m_state == Done ? 0.85 + 0.15 * std::sin(t * 3) : 0.55 + 0.25 * std::sin(t * 2));
        QVector<RVec> core; const int n = 20; const double rad = 0.11 + 0.015 * pulse;
        for (int i = 0; i < n; ++i) { const double a = 2 * kPi * i / n; core.push_back({rad * std::cos(a), -0.62 + rad * std::sin(a), 0.385}); }
        QColor cc = shade(acc, 0.55 + 0.6 * pulse, 0.0);
        pushFace(core, {0, 0, 1}, cc, true, pulse, nullptr, {});
        // chest bars
        for (int i = -1; i <= 1; i += 2) {
            const double bw = 0.1 * (0.5 + 0.5 * std::sin(t * 3 * speed + i)), x0 = i * 0.18;
            pushFace({{x0 - 0.045, -0.7, 0.385}, {x0 + 0.045, -0.7, 0.385}, {x0 + 0.045, -0.7 + 0.08 + bw, 0.385}, {x0 - 0.045, -0.7 + 0.08 + bw, 0.385}}, {0, 0, 1}, acc.darker(120), true, 0.3, nullptr, {});
        }
        // antenna tip
        const double tip = 0.5 + 0.5 * std::sin(t * (m_state == Thinking ? 9 : 3));
        RVec tc{0.25, 1.24, 0}; QVector<RVec> tp; for (int i = 0; i < 10; ++i) { const double a = 2 * kPi * i / 10; tp.push_back({tc.x + 0.06 * std::cos(a), tc.y + 0.06 * std::sin(a), 0.075}); }
        QVector<RVec> tpw; for (const RVec& v : tp) tpw.push_back(applyPose(applyPose(v, {0.25, 1.0, 0}, pAnt), headPivot, pHead));
        pushFace(tpw, {0, 0, 1}, shade(acc, 0.7 + 0.6 * tip, 0.0), true, tip, nullptr, {});
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });

    // ---- background glow + floor ----------------------------------------------------------------
    {
        QRadialGradient g(QPointF(cx, cy), std::min(unit * 2.3, std::min(W, H) * 0.5));
        QColor a = acc; a.setAlpha(m_state == Thinking ? 70 : 38); g.setColorAt(0, a); a.setAlpha(0); g.setColorAt(1, a);
        p.fillRect(rect(), g);
        const double fy = cy + 1.38 * unit;
        const double pr = 0.5 + 0.5 * std::sin(t * 2.0);
        QRadialGradient sh(QPointF(cx, fy + unit * 0.12), unit * 0.9);
        sh.setColorAt(0, QColor(0, 0, 0, 90)); sh.setColorAt(1, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen); p.setBrush(sh); p.drawEllipse(QPointF(cx, fy + unit * 0.12), unit * 0.9, unit * 0.18);
        for (int i = 0; i < 3; ++i) {
            QColor rc = acc; rc.setAlpha(int((110 - i * 32) * (0.6 + 0.4 * pr)));
            p.setBrush(Qt::NoBrush); p.setPen(QPen(rc, 1.6));
            const double rr = unit * (0.42 + i * 0.18 + 0.03 * pr);
            p.drawEllipse(QPointF(cx, fy - bob * unit * 0.2), rr, rr * 0.2);
        }
    }

    // ---- orbiting data bits (behind) -------------------------------------------------------------
    auto bits = [&](bool front) {
        const int count = m_state == Thinking ? 26 : 12;
        for (int i = 0; i < count; ++i) {
            const double a = t * (m_state == Thinking ? 1.9 : 0.5) + i * (2 * kPi / count);
            const double rad = 1.15 + 0.12 * std::sin(i * 1.7 + t);
            RVec v{rad * std::cos(a), -0.35 + 0.28 * std::sin(a * 1.3 + i) + (i % 3) * 0.28 - 0.2, rad * std::sin(a) * 0.9};
            const bool isFront = v.z > 0;
            if (isFront != front) continue;
            const QPointF pt = project(v);
            const double k = (v.z + 1.4) / 2.8;
            QColor c = acc; c.setAlpha(int(60 + 150 * k));
            p.setPen(Qt::NoPen); p.setBrush(c);
            const double sz = (m_state == Thinking ? 2.4 : 1.7) * (0.6 + k);
            if (i % 4 == 0) p.drawRect(QRectF(pt.x() - sz, pt.y() - sz, sz * 2, sz * 2)); else p.drawEllipse(pt, sz, sz);
        }
    };
    bits(false);

    // ---- faces ------------------------------------------------------------------------------------
    p.setBrush(Qt::NoBrush);
    for (const Item& it : items) {
        if (it.emissive) {
            if (it.glow > 0.05) {
                QColor g = it.col; g.setAlpha(int(70 * it.glow));
                const QRectF br = it.poly.boundingRect();
                QRadialGradient rg(it.centre, std::max(br.width(), br.height()) * 1.4);
                rg.setColorAt(0, g); g.setAlpha(0); rg.setColorAt(1, g);
                p.setPen(Qt::NoPen); p.setBrush(rg); p.drawEllipse(it.centre, br.width() * 1.4, br.height() * 1.4);
            }
            p.setPen(Qt::NoPen); p.setBrush(it.col); p.drawPolygon(it.poly);
        } else {
            p.setPen(QPen(it.col, 0.7)); p.setBrush(it.col); p.drawPolygon(it.poly);
        }
    }
    bits(true);

    // thinking: holographic scan line sweeping across the torso
    if (m_state == Thinking) {
        const double ph = std::fmod(st * 0.9, 1.0);
        const double y = cy + (1.0 - ph * 2.4) * unit * 0.9;
        QColor sc = acc; sc.setAlpha(110);
        p.setPen(QPen(sc, 1.5)); p.drawLine(QPointF(cx - unit * 0.95, y), QPointF(cx + unit * 0.95, y));
        sc.setAlpha(26); p.setPen(Qt::NoPen); p.setBrush(sc); p.drawRect(QRectF(cx - unit * 0.95, y - 12, unit * 1.9, 12));
    }

    // caption
    if (capOn) {
        QFont f = font(); f.setPixelSize(12); f.setBold(true); p.setFont(f);
        QString txt = m_caption;
        if (m_state == Thinking) txt += QString(int(t * 3) % 4, QChar('.'));
        p.setPen(QColor(255, 255, 255, 200));
        p.drawText(QRectF(0, H - 26, W, 22), Qt::AlignCenter, txt);
    }
}
