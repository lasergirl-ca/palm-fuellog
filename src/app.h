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
Coord  AppFormHeight(FormType *form);   /* the window height after AppFitForm: lay rows out to this, not the display */
extern UInt16 gDisplayChanges;          /* winDisplayChangedEvents seen, for the About diagnostics */

/* Form scrolling. Palm forms do not scroll, so the content objects move
 * under the pinned ones (the bottom buttons and anything in the title bar).
 * Call AppScrollReset in frmOpenEvent before laying out; AppScrollKey from
 * keyDownEvent for the page keys and the 5-way; AppScrollToField from
 * fldEnterEvent so a field behind the keyboard comes into view. */
void    AppScrollReset(void);
Int16   gScrollYPublic(void);   /* current offset, for a form that must unscroll before refitting */
Boolean AppScrollKey(FormType *form, const EventType *event, const UInt16 *pinnedIds, UInt16 n);
void    AppScrollToField(FormType *form, UInt16 fieldId, const UInt16 *pinnedIds, UInt16 n);
/* Shift every unpinned object by dy (negative = content moves up) and redraw. */
void    AppScrollBy(FormType *form, Int16 dy, const UInt16 *pinnedIds, UInt16 n);

/* Which fill the fill form edits: dmMaxRecordIndex for a new one. */
extern UInt16 gEditIndex;

#endif
