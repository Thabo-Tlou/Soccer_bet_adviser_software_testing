#pragma once
// A small software-rendered 3D robot analyst (QPainter only - no OpenGL / Qt Quick 3D needed).
// States: Idle (looks around / follows the cursor), Thinking (scans, rings orbit), Done (nods, core glows).
#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

#include <array>

struct RVec { double x = 0, y = 0, z = 0; };

class RobotWidget : public QWidget {
    Q_OBJECT
public:
    enum State { Idle, Thinking, Done };
    explicit RobotWidget(QWidget* parent = nullptr);

    void setState(State s);
    State state() const { return m_state; }
    void setAccent(const QColor& c) { m_accent = c; update(); }
    void setCaption(const QString& text) { m_caption = text; update(); }
    QSize sizeHint() const override { return {240, 270}; }
    QSize minimumSizeHint() const override { return {150, 170}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Face { QVector<RVec> v; RVec n; QColor base; bool emissive = false; double glow = 0; };
    struct Part { QVector<Face> faces; RVec pivot; int id = 0; };
    enum PartId { Body, Head, ArmL, ArmR, Antenna, Eyes };
    void build();
    int addPart(RVec pivot, int id);
    void addBox(int part, RVec c, RVec s, const QColor& col);
    void addSphere(int part, RVec c, RVec r, const QColor& col, int lon, int lat, double squarish = 1.0);
    void addCylinderY(int part, RVec c, double r0, double r1, double h, const QColor& col, int seg);
    void addCylinderX(int part, RVec c, double r, double h, const QColor& col, int seg);
    void addPoly(int part, const QVector<RVec>& pts, RVec outward, const QColor& col, bool emissive, double glow = 1.0);
    QVector<Part> m_parts;

    State m_state = Idle;
    QColor m_accent = QColor("#A9BF86");
    QString m_caption;
    double m_stateT = 0;
    // interaction
    QPointF m_look{0, 0}, m_lookTarget{0, 0};
    bool m_hover = false, m_drag = false;
    double m_userYaw = 0, m_dragStartX = 0, m_dragStartYaw = 0;
};
