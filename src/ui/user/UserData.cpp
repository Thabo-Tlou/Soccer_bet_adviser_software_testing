#include "UserData.h"
#include "ui/Formatting.h"
#include "core/DateUtil.h"
#include <QSettings>
#include <algorithm>
#include <set>

static QSettings favSettings() { return QSettings("SoccerBetAdviser", "SoccerBetAdviser"); }

UserDataHub::UserDataHub(AppController* c, QObject* parent) : QObject(parent), m_c(c) {
    const QStringList saved = favSettings().value("user/favourites").toStringList();
    m_favs = QSet<QString>(saved.begin(), saved.end());
    connect(c, &AppController::initialised, this, [this]() { reindex(); emit dataChanged(); });
    connect(c->live(), &LiveDataService::fixturesChanged, this, [this]() { emit dataChanged(); });
    connect(c->live(), &LiveDataService::demoTick, this, [this]() { emit dataChanged(); });
    connect(c, &AppController::settingsChanged, this, [this]() { m_picks.clear(); emit dataChanged(); });
    reindex();
}

void UserDataHub::reindex() {
    m_byDate.clear();
    m_indexed = false;
    if (!m_c->ready()) return;
    const auto& ms = m_c->service().dataset().matches();
    for (size_t i = 0; i < ms.size(); ++i) m_byDate[ms[i].date].push_back(static_cast<int>(i));
    m_indexed = true;
}

// ------------------------------------------------------------------ matches

MatchItem UserDataHub::fromFixture(const sba::Fixture& f) const {
    MatchItem it;
    it.fx = f;
    it.day = Fmt::local(f.kickoffEpoch).date();
    it.sortKey = f.kickoffEpoch;
    return it;
}

MatchItem UserDataHub::fromHist(const sba::HistMatch& h) const {
    MatchItem it;
    sba::Fixture f;
    f.id = "dataset";
    f.competition = "Premier League";
    f.home = f.homeDataset = h.home;
    f.away = f.awayDataset = h.away;
    f.status = "FINISHED";
    f.homeGoals = h.fthg; f.awayGoals = h.ftag;
    f.kickoffEpoch = Fmt::ukEpoch(h.date, h.timeMin);        // dataset times are UK local time
    f.source = "dataset";
    it.fx = f;
    it.day = dateOf(h.date);
    it.fromDataset = true;
    it.sortKey = f.kickoffEpoch;
    return it;
}

std::vector<MatchItem> UserDataHub::matchesOn(const QDate& day) const {
    std::vector<MatchItem> out;
    std::set<QString> apiKeys;
    for (const auto& f : m_c->live()->fixtures()) {
        if (f.kickoffEpoch <= 0) continue;
        MatchItem it = fromFixture(f);
        apiKeys.insert(it.key());
        if (it.day == day) out.push_back(it);
    }
    if (m_indexed) {
        auto found = m_byDate.find(serialOf(day));
        if (found != m_byDate.end()) {
            const auto& ms = m_c->service().dataset().matches();
            for (int idx : found->second) {
                MatchItem it = fromHist(ms[static_cast<size_t>(idx)]);
                if (apiKeys.count(it.key())) continue;      // the live feed wins over the recorded copy
                out.push_back(it);
            }
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const MatchItem& a, const MatchItem& b) { return a.sortKey < b.sortKey; });
    return out;
}

std::vector<MatchItem> UserDataHub::upcoming(int days) const {
    std::vector<MatchItem> out;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto& f : m_c->live()->fixtures()) {
        if (!f.isUpcoming() || f.kickoffEpoch <= 0) continue;
        if (f.kickoffEpoch < now - 7200 || f.kickoffEpoch > now + static_cast<qint64>(days) * 86400) continue;
        out.push_back(fromFixture(f));
    }
    std::sort(out.begin(), out.end(), [](const MatchItem& a, const MatchItem& b) { return a.sortKey < b.sortKey; });
    return out;
}

QDate UserDataHub::lastResultDate() const {
    return m_c->ready() ? dateOf(m_c->service().dataset().lastDate()) : QDate();
}

