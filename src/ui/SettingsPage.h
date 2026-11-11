#pragma once
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include "AppController.h"
#include <QComboBox>
#include "Widgets.h"

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(AppController* c, QWidget* parent = nullptr);
private:
    void load();
    AppSettings collect() const;
    AppController* m_c;
    QLineEdit *m_fdKey, *m_oddsKey, *m_comp, *m_sport, *m_regions;
    QSpinBox *m_live, *m_fix, *m_odds, *m_age, *m_minMatches;
    QDoubleSpinBox *m_ev, *m_edge, *m_dis, *m_shrink, *m_rf;
    QCheckBox *m_auto, *m_demo, *m_motion, *m_autoResults;
    QComboBox *m_zone, *m_riskProf;
    QDoubleSpinBox *m_bank, *m_maxPct, *m_minStake, *m_round;
    QLineEdit* m_cur;
    QLabel *m_zoneInfo, *m_resultsInfo;
    QLabel* m_info;
    Banner* m_banner;
};
