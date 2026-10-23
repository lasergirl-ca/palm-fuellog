/* SP3 sample graph: the six full-to-full economy points from the mock-ups,
 * drawn at double density when the device has it. */
#ifndef GRAPH_H
#define GRAPH_H
#include <PalmOS.h>
#include "sections.h"

/* Draw the graph inside plot (standard coordinates). selected is the index
 * of the point to call out, or -1. */
void GraphDraw(const RectangleType *plot, Int16 selected) SEC2;

/* Index of the point nearest a pen position (standard coordinates), or -1
 * when the pen is outside plot. */
Int16 GraphHitTest(const RectangleType *plot, Coord x, Coord y) SEC2;

#endif
