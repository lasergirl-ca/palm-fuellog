/* Shared application state and small UI helpers (device only). */
#ifndef APP_H
#define APP_H
#include <PalmOS.h>
#include "prefs.h"
#include "records.h"

extern FuelPrefs gPrefs;
extern char gThousands, gPoint;    /* from the Formats panel */

void   AppSetFieldText(FormType *form, UInt16 fieldId, const char *text);
const char *AppGetFieldText(FormType *form, UInt16 fieldId);
void   AppSetLabel(FormType *form, UInt16 labelId, const char *text);
UInt16 AppToday(void);                           /* days since 1904 */
UInt16 AppNowMinutes(void);                      /* minutes since midnight */
void   AppTimeText(char *buf, UInt16 minutes);   /* per the Formats panel, buf >= timeStringLength */
void   AppSetupDIA(FormType *form);              /* custom policy, 160 to 225 rows */
void   AppFitForm(FormType *form, const UInt16 *bottomIds, UInt16 n);  /* resize and pin controls */

/* Which fill the fill form edits: dmMaxRecordIndex for a new one. */
extern UInt16 gEditIndex;

#endif
