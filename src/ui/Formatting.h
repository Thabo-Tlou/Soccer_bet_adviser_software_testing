#pragma once
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTimeZone>
#include <cmath>

namespace Fmt {
inline QString pct(double v, int d = 1) { return std::isfinite(v) ? QString("%1%").arg(v * 100.0, 0, 'f', d) : QStringLiteral("-"); }
inline QString signedPct(double v, int d = 1) { return std::isfinite(v) ? QString("%1%2%").arg(v >= 0 ? "+" : "").arg(v * 100.0, 0, 'f', d) : QStringLiteral("-"); }
inline QString num(double v, int d = 2) { return std::isfinite(v) ? QString::number(v, 'f', d) : QStringLiteral("-"); }
// ---- Time zone: every time shown in the app is converted to the zone chosen in Settings (default UTC+2, Africa/Maseru).
inline QTimeZone& zoneRef() { static QTimeZone z(QByteArray("UTC")); return z; }
inline void setZone(const QString& id) {
    QTimeZone z(id.toUtf8());
    if (!z.isValid()) z = QTimeZone(2 * 3600);              // fixed UTC+2 if the id is unknown on this system
    zoneRef() = z;
}
inline QDateTime local(qint64 epoch) { return QDateTime::fromSecsSinceEpoch(epoch, Qt::UTC).toTimeZone(zoneRef()); }
inline QDate today() { return QDateTime::currentDateTimeUtc().toTimeZone(zoneRef()).date(); }
inline QString zoneLabel() {
    const int off = zoneRef().offsetFromUtc(QDateTime::currentDateTimeUtc());
    const int a = std::abs(off) / 60;
    return QString("UTC%1%2%3").arg(off < 0 ? "-" : "+").arg(a / 60).arg(a % 60 ? QString(":%1").arg(a % 60, 2, 10, QChar('0')) : QString());
}
// The bundled dataset stores kickoff times in UK local time (BST/GMT). This converts them to a real UTC epoch.
inline qint64 ukEpoch(int daySerial, int timeMin) {
    const QDate d = QDate(1970, 1, 1).addDays(daySerial);
    const int tm = timeMin >= 0 ? timeMin : 12 * 60;
    const QTimeZone uk(QByteArray("Europe/London"));
    const QDateTime dt = uk.isValid() ? QDateTime(d, QTime(tm / 60, tm % 60), uk) : QDateTime(d, QTime(tm / 60, tm % 60), Qt::UTC);
    return dt.toSecsSinceEpoch();
}
inline QString kickoff(qint64 epoch) { return epoch > 0 ? local(epoch).toString("ddd d MMM  HH:mm") : QStringLiteral("-"); }
inline QString ago(qint64 epoch) {
    if (epoch <= 0) return QStringLiteral("never");
    const qint64 s = QDateTime::currentSecsSinceEpoch() - epoch;
    if (s < 60) return QStringLiteral("just now");
    if (s < 3600) return QString("%1 min ago").arg(s / 60);
    if (s < 86400) return QString("%1 h ago").arg(s / 3600);
    return QString("%1 d ago").arg(s / 86400);
}
inline QString html(const QString& s) { return s.toHtmlEscaped(); }
}  // namespace Fmt
