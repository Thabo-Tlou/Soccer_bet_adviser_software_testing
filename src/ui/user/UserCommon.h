#pragma once
// Small shared types for the user-side screens.
#include <QDate>
#include <QDateTime>
#include <QString>
#include <memory>
#include "core/Fixture.h"
#include "core/PredictionService.h"

// One row on the scores list: an API fixture, a recorded result from the dataset, or a match the user picked by hand.
struct MatchItem {
    sba::Fixture fx;
    QDate day;                   // the day the match is listed under
    bool fromDataset = false;    // recorded result (no live feed)
    bool manual = false;         // built on the Predictions screen
    qint64 sortKey = 0;

    QString home() const { return QString::fromStdString(fx.homeDataset.empty() ? fx.home : fx.homeDataset); }
    QString away() const { return QString::fromStdString(fx.awayDataset.empty() ? fx.away : fx.awayDataset); }
    QString competition() const { return fx.competition.empty() ? QStringLiteral("Premier League") : QString::fromStdString(fx.competition); }
    bool hasScore() const { return fx.homeGoals >= 0 && fx.awayGoals >= 0; }
    bool canPredict() const { return !fx.homeDataset.empty() && !fx.awayDataset.empty(); }
    bool predictable() const { return canPredict() && !fx.isLive() && !fx.isFinished() && (fx.isUpcoming() || manual); }
    // Date used for storage / model requests (UTC date of kickoff for API fixtures).
    QString utcDateIso() const {
        if (fromDataset || manual || fx.kickoffEpoch <= 0) return day.toString(Qt::ISODate);
        return QDateTime::fromSecsSinceEpoch(fx.kickoffEpoch).toUTC().date().toString(Qt::ISODate);
    }
    QString key() const { return home() + "|" + away() + "|" + utcDateIso(); }
};

// Result of running the models for one match (cached by the data hub).
struct PickInfo {
    bool ok = false;
    QString error;
    std::shared_ptr<sba::Analysis> a;
    int best = 0;          // most likely outcome 0 = home, 1 = draw, 2 = away
    double p = 0.0;        // its probability
};

inline QString outcomeSymbol(int o) { return o == 0 ? "1" : (o == 1 ? "X" : "2"); }

inline QString outcomeText(const MatchItem& m, int o) {
    return o == 0 ? m.home() + " to win" : (o == 1 ? QStringLiteral("Draw") : m.away() + " to win");
}

inline QString pctText(double v) { return QString::number(v * 100.0, 'f', 0) + "%"; }
