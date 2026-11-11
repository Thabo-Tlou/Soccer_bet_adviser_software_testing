#pragma once
// Custom-painted building blocks of the user side (all black / white, green for live and highlights).
#include <QFrame>
#include <QLabel>
#include <QWidget>
#include <array>
#include <QStringList>
#include <vector>
#include "ui/art/Icons.h"
#include "ui/art/PitchWidget.h"
#include "ui/art/RobotWidget.h"
#include "UserCommon.h"
#include "UserData.h"

class QFrame; class QVBoxLayout;
// Full-width content surface for a page (no max width, no side gutters); returns the surface and its layout.
QFrame* makeColumn(QWidget* page, QVBoxLayout*& inner);

// Text tabs with a green underline on the active one (also used, in dark mode, for the main navigation).
class NavTabs : public QWidget {
    Q_OBJECT
public:
    explicit NavTabs(QWidget* parent = nullptr);
    void setTabs(const QStringList& labels, const QList<Icons::Name>& icons);
    int current() const { return m_cur; }
    void setCurrent(int i, bool notify = false);
    QStringList tabLabels() const { return m_labels; }
    QList<Icons::Name> tabIcons() const { return m_icons; }
    QSize sizeHint() const override;
signals:
    void changed(int index);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    QRect tabRect(int i) const;
    int indexAt(const QPoint& p) const;
    QStringList m_labels; QList<Icons::Name> m_icons;
    int m_cur = 0, m_hover = -1;
};

class TextTabs : public QWidget {
    Q_OBJECT
public:
    explicit TextTabs(bool dark = false, QWidget* parent = nullptr);
    void setTabs(const QStringList& t);
    void setLabel(int i, const QString& t);
    int current() const { return m_cur; }
    void setCurrent(int i, bool notify = false);
    QSize sizeHint() const override;
signals:
    void changed(int index);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    int indexAt(int x) const;
    QRect tabRect(int i) const;
    QStringList m_tabs;
    int m_cur = 0, m_hover = -1;
    bool m_dark;
};

// Seven-day strip centred on the selected day, like the livescore date selector.
class DateStrip : public QWidget {
    Q_OBJECT
public:
    explicit DateStrip(QWidget* parent = nullptr);
    QDate date() const { return m_sel; }
    void setDate(const QDate& d, bool notify = false);
    QSize sizeHint() const override { return {600, 58}; }
signals:
    void dateChanged(const QDate& d);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    int cellAt(const QPoint& p) const;   // -1 left arrow, 7 right arrow, 0..6 days, -2 none
    QDate m_sel;
    int m_hover = -2;
};

// Group header ("Premier League"), with an optional count on the right.
class SectionHeader : public QWidget {
    Q_OBJECT
public:
    explicit SectionHeader(const QString& title, const QString& right = QString(), QWidget* parent = nullptr);
    QSize sizeHint() const override { return {400, 34}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title, m_right;
};

// One match line: status | home | score | away | pick | star.
class MatchRow : public QWidget {
    Q_OBJECT
public:
    explicit MatchRow(QWidget* parent = nullptr);
    void setItem(const MatchItem& m, const PickInfo* pick, bool fav);
    const MatchItem& item() const { return m_item; }
    QSize sizeHint() const override { return {600, 52}; }
    static QString statusText(const MatchItem& m);
signals:
    void clicked();
    void favToggled();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    MatchItem m_item;
    bool m_hasPick = false, m_value = false, m_fav = false, m_hover = false, m_overStar = false;
    int m_best = 0;
    double m_p = 0;
};

// Home / Draw / Away probabilities as three bars, with the bookmaker view next to it when odds exist.
class ProbRows : public QWidget {
    Q_OBJECT
public:
    explicit ProbRows(QWidget* parent = nullptr);
    void set(const QString labels[3], const sba::Probs3& model, bool hasMarket, const sba::Probs3& market);
    QSize sizeHint() const override { return {560, 150}; }
    QSize minimumSizeHint() const override { return {420, 150}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_labels[3];
    sba::Probs3 m_model{}, m_market{};
    bool m_hasMarket = false;
};

// Last results as small squares (W green, D grey, L black); oldest first.
class FormChips : public QWidget {
    Q_OBJECT
public:
    explicit FormChips(QWidget* parent = nullptr);
    void setForm(const QString& f);
    QSize sizeHint() const override;
    static void paintChips(QPainter& p, const QString& form, int x, int y, int size, int gap);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_form;
};

// Club crest (real logo when available, generated crest otherwise).
void paintTeamBadge(QPainter& p, const QPointF& centre, double radius, const QString& team, bool dark);

// Builds the stadium graphic for a match: live score / minute, full-time shot volume, or the model's probabilities.
PitchData pitchDataFor(UserDataHub* hub, const MatchItem& m);

// Dark card with the 3D robot analyst on the left and the headline verdict on the right.
class AiPanel : public QFrame {
    Q_OBJECT
public:
    explicit AiPanel(QWidget* parent = nullptr);
    void showIdle(const QString& headline, const QString& note);
    void showThinking(const QString& caption, const QString& note);
    void showResult(const QString& kicker, const QString& headline, const QString& pct, const QString& note, const QColor& accent);
    RobotWidget* robot() const { return m_robot; }
    // Shows the three outcome probabilities on the right-hand side of the panel (highlighting the model's pick).
    void setBreakdown(const QStringList& names, const std::array<double, 3>& p, int best);
    void clearBreakdown() { m_hasBreak = false; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    RobotWidget* m_robot;
    QLabel *m_kicker, *m_head, *m_pct, *m_note;
    QStringList m_bnames; std::array<double, 3> m_bp{}; int m_bbest = -1; bool m_hasBreak = false;
};

// League table.
class StandingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit StandingsWidget(QWidget* parent = nullptr);
    void setRows(const std::vector<TableRow>& r);
    QSize sizeHint() const override { return {700, 32 + 42 * 20}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    std::vector<TableRow> m_rows;
};

// One saved pick in "My picks".
class PickRowWidget : public QWidget {
    Q_OBJECT
public:
    enum State { Pending, Won, Lost };
    explicit PickRowWidget(QWidget* parent = nullptr);
    void set(const QString& date, const QString& teams, const QString& pick, State s, const QString& result);
    void setTeams(const QString& home, const QString& away) { m_home = home; m_away = away; update(); }
    QSize sizeHint() const override { return {600, 56}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_date, m_teams, m_pick, m_result, m_home, m_away;
    State m_state = Pending;
};
