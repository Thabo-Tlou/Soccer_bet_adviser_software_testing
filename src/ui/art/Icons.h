#pragma once
// One consistent line-icon set (24-unit grid, rounded 1.8 stroke), painted in code so every icon can take any colour
// and stay crisp at any size. Used by both the admin sidebar and the customer navigation, buttons, toasts and empty states.
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

namespace Icons {

enum Name {
    Home, Dashboard, Matches, Analysis, Performance, Search, Settings, Table, Picks, Predict, Ball, Refresh, Check, Alert,
    Info, ArrowRight, Shield, Clock, Trophy, Download, Folder, Bolt, User, Target, Eye, Lock, Live, Calendar
};

void paint(QPainter& p, Name n, const QRectF& r, const QColor& color, double stroke = 1.8);
QPixmap pixmap(Name n, const QColor& color, int px, double stroke = 1.8);
QIcon icon(Name n, const QColor& color, int px = 20, double stroke = 1.8);

}  // namespace Icons
