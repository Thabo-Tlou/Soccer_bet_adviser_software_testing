#pragma once
// ============================================================================================
//  Design tokens - the single source of truth for colour, shape, spacing and type.
//  Both the admin theme (Theme.h) and the customer theme (UserTheme.h) are built from these,
//  and so are the painted widgets (pitch, robot, crests). Change a value here, change the whole app.
//  Contrast: text on surface >= 7:1, muted text >= 4.5:1, white on primary >= 6:1 (WCAG AA/AAA).
// ============================================================================================
#include <QColor>
#include <QString>

namespace DS {

// ---- The brand palette (from the "Soccer Players" reference palette) -----------------------------------
inline const QColor paletteVioletWater{"#d1d8e6"}, paletteTransformer{"#a5adb7"}, paletteThymeGreen{"#737c6c"},
                    paletteGreenLeaf{"#4f6a30"},   paletteTurtleSkin{"#3b4021"},  paletteNightmare{"#151f12"};

// ---- Neutral scale: Violet Water -> Transformer at the light end, Turtle Skin -> Nightmare at the dark end.
//      (500 is Thyme Green darkened a touch so small text on white keeps >= 4.5:1.)
inline const QColor slate50{"#F7F9FC"},  slate100{"#EEF1F7"}, slate200{"#DEE3EE"}, slate300 = paletteVioletWater, slate400 = paletteTransformer,
                    slate500{"#5F6858"}, slate600{"#474F3C"}, slate700 = paletteTurtleSkin, slate800{"#262D1C"}, slate900 = paletteNightmare;

// ---- Brand accent: the Green Leaf scale ----------------------------------------------------------------
inline const QColor leaf50{"#F3F6EC"},  leaf100{"#E5ECD6"}, leaf200{"#CBD9B3"}, leaf300{"#A9BF86"}, leaf400{"#84A05A"},
                    leaf500{"#66843F"}, leaf600 = paletteGreenLeaf, leaf700{"#3F5526"}, leaf800{"#34451F"}, leaf900 = paletteTurtleSkin;
inline const QColor steel600{"#5F6F8F"};   // cool counterpoint derived from Violet Water (used for "away" in probability visuals)

// ---- Semantic colours (earthy, so they sit comfortably next to the greens) ---------------------------------
inline const QColor success50{"#EAF5EE"}, success600{"#2F7D4A"}, success700{"#236039"};
inline const QColor warning50{"#FBF4E4"}, warning600{"#B7791F"}, warning700{"#8A5A12"};
inline const QColor danger50{"#FBEFEA"},  danger600{"#B3402A"},  danger700{"#8F2F1D"};
inline const QColor info50{"#EBF2F7"},    info600{"#3E6A8A"},    info700{"#2F5370"};

// ---- Roles (use these in components, not the raw scales) --------------------------------------
inline const QColor bg             = slate100;   // app background
inline const QColor surface        = QColor("#FFFFFF");
inline const QColor surfaceAlt     = slate50;    // table stripes, input wells
inline const QColor border         = slate200;
inline const QColor borderStrong   = slate300;   // Violet Water
inline const QColor text           = slate900;   // Nightmare
inline const QColor textMuted      = slate600;
inline const QColor textFaint      = slate500;
inline const QColor primary        = leaf600;    // Green Leaf
inline const QColor primaryHover   = leaf700;
inline const QColor primaryPressed = leaf800;
inline const QColor primarySoft    = leaf50;     // selected / active backgrounds
inline const QColor primarySoftText= leaf800;
inline const QColor live           = QColor("#C8372D");   // "live" is always red
inline const QColor focusRing      = leaf300;

// ---- Dark feature surfaces: Nightmare -> Turtle Skin -> Green Leaf (hero, stadium, analyst panels) -------
inline const QColor stadiumTop = paletteNightmare, stadiumBottom{"#2B3320"};
inline const QColor heroFrom = paletteNightmare, heroMid = paletteTurtleSkin, heroTo = paletteGreenLeaf;

// ---- Shape, spacing, type -------------------------------------------------------------------------
inline constexpr int radiusSm = 8, radiusMd = 12, radiusLg = 16;
inline constexpr int space1 = 4, space2 = 8, space3 = 12, space4 = 16, space5 = 24, space6 = 32, space7 = 48;   // 4-pt grid
inline constexpr int fontBody = 13, fontSmall = 12, fontLabel = 11, fontH3 = 15, fontH2 = 18, fontH1 = 26, fontDisplay = 40;  // px
inline constexpr int shadowBlur = 20, shadowOffsetY = 3, shadowAlpha = 22;                                        // "elevation 1"

inline QString hex(const QColor& c) { return c.name(QColor::HexRgb); }

// Replaces "@token" placeholders in a stylesheet. Longer names are listed first so "@primaryHover" never matches "@primary".
inline QString applyTokens(QString s) {
    struct T { const char* k; QColor v; };
    const T tokens[] = {
        {"@primaryPressed", primaryPressed}, {"@primarySoftText", primarySoftText}, {"@primarySoft", primarySoft}, {"@primaryHover", primaryHover}, {"@primary", primary},
        {"@borderStrong", borderStrong}, {"@border", border}, {"@surfaceAlt", surfaceAlt}, {"@surface", surface},
        {"@textMuted", textMuted}, {"@textFaint", textFaint}, {"@text", text}, {"@focusRing", focusRing}, {"@bg", bg},
        {"@success50", success50}, {"@success600", success600}, {"@success700", success700},
        {"@warning50", warning50}, {"@warning600", warning600}, {"@warning700", warning700},
        {"@danger50", danger50}, {"@danger600", danger600}, {"@danger700", danger700},
        {"@info50", info50}, {"@info600", info600}, {"@live", live},
        {"@slate100", slate100}, {"@slate200", slate200}, {"@slate300", slate300}, {"@slate400", slate400},
        {"@slate500", slate500}, {"@slate600", slate600}, {"@slate700", slate700}, {"@slate800", slate800}, {"@slate900", slate900}, {"@slate50", slate50},
        {"@leaf100", leaf100}, {"@leaf200", leaf200}, {"@leaf300", leaf300}, {"@leaf600", leaf600}, {"@leaf700", leaf700}, {"@leaf800", leaf800}, {"@leaf50", leaf50}};
    for (const T& t : tokens) s.replace(t.k, hex(t.v));
    return s;
}

}  // namespace DS
