/* Vehicle lookups shared by the forms, and the two vehicle forms. */
#ifndef VEHICLES_H
#define VEHICLES_H
#include <PalmOS.h>
#include "records.h"
#include "sections.h"

/* Index in the vehicle database of the vehicle with this id, or -1. */
Int16  VehFindById(UInt8 id) FORMS_SEC;
/* Fills the record for id; false when there is none (a default is filled in). */
Boolean VehGet(UInt8 id, VehicleRecord *v) FORMS_SEC;
UInt8  VehNextId(void) FORMS_SEC;                 /* lowest unused id 1..16, 0 when full */
UInt16 VehFillCount(UInt8 id) FORMS_SEC;          /* fills that reference this vehicle */
UInt32 VehLastOdometer(UInt8 id) FORMS_SEC;       /* km x 10 of its latest fill, else its start odometer, else 0 */
/* The vehicle a new fill goes to: the current one, else the first live one, else 1. */
UInt8  VehDefaultForNewFill(void) FORMS_SEC;

/* Fill a popup list with the live vehicles (plus `alwaysId` even if archived,
 * and "All vehicles" when includeAll). Returns the count; *selOut is the row
 * for selectedId. The arrays must hold vehMaxVehicles + 1 entries. */
UInt16 VehBuildChoices(ListType *lst, char names[][vehNameLen], char *ptrs[], UInt8 ids[],
                       Boolean includeAll, UInt8 alwaysId, UInt8 selectedId, Int16 *selOut) FORMS_SEC;

Boolean VehiclesFormHandleEvent(EventType *event) FORMS_SEC;
Boolean EditVehicleFormHandleEvent(EventType *event) FORMS_SEC;
extern UInt16 gEditVehicleIndex;        /* dmMaxRecordIndex = new */

#endif