bool UserDataHub::featured(MatchItem& out) const {
    std::vector<MatchItem> all = matchesOn(Fmt::today());
    for (const auto& m : upcoming(7)) all.push_back(m);
    if (all.empty()) all = matchesOn(defaultDate());
    const MatchItem* best = nullptr; int bestScore = -1;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto& m : all) {
        int score = 0;
        if (m.fx.isLive()) score = 1000;
        else if (m.fx.isUpcoming()) score = 500 - int(std::min<qint64>(400, std::max<qint64>(0, (m.fx.kickoffEpoch - now) / 3600)));
        else if (m.fx.isFinished()) score = 100 + std::max(0, m.fx.homeGoals) + std::max(0, m.fx.awayGoals);
        else continue;
        if (score > bestScore) { bestScore = score; best = &m; }
    }
    if (!best) return false;
    out = *best;
    return true;
}

QDate UserDataHub::defaultDate() const {
    if (hasFixtures() || demoOn()) return Fmt::today();
    const QDate last = lastResultDate();
    return last.isValid() ? last : Fmt::today();
}

// ------------------------------------------------------------------ favourites

void UserDataHub::toggleFav(const QString& key) {
    if (m_favs.contains(key)) m_favs.remove(key); else m_favs.insert(key);
    QSettings q = favSettings();
    q.setValue("user/favourites", QStringList(m_favs.begin(), m_favs.end()));
    emit favsChanged();
}

// ------------------------------------------------------------------ predictions

QString UserDataHub::pickKey(const MatchItem& m) {
    QString k = m.key();
    double b[3];
    if (m.fx.bestOdds(b)) k += QString("#%1/%2/%3").arg(b[0], 0, 'f', 2).arg(b[1], 0, 'f', 2).arg(b[2], 0, 'f', 2);
    return k;
}

const PickInfo* UserDataHub::pick(const MatchItem& m) const {
    auto it = m_picks.constFind(pickKey(m));
    return it == m_picks.constEnd() ? nullptr : &it.value();
}

void UserDataHub::requestPick(const MatchItem& m) {
    if (!m_c->ready() || !m.predictable()) return;
    const QString key = pickKey(m);
    if (m_picks.contains(key) || m_pending.contains(key)) return;

    sba::AnalysisRequest req;
    req.home = m.home().toStdString();
    req.away = m.away().toStdString();
    req.kickoffDateIso = m.utcDateIso().toStdString();
    req.kickoffIso = m.fx.kickoffIso;
    req.kickoffEpoch = m.manual ? 0 : m.fx.kickoffEpoch;
    req.nowEpoch = QDateTime::currentSecsSinceEpoch();
    if (m.manual) req.matchStarted = m.day.addDays(1).startOfDay().toSecsSinceEpoch() <= req.nowEpoch;
    else req.matchStarted = m.fx.kickoffEpoch > 0 && m.fx.kickoffEpoch <= req.nowEpoch;
    double best[3];
    if (m.fx.bestOdds(best)) {
        req.hasOdds = true;
        for (int i = 0; i < 3; ++i) req.odds.odds[i] = best[i];
        req.odds.manual = m.manual;
        req.odds.source = m.manual ? "manual entry" : "The Odds API (best price)";
        req.odds.fetchedAtEpoch = m.manual ? 0 : m.fx.oddsFetchedEpoch;
    }
    m_pending.insert(key);
    m_c->analyse(req, [this, key](bool ok, const sba::Analysis& a, const QString& err) {
        m_pending.remove(key);
        PickInfo pi;
        pi.ok = ok;
        pi.error = err;
        if (ok) {
            pi.a = std::make_shared<sba::Analysis>(a);
            const auto& p = a.reasoning.model;
            pi.best = static_cast<int>(std::max_element(p.begin(), p.end()) - p.begin());
            pi.p = p[static_cast<size_t>(pi.best)];
        }
        m_picks.insert(key, pi);
        emit pickReady(key);
    });
}

