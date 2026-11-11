#include "Crest.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainterPath>
#include <QStandardPaths>
#ifdef SBA_HAS_SVG
#include <QSvgRenderer>
#endif
#include <QUrl>
#include <cmath>

namespace {
using P = ClubStyle::Pattern;
struct Row { const char* name; const char* a; const char* b; P pat; const char* code; };
// Kit colours per club (dataset names). Used only for the generated fallback crest.
const Row kClubs[] = {
    {"Arsenal", "#EF0107", "#FFFFFF", P::Plain, "ARS"},   {"Aston Villa", "#670E36", "#95BFE5", P::Halves, "AVL"},
    {"Bournemouth", "#DA291C", "#111111", P::Stripes, "BOU"}, {"Brentford", "#E30613", "#FFFFFF", P::Stripes, "BRE"},
    {"Brighton", "#0057B8", "#FFFFFF", P::Stripes, "BHA"}, {"Burnley", "#6C1D45", "#99D6EA", P::Halves, "BUR"},
    {"Cardiff", "#0070B5", "#E4002B", P::Plain, "CAR"},   {"Chelsea", "#034694", "#DBA111", P::Plain, "CHE"},
    {"Coventry", "#59CBE8", "#12284C", P::Plain, "COV"},  {"Crystal Palace", "#1B458F", "#C4122E", P::Stripes, "CRY"},
    {"Everton", "#003399", "#FFFFFF", P::Plain, "EVE"},   {"Fulham", "#F4F4F4", "#111111", P::Plain, "FUL"},
    {"Huddersfield", "#0E63AD", "#FFFFFF", P::Stripes, "HUD"}, {"Hull", "#F5A12D", "#111111", P::Stripes, "HUL"},
    {"Ipswich", "#3A64A3", "#FFFFFF", P::Plain, "IPS"},   {"Leeds", "#F4F4F4", "#1D428A", P::Plain, "LEE"},
    {"Leicester", "#003090", "#FDBE11", P::Plain, "LEI"}, {"Liverpool", "#C8102E", "#F6EB61", P::Plain, "LIV"},
    {"Luton", "#F78F1E", "#13233F", P::Plain, "LUT"},     {"Man City", "#6CABDD", "#1C2C5B", P::Plain, "MCI"},
    {"Man United", "#DA291C", "#FBE122", P::Plain, "MUN"}, {"Newcastle", "#1A1A1A", "#FFFFFF", P::Stripes, "NEW"},
    {"Norwich", "#FFF200", "#00A650", P::Plain, "NOR"},   {"Nott'm Forest", "#DD0000", "#FFFFFF", P::Plain, "NFO"},
    {"Sheffield United", "#EE2737", "#FFFFFF", P::Stripes, "SHU"}, {"Southampton", "#D71920", "#FFFFFF", P::Stripes, "SOU"},
    {"Stoke", "#E03A3E", "#FFFFFF", P::Stripes, "STK"},   {"Sunderland", "#EB172B", "#FFFFFF", P::Stripes, "SUN"},
    {"Swansea", "#F4F4F4", "#111111", P::Plain, "SWA"},   {"Tottenham", "#132257", "#FFFFFF", P::Plain, "TOT"},
    {"Watford", "#FBEE23", "#111111", P::Plain, "WAT"},   {"West Brom", "#122F67", "#FFFFFF", P::Stripes, "WBA"},
    {"West Ham", "#7A263A", "#1BB1E7", P::Halves, "WHU"}, {"Wolves", "#FDB913", "#111111", P::Plain, "WOL"},
};

double luminance(const QColor& c) { return 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF(); }
}

CrestProvider& CrestProvider::instance() { static CrestProvider p; return p; }

CrestProvider::CrestProvider() {
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/crests";
    QDir().mkpath(m_cacheDir);
    m_notify.setSingleShot(true);
    m_notify.setInterval(120);
    connect(&m_notify, &QTimer::timeout, this, &CrestProvider::crestsChanged);
    scanDirs();
}

QString CrestProvider::normalise(const QString& team) {
    QString out;
    for (const QChar c : team.toLower()) if (c.isLetterOrNumber()) out += c;
    return out;
}

void CrestProvider::setLogoDirs(const QStringList& dirs) { m_dirs = dirs; m_missing.clear(); scanDirs(); m_notify.start(); }

