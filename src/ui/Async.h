#pragma once
// Runs `work` on the thread pool and delivers its result to `done` on the UI thread.
#include <QFutureWatcher>
#include <QtConcurrent>
#include <type_traits>

template <typename Work, typename Done>
void runAsync(QObject* context, Work work, Done done) {
    using R = std::invoke_result_t<Work>;
    auto* watcher = new QFutureWatcher<R>(context);
    QObject::connect(watcher, &QFutureWatcherBase::finished, context, [watcher, done]() {
        done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(work));
}
