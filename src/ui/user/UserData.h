#pragma once
// Everything the user screens need from the application: merged match lists, favourites,
// cached predictions, league table and head-to-head history. UI code never touches the models directly.
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <map>
#include <vector>
#include "UserCommon.h"
#include "ui/AppController.h"

struct TableRow {
    QString team;
    int played = 0, won = 0, drawn = 0, lost = 0, gf = 0, ga = 0, points = 0;
    QString form;          // last results, oldest first: W / D / L
    int gd() const { return gf - ga; }
};

class UserDataHub : public QObject {
    Q_OBJECT
public:
    explicit UserDataHub(AppController* c, QObject* parent = nullptr);
    AppController* controller() const { return m_c; }

    // ---- matches
    std::vector<MatchItem> matchesOn(const QDate& day) const;
    std::vector<MatchItem> upcoming(int days) const;      // API fixtures only
    QDate defaultDate() const;                            // today, or the latest recorded result when nothing is live
    QDate lastResultDate() const;
    bool featured(MatchItem& out) const;                  // the most relevant match right now: live, else next up, else best recent result
    bool hasFixtures() const {                             // real (non-demo) fixtures present?
        for (const auto& f : m_c->live()->fixtures()) if (!f.demo) return true;
        return false;
    }
    bool demoOn() const { return m_c->settings().demoLive; }

    // ---- favourites
    bool isFav(const QString& key) const { return m_favs.contains(key); }
    void toggleFav(const QString& key);

    // ---- predictions
    const PickInfo* pick(const MatchItem& m) const;
    void requestPick(const MatchItem& m);
    bool savePick(const MatchItem& m, QString* message);
    int settleFromData();                                 // marks saved picks whose result is already in the dataset

    // ---- history
    std::vector<int> seasons() const;                     // newest first
    static QString seasonLabel(int season);
    std::vector<TableRow> table(int season) const;
    std::vector<sba::HistMatch> h2h(const QString& home, const QString& away, int n) const;
    QString form(const QString& team, const QDate& before, int n) const;
    const sba::HistMatch* findHist(const MatchItem& m) const;

    static int serialOf(const QDate& d) { return static_cast<int>(d.toJulianDay() - QDate(1970, 1, 1).toJulianDay()); }
    static QDate dateOf(int serial) { return QDate(1970, 1, 1).addDays(serial); }

signals:
    void dataChanged();
    void favsChanged();
    void pickReady(const QString& key);

private:
    void reindex();
    static QString pickKey(const MatchItem& m);
    MatchItem fromHist(const sba::HistMatch& h) const;
    MatchItem fromFixture(const sba::Fixture& f) const;

    AppController* m_c;
    QSet<QString> m_favs;
    QHash<QString, PickInfo> m_picks;
    QSet<QString> m_pending;
    std::map<int, std::vector<int>> m_byDate;   // date serial -> indices into dataset matches
    bool m_indexed = false;
};