void CrestProvider::scanDirs() {
    m_localFiles.clear();
    QStringList dirs = m_dirs; dirs << m_cacheDir;
    for (const QString& d : dirs) {
        const auto files = QDir(d).entryInfoList({"*.png", "*.svg", "*.webp", "*.jpg", "*.jpeg"}, QDir::Files);
        for (const QFileInfo& fi : files) {
            const QString k = normalise(fi.completeBaseName());
            if (!m_localFiles.contains(k) || d != m_cacheDir) m_localFiles.insert(k, fi.absoluteFilePath());   // user folders win over cache
        }
    }
}

QImage CrestProvider::decode(const QByteArray& bytes, const QString& hint) {
    if (bytes.isEmpty()) return {};
    const bool svg = hint.endsWith(".svg", Qt::CaseInsensitive) || bytes.left(400).contains("<svg");
    if (svg) {
#ifdef SBA_HAS_SVG
        QSvgRenderer r(bytes);
        if (!r.isValid()) return {};
        QSizeF sz = r.defaultSize(); if (sz.isEmpty()) sz = QSizeF(1, 1);
        const double s = 256.0 / std::max(sz.width(), sz.height());
        QImage img(int(std::ceil(sz.width() * s)), int(std::ceil(sz.height() * s)), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img); r.render(&p); p.end();
        return img;
#else
        return {};                       // built without the Qt Svg module: SVG logos are skipped
#endif
    }
    QImage img; img.loadFromData(bytes);
    if (img.isNull()) return {};
    if (std::max(img.width(), img.height()) > 384) img = img.scaled(384, 384, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void CrestProvider::registerUrl(const QString& team, const QString& url) {
    const QString key = normalise(team);
    if (key.isEmpty() || url.isEmpty()) return;
    m_urls.insert(key, url);
    if (m_real.contains(key) || m_localFiles.contains(key) || m_requested.contains(key)) return;
    fetch(key, url);
}

void CrestProvider::fetch(const QString& key, const QString& url) {
    const QUrl u(url);
    if (!u.isValid() || (u.scheme() != "https" && u.scheme() != "http")) return;
    m_requested.insert(key);
    QNetworkRequest req(u);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SoccerBetAdviser/1.0");
    QNetworkReply* rep = m_net.get(req);
    connect(rep, &QNetworkReply::finished, this, [this, rep, key, url] {
        rep->deleteLater();
        if (rep->error() != QNetworkReply::NoError) return;
        const QByteArray bytes = rep->readAll();
        QString ext = QFileInfo(QUrl(url).path()).suffix().toLower();
        if (ext != "svg" && ext != "png" && ext != "webp" && ext != "jpg") ext = bytes.left(400).contains("<svg") ? "svg" : "png";
        const QImage img = decode(bytes, "." + ext);
        if (img.isNull()) return;
        QFile f(m_cacheDir + "/" + key + "." + ext);
        if (f.open(QIODevice::WriteOnly)) { f.write(bytes); f.close(); }
        m_real.insert(key, img);
        m_notify.start();
    });
}

const QImage* CrestProvider::realImage(const QString& team) {
    const QString key = normalise(team);
    if (auto it = m_real.constFind(key); it != m_real.constEnd()) return &it.value();
    if (m_missing.contains(key)) return nullptr;
    if (auto lf = m_localFiles.constFind(key); lf != m_localFiles.constEnd()) {
        QFile f(lf.value());
        if (f.open(QIODevice::ReadOnly)) {
            const QImage img = decode(f.readAll(), lf.value());
            if (!img.isNull()) return &m_real.insert(key, img).value();
        }
    }
    m_missing.insert(key);
    return nullptr;
}

bool CrestProvider::hasRealLogo(const QString& team) { return realImage(team) != nullptr; }

ClubStyle CrestProvider::styleFor(const QString& team) {
    ClubStyle s;
    for (const Row& r : kClubs) {
        if (team.compare(r.name, Qt::CaseInsensitive) == 0) {
            s.primary = QColor(r.a); s.secondary = QColor(r.b); s.pattern = r.pat; s.code = r.code;
            return s;
        }
    }
    // Unknown club: stable colour from the name, code from the first letters.
    const uint h = qHash(team.toLower());
    s.primary = QColor::fromHsl(int(h % 360), 150, 70);
    s.secondary = QColor::fromHsl(int((h / 7) % 360), 120, 200);
    s.pattern = ClubStyle::Plain;
    QString code;
    for (const QString& w : team.split(' ', Qt::SkipEmptyParts)) if (code.size() < 3) code += w[0].toUpper();
    s.code = code.size() >= 2 ? code : team.left(3).toUpper();
    return s;
}

void CrestProvider::paintGenerated(QPainter& p, const QRectF& rect, const ClubStyle& s, bool onDark) {
    const double d = std::min(rect.width(), rect.height());
    const QPointF c = rect.center();
    const double R = d / 2.0 - 0.6;
    if (R < 3) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    if (R > 12 && !onDark) { p.setBrush(QColor(0, 0, 0, 34)); p.drawEllipse(c + QPointF(0, R * 0.07), R, R); }
    p.setBrush(s.secondary);
    p.drawEllipse(c, R, R);
    const double ri = R * 0.84;
    QPainterPath disc; disc.addEllipse(c, ri, ri);
    p.save();
    p.setClipPath(disc);
    p.fillRect(QRectF(c.x() - ri, c.y() - ri, ri * 2, ri * 2), s.primary);
    p.setBrush(s.secondary);
    const double band = ri * 0.36;
    switch (s.pattern) {
    case ClubStyle::Stripes:
        for (int i = -4; i <= 4; i += 2) p.drawRect(QRectF(c.x() + i * band - band / 2, c.y() - ri, band, ri * 2));
        break;
    case ClubStyle::Hoops:
        for (int i = -4; i <= 4; i += 2) p.drawRect(QRectF(c.x() - ri, c.y() + i * band - band / 2, ri * 2, band));
        break;
    case ClubStyle::Halves:
        p.drawRect(QRectF(c.x(), c.y() - ri, ri, ri * 2));
        break;
    case ClubStyle::Sash: {
        QTransform t; t.translate(c.x(), c.y()); t.rotate(-35);
        p.setTransform(t, true); p.drawRect(QRectF(-ri * 1.4, -band * 0.7, ri * 2.8, band * 1.4));
        break; }
    case ClubStyle::Plain: {
        // thin inner ring in the secondary colour so plain crests still read as badges
        p.setBrush(Qt::NoBrush); p.setPen(QPen(s.secondary, std::max(1.0, ri * 0.07)));
        p.drawEllipse(c, ri * 0.8, ri * 0.8);
        break; }
    }
    QLinearGradient g(c.x(), c.y() - ri, c.x(), c.y() + ri);
    g.setColorAt(0, QColor(255, 255, 255, 70)); g.setColorAt(0.5, QColor(255, 255, 255, 0)); g.setColorAt(1, QColor(0, 0, 0, 40));
    p.resetTransform(); p.setPen(Qt::NoPen); p.setBrush(g); p.drawEllipse(c, ri, ri);
    p.restore();

    if (R >= 9) {
        QFont f = p.font(); f.setBold(true); f.setPixelSize(std::max(6, int(R * (s.code.size() > 2 ? 0.58 : 0.7)))); f.setLetterSpacing(QFont::PercentageSpacing, 96);
        p.setFont(f);
        const QRectF tr(c.x() - ri, c.y() - ri * 0.42, ri * 2, ri * 0.84);
        if (s.pattern != ClubStyle::Plain) {
            const QFontMetricsF fm(f);
            const double w = fm.horizontalAdvance(s.code) + R * 0.34, h = fm.height() * 0.86;
            p.setBrush(QColor(8, 12, 10, 150)); p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(c.x() - w / 2, c.y() - h / 2, w, h), h * 0.3, h * 0.3);
            p.setPen(Qt::white);
        } else {
            p.setPen(luminance(s.primary) > 0.58 ? QColor("#0B1410") : QColor("#FFFFFF"));
        }
        p.drawText(tr, Qt::AlignCenter, s.code);
    }
    p.restore();
}

void CrestProvider::paint(QPainter& p, const QRectF& r, const QString& team, bool onDark) {
    if (const QImage* img = realImage(team)) {
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        const double d = std::min(r.width(), r.height());
        if (onDark) {                      // keep dark crests readable on dark surfaces
            p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, 235));
            p.drawEllipse(r.center(), d / 2, d / 2);
        }
        const double inner = onDark ? d * 0.78 : d;
        QSizeF sz = img->size(); sz.scale(inner, inner, Qt::KeepAspectRatio);
        p.drawImage(QRectF(r.center().x() - sz.width() / 2, r.center().y() - sz.height() / 2, sz.width(), sz.height()), *img);
        p.restore();
        return;
    }
    paintGenerated(p, r, styleFor(team), onDark);
}

QPixmap CrestProvider::pixmap(const QString& team, int px, bool onDark) {
    const qreal dpr = 2.0;
    QPixmap pm(int(px * dpr), int(px * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paint(p, QRectF(0, 0, px, px), team, onDark);
    return pm;
}

QIcon CrestProvider::icon(const QString& team, int px) { return QIcon(pixmap(team, px)); }
