#pragma once
// shared design tokens (design/Tokens.h): light surfaces with the Green Leaf accent.
#include <QColor>
#include <QFont>
#include <QString>
#include "design/Tokens.h"

namespace Theme {

// Surfaces
inline const QColor bg0 = DS::bg;               // page background
inline const QColor bg1 = DS::surface;          // top bar, inputs, white surfaces
inline const QColor bg2 = DS::surface;          // cards, tables
inline const QColor bg3 = DS::slate100;         // hover / soft fills / tracks
inline const QColor border = DS::border;
inline const QColor borderStrong = DS::borderStrong;

// Text
inline const QColor text = DS::text;
inline const QColor muted = DS::textMuted;
inline const QColor dim = DS::textFaint;

// Accent
inline const QColor primary = DS::primary;
inline const QColor primaryHover = DS::primaryHover;
inline const QColor deep = DS::leaf100;       // selected row background
inline const QColor deeper = DS::leaf50;      // soft accent background
inline const QColor tint = DS::leaf800;       // text on soft accent backgrounds

// Semantic
inline const QColor success = DS::success600;
inline const QColor danger = DS::danger600;
inline const QColor warning = DS::warning600;
inline const QColor info = DS::info600;
inline const QColor live = DS::live;

// Outcome colours for probability visuals (home / draw / away)
inline const QColor homeColor = DS::leaf600;
inline const QColor drawColor = DS::slate400;
inline const QColor awayColor = DS::steel600;

QString hex(const QColor& c);
QString stylesheet();
QFont uiFont(int pointSize = 10, QFont::Weight w = QFont::Normal);
QFont monoFont(int pointSize = 10);
QColor withAlpha(const QColor& c, int alpha);

}  // namespace Theme
