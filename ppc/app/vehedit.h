/* Vehicles without the window: the list's rows and the rules of the Palm's
 * Edit Vehicle form (src/vehicles.c). Plain C over store/db.h, host-tested.
 * Text is Palm Latin (cp1252). */
#ifndef VEHEDIT_H
#define VEHEDIT_H
#include "../../src/ptypes.h"
#include "../../src/records.h"
#include "../../src/prefs.h"

#define vehEditNew 0xFFFF
#define veText 16

/* What the form holds. */
typedef struct {
    char name[vehNameLen];
    char start[veText];         /* the odometer tracking began at, in the vehicle's unit */
    char tank[veText];          /* tank in litres, or the battery in kWh for an electric car */
    char homeRate[veText];      /* cents per kWh */
    UInt8 powertrain, odoUnit, grade, tripMode, archived;
    UInt8 currency;             /* 0 = use Prefs, else cur* + 1 */
} VehEditForm;

typedef struct {
    VehicleRecord veh;
    UInt16 index;               /* database index, or vehEditNew */
} VehEdit;

UInt8  VehNextId(void);                 /* 1 to 16, or 0 when all sixteen are taken */
UInt16 VehFillCount(UInt8 id);
UInt32 VehLastOdometer(UInt8 id);       /* km x 10: the last fill's, else the starting reading */

/* Database indexes of the vehicles to list, in the order they were made. */
UInt16 VehListCollect(int showArchived, UInt16 *index, UInt16 max);
/* One row: name (with "(archived)"), "Fuel" / "Hybrid" / "Plug-in" / "Electric",
 * the fill count and the last odometer in the Prefs unit. Each at least 40 bytes. */
int VehListRow(UInt16 dbIndex, const FuelPrefs *p, char point, char thousands, char *name, char *kind, char *fills, char *odo);

/* Start on the vehicle at index, or a new one. Returns 0 when a new one cannot be made (sixteen already). */
int VehEditBegin(VehEdit *e, UInt16 index, char point, VehEditForm *f);
const char *VehEditTankLabel(UInt8 powertrain);     /* "Tank (L):" or "Battery kWh:" */
int VehEditShowsGrade(UInt8 powertrain);            /* not for an electric car */
int VehEditShowsHomeRate(UInt8 powertrain);         /* electric and plug-in */
/* "" for a new vehicle or one with no fills; else "12 fills: archive it instead of deleting". 48 bytes. */
void VehEditStatus(const VehEdit *e, char *buf);
int VehEditCanDelete(const VehEdit *e);

#define veSaved      0
#define veNeedName   1
#define veBadNumber  2
#define veSaveFailed 3
Int32 VehEditSave(VehEdit *e, const VehEditForm *f, char point, char thousands, FuelPrefs *prefs);
/* Only a vehicle with no fills; the list goes back to All vehicles if it showed this one. */
Int32 VehEditDelete(VehEdit *e, FuelPrefs *prefs);

#endif
