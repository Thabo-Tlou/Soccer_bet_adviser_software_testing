#pragma once
// Welcome page: says what the app does, shows live data (stats, featured match, league snapshot) and routes people
// into the system with clear calls to action.
#include <QWidget>
#include "UserData.h"
#include "UserWidgets.h"

class UserHomePage : public QWidget {
    Q_OBJECT
public:
    explicit UserHomePage(UserDataHub* hub, QWidget* parent = nullptr);
    void refresh();
signals:
    void gotoTab(int tab);                  // 1 Scores, 2 Predictions, 3 Table, 4 My picks
    void openMatch(const MatchItem& m);
private:
    UserDataHub* m_hub;
    QLabel *m_statMatches, *m_statMatchesSub, *m_statClubs, *m_statClubsSub, *m_statDate, *m_statDateSub, *m_statModels, *m_statModelsSub;
    QLabel *m_featKicker, *m_trustText, *m_trustMetric;
    PitchWidget* m_pitch;
    QWidget* m_featBox;
    QWidget* m_mini;
    MatchItem m_feat;
    bool m_hasFeat = false;
};
