#include "ResultsUpdater.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

ResultsUpdater::ResultsUpdater(QObject* parent) : QObject(parent) {
    m_guard.setSingleShot(true);
    connect(&m_guard, &QTimer::timeout, this, [this]() { done(false, "Timed out - using the results already saved."); });
}

QString ResultsUpdater::seasonCode(const QDate& d) {
    const int start = d.month() >= 7 ? d.year() : d.year() - 1;      // seasons start in August
    return QString("%1%2").arg(start % 100, 2, 10, QChar('0')).arg((start + 1) % 100, 2, 10, QChar('0'));
}

QString ResultsUpdater::cacheDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/results"; }

QStringList ResultsUpdater::cachedFiles() {
    QStringList out;
    const auto files = QDir(cacheDir()).entryInfoList({"E0_*.csv"}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) out << fi.absoluteFilePath();
    return out;
}

bool ResultsUpdater::looksLikeResultsCsv(const QByteArray& bytes) {
    if (bytes.size() < 200) return false;
    const QByteArray head = bytes.left(600);
    return head.contains("HomeTeam") && head.contains("FTHG") && head.contains("Date");
}

qint64 ResultsUpdater::lastSuccessEpoch() const {
    qint64 best = 0;
    for (const QString& f : cachedFiles()) best = std::max<qint64>(best, QFileInfo(f).lastModified().toSecsSinceEpoch());
    return best;
}

void ResultsUpdater::done(bool ok, const QString& msg) {
    if (!m_running) return;
    m_running = false;
    m_guard.stop();
    emit finished(ok, msg);
}

void ResultsUpdater::fetch(const QString& baseUrl, int timeoutMs) {
    if (m_running) return;
    m_running = true;
    QDir().mkpath(cacheDir());
    const QString code = seasonCode(QDate::currentDate());
    const QString base = qEnvironmentVariableIsSet("SBA_RESULTS_URL") ? qEnvironmentVariable("SBA_RESULTS_URL") : baseUrl;   // test hook
    QNetworkRequest req(QUrl(base + "/" + code + "/E0.csv"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(timeoutMs);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SoccerBetAdviser/2.0");
    m_guard.start(timeoutMs + 1500);
    QNetworkReply* rep = m_net.get(req);
    connect(rep, &QNetworkReply::finished, this, [this, rep, code]() {
        rep->deleteLater();
        if (!m_running) return;                                      // already timed out
        if (rep->error() != QNetworkReply::NoError) { done(false, "Could not download latest results (" + rep->errorString() + "). Using saved results."); return; }
        const QByteArray bytes = rep->readAll();
        if (!looksLikeResultsCsv(bytes)) { done(false, "The results download was not in the expected format. Using saved results."); return; }
        QFile f(cacheDir() + "/E0_" + code + ".csv");
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { done(false, "Could not save the downloaded results."); return; }
        f.write(bytes); f.close();
        done(true, "Latest results downloaded.");
    });
}
