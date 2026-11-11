#pragma once
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include "UserWidgets.h"

class PitchWidget;
class MarketsPanel;

// Match screen: scoreboard header, then Prediction / Odds / H2H / Stats tabs.
class UserMatchPage : public QWidget {
    Q_OBJECT
public:
    explicit UserMatchPage(UserDataHub* hub, QWidget* parent = nullptr);
    void showMatch(const MatchItem& m);
    bool isManual() const { return m_item.manual; }
    void showTab(const QString& name);
    void devFillOdds();
signals:
    void backRequested();
    void slipLegAdded(bool replaced);
private:
    void applyUpdate();
    void buildBody();
    void buildPrediction(QVBoxLayout* v);
    void buildMarkets(QVBoxLayout* v);
    void buildOdds(QVBoxLayout* v);
    void buildH2H(QVBoxLayout* v);
    void buildStats(QVBoxLayout* v);
    QStringList tabNames() const;

    UserDataHub* m_hub;
    MatchItem m_item;
    bool m_hasItem = false;
    QLabel* m_context;
    PitchWidget* m_header;
    QPushButton* m_follow;
    void refreshHeader();
    TextTabs* m_tabs;
    QScrollArea* m_scroll;
    MarketsPanel* m_markets = nullptr;
    QString m_tabName = "Prediction";
};
