#pragma once
// Shared UI building blocks for both the admin and customer sides:
//   Toast        - short, non-blocking confirmation / error messages (bottom-right of the window)
//   EmptyState   - friendly "nothing here yet" panel with an optional call-to-action
//   applyElevation - subtle drop shadow used by cards
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include "../art/Icons.h"
#include "Tokens.h"

class Toast : public QFrame {
    Q_OBJECT
public:
    enum Kind { Info, Success, Warning, Error };
    // Shows `text` over the window that contains `anchor`. Safe to call from anywhere on the GUI thread.
    static void show(QWidget* anchor, const QString& text, Kind kind = Info, int durationMs = 3600);
private:
    Toast(QWidget* window, const QString& text, Kind kind);
    void place();
};

class EmptyState : public QWidget {
    Q_OBJECT
public:
    explicit EmptyState(Icons::Name icon, const QString& title, const QString& text, const QString& actionText = QString(), QWidget* parent = nullptr);
signals:
    void actionClicked();
protected:
    void paintEvent(QPaintEvent*) override;
private:
    Icons::Name m_icon;
};

inline void applyElevation(QWidget* w, int blur = DS::shadowBlur, int dy = DS::shadowOffsetY, int alpha = DS::shadowAlpha) {
    auto* fx = new QGraphicsDropShadowEffect(w);
    fx->setBlurRadius(blur); fx->setOffset(0, dy); fx->setColor(QColor(15, 23, 42, alpha));
    w->setGraphicsEffect(fx);
}
