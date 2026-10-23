/* Canonical packed record layouts for the five FuelLog databases.
 *
 * Every record is packed big-endian, one byte at a time, into a caller
 * buffer: a 68000 faults on an unaligned 32-bit access, and the 9-byte
 * points entry puts every later field on an odd offset. Values are stored
 * metric in fixed point (see the design spec, Data model). Plain C with no
 * Palm headers, so the layouts are host-tested byte for byte. */
#ifndef RECORDS_H
#define RECORDS_H
#include "ptypes.h"
#include "sections.h"

#define recSchemaVersion 1

/* ---- Fill-up (FuelLog-ZFlg, type Data) ---- */

#define fillFlagFullTank     0x0001   /* or charged to full */
#define fillFlagMissedPrev   0x0002
#define fillFlagTripEntered  0x0004
#define fillFlagCharge       0x0008   /* volume holds Wh, price is per kWh */

#define fillGradeRegular  0
#define fillGradeMid      1
#define fillGradePremium  2
#define fillGradeDiesel   3
#define fillGradeHomeAC   4
#define fillGradePublicAC 5
#define fillGradeDCFast   6

#define fillPaymentCash 0   /* for a charge: on the hydro bill */
#define fillPaymentCard 1

#define fillMaxPoints 4
#define fillMaxNotes  256          /* bytes including the terminator */
#define fillHeaderSize 39
#define fillTimeUnknown 0xFFFF     /* no time entered */
#define fillPointSize  9

typedef struct {
    UInt8 programme;
    Int32 earned;
    Int32 redeemed;
} FillPoints;

typedef struct {
    UInt16 date;          /* days since 1904-01-01 */
    UInt16 time;          /* minutes since midnight, or fillTimeUnknown */
    UInt16 flags;
    UInt8  vehicle;
    UInt8  grade;
    UInt32 odometer;      /* km x 10 */
    UInt32 trip;          /* km x 10 */
    UInt32 volume;        /* mL, or Wh for a charge */
    UInt32 price;         /* ten-thousandths of a currency unit per L or kWh */
    UInt32 total;         /* cents actually paid */
    UInt32 discount;      /* cents taken off by points */
    UInt8  currency;
    UInt32 fxRate;        /* home units per fill unit x 1,000,000 */
    UInt8  payment;
    UInt8  pointCount;
    FillPoints points[fillMaxPoints];
    char   notes[fillMaxNotes];
} FillRecord;

/* ---- Vehicle (FuelVeh-ZFlg, type Vehi) ---- */

#define vehNameLen 24
#define vehRecordSize 47
#define vehMaxVehicles 16

#define vehPowerFuel     0
#define vehPowerHybrid   1
#define vehPowerPlugIn   2
#define vehPowerElectric 3
#define vehOdoKm    0
#define vehOdoMiles 1
#define vehTripReset 0
#define vehTripNever 1
#define vehTripNone  2
#define vehCurrencyUsePrefs 255

typedef struct {
    UInt8  id;
    char   name[vehNameLen];
    UInt8  powertrain;
    UInt8  odoUnit;
    UInt8  tripMode;
    UInt32 startOdo;      /* km x 10 */
    UInt32 tankSize;      /* mL */
    UInt32 batteryWh;
    UInt32 homeRate;      /* ten-thousandths of a currency unit per kWh */
    UInt8  fuelDefault;
    UInt8  defaultCurrency;
    UInt8  archived;
} VehicleRecord;

/* ---- Rewards programme (FuelPgm-ZFlg, type Pgms) ---- */

#define pgmNameLen 24
#define pgmAliasLen 64
#define pgmFixedSize 64
#define pgmMaxProgrammes 16

#define pgmEarnNone        0
#define pgmEarnPerLitre    1
#define pgmEarnPerUSGallon 2
#define pgmEarnPerCurrency 3
#define pgmPaymentAny  0
#define pgmPaymentCash 1
#define pgmPaymentCard 2
#define pgmRedeemNone      0
#define pgmRedeemMoneyOff  1
#define pgmRedeemPerVolume 2
#define pgmVolLitre    0
#define pgmVolUSGallon 1
#define pgmExpiryNone        0
#define pgmExpiryMonthAfter  1
#define pgmExpiryMonths      2

