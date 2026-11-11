#pragma once
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include "UserWidgets.h"

// Upcoming fixtures with the model's pick, plus a form to get a prediction for any two teams.
class UserPredictionsPage : public QWidget {
    Q_OBJECT
public:
    explicit UserPredictionsPage(UserDataHub* hub, QWidget* parent = nullptr);
signals:
    void openMatch(const MatchItem& m);
private:
    void populateTeams();
    void submit();
    void rebuild();
    UserDataHub* m_hub;
    QComboBox *m_home, *m_away;
    QDateEdit* m_date;
    QLineEdit* m_odds[3];
    QLabel* m_msg;
    TextTabs* m_filter;
    QScrollArea* m_scroll;
    QTimer m_debounce;
};

// League table for a season, computed from the match history.
class UserTablePage : public QWidget {
    Q_OBJECT
public:
    explicit UserTablePage(UserDataHub* hub, QWidget* parent = nullptr);
private:
    void populate();
    void rebuild();
    UserDataHub* m_hub;
    QComboBox* m_season;
    StandingsWidget* m_table;
    QLabel* m_sub;
    QScrollArea* m_scroll;
};

// Predictions the user saved, with their outcome once the match has been played.
class UserPicksPage : public QWidget {
    Q_OBJECT
public:
    explicit UserPicksPage(UserDataHub* hub, QWidget* parent = nullptr);
    void refresh();
private:
    void updateResults();
    UserDataHub* m_hub;
    QLabel *m_summary, *m_note;
    QPushButton* m_update;
    QScrollArea* m_scroll;
};
