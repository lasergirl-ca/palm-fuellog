/* Rewards programmes without the window: balances, the list's rows and the
 * rules of the Palm's Edit Programme form (src/programmes.c). Plain C over
 * store/db.h, host-tested. Text is Palm Latin (cp1252). */
#ifndef PGMEDIT_H
#define PGMEDIT_H
#include "../../src/ptypes.h"
#include "../../src/records.h"

#define pgmEditNew 0xFFFF
#define peText 16

UInt8  PgmNextId(void);                         /* 1 to 16, or 0 when all sixteen are taken */
UInt16 PgmUseCount(UInt8 id);                   /* fills with an entry for this programme */
Int32  PgmBalance(UInt8 id, Int32 opening);     /* opening + earned - redeemed over all fills */
/* Points expiring from today to the end of next month (first in, first out); *when is the first such date. */
Int32  PgmExpiring(const ProgrammeRecord *p, UInt16 today, UInt16 *when);
int    PgmIsMoney(const ProgrammeRecord *p);    /* counts in cents, shown as dollars */
void   PgmValueText(char *buf, const ProgrammeRecord *p, Int32 points, char point, char thousands);  /* "3,399 pts" or "$10.20"; 24 bytes */
void   PgmRateText(char *buf, UInt32 earnRate, char point);    /* "10" or "2.5" */
/* The Palm's six presets, when the log has no programmes and no fill has points: returns how many went in. */
UInt8  PgmInstallPresets(UInt8 homeCurrency);

/* Database indexes of the programmes, most used first and archived last. */
UInt16 PgmListCollect(UInt16 *index, UInt16 max);
/* One row: the name, the balance, and a note: what expires soon, else the earn rule. 48 bytes each. */
int PgmListRow(UInt16 dbIndex, UInt16 today, char point, char thousands, char *name, char *balance, char *note);

/* What the form holds. */
typedef struct {
    char name[pgmNameLen], earn[peText], premium[peText], block[peText], value[peText];
    char maxVol[peText], cap[peText], months[peText], opening[peText], aliases[pgmAliasLen];
    UInt8 currency, money, basis, payment, autoApply, redeem, volUnit, expiry;
} PgmEditForm;

typedef struct {
    ProgrammeRecord pgm;
    UInt16 index;               /* database index, or pgmEditNew */
} PgmEdit;

/* Start on the programme at index, or a new one in the home currency. Returns 0 when sixteen exist. */
int PgmEditBegin(PgmEdit *e, UInt16 index, UInt8 homeCurrency, char point, char thousands, PgmEditForm *f);
const char *PgmEditEarnLabel(int money);        /* "Earn pts:" or "Earn c:" */
const char *PgmEditOpeningLabel(int money);     /* "Opening pts:" or "Opening $:" */
const char *PgmEditValueLabel(UInt8 redeem);    /* "= $:" or "= c off:" */
/* "Balance now 3,399 pts, about $3.40", or empty for a new programme. 64 bytes. */
void PgmEditStatus(const PgmEdit *e, char point, char thousands, char *buf);
/* What the archive action is: NULL for a new programme, else "Restore", "Archive" (it has fills) or "Delete". */
const char *PgmEditArchiveLabel(const PgmEdit *e);

#define peSaved      0
#define peNeedName   1
#define peBadNumber  2
#define peSaveFailed 3
Int32 PgmEditSave(PgmEdit *e, const PgmEditForm *f, char point, char thousands);
/* The archive action, on the programme as stored (unsaved edits are dropped, as on the Palm). */
Int32 PgmEditArchive(PgmEdit *e);

#endif
