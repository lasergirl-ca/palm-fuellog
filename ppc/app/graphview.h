/* Graphs without the window: the points, their range and mean, the series,
 * and the axis labels, as the Palm's Graphs form works them out
 * (src/graphs_form.c). Economy per full-to-full segment, price per fill, or
 * spend per month; one series per vehicle under All vehicles (spend pools
 * the fleet). Plain C over store/db.h, host-tested; the window only draws. */
#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H
#include "../../src/ptypes.h"
#include "../../src/records.h"
#include "../../src/prefs.h"

#define gvEconomy 0
#define gvPrice   1
#define gvSpend   2
#define gvMax     256
#define gvSeries  8
#define gvLabels  8

typedef struct {
    UInt8 kind;
    UInt8 electric;             /* economy of one electric car: kWh/100 km, the unit cannot be chosen */
    UInt16 n;
    Int32 x[gvMax];             /* days since 1904, or the month index from firstMonth */
    Int32 y[gvMax];             /* economy or price in the display unit x 10 (price: ten-thousandths), spend in cents */
    UInt8 s[gvMax];             /* series per point */
    UInt8 seriesCount, seriesId[gvSeries];
    char seriesName[gvSeries][vehNameLen];
    Int32 min, max, mean, xmin, xmax;   /* min < max and xmin < xmax once there is a point */
    UInt16 firstMonth;          /* year x 12 + month - 1 of spend's first month */
} GraphData;

void GraphViewCollect(GraphData *g, UInt8 kind, const FuelPrefs *p);
/* A value as the graph labels it: "8.5", "176.9" or "$1.789", "$85.71". 20 bytes. */
void GraphViewValueText(const GraphData *g, const FuelPrefs *p, Int32 v, char point, char *buf);
/* "Aug 10: 8.5" or "Nov 2026: $85.71" for point k. 40 bytes. */
void GraphViewPointText(const GraphData *g, const FuelPrefs *p, UInt16 k, char point, char *buf);
/* "Low 8.1, high 9.4, mean 8.6". 64 bytes. */
void GraphViewSummary(const GraphData *g, const FuelPrefs *p, char point, char *buf);
/* The economy unit's name over the graph ("L/100 km", or "kWh/100 km" for an electric car). */
const char *GraphViewUnit(const GraphData *g, const FuelPrefs *p);
/* Labels along the x axis: where (in x units) and what ("Aug", or the first and last dates). Returns how many. */
UInt8 GraphViewXLabels(const GraphData *g, Int32 at[gvLabels], char text[gvLabels][12]);
/* The point nearest (px, py) given the plot's mapping, within maxDist2 (squared): its index, or -1.
 * The window passes its own mapping as x and y arrays already in pixels. */
Int16 GraphViewNearest(const Int32 *px, const Int32 *py, UInt16 n, Int32 tx, Int32 ty, Int32 maxDist2);

#endif
