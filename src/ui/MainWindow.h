#pragma once
#include <QButtonGroup>
#include <QMainWindow>
#include <QStackedWidget>
#include "AppController.h"
#include "AnalysisPage.h"
#include "Widgets.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppController* c, QWidget* parent = nullptr);
    // Developer aid: renders every page to PNG files in `dir` (used by --screenshots).
    void captureScreens(const QString& dir);
    void openSettings() { goTo(5); }
signals:
    void userViewRequested();
private:
    void goTo(int index);
    void refreshChips();
    AppController* m_c;
    QStackedWidget* m_stack;
    QLabel *m_crumb, *m_statusText, *m_statusRight;
    Badge *m_modelChip, *m_apiChip;
    QButtonGroup* m_nav;
    AnalysisPage* m_analysis;
};
