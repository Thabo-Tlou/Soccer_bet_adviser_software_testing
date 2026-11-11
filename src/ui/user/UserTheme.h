#pragma once
// Customer-side look: clean white surfaces, slate text, Green Leaf accent. All values come from design/Tokens.h.
// Live is always red, success green, everything interactive Green Leaf.
#include <QColor>
#include <QFont>
#include <QString>
#include "ui/design/Tokens.h"

namespace UT {

inline const QColor ink = DS::text;                 // primary text
inline const QColor black = DS::text;               // legacy name for primary text
inline const QColor white = DS::surface;
inline const QColor page = DS::slate50;             // app background
inline const QColor band = DS::slate50;            // group headers, notice bars
inline const QColor line = DS::border;             // separators
inline const QColor hover = QColor("#F6F9F0");      // row hover (a whisper of green)
inline const QColor muted = DS::textFaint;
inline const QColor soft = DS::textMuted;
inline const QColor accent = DS::primary;
inline const QColor accentDark = DS::primaryHover;
inline const QColor accentSoft = DS::primarySoft;
inline const QColor accentOnDark = DS::leaf300;
inline const QColor success = DS::success600;
inline const QColor danger = DS::danger600;
inline const QColor live = DS::live;

QFont font(int px, bool bold = false);
QString hex(const QColor& c);
QString stylesheet();

}  // namespace UT
