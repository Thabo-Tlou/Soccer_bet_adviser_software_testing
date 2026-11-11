#pragma once
// Bet slip: the selections you added from the Markets tabs, with the stake, the potential return and clear advice.
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include "UserData.h"
#include "UserWidgets.h"

class UserSlipPage : public QWidget {
    Q_OBJECT
public:
    explicit UserSlipPage(UserDataHub* hub, QWidget* parent = nullptr);
signals:
    void browseRequested();
    void countChanged(int legs);
private:
    void refresh();
    void rebuildLegs();
    void refreshSummary();
    QString money(double v) const;
    double shrink() const;

    UserDataHub* m_hub;
    AppController* m_c;
    QWidget* m_legsHost;
    QVBoxLayout* m_legsLayout;
    QWidget* m_summaryCard;
    QLabel *m_title, *m_count, *m_headline, *m_rows, *m_warn, *m_suggest;
    QDoubleSpinBox* m_stake;
    QPushButton *m_use, *m_clear;
    QCheckBox* m_singles;
    QString m_sig;
};