bool UserDataHub::savePick(const MatchItem& m, QString* message) {
    const PickInfo* pi = pick(m);
    if (!pi || !pi->ok || !pi->a) { if (message) *message = "No prediction to save yet."; return false; }
    const auto& r = pi->a->reasoning;
    const auto& e = pi->a->evidence;
    sba::PredictionRecord rec;
    rec.home = e.home; rec.away = e.away; rec.kickoffIso = e.kickoffIso;
    rec.matchKey = m.key().toStdString();
    rec.createdIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    rec.modelVersion = m_c->service().modelVersion();
    rec.hasRf = e.rfAvailable; rec.rf = e.rf; rec.poisson = e.poisson; rec.finalP = e.blended;
    rec.hasMarket = r.hasMarket; rec.market = r.market;
    if (e.hasOdds) {
        for (int i = 0; i < 3; ++i) rec.odds[i] = e.odds.odds[i];
        rec.oddsSource = e.odds.source;
        rec.oddsFetchedAt = e.odds.fetchedAtEpoch;
    }
    rec.decision = sba::decisionLabel(r.decision);
    rec.candidate = r.candidate;
    if (r.candidate >= 0 && r.hasMarket) { rec.edge = r.edge[static_cast<size_t>(r.candidate)]; rec.ev = r.ev[static_cast<size_t>(r.candidate)]; }
    rec.explanation = r.headline;
    bool created = false;
    const auto id = m_c->store().insertPrediction(rec, &created);
    if (id <= 0) { if (message) *message = "Could not save: " + QString::fromStdString(m_c->store().error()); return false; }
    if (created) m_c->notifyPredictionsChanged();
    if (message) *message = created ? "Saved to My picks." : "Already in My picks - the original was kept.";
    return true;
}

int UserDataHub::settleFromData() {
    if (!m_indexed) return 0;
    int n = 0;
    const auto& ms = m_c->service().dataset().matches();
    const std::string now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    for (const auto& r : m_c->store().all()) {
        if (r.actual >= 0 || r.matchKey.size() < 10) continue;
        const QDate d = QDate::fromString(QString::fromStdString(r.matchKey.substr(r.matchKey.size() - 10)), Qt::ISODate);
        if (!d.isValid()) continue;
        for (int off = -1; off <= 1; ++off) {
            auto f = m_byDate.find(serialOf(d) + off);
            if (f == m_byDate.end()) continue;
            bool done = false;
            for (int idx : f->second) {
                const auto& x = ms[static_cast<size_t>(idx)];
                if (x.home == r.home && x.away == r.away && m_c->store().settle(r.id, x.target(), now)) { ++n; done = true; break; }
            }
            if (done) break;
        }
    }
    if (n) m_c->notifyPredictionsChanged();
    return n;
}

// ------------------------------------------------------------------ history

QString UserDataHub::seasonLabel(int s) { return QString("%1/%2").arg(s).arg((s + 1) % 100, 2, 10, QChar('0')); }

std::vector<int> UserDataHub::seasons() const {
    std::set<int, std::greater<int>> s;
    if (m_c->ready()) for (const auto& m : m_c->service().dataset().matches()) s.insert(m.season);
    return std::vector<int>(s.begin(), s.end());
}

