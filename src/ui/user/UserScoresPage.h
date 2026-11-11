#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include "UserWidgets.h"

// Home screen: date strip, filters and the match list grouped by competition (livescore style).
class UserScoresPage : public QWidget {
    Q_OBJECT
public:
    UserScoresPage(UserDataHub* hub, QWidget* parent = nullptr);
    void showLatest();
signals:
    void openMatch(const MatchItem& m);
private:
    void rebuild();
    void updateNotice();
    void updateHero(const std::vector<MatchItem>& all);
    UserDataHub* m_hub;
    DateStrip* m_dates;
    TextTabs* m_filter;
    QLineEdit* m_search;
    QFrame* m_notice;
    QLabel* m_noticeText;
    QPushButton *m_noticeConnect, *m_noticeDemo;
    QScrollArea* m_scroll;
    QWidget* m_heroBox;
    PitchWidget* m_hero;
    MatchItem m_heroItem;
    bool m_hasHero = false;
    QVBoxLayout* m_content = nullptr;
    QWidget* m_list = nullptr;
    QTimer m_debounce;
    bool m_userNavigated = false;
};