typedef struct {
    UInt8  id;
    char   name[pgmNameLen];
    UInt8  currency;
    UInt8  earnBasis;
    UInt32 earnRate;         /* points x 100 per basis unit */
    UInt16 premiumMult;      /* percent, 100 = none */
    UInt8  requiresPayment;
    Int32  openingBalance;
    UInt32 valuePerPoint;    /* thousandths of a cent */
    UInt8  autoApply;
    UInt8  archived;
    UInt8  redeemKind;
    UInt32 redeemBlock;      /* points per step */
    UInt32 redeemValue;      /* thousandths of currency per step (per volume unit for discounts) */
    UInt8  redeemVolUnit;
    UInt32 redeemMaxVol;     /* mL cap per fill */
    UInt32 redeemMaxPerUnit; /* thousandths of currency per volume unit */
    UInt8  expiryRule;
    UInt8  expiryMonths;
    char   aliases[pgmAliasLen];   /* comma-separated, for import headers */
} ProgrammeRecord;

/* ---- Service type (FuelSvT-ZFlg, type SvcT) ---- */

#define svtNameLen 24
#define svtDetailLen 32
#define svtRecordSize 71
#define svtMaxPerVehicle 31      /* id is a bit position; bit 31 is "Other" */

typedef struct {
    UInt8  id;               /* bit position 0 to 30 within its vehicle */
    UInt8  vehicle;
    char   name[svtNameLen];
    UInt32 intervalDist;     /* km x 10, 0 = none */
    UInt8  intervalMonths;   /* 0 = none */
    UInt32 warnDist;         /* km x 10 */
    UInt16 warnDays;
    UInt8  alarm;
    UInt8  archived;
    char   defaultDetail[svtDetailLen];
} ServiceTypeRecord;

/* ---- Service visit (FuelSvL-ZFlg, type SvcL) ---- */

#define svlOtherLen 24
#define svlShopLen 24
#define svlDetailLen 32
#define svlFixedSize 101
#define svlOtherBit 0x80000000UL

typedef struct {
    UInt16 date;
    UInt8  vehicle;
    UInt32 odometer;         /* km x 10 */
    UInt32 services;         /* bitmask of service type ids; bit 31 = Other */
    char   other[svlOtherLen];
    UInt32 cost;             /* cents */
    UInt8  currency;
    UInt32 fxRate;
    UInt8  payment;
    char   shop[svlShopLen];
    char   detail[svlDetailLen];
    char   notes[fillMaxNotes];
} ServiceVisitRecord;

/* ---- AppInfo block shared by all five databases ---- */

#define appInfoSize 4
typedef struct {
    UInt16 schemaVersion;
    UInt16 reserved;
} AppInfoBlock;

/* Pack functions write into buf and return the number of bytes used; the
 * caller sizes buf with the *Size functions. Unpack functions read from
 * buf of len bytes and return 0, or -1 when the bytes are too short or
 * malformed. */
UInt32 FillSize(const FillRecord *r) EXPORT_SEC;
UInt32 FillPack(const FillRecord *r, UInt8 *buf) EXPORT_SEC;
Int32  FillUnpack(const UInt8 *buf, UInt32 len, FillRecord *r) EXPORT_SEC;
/* Sort order of the fill database: date, then odometer. Works on packed bytes. */
Int32  FillCompare(const UInt8 *a, const UInt8 *b) EXPORT_SEC;

UInt32 VehiclePack(const VehicleRecord *r, UInt8 *buf) EXPORT_SEC;
Int32  VehicleUnpack(const UInt8 *buf, UInt32 len, VehicleRecord *r) EXPORT_SEC;

UInt32 ProgrammeSize(const ProgrammeRecord *r) EXPORT_SEC;
UInt32 ProgrammePack(const ProgrammeRecord *r, UInt8 *buf) EXPORT_SEC;
Int32  ProgrammeUnpack(const UInt8 *buf, UInt32 len, ProgrammeRecord *r) EXPORT_SEC;

UInt32 ServiceTypePack(const ServiceTypeRecord *r, UInt8 *buf) EXPORT_SEC;
Int32  ServiceTypeUnpack(const UInt8 *buf, UInt32 len, ServiceTypeRecord *r) EXPORT_SEC;

UInt32 ServiceVisitSize(const ServiceVisitRecord *r) EXPORT_SEC;
UInt32 ServiceVisitPack(const ServiceVisitRecord *r, UInt8 *buf) EXPORT_SEC;
Int32  ServiceVisitUnpack(const UInt8 *buf, UInt32 len, ServiceVisitRecord *r) EXPORT_SEC;
Int32  ServiceVisitCompare(const UInt8 *a, const UInt8 *b) EXPORT_SEC;

UInt32 AppInfoPack(const AppInfoBlock *a, UInt8 *buf) EXPORT_SEC;
Int32  AppInfoUnpack(const UInt8 *buf, UInt32 len, AppInfoBlock *a) EXPORT_SEC;

#endif
