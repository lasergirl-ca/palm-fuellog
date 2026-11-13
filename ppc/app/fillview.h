/* What the fill list shows, worked out from the databases: which fills,
 * each row's five columns as text, and the footer. The same rules as the
 * Palm's list (src/fill_list.c). Plain C over store/db.h, host-tested;
 * the window only draws what this returns. Text is Palm Latin (cp1252). */
#ifndef FILLVIEW_H
#define FILLVIEW_H
#include "../../src/ptypes.h"
#include "../../src/records.h"
#include "../../src/prefs.h"

#define fillViewCols 5          /* date, odometer, volume, total, economy */
#define fillViewText 24

/* Database indexes of the fills that show, newest first: all, or one vehicle's (255 = all). */
UInt16 FillViewCollect(UInt8 vehicle, UInt16 *index, UInt16 max);
/* Economy closing at the fill at database index k: L/100 km x 10, or kWh/100 km x 10
 * for a charge; 0 for a partial fill, a first fill or a break in the chain. */
UInt32 FillViewRowEconomy(UInt16 k, const FillRecord *f);
/* The list's figure x 10: distance per unit (km/L, MPG, km or miles per kWh). */
UInt32 FillViewEcoDisplay(UInt32 eco, int charge, const FuelPrefs *p);
/* The economy column's three-letter title. */
const char *FillViewEcoHeader(int electric, const FuelPrefs *p);
/* 1 when the selected vehicle is electric: the footer's average is over charges. */
int FillViewElectric(const FuelPrefs *p);
/* The row's columns for the fill at database index k; returns 0 when it cannot be read. */
int FillViewRow(UInt16 k, const FuelPrefs *p, char point, char thousands, char col[fillViewCols][fillViewText]);
/* "Avg 11.8 KPL" (empty when unknown) and "Nov $85.71" for today's month. */
void FillViewFooter(const FuelPrefs *p, UInt16 today, char point, char thousands, char *avg, char *month);

#endif
