#include <QApplication>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QTimer>
#include "ui/AppController.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/user/UserTheme.h"
#include "ui/user/UserWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SoccerBetAdviser");
    QCoreApplication::setApplicationName("SoccerBetAdviser");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setFont(Theme::uiFont(10));

    AppController controller;
    UserWindow user(&controller);     // what customers see: scores, predictions, table, picks
    MainWindow admin(&controller);    // the admin panel: data keys, thresholds, performance, analysis tools

    // The two sides use different style sheets, so the application style is swapped when the side changes.
    auto showUser = [&]() { app.setStyleSheet(UT::stylesheet()); admin.hide(); user.show(); };
    auto showAdmin = [&]() { app.setStyleSheet(Theme::stylesheet()); user.hide(); admin.show(); };
    QObject::connect(&user, &UserWindow::adminRequested, &app, showAdmin);
    QObject::connect(&admin, &MainWindow::userViewRequested, &app, showUser);
    QObject::connect(&controller, &AppController::settingsRequested, &app, [&]() { showAdmin(); admin.openSettings(); });

    const QStringList args = QCoreApplication::arguments();
    const int ai = args.indexOf("--screenshots");
    const int ui = args.indexOf("--user-screenshots");
    if (ai >= 0 && ai + 1 < args.size()) showAdmin(); else showUser();
    controller.start();               // loads models/data on a worker thread; the UI stays responsive

    // Developer aids:  --screenshots <folder> (admin)  /  --user-screenshots <folder> (user side) render pages to PNG and exit.
    if (ai >= 0 && ai + 1 < args.size()) {
        const QString dir = args.at(ai + 1);
        QObject::connect(&controller, &AppController::backtestFinished, &app, [&, dir]() {
            QTimer::singleShot(1500, &app, [&, dir]() { admin.captureScreens(dir); app.quit(); });
        });
    } else if (ui >= 0 && ui + 1 < args.size()) {
        const QString dir = args.at(ui + 1);
        QObject::connect(&controller, &AppController::initialised, &app, [&, dir]() {
            QTimer::singleShot(1500, &app, [&, dir]() { user.captureScreens(dir); app.quit(); });
        });
    }
    return app.exec();
}
