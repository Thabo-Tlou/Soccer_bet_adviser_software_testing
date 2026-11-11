#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QTableWidget>
#include <QWidget>
#include "AppController.h"
#include "Widgets.h"

class MatchesPage : public QWidget {
    Q_OBJECT
public:
    explicit MatchesPage(AppController* c, QWidget* parent = nullptr);
signals:
    void openAnalysis(const sba::Fixture& f);
private:
    void rebuild();
    void updateStatus();
    void showDetail();
    AppController* m_c;
    QComboBox* m_statusFilter;
    QLineEdit* m_teamFilter;
    QDateEdit *m_from, *m_to;
    QCheckBox *m_dateOn, *m_auto;
    QTableWidget *m_table, *m_books;
    TeamVs* m_vs;
    bool m_recorded = false;
    Banner* m_banner;
    QLabel *m_updated, *m_detailTitle, *m_detailNote;
    QPushButton* m_analyse;
    std::vector<sba::Fixture> m_rows;
};
