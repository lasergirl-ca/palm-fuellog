/* Rewards programmes: lookups shared by the forms, the presets install,
 * and the Programmes list and Edit Programme forms. */
#ifndef PROGRAMMES_H
#define PROGRAMMES_H
#include <PalmOS.h>
#include "records.h"
#include "sections.h"

Int16   PgmFindById(UInt8 id) FORMS_SEC;                    /* database index, or -1 */
Err     PgmGetAt(UInt16 index, ProgrammeRecord *p) FORMS_SEC;
Boolean PgmGet(UInt8 id, ProgrammeRecord *p) FORMS_SEC;     /* false when none; a default is filled in */
UInt8   PgmNextId(void) FORMS_SEC;                          /* lowest unused id 1..16, 0 when full */
UInt16  PgmUseCount(UInt8 id) FORMS_SEC;                    /* fills with an entry for this programme */
Int32   PgmBalance(UInt8 id, Int32 opening) FORMS_SEC;      /* opening + earned - redeemed over all fills */
Boolean PgmIsMoney(const ProgrammeRecord *p) FORMS_SEC;     /* counts in cents, shown as dollars */
void    PgmValueText(char *buf, const ProgrammeRecord *p, Int32 points) FORMS_SEC;  /* "3,399 pts" or "$10.20" (buf 20) */
void    PgmRateText(char *buf, UInt32 earnRate) FORMS_SEC;  /* "10" or "2.5" */
/* Install the six presets when there are no programmes and no fills yet. */
void    PgmInstallPresets(void) FORMS_SEC;

/* Fill a popup list with the live programmes, optionally with "(none)"
 * first and `alwaysId` even if archived. Arrays hold pgmMaxProgrammes + 1. */
UInt16  PgmBuildChoices(ListType *lst, char names[][pgmNameLen], char *ptrs[], UInt8 ids[],
                        Boolean includeNone, UInt8 alwaysId, UInt8 selectedId, Int16 *selOut) FORMS_SEC;

Boolean ProgrammesFormHandleEvent(EventType *event) FORMS_SEC;
Boolean EditProgrammeFormHandleEvent(EventType *event) FORMS_SEC;
extern UInt16 gEditProgrammeIndex;    /* dmMaxRecordIndex = new */

#endif
