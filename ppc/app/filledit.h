/* New Fill and Edit Fill, without the window: the rules of the Palm's form
 * (src/fill_form.c) over the text the six number fields hold. Any two of
 * volume, price and total give the third; a blank odometer comes from the
 * trip; a new fill earns its auto-apply programmes. Plain C over
 * store/db.h, host-tested; the window copies text in and out and shows
 * what these calls return. Text is Palm Latin (cp1252). */
#ifndef FILLEDIT_H
#define FILLEDIT_H
#include "../../src/ptypes.h"
#include "../../src/records.h"
#include "../../src/prefs.h"

#define fillEditNew 0xFFFF      /* index of a fill not yet in the database */
#define feText 16

/* The three linked quantities, as bits. */
#define feVolume 1
#define fePrice  2
#define feTotal  4

/* What the number fields hold, as typed. */
typedef struct {
    char odo[feText], trip[feText], volume[feText], price[feText], total[feText], rate[feText];
} FillEditText;

typedef struct {
    UInt8 pgm;
    Int32 earned, redeemed;
    UInt8 autoEarned;           /* earned follows the volume and price; a stored value stays as stored */
} FillEditPoints;

typedef struct {
    FillRecord fill;            /* the record as loaded or begun; Save completes it */
    VehicleRecord veh;          /* the fill's vehicle: odometer unit, powertrain, home rate */
    UInt16 index;               /* database index, or fillEditNew */
    UInt32 prevOdo;             /* the vehicle's reading before this fill, km x 10; 0 = none */
    UInt8 order[3];             /* the quantities in the order they were typed, newest last */
    UInt8 entryVol;             /* unitVol* the volume and price are typed in */
    UInt8 charge;               /* a charging session: kWh and a price per kWh */
    FillEditPoints pts[fillMaxPoints];
    UInt8 ptsCount, ptsTouched;
    UInt32 discount;            /* cents taken off by redemptions */
    FuelPrefs *prefs;           /* Save keeps the last payment, currency and rate here */
    char point, thousands;
} FillEdit;

/* A log with no vehicle gets one, "My car", so a first fill can be saved
 * before the Vehicles slice exists. Returns 1 when it made one. */
int FillEditEnsureVehicle(void);

/* Start on the fill at index, or a new one (fillEditNew) dated today at
 * `minutes` past midnight. Fills t with what the fields show. */
void FillEditBegin(FillEdit *e, UInt16 index, FuelPrefs *prefs, UInt16 today, UInt16 minutes, char point, char thousands, FillEditText *t);

/* ---- what the form shows around the fields ---- */
const char *FillEditTitle(const FillEdit *e);          /* "New Fill", "Edit Charge", ... */
const char *FillEditVolumeLabel(const FillEdit *e);    /* "Volume:" or "Energy kWh:" */
void        FillEditPriceLabel(const FillEdit *e, char *buf);   /* "Price c/L:", at least 16 bytes */
const char *FillEditGradeLabel(const FillEdit *e);     /* "Grade:" or "Where:" */
const char *FillEditFullLabel(const FillEdit *e);      /* "Full tank" or "Charged to full" */
const char *FillEditCashLabel(const FillEdit *e);      /* "Cash", or "Bill" for a charge */
int         FillEditForeign(const FillEdit *e);        /* the rate field shows */
void        FillEditRateLabel(const FillEdit *e, char *buf);    /* "CAD per USD", at least 16 bytes */
/* The grades this vehicle can take, in popup order; returns how many. */
UInt8       FillEditGrades(const FillEdit *e, UInt8 map[7]);
const char *FillEditGradeName(UInt8 grade);
void        FillEditOdoHint(const FillEdit *e, char *buf);      /* "prev 81,190" or empty; at least 24 bytes */
/* "= odo", "check odo", "odo 81,730" (a blank odometer will be this) or empty. */
void        FillEditTripHint(const FillEdit *e, const FillEditText *t, char *buf);
void        FillEditPointsText(const FillEdit *e, char *buf);   /* "none", "Petro-Points 460 pts", "2 programmes"; 48 bytes */

/* ---- what the user does ---- */

/* A quantity field changed. Works the third out when two are known,
 * writes it into t, and returns its bit (0 when nothing was worked out);
 * hint says which ("total calculated"), at least 24 bytes. */
UInt8 FillEditTyped(FillEdit *e, UInt8 which, FillEditText *t, char *hint);
/* The same without a keystroke, after anything else changed. */
UInt8 FillEditRecompute(FillEdit *e, FillEditText *t, char *hint);
/* These return 1 when the form went between fuel and charge, which clears the quantities. */
int  FillEditSetVehicle(FillEdit *e, UInt8 id, FillEditText *t);
int  FillEditSetGrade(FillEdit *e, UInt8 grade, FillEditText *t);
void FillEditSetCurrency(FillEdit *e, UInt8 currency, FillEditText *t);
void FillEditSetEntryUnit(FillEdit *e, UInt8 unit, FillEditText *t);
void FillEditSetPayment(FillEdit *e, UInt8 payment);

/* ---- save and delete ---- */
#define feSaved       0
#define feBadNumber   1     /* a field could not be read, or there is no odometer */
#define feNeedVolume  2     /* neither a volume nor a total */
#define feOdoMismatch 3     /* trip and odometer disagree: ask, with the three texts */
#define feSaveFailed  4

#define feOdoAsk     0      /* stop at a mismatch and say so */
#define feOdoUseTrip 1      /* take the previous reading plus the trip */
#define feOdoKeep    2      /* save the odometer as typed */

typedef struct {
    UInt16 date, time;      /* days since 1904; minutes, or fillTimeUnknown */
    UInt8 full, missed;
    const char *note;
    UInt8 odoChoice;        /* feOdo* */
} FillEditInput;

/* Reads the fields and stores the fill. On feOdoMismatch ask[0] is the trip,
 * ask[1] how far the odometer moved and ask[2] the reading the trip gives.
 * indexOut gets the saved fill's place in the database. */
Int32 FillEditSave(FillEdit *e, const FillEditText *t, const FillEditInput *in, char ask[3][24], UInt16 *indexOut);
/* A user deletion of the fill being edited: tombstone, then remove. */
Int32 FillEditDelete(FillEdit *e);

#endif
