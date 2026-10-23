/* Vehicle lookups shared by the forms, and the two vehicle forms. */
#ifndef VEHICLES_H
#define VEHICLES_H
#include <PalmOS.h>
#include "records.h"

/* Index in the vehicle database of the vehicle with this id, or -1. */
Int16  VehFindById(UInt8 id);
/* Fills the record for id; false when there is none (a default is filled in). */
Boolean VehGet(UInt8 id, VehicleRecord *v);
UInt8  VehNextId(void);                 /* lowest unused id 1..16, 0 when full */
UInt16 VehFillCount(UInt8 id);          /* fills that reference this vehicle */
UInt32 VehLastOdometer(UInt8 id);       /* km x 10 of its latest fill, else its start odometer, else 0 */
/* The vehicle a new fill goes to: the current one, else the first live one, else 1. */
UInt8  VehDefaultForNewFill(void);

/* Fill a popup list with the live vehicles (plus `alwaysId` even if archived,
 * and "All vehicles" when includeAll). Returns the count; *selOut is the row
 * for selectedId. The arrays must hold vehMaxVehicles + 1 entries. */
UInt16 VehBuildChoices(ListType *lst, char names[][vehNameLen], char *ptrs[], UInt8 ids[],
                       Boolean includeAll, UInt8 alwaysId, UInt8 selectedId, Int16 *selOut);

Boolean VehiclesFormHandleEvent(EventType *event);
Boolean EditVehicleFormHandleEvent(EventType *event);
extern UInt16 gEditVehicleIndex;        /* dmMaxRecordIndex = new */

#endif
