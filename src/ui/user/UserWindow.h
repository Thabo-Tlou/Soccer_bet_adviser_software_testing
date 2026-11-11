#pragma once
// The user side of the application: a livescore-style scores, predictions, table and picks interface.
#include <QMainWindow>
#include <QStackedWidget>
#include "UserHomePage.h"
#include "UserSlipPage.h"
#include "UserMatchPage.h"
#include "UserOtherPages.h"
#include "UserScoresPage.h"
#include "UserWidgets.h"

class UserWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit UserWindow(AppController* c, QWidget* parent = nullptr);
    UserDataHub* hub() { return m_hub; }
    // Developer aid: renders the main screens to PNG files in `dir` (used by --user-screenshots).
    void captureScreens(const QString& dir);
signals:
    void adminRequested();
private:
    void openMatch(const MatchItem& m);
    void goTab(int i);
    enum Tab { Home = 0, Scores, Predictions, Table, Picks, Slip, MatchPage };
    AppController* m_c;
    UserDataHub* m_hub;
    NavTabs* m_nav;
    QLabel* m_status;
    QStackedWidget* m_stack;
    UserHomePage* m_home;
    UserSlipPage* m_slip;
    UserScoresPage* m_scores;
    UserPredictionsPage* m_predictions;
    UserTablePage* m_table;
    UserPicksPage* m_picks;
    UserMatchPage* m_match;
    int m_prev = 0;
};
