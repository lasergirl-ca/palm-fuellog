#include "graph.h"
#include "device.h"

/* Day offset from Aug 10 and L/100 km x 10, from the fill list mock-up. */
static const Int16 kDay[]   = { 0, 12, 24, 36, 59, 70 };
static const Int16 kValue[] = { 85, 83, 82, 83, 81, 84 };
static const char *kLabel[] = { "Aug 10", "Aug 22", "Sep 3", "Sep 15", "Oct 8", "Oct 19" };
#define kPoints 6
#define kMean   83
#define kLow    80   /* axis range 8.0 to 8.6 */
#define kHigh   86

/* Plot geometry in native (possibly double) coordinates. */
typedef struct {
    Coord x0, y0, x1, y1;   /* axes box */
    Coord s;                /* scale: 1 or 2 */
} Geom;

static Coord PointX(const Geom *g, Int16 i)
{
    return g->x0 + (Coord)(((Int32)(g->x1 - g->x0) * kDay[i]) / kDay[kPoints - 1]);
}

static Coord PointY(const Geom *g, Int16 value)
{
    return g->y1 - (Coord)(((Int32)(g->y1 - g->y0) * (value - kLow)) / (kHigh - kLow));
}

static void DrawText(const char *s, Coord x, Coord y)
{
    WinDrawChars(s, StrLen(s), x, y);
}

static void FormatTenths(char *out, Int16 v)
{
    StrIToA(out, v / 10);
    StrCat(out, ".");
    StrIToA(out + StrLen(out), v % 10);
}

void GraphDraw(const RectangleType *plot, Int16 selected)
{
    UInt16 oldSys = 0;
    Geom g;
    RectangleType r;
    Int16 i;
    char buf[24];
    Coord fh;

    if (DeviceHasHiRes())
        oldSys = WinSetCoordinateSystem(kCoordinatesDouble);
    g.s = DeviceHasHiRes() ? 2 : 1;

    r.topLeft.x = plot->topLeft.x * g.s;
    r.topLeft.y = plot->topLeft.y * g.s;
    r.extent.x = plot->extent.x * g.s;
    r.extent.y = plot->extent.y * g.s;
    WinEraseRectangle(&r, 0);

    FntSetFont(stdFont);
    fh = FntLineHeight();

    /* Leave room for the y labels on the left and the x labels below. */
    g.x0 = r.topLeft.x + FntCharsWidth("8.6", 3) + 3 * g.s;
    g.x1 = r.topLeft.x + r.extent.x - 4 * g.s;
    g.y0 = r.topLeft.y + fh;              /* room for a callout above */
    g.y1 = r.topLeft.y + r.extent.y - fh - 2 * g.s;

    /* Axes and y labels. */
    WinDrawLine(g.x0, g.y0, g.x0, g.y1);
    WinDrawLine(g.x0, g.y1, g.x1, g.y1);
    DrawText("8.6", r.topLeft.x, g.y0 - fh / 2);
    DrawText("8.0", r.topLeft.x, g.y1 - fh / 2);

    /* Dashed mean. */
    WinDrawGrayLine(g.x0 + 1, PointY(&g, kMean), g.x1, PointY(&g, kMean));
    FormatTenths(buf, kMean);
    DrawText(buf, r.topLeft.x, PointY(&g, kMean) - fh / 2);

    /* The line and its points. */
    for (i = 0; i < kPoints; i++) {
        Coord x = PointX(&g, i), y = PointY(&g, kValue[i]);
        RectangleType dot;
        if (i > 0)
            WinDrawLine(PointX(&g, i - 1), PointY(&g, kValue[i - 1]), x, y);
        dot.topLeft.x = x - g.s; dot.topLeft.y = y - g.s;
        dot.extent.x = 2 * g.s + 1; dot.extent.y = 2 * g.s + 1;
        WinDrawRectangle(&dot, 0);
    }

    /* X labels at the first, middle and last point. */
    DrawText("Aug", PointX(&g, 0), g.y1 + 2 * g.s);
    DrawText("Sep", PointX(&g, 2) - FntCharsWidth("Sep", 3) / 2, g.y1 + 2 * g.s);
    DrawText("Oct", PointX(&g, 5) - FntCharsWidth("Oct", 3), g.y1 + 2 * g.s);

    /* Callout for the selected point, drawn above the plot. */
    if (selected >= 0 && selected < kPoints) {
        Coord x = PointX(&g, selected), y = PointY(&g, kValue[selected]);
        RectangleType ring;
        Coord w;
        StrCopy(buf, kLabel[selected]);
        StrCat(buf, ": ");
        FormatTenths(buf + StrLen(buf), kValue[selected]);
        w = FntCharsWidth(buf, StrLen(buf));
        ring.topLeft.x = x - 3 * g.s; ring.topLeft.y = y - 3 * g.s;
        ring.extent.x = 6 * g.s + 1; ring.extent.y = 6 * g.s + 1;
        WinDrawRectangleFrame(roundFrame, &ring);
        if (x + w > g.x1) x = g.x1 - w;
        DrawText(buf, x, r.topLeft.y);
    }

    if (DeviceHasHiRes())
        WinSetCoordinateSystem(oldSys);
}

Int16 GraphHitTest(const RectangleType *plot, Coord x, Coord y)
{
    Geom g;
    Int16 i, best = -1;
    Coord bestDist = 0;

    if (!RctPtInRectangle(x, y, plot))
        return -1;
    /* Same geometry in standard coordinates (scale 1, label widths approximated). */
    g.s = 1;
    g.x0 = plot->topLeft.x + 18;
    g.x1 = plot->topLeft.x + plot->extent.x - 4;
    g.y0 = g.y1 = 0;
    for (i = 0; i < kPoints; i++) {
        Coord d = PointX(&g, i) - x;
        if (d < 0) d = -d;
        if (best < 0 || d < bestDist) { best = i; bestDist = d; }
    }
    return best;
}
