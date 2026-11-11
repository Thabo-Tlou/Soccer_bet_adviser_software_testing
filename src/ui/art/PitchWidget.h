#pragma once
// Animated stadium graphic: upcoming (model probabilities as pitch territories),
// live (score, minute tracker, pulse) and finished (score + shot volume) matches.
#include <QWidget>

struct PitchData {
    enum Kind { Upcoming, Live, Finished } kind = Upcoming;
    QString home, away;
    int homeGoals = -1, awayGoals = -1;
    int minute = -1;
    bool halfTime = false;
    QString subtitle;        // competition, shown top-left
    QString whenText;        // kickoff text for upcoming matches
    bool demo = false;
    bool hasProbs = false;   double pHome = 0, pDraw = 0, pAway = 0;
    bool hasStats = false;   int shotsH = 0, shotsA = 0, sotH = 0, sotA = 0, cornersH = 0, cornersA = 0;
    bool operator==(const PitchData& o) const {
        return kind == o.kind && home == o.home && away == o.away && homeGoals == o.homeGoals && awayGoals == o.awayGoals &&
               minute == o.minute && halfTime == o.halfTime && subtitle == o.subtitle && whenText == o.whenText && demo == o.demo &&
               hasProbs == o.hasProbs && pHome == o.pHome && pDraw == o.pDraw && pAway == o.pAway && hasStats == o.hasStats &&
               shotsH == o.shotsH && shotsA == o.shotsA && sotH == o.sotH && sotA == o.sotA;
    }
};

class PitchWidget : public QWidget {
    Q_OBJECT
public:
    explicit PitchWidget(QWidget* parent = nullptr);
    void setData(const PitchData& d);
    const PitchData& data() const { return m_d; }
    void setClickable(bool on);
    QSize sizeHint() const override { return {720, 260}; }
    QSize minimumSizeHint() const override { return {320, 150}; }
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    PitchData m_d;
    double m_changeT = 0;
    double m_fromH = 1 / 3.0, m_fromD = 1 / 3.0;   // probability animation start
    bool m_clickable = false;
};
