#pragma once
#include <QTableWidget>
#include <QWidget>
#include "AppController.h"
#include "Widgets.h"

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(AppController* c, QWidget* parent = nullptr);
signals:
    void openAnalysis(const sba::Fixture& f);
    void gotoPage(int index);
private:
    void refresh();
    void refreshFixtures();
    void refreshStatus();
    AppController* m_c;
    StatTile *m_model, *m_data, *m_tracked, *m_quality;
    QTableWidget* m_table;
    Card* m_fxCard;
    QLabel *m_status, *m_note;
    std::vector<sba::Fixture> m_shown;
    int m_generation = 0;
};
