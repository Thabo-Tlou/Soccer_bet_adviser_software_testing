// Design-system checks: palette contrast (WCAG) and stylesheet integrity. Needs QtGui only (no display).
#include <QColor>
#include <cmath>
#include <cstdio>
#include "ui/Theme.h"
#include "ui/design/Tokens.h"
#include "ui/user/UserTheme.h"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++g_fail; } } while (0)

static double lin(double c) { return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
static double lum(const QColor& c) { return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF()); }
static double contrast(const QColor& a, const QColor& b) { const double x = lum(a), y = lum(b); return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05); }

static void pair(const char* name, const QColor& fg, const QColor& bg, double min) {
    const double r = contrast(fg, bg);
    std::printf("  %-34s %5.2f:1  (need %.1f)%s\n", name, r, min, r >= min ? "" : "   <-- TOO LOW");
    CHECK(r >= min);
}

int main() {
    using namespace DS;
    std::printf("Contrast\n");
    pair("text on surface", text, surface, 7.0);
    pair("text on app background", text, bg, 7.0);
    pair("muted text on surface", textMuted, surface, 7.0);
    pair("faint text on surface", textFaint, surface, 4.5);
    pair("faint text on app background", textFaint, bg, 4.5);
    pair("faint text on slate50", textFaint, slate50, 4.5);
    pair("white on primary (buttons)", surface, primary, 4.5);
    pair("white on primary hover", surface, primaryHover, 4.5);
    pair("primary text on surface (links)", primary, surface, 4.5);
    pair("primarySoftText on primarySoft", primarySoftText, primarySoft, 7.0);
    pair("primary text on primarySoft", primary, primarySoft, 4.5);
    pair("white on live (LIVE chips)", surface, live, 4.5);
    pair("live text on surface", live, surface, 4.5);
    pair("success700 on success50", success700, success50, 4.5);
    pair("success600 on surface", success600, surface, 4.5);
    pair("warning700 on warning50", warning700, warning50, 4.5);
    pair("danger700 on danger50", danger700, danger50, 4.5);
    pair("danger600 on surface", danger600, surface, 4.5);
    pair("info700 on info50", info700, info50, 4.5);
    pair("white on danger600", surface, danger600, 4.5);
    pair("white on steel600 (away bar)", surface, steel600, 4.5);
    pair("white on slate500 (draw bar)", surface, slate500, 4.5);
    pair("white on stadium surface", surface, stadiumBottom, 7.0);
    pair("violet water on hero start", paletteVioletWater, heroFrom, 7.0);
    pair("violet water on hero middle", paletteVioletWater, heroMid, 4.5);
    pair("leaf200 on hero start", leaf200, heroFrom, 7.0);
    pair("white on hero end (green leaf)", surface, heroTo, 4.5);
    pair("leaf800 on white (hero button)", leaf800, surface, 7.0);

    std::printf("Palette: the six brand colours are present and unchanged\n");
    CHECK(paletteVioletWater.name() == "#d1d8e6" && paletteTransformer.name() == "#a5adb7" && paletteThymeGreen.name() == "#737c6c");
    CHECK(paletteGreenLeaf.name() == "#4f6a30" && paletteTurtleSkin.name() == "#3b4021" && paletteNightmare.name() == "#151f12");
    CHECK(primary == paletteGreenLeaf && text == paletteNightmare && borderStrong == paletteVioletWater && slate400 == paletteTransformer);

    std::printf("Stylesheets\n");
    const QString a = Theme::stylesheet(), u = UT::stylesheet();
    CHECK(a.size() > 2000 && u.size() > 1500);
    CHECK(!a.contains('@') && !u.contains('@'));              // every @token was substituted
    CHECK(DS::applyTokens("@slate500 @slate50 @primaryHover @primary @textMuted @text") == QString("%1 %2 %3 %4 %5 %6").arg(hex(slate500), hex(slate50), hex(primaryHover), hex(primary), hex(textMuted), hex(text)));

    std::printf(g_fail ? "\n%d check(s) FAILED\n" : "\nAll design checks passed\n", g_fail);
    return g_fail ? 1 : 0;
}