std::vector<TableRow> UserDataHub::table(int season) const {
    std::map<QString, TableRow> rows;
    if (!m_c->ready()) return {};
    for (const auto& m : m_c->service().dataset().matches()) {
        if (m.season != season) continue;
        const QString h = QString::fromStdString(m.home), a = QString::fromStdString(m.away);
        TableRow& H = rows[h]; TableRow& A = rows[a];
        H.team = h; A.team = a;
        ++H.played; ++A.played;
        H.gf += m.fthg; H.ga += m.ftag; A.gf += m.ftag; A.ga += m.fthg;
        if (m.ftr == 'H') { ++H.won; ++A.lost; H.points += 3; H.form += 'W'; A.form += 'L'; }
        else if (m.ftr == 'A') { ++A.won; ++H.lost; A.points += 3; A.form += 'W'; H.form += 'L'; }
        else { ++H.drawn; ++A.drawn; ++H.points; ++A.points; H.form += 'D'; A.form += 'D'; }
    }
    // Finished matches from the live feed that are newer than the bundled history (only when a key is configured).
    {
        std::set<std::string> have;
        for (const auto& m : m_c->service().dataset().matches()) have.insert(std::to_string(m.date) + "|" + m.home + "|" + m.away);
        std::vector<const sba::Fixture*> extra;
        for (const auto& f : m_c->live()->fixtures())
            if (!f.demo && f.isFinished() && !f.homeDataset.empty() && !f.awayDataset.empty() && f.homeGoals >= 0 && f.awayGoals >= 0 && f.kickoffEpoch > 0) extra.push_back(&f);
        std::sort(extra.begin(), extra.end(), [](const sba::Fixture* a, const sba::Fixture* b) { return a->kickoffEpoch < b->kickoffEpoch; });
        for (const sba::Fixture* f : extra) {
            const int serial = static_cast<int>(f->kickoffEpoch / 86400);
            if (sba::seasonStartYearFor(serial) != season) continue;
            if (have.count(std::to_string(serial) + "|" + f->homeDataset + "|" + f->awayDataset)) continue;
            const QString h = QString::fromStdString(f->homeDataset), a = QString::fromStdString(f->awayDataset);
            TableRow& H = rows[h]; TableRow& A = rows[a];
            H.team = h; A.team = a; ++H.played; ++A.played;
            H.gf += f->homeGoals; H.ga += f->awayGoals; A.gf += f->awayGoals; A.ga += f->homeGoals;
            if (f->homeGoals > f->awayGoals) { ++H.won; ++A.lost; H.points += 3; H.form += 'W'; A.form += 'L'; }
            else if (f->homeGoals < f->awayGoals) { ++A.won; ++H.lost; A.points += 3; A.form += 'W'; H.form += 'L'; }
            else { ++H.drawn; ++A.drawn; ++H.points; ++A.points; H.form += 'D'; A.form += 'D'; }
        }
    }
    std::vector<TableRow> out;
    for (auto& kv : rows) { if (kv.second.form.size() > 5) kv.second.form = kv.second.form.right(5); out.push_back(kv.second); }
    std::sort(out.begin(), out.end(), [](const TableRow& a, const TableRow& b) {
        if (a.points != b.points) return a.points > b.points;
        if (a.gd() != b.gd()) return a.gd() > b.gd();
        if (a.gf != b.gf) return a.gf > b.gf;
        return a.team < b.team;
    });
    return out;
}

std::vector<sba::HistMatch> UserDataHub::h2h(const QString& home, const QString& away, int n) const {
    std::vector<sba::HistMatch> out;
    if (!m_c->ready()) return out;
    const std::string h = home.toStdString(), a = away.toStdString();
    const auto& ms = m_c->service().dataset().matches();
    for (auto it = ms.rbegin(); it != ms.rend() && static_cast<int>(out.size()) < n; ++it)
        if ((it->home == h && it->away == a) || (it->home == a && it->away == h)) out.push_back(*it);
    return out;
}

QString UserDataHub::form(const QString& team, const QDate& before, int n) const {
    QString f;
    if (!m_c->ready()) return f;
    const std::string t = team.toStdString();
    const int cut = serialOf(before);
    const auto& ms = m_c->service().dataset().matches();
    for (auto it = ms.rbegin(); it != ms.rend() && f.size() < n; ++it) {
        if (it->date >= cut) continue;
        if (it->home == t) f += it->ftr == 'H' ? 'W' : (it->ftr == 'D' ? 'D' : 'L');
        else if (it->away == t) f += it->ftr == 'A' ? 'W' : (it->ftr == 'D' ? 'D' : 'L');
    }
    std::reverse(f.begin(), f.end());
    return f;
}

const sba::HistMatch* UserDataHub::findHist(const MatchItem& m) const {
    if (!m_indexed) return nullptr;
    const QDate d = QDate::fromString(m.utcDateIso(), Qt::ISODate);
    if (!d.isValid()) return nullptr;
    const auto& ms = m_c->service().dataset().matches();
    const std::string h = m.home().toStdString(), a = m.away().toStdString();
    for (int off = -1; off <= 1; ++off) {
        auto f = m_byDate.find(serialOf(d) + off);
        if (f == m_byDate.end()) continue;
        for (int idx : f->second) {
            const auto& x = ms[static_cast<size_t>(idx)];
            if (x.home == h && x.away == a) return &x;
        }
    }
    return nullptr;
}
