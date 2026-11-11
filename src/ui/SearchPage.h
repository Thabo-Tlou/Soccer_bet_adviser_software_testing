#pragma once
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QWidget>
#include "AppController.h"
#include "Widgets.h"

class SearchPage : public QWidget {
    Q_OBJECT
public:
    explicit SearchPage(AppController* c, QWidget* parent = nullptr);
private:
    void search();
    void updateInfo();
    AppController* m_c;
    QLineEdit* m_query;
    QListWidget* m_list;
    QPlainTextEdit* m_preview;
    QLabel *m_info, *m_path;
    std::vector<sba::SearchHit> m_hits;
};
