#pragma once
// One shared ~30 fps clock for every animated widget (pitch light sweep, robot, live pulse).
// Widgets only repaint while visible, so idle screens cost nothing.
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class AnimClock : public QObject {
    Q_OBJECT
public:
    static AnimClock& instance() { static AnimClock c; return c; }
    double seconds() const { return m_t.elapsed() / 1000.0; }
    void setEnabled(bool on) { if (on) m_timer.start(); else m_timer.stop(); }
signals:
    void tick();
private:
    AnimClock() { m_t.start(); m_timer.setInterval(33); m_timer.setTimerType(Qt::CoarseTimer); connect(&m_timer, &QTimer::timeout, this, &AnimClock::tick); m_timer.start(); }
    QElapsedTimer m_t;
    QTimer m_timer;
};

// Smooth easing helper used by several widgets.
inline double easeOutCubic(double t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); const double u = 1 - t; return 1 - u * u * u; }
