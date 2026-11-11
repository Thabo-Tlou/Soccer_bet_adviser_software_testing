#include "Components.h"

#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

// ------------------------------------------------------------------------------------------ Toast
static QList<QPointer<Toast>>& liveToasts() { static QList<QPointer<Toast>> l; return l; }

Toast::Toast(QWidget* window, const QString& text, Kind kind) : QFrame(window) {
    QColor accent = DS::leaf600; Icons::Name ic = Icons::Info;
    if (kind == Success) { accent = DS::success600; ic = Icons::Check; }
    else if (kind == Warning) { accent = DS::warning600; ic = Icons::Alert; }
    else if (kind == Error) { accent = DS::danger600; ic = Icons::Alert; }
    setObjectName("Toast");
    setStyleSheet(QString("QFrame#Toast { background: %1; border-radius: 12px; border: 1px solid %2; } QLabel { color: #ffffff; background: transparent; font-size: 13px; }")
                      .arg(DS::hex(DS::slate900), DS::hex(DS::slate700)));
    auto* l = new QHBoxLayout(this); l->setContentsMargins(14, 11, 18, 11); l->setSpacing(11);
    auto* icon = new QLabel; icon->setFixedSize(22, 22);
    QPixmap pm(44, 44); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
    { QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(accent); p.drawEllipse(QRectF(0, 0, 22, 22)); Icons::paint(p, ic, QRectF(4.5, 4.5, 13, 13), Qt::white, 2.2); }
    icon->setPixmap(pm); icon->setScaledContents(true);
    auto* t = new QLabel(text); t->setWordWrap(true); t->setMaximumWidth(380);
    l->addWidget(icon); l->addWidget(t, 1);
    applyElevation(this, 28, 6, 70);
    adjustSize();
}

void Toast::place() {
    QWidget* w = parentWidget();
    if (!w) return;
    int offset = 0;
    for (const auto& t : liveToasts()) if (t && t != this && t->isVisible()) offset += t->height() + 10;
    adjustSize();
    move(w->width() - width() - 28, w->height() - height() - 52 - offset);
}

void Toast::show(QWidget* anchor, const QString& text, Kind kind, int ms) {
    if (!anchor || text.isEmpty()) return;
    QWidget* win = anchor->window();
    auto* t = new Toast(win, text, kind);
    liveToasts().append(t);
    t->place();
    t->raise();
    t->QWidget::show();
    auto* fx = new QGraphicsOpacityEffect(t);        // fade in/out (the shadow effect is replaced by opacity while animating)
    t->setGraphicsEffect(fx);
    auto* in = new QPropertyAnimation(fx, "opacity", t); in->setDuration(160); in->setStartValue(0.0); in->setEndValue(1.0); in->start(QAbstractAnimation::DeleteWhenStopped);
    QTimer::singleShot(ms, t, [t, fx]() {
        auto* out = new QPropertyAnimation(fx, "opacity", t); out->setDuration(260); out->setStartValue(1.0); out->setEndValue(0.0);
        connect(out, &QPropertyAnimation::finished, t, &QObject::deleteLater);
        out->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// ------------------------------------------------------------------------------------------ EmptyState
EmptyState::EmptyState(Icons::Name icon, const QString& title, const QString& text, const QString& actionText, QWidget* parent) : QWidget(parent), m_icon(icon) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(24, 28, 24, 32); v->setSpacing(6);
    v->addSpacing(64);                                   // room for the painted icon badge
    auto* t = new QLabel(title); t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet(QString("font-size:16px; font-weight:700; color:%1; background:transparent;").arg(DS::hex(DS::text)));
    auto* d = new QLabel(text); d->setAlignment(Qt::AlignCenter); d->setWordWrap(true); d->setMaximumWidth(460);
    d->setStyleSheet(QString("font-size:13px; color:%1; background:transparent;").arg(DS::hex(DS::textMuted)));
    d->setFixedWidth(440);                                                  // wrapped, centred text needs a definite size to lay out reliably
    d->setMinimumHeight(QFontMetrics(d->font()).boundingRect(0, 0, 440, 10000, Qt::TextWordWrap, text).height() + 6);
    v->addWidget(t, 0, Qt::AlignHCenter); v->addWidget(d, 0, Qt::AlignHCenter);
    if (!actionText.isEmpty()) {
        auto* b = new QPushButton(actionText); b->setCursor(Qt::PointingHandCursor); b->setProperty("primary", true); b->setProperty("ubtn", "solid");
        v->addSpacing(8); v->addWidget(b, 0, Qt::AlignHCenter);
        connect(b, &QPushButton::clicked, this, &EmptyState::actionClicked);
    }
    v->addStretch(1);
}

void EmptyState::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(width() / 2.0, 28 + 32);
    p.setPen(Qt::NoPen); p.setBrush(DS::leaf50); p.drawEllipse(c, 32, 32);
    p.setBrush(DS::leaf100); p.drawEllipse(c, 24, 24);
    Icons::paint(p, m_icon, QRectF(c.x() - 13, c.y() - 13, 26, 26), DS::leaf700, 1.9);
}
