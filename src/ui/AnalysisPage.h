#pragma once
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QTextBrowser>
#include <QWidget>
#include "AppController.h"
#include "Widgets.h"
#include "art/PitchWidget.h"
#include "art/RobotWidget.h"
#include <QElapsedTimer>

class MarketsPanel;
class AnalysisPage : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisPage(AppController* c, QWidget* parent = nullptr);
    void loadFixture(const sba::Fixture& f);
    void devFillMarketOdds();      // prefill from the Matches screen and run
private:
    void populateTeams();
    void runAnalysis();
    void showResult(const sba::Analysis& a);
    void saveCurrent();
    bool buildRequest(sba::AnalysisRequest& req);
    AppController* m_c;
    QComboBox *m_home, *m_away;
    QDateEdit* m_date;
    QDoubleSpinBox *m_o1, *m_oX, *m_o2;
    QLabel *m_oddsInfo, *m_headline;
    PitchWidget* m_stage;
    MarketsPanel* m_markets;
    RobotWidget* m_robot;
    QElapsedTimer m_runTimer;
    void updateStage();
    Badge* m_badge;
    QPushButton *m_run, *m_save;
    Banner* m_banner;
    OutcomeBar *m_bars[3];
    QLabel *m_metrics[5];
    QTableWidget* m_compare;
    QTextBrowser* m_explain;
    QWidget* m_results;
    bool m_apiOdds = false;
    sba::Fixture m_fixture;
    bool m_hasFixture = false;
    sba::Analysis m_last;
    bool m_hasLast = false;
    std::string m_lastDateIso;
};
