#pragma once
#include <QTableWidget>
#include <QWidget>
#include "AppController.h"
#include "Widgets.h"

class PerformancePage : public QWidget {
    Q_OBJECT
public:
    explicit PerformancePage(AppController* c, QWidget* parent = nullptr);
private:
    void showBacktest();
    void showHistory();
    void updateResults();
    AppController* m_c;
    QPushButton *m_run, *m_settle, *m_plain, *m_aware;
    QLabel *m_status, *m_prospective, *m_bettingNote;
    StatTile *m_t1, *m_t2, *m_t3, *m_t4;
    QTableWidget *m_models, *m_betting, *m_history, *m_goals;
    QLabel* m_goalsNote;
    CalibrationChart* m_cal;
    ConfusionWidget* m_conf;
    Banner* m_banner;
    bool m_showAware = true;
};
