/* New Fill and Edit Fill without the window (filledit.h). Follows the Palm
 * form, src/fill_form.c, rule for rule; where the two differ it says so. */
#include "filledit.h"
#include "../store/db.h"
#include "../../src/calc.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <string.h>

static FillRecord gScan;
static ProgrammeRecord gPgm;

static int IsCharge(UInt8 grade) { return grade >= fillGradeHomeAC; }

/* ---- vehicles ---- */

int FillEditEnsureVehicle(void)
{
    VehicleRecord v;
    UInt8 buf[vehRecordSize + recTrailerSize + 8];
    if (DbCount(dbVehicles)) return 0;
    memset(&v, 0, sizeof v);
    v.id = 1;
    strcpy(v.name, "My car");
    v.defaultCurrency = vehCurrencyUsePrefs;
    return DbAppend(dbVehicles, buf, VehiclePack(&v, buf)) == dbOk;
}

/* The vehicle a new fill starts on: the one the list shows, else the first in use. */
static UInt8 DefaultVehicle(const FuelPrefs *p)
{
    VehicleRecord v;
    UInt16 n = DbCount(dbVehicles), i;
    if (p->currentVehicle != 255) return p->currentVehicle;
    for (i = 0; i < n; i++) if (DbGetVehicle(i, &v) == dbOk && !v.archived) return v.id;
    return 1;
}

/* The vehicle's reading before this fill: its last fill for a new one, the
 * one before it in the log for an edit, else the reading tracking began at.
 * (The Palm's edit takes the record before it whatever its vehicle.) */
static UInt32 PrevOdo(const FillEdit *e)
{
    VehicleRecord v;
    UInt16 n = e->index == fillEditNew ? DbCount(dbFills) : e->index;
    while (n--) if (DbGetFill(n, &gScan) == dbOk && gScan.vehicle == e->fill.vehicle) return gScan.odometer;
    return DbVehicleById(e->fill.vehicle, &v) ? v.startOdo : 0;
}

static void LoadVehicle(FillEdit *e)
{
    if (!DbVehicleById(e->fill.vehicle, &e->veh)) {
        memset(&e->veh, 0, sizeof e->veh);
        e->veh.id = e->fill.vehicle;
        e->veh.defaultCurrency = vehCurrencyUsePrefs;
    }
}

/* ---- points ---- */

static int PaymentOk(const FillEdit *e, const ProgrammeRecord *p)
{
    if (p->requiresPayment == pgmPaymentCash) return e->fill.payment == fillPaymentCash;
    if (p->requiresPayment == pgmPaymentCard) return e->fill.payment == fillPaymentCard;
    return 1;
}

/* A new fill starts with every auto-apply programme that fits its payment
 * and is run in the currency it is paid in. */
static void AutoApply(FillEdit *e)
{
    UInt16 n = DbCount(dbProgrammes), i;
    e->ptsCount = 0;
    for (i = 0; i < n && e->ptsCount < fillMaxPoints; i++) {
        if (DbGetProgramme(i, &gPgm) != dbOk || gPgm.archived || !gPgm.autoApply || !PaymentOk(e, &gPgm)) continue;
        if (gPgm.currency != e->fill.currency) continue;
        e->pts[e->ptsCount].pgm = gPgm.id; e->pts[e->ptsCount].earned = 0;
        e->pts[e->ptsCount].redeemed = 0; e->pts[e->ptsCount].autoEarned = 1;
        e->ptsCount++;
    }
}

/* Auto-earned points and the discount for this volume (mL) and gross price
 * (cents before any discount). */
static void PointsRecompute(FillEdit *e, UInt32 ml, UInt32 gross)
{
    UInt8 i;
    e->discount = 0;
    for (i = 0; i < e->ptsCount; i++) {
        if (!DbProgrammeById(e->pts[i].pgm, &gPgm)) continue;
        if (e->pts[i].autoEarned)
            e->pts[i].earned = PaymentOk(e, &gPgm)
                ? (Int32)CalcEarnedPoints(gPgm.earnRate, gPgm.earnBasis, ml, gross, e->fill.grade == fillGradePremium ? gPgm.premiumMult : 100) : 0;
        if (e->pts[i].redeemed > 0) {
            CalcRedeemRule r;
            r.kind = gPgm.redeemKind; r.block = gPgm.redeemBlock; r.value = gPgm.redeemValue;
            r.volUnit = gPgm.redeemVolUnit; r.maxVol = gPgm.redeemMaxVol; r.maxPerUnit = gPgm.redeemMaxPerUnit;
            e->discount += CalcDiscountCents(&r, (UInt32)e->pts[i].redeemed, ml, gross > e->discount ? gross - e->discount : 0);
        }
    }
}

void FillEditPointsText(const FillEdit *e, char *buf)
{
    if (e->ptsCount == 0) { strcpy(buf, "none"); return; }
    if (e->ptsCount == 1 && DbProgrammeById(e->pts[0].pgm, &gPgm)) {
        Int32 v = e->pts[0].earned - e->pts[0].redeemed;
        size_t n;
        strncpy(buf, gPgm.name, pgmNameLen - 1); buf[pgmNameLen - 1] = 0;
        strcat(buf, " ");
        n = strlen(buf);
        if (gPgm.valuePerPoint == 1000) {           /* a programme counted in money */
            strcpy(buf + n, v < 0 ? "-$" : "$");
            TextFixed(buf + strlen(buf), v < 0 ? -v : v, 2, e->point, e->thousands);
        } else {
            TextFixed(buf + n, v, 0, e->point, e->thousands);
            strcat(buf, " pts");
        }
        return;
    }
    TextFixed(buf, e->ptsCount, 0, e->point, 0);
    strcat(buf, " programmes");
}

/* ---- the three quantities ---- */

/* Volume is mL for fuel and Wh for a charge (typed as kWh to one decimal). */
static Int32 ReadVolume(const FillEdit *e, const FillEditText *t, UInt32 *ml)
{
    UInt32 v;
    Int32 r;
    if (e->charge) {
        r = TextParseFixed(t->volume, 1, e->point, e->thousands, &v);
        if (r == 1) *ml = v * 100;
        return r;
    }
    r = TextParseFixed(t->volume, UnitVolDecimals(e->entryVol), e->point, e->thousands, &v);
    if (r == 1) *ml = UnitVolFromDisplay(v, e->entryVol);
    return r;
}

static Int32 ReadPrice(const FillEdit *e, const FillEditText *t, UInt32 *price)
{
    UInt32 v;
    Int32 r = TextParsePrice(t->price, e->prefs->priceFormat == unitPriceFormatCents, e->point, e->thousands, &v);
    if (r == 1) *price = e->charge ? v : UnitPriceFromDisplay(v, e->entryVol);
    return r;
}

static Int32 ReadTotal(const FillEdit *e, const FillEditText *t, UInt32 *cents)
{
    return TextParseFixed(t->total, 2, e->point, e->thousands, cents);
}

static void ShowVolume(const FillEdit *e, FillEditText *t, UInt32 ml)
{
    if (e->charge) TextFixed(t->volume, (Int32)((ml + 50) / 100), 1, e->point, 0);
    else TextFixed(t->volume, (Int32)UnitVolToDisplay(ml, e->entryVol), UnitVolDecimals(e->entryVol), e->point, 0);
}

static void ShowPrice(const FillEdit *e, FillEditText *t, UInt32 price)
{
    TextPrice(t->price, e->charge ? price : UnitPriceToDisplay(price, e->entryVol), e->prefs->priceFormat == unitPriceFormatCents, e->point);
}

static void ShowTotal(const FillEdit *e, FillEditText *t, UInt32 cents)
{
    TextFixed(t->total, (Int32)cents, 2, e->point, 0);
}

static void ShowRate(const FillEdit *e, FillEditText *t)
{
    if (FillEditForeign(e)) TextFixed(t->rate, (Int32)(e->fill.fxRate / 100), 4, e->point, 0);
    else t->rate[0] = 0;
}

/* Keep the quantities in the order they were typed, newest last. */
static void Touch(FillEdit *e, UInt8 which)
{
    UInt8 others[3], n = 0, i;
    for (i = 0; i < 3; i++) if (e->order[i] && e->order[i] != which) others[n++] = e->order[i];
    e->order[0] = n > 1 ? others[n - 2] : 0;
    e->order[1] = n > 0 ? others[n - 1] : 0;
    e->order[2] = which;
}

static void ClearQuantities(FillEdit *e, FillEditText *t)
{
    t->volume[0] = t->price[0] = t->total[0] = 0;
    e->order[0] = e->order[1] = e->order[2] = 0;
}

UInt8 FillEditRecompute(FillEdit *e, FillEditText *t, char *hint)
{
    UInt32 ml = 0, price = 0, cents = 0, gross;
    Int32 hv = ReadVolume(e, t, &ml), hp = ReadPrice(e, t, &price), ht = ReadTotal(e, t, &cents);
    UInt8 last = e->order[2], prev = e->order[1];
    UInt8 have = (UInt8)((hv == 1 ? feVolume : 0) | (hp == 1 ? fePrice : 0) | (ht == 1 ? feTotal : 0));
    UInt8 target = 0;

    /* Points first: auto-earned points follow the volume and gross price,
     * and redemptions take a discount off the total. */
    if ((have & feVolume) && (have & fePrice)) gross = CalcTotalCents(price, ml);
    else gross = cents + e->discount;
    PointsRecompute(e, ml, gross);

    if (last && prev) target = (UInt8)(7 & ~(last | prev));
    else if (have == (feVolume | fePrice)) target = feTotal;
    else if (have == (feVolume | feTotal)) target = fePrice;
    else if (have == (fePrice | feTotal)) target = feVolume;
    /* Never the field being typed in: emptying a field to retype it must leave it empty.
     * (The Palm form refills it from the other two.) */
    if (target == last) target = 0;
    hint[0] = 0;
    if (target == feTotal && (have & feVolume) && (have & fePrice)) {
        ShowTotal(e, t, gross > e->discount ? gross - e->discount : 0);
        strcpy(hint, e->discount ? "total less points" : "total calculated");
        return feTotal;
    }
    if (target == fePrice && (have & feVolume) && (have & feTotal) && ml) {
        ShowPrice(e, t, CalcMulDiv(cents + e->discount, 100000UL, ml));
        strcpy(hint, "price calculated");
        return fePrice;
    }
    if (target == feVolume && (have & fePrice) && (have & feTotal) && price) {
        ShowVolume(e, t, CalcMulDiv(cents + e->discount, 100000UL, price));
        strcpy(hint, "volume calculated");
        return feVolume;
    }
    return 0;
}

UInt8 FillEditTyped(FillEdit *e, UInt8 which, FillEditText *t, char *hint)
{
    Touch(e, which);
    return FillEditRecompute(e, t, hint);
}

/* ---- labels and hints ---- */

const char *FillEditTitle(const FillEdit *e)
{
    if (e->index == fillEditNew) return e->charge ? "New Charge" : "New Fill";
    return e->charge ? "Edit Charge" : "Edit Fill";
}

const char *FillEditVolumeLabel(const FillEdit *e) { return e->charge ? "Energy kWh:" : "Volume:"; }
const char *FillEditGradeLabel(const FillEdit *e) { return e->charge ? "Where:" : "Grade:"; }
const char *FillEditFullLabel(const FillEdit *e) { return e->charge ? "Charged to full" : "Full tank"; }
/* Home charging is paid on the hydro bill; payment 0 means that for a charge. */
const char *FillEditCashLabel(const FillEdit *e) { return e->charge ? "Bill" : "Cash"; }

void FillEditPriceLabel(const FillEdit *e, char *buf)
{
    strcpy(buf, e->prefs->priceFormat == unitPriceFormatCents ? "Price c" : "Price $");
    if (e->charge) strcat(buf, "/kWh:");
    else strcat(buf, e->entryVol == unitVolLitres ? "/L:" : "/gal:");   /* the unit beside Volume says which gallon */
}

int FillEditForeign(const FillEdit *e) { return e->fill.currency != e->prefs->homeCurrency; }

void FillEditRateLabel(const FillEdit *e, char *buf)
{
    strcpy(buf, CurrencyCode(e->prefs->homeCurrency));
    strcat(buf, " per ");
    strcat(buf, CurrencyCode(e->fill.currency));
}

const char *FillEditGradeName(UInt8 grade)
{
    static const char *const names[7] = { "Regular", "Mid", "Premium", "Diesel", "Home AC", "Public AC", "DC fast" };
    return grade < 7 ? names[grade] : "";
}

UInt8 FillEditGrades(const FillEdit *e, UInt8 map[7])
{
    UInt8 from = e->veh.powertrain == vehPowerElectric ? 4 : 0;
    UInt8 to = (e->veh.powertrain == vehPowerFuel || e->veh.powertrain == vehPowerHybrid) ? 4 : 7;
    UInt8 g, n = 0, found = 0;
    for (g = from; g < to; g++) { map[n++] = g; if (g == e->fill.grade) found = 1; }
    /* A stored grade the vehicle no longer offers stays choosable, so opening a fill never changes it. */
    if (!found && e->fill.grade < 7) map[n++] = e->fill.grade;
    return n;
}

void FillEditOdoHint(const FillEdit *e, char *buf)
{
    buf[0] = 0;
    if (!e->prevOdo) return;
    strcpy(buf, "prev ");
    TextFixed(buf + 5, (Int32)(UnitDistToDisplay(e->prevOdo, e->veh.odoUnit) / 10), 0, e->point, e->thousands);
}

void FillEditTripHint(const FillEdit *e, const FillEditText *t, char *buf)
{
    UInt32 odo = 0, trip = 0;
    Int32 ho = TextParseFixed(t->odo, 1, e->point, e->thousands, &odo);
    Int32 ht = TextParseFixed(t->trip, 1, e->point, e->thousands, &trip);
    buf[0] = 0;
    if (ho == 1) odo = UnitDistFromDisplay(odo, e->veh.odoUnit);
    if (ht == 1) trip = UnitDistFromDisplay(trip, e->veh.odoUnit);
    if (ht == 1 && trip == 0) ht = 0;          /* a trip of 0 means not entered */
    if (ho == 1 && ht == 1 && e->prevOdo && odo > e->prevOdo) {
        UInt32 moved = odo - e->prevOdo, gap = moved > trip ? moved - trip : trip - moved;
        UInt32 tol = moved / 50 > 50 ? moved / 50 : 50;   /* 2 % or 5.0 km, whichever is larger */
        strcpy(buf, gap <= tol ? "= odo" : "check odo");
    } else if (ho != 1 && ht == 1 && e->prevOdo) {
        /* Blank odometer: it will be the previous one plus the trip. */
        strcpy(buf, "odo ");
        TextFixed(buf + 4, (Int32)(UnitDistToDisplay(e->prevOdo + trip, e->veh.odoUnit) / 10), 0, e->point, e->thousands);
    }
}

/* ---- begin ---- */

/* The unit to type a fill in: Prefs' unit, except that a USD fill on a
 * litres setup is typed in US gallons, since that is what the pump says. */
static UInt8 EntryUnitFor(const FillEdit *e, UInt8 currency)
{
    return (currency == curUSD && e->prefs->volumeUnit == unitVolLitres) ? unitVolUSGal : e->prefs->volumeUnit;
}

void FillEditBegin(FillEdit *e, UInt16 index, FuelPrefs *prefs, UInt16 today, UInt16 minutes, char point, char thousands, FillEditText *t)
{
    UInt8 i;
    memset(e, 0, sizeof *e);
    memset(t, 0, sizeof *t);
    e->prefs = prefs; e->point = point; e->thousands = thousands;
    e->index = fillEditNew;
    if (index != fillEditNew && DbGetFill(index, &e->fill) == dbOk) {
        e->index = index;
        /* Stored points are kept as they are. */
        for (i = 0; i < e->fill.pointCount && i < fillMaxPoints; i++) {
            e->pts[i].pgm = e->fill.points[i].programme; e->pts[i].earned = e->fill.points[i].earned;
            e->pts[i].redeemed = e->fill.points[i].redeemed; e->pts[i].autoEarned = 0;
        }
        e->ptsCount = (UInt8)(e->fill.pointCount < fillMaxPoints ? e->fill.pointCount : fillMaxPoints);
        e->ptsTouched = 1;
        e->discount = e->fill.discount;
        LoadVehicle(e);
    } else {
        memset(&e->fill, 0, sizeof e->fill);
        e->fill.date = today;
        e->fill.time = minutes;
        e->fill.vehicle = DefaultVehicle(prefs);
        e->fill.flags = fillFlagFullTank;
        LoadVehicle(e);
        e->fill.grade = e->veh.fuelDefault;
        /* An electric car charges; a plug-in may do either. */
        if (e->veh.powertrain == vehPowerElectric && !IsCharge(e->fill.grade)) e->fill.grade = fillGradeHomeAC;
        e->fill.currency = CurrencyForNewEntry(e->veh.defaultCurrency, prefs->defaultCurrency, prefs->lastCurrency);
        e->fill.fxRate = CurrencyDefaultRate(e->fill.currency, prefs->homeCurrency, prefs->fxRate);
        e->fill.payment = prefs->lastPayment;
        AutoApply(e);
    }
    e->prevOdo = PrevOdo(e);
    e->entryVol = EntryUnitFor(e, e->fill.currency);
    e->charge = (UInt8)IsCharge(e->fill.grade);
    /* The odometer shows whole units like the dash; tenths are accepted when typed. */
    if (e->fill.odometer) TextFixed(t->odo, (Int32)(UnitDistToDisplay(e->fill.odometer, e->veh.odoUnit) / 10), 0, point, 0);
    if (e->fill.trip) TextFixed(t->trip, (Int32)UnitDistToDisplay(e->fill.trip, e->veh.odoUnit), 1, point, 0);
    if (e->fill.volume) ShowVolume(e, t, e->fill.volume);
    if (e->fill.price) ShowPrice(e, t, e->fill.price);
    else if (e->charge && e->index == fillEditNew && e->fill.grade == fillGradeHomeAC && e->veh.homeRate) ShowPrice(e, t, e->veh.homeRate);
    if (e->fill.total) ShowTotal(e, t, e->fill.total);
    ShowRate(e, t);
    PointsRecompute(e, e->fill.volume, e->fill.total + e->fill.discount);
}

/* ---- changes ---- */

int FillEditSetVehicle(FillEdit *e, UInt8 id, FillEditText *t)
{
    if (id == e->fill.vehicle) return 0;
    e->fill.vehicle = id;
    LoadVehicle(e);
    e->prevOdo = PrevOdo(e);
    if (e->veh.powertrain == vehPowerElectric && !IsCharge(e->fill.grade)) e->fill.grade = fillGradeHomeAC;
    if ((e->veh.powertrain == vehPowerFuel || e->veh.powertrain == vehPowerHybrid) && IsCharge(e->fill.grade)) e->fill.grade = e->veh.fuelDefault;
    if (IsCharge(e->fill.grade) != e->charge) {
        e->charge = (UInt8)IsCharge(e->fill.grade);
        ClearQuantities(e, t);          /* litres are not kWh */
        return 1;
    }
    return 0;
}

int FillEditSetGrade(FillEdit *e, UInt8 grade, FillEditText *t)
{
    int charge = IsCharge(grade), switched = 0;
    if (charge != e->charge) {
        e->charge = (UInt8)charge;
        ClearQuantities(e, t);
        switched = 1;
    }
    /* A home charge with no price yet starts at the vehicle's home rate. */
    if (charge && grade == fillGradeHomeAC && e->veh.homeRate && !t->price[0]) {
        ShowPrice(e, t, e->veh.homeRate);
        Touch(e, fePrice);
    }
    e->fill.grade = grade;      /* premium changes auto points: the caller recomputes */
    return switched;
}

void FillEditSetEntryUnit(FillEdit *e, UInt8 unit, FillEditText *t)
{
    UInt32 ml = 0, price = 0;
    int hadVol = !e->charge && ReadVolume(e, t, &ml) == 1, hadPrice = !e->charge && ReadPrice(e, t, &price) == 1;
    e->entryVol = unit < 3 ? unit : unitVolLitres;
    if (hadVol) ShowVolume(e, t, ml);       /* what is typed is converted */
    if (hadPrice) ShowPrice(e, t, price);
}

void FillEditSetCurrency(FillEdit *e, UInt8 currency, FillEditText *t)
{
    if (currency >= curCount) return;
    e->fill.currency = currency;
    e->fill.fxRate = CurrencyDefaultRate(currency, e->prefs->homeCurrency, e->prefs->fxRate);
    ShowRate(e, t);
    /* The programmes of the other currency come and go with it, like card-only ones with the payment. */
    if (e->index == fillEditNew && !e->ptsTouched) AutoApply(e);
    if (EntryUnitFor(e, currency) != e->entryVol) FillEditSetEntryUnit(e, EntryUnitFor(e, currency), t);
}

void FillEditSetPayment(FillEdit *e, UInt8 payment)
{
    e->fill.payment = payment == fillPaymentCard ? fillPaymentCard : fillPaymentCash;
    if (e->index == fillEditNew && !e->ptsTouched) AutoApply(e);
}

/* ---- save and delete ---- */

Int32 FillEditSave(FillEdit *e, const FillEditText *t, const FillEditInput *in, char ask[3][24], UInt16 *indexOut)
{
    UInt32 odo = 0, trip = 0, ml = 0, price = 0, cents = 0;
    Int32 ho = TextParseFixed(t->odo, 1, e->point, e->thousands, &odo);
    Int32 ht = TextParseFixed(t->trip, 1, e->point, e->thousands, &trip);
    Int32 hv = ReadVolume(e, t, &ml), hp = ReadPrice(e, t, &price), hc = ReadTotal(e, t, &cents);
    FuelPrefs *p = e->prefs;
    UInt16 at = 0;
    UInt8 i;

    if (ho == 1) odo = UnitDistFromDisplay(odo, e->veh.odoUnit);
    if (ht == 1) trip = UnitDistFromDisplay(trip, e->veh.odoUnit);
    if (ht == 1 && trip == 0) ht = 0;
    if (ho != 1 && ht == 1 && e->prevOdo) odo = e->prevOdo + trip;     /* quick entry */
    if (ho < 0 || ht < 0 || hv < 0 || hp < 0 || hc < 0 || odo == 0) return feBadNumber;
    if (ho == 1 && ht == 1 && e->prevOdo && odo > e->prevOdo) {
        /* The trip meter and the odometer disagree: offer the corrected reading. */
        UInt32 moved = odo - e->prevOdo, gap = moved > trip ? moved - trip : trip - moved;
        UInt32 tol = moved / 50 > 50 ? moved / 50 : 50;
        if (gap > tol) {
            if (in->odoChoice == feOdoUseTrip) odo = e->prevOdo + trip;
            else if (in->odoChoice != feOdoKeep) {
                TextFixed(ask[0], (Int32)UnitDistToDisplay(trip, e->veh.odoUnit), 1, e->point, e->thousands);
                strcat(ask[0], " "); strcat(ask[0], UnitDistLabel(e->veh.odoUnit));
                TextFixed(ask[1], (Int32)UnitDistToDisplay(moved, e->veh.odoUnit), 1, e->point, e->thousands);
                strcat(ask[1], " "); strcat(ask[1], UnitDistLabel(e->veh.odoUnit));
                TextFixed(ask[2], (Int32)(UnitDistToDisplay(e->prevOdo + trip, e->veh.odoUnit) / 10), 0, e->point, e->thousands);
                return feOdoMismatch;
            }
        }
    }
    if (hv != 1 && hc != 1) return feNeedVolume;
    if (hv == 1 && hp == 1 && hc != 1) { UInt32 gross = CalcTotalCents(price, ml); cents = gross > e->discount ? gross - e->discount : 0; }
    if (FillEditForeign(e)) {
        UInt32 rate = 0;
        Int32 hr = TextParseFixed(t->rate, 4, e->point, e->thousands, &rate);
        if (hr < 0) return feBadNumber;
        e->fill.fxRate = hr == 1 && rate ? rate * 100 : CurrencyDefaultRate(e->fill.currency, p->homeCurrency, p->fxRate);
    } else e->fill.fxRate = fxOne;

    e->fill.date = in->date;
    e->fill.time = in->time;
    e->fill.odometer = odo;
    e->fill.trip = trip;
    e->fill.volume = ml;
    e->fill.price = price;
    e->fill.total = cents;
    e->fill.clockShift = 0;     /* dated by a Windows Mobile clock, which runs past 2031: established, no shift */
    e->fill.flags = (UInt16)((e->fill.flags & ~(fillFlagFullTank | fillFlagTripEntered | fillFlagCharge | fillFlagMissedPrev))
                 | (in->full ? fillFlagFullTank : 0)
                 | (in->missed ? fillFlagMissedPrev : 0)
                 | (ht == 1 ? fillFlagTripEntered : 0)
                 | (IsCharge(e->fill.grade) ? fillFlagCharge : 0));
    e->fill.discount = e->discount;
    e->fill.pointCount = 0;
    for (i = 0; i < e->ptsCount; i++) {
        if (e->pts[i].earned == 0 && e->pts[i].redeemed == 0) continue;   /* an empty row is no entry */
        e->fill.points[e->fill.pointCount].programme = e->pts[i].pgm;
        e->fill.points[e->fill.pointCount].earned = e->pts[i].earned;
        e->fill.points[e->fill.pointCount].redeemed = e->pts[i].redeemed;
        e->fill.pointCount++;
    }
    if (in->note) { strncpy(e->fill.notes, in->note, fillMaxNotes - 1); e->fill.notes[fillMaxNotes - 1] = 0; }

    /* The edited fill goes in before the old one comes out, so a failed add loses nothing. */
    if (DbAddFill(&e->fill, &at) != dbOk) return feSaveFailed;
    if (e->index != fillEditNew) {
        UInt16 old = (UInt16)(at <= e->index ? e->index + 1 : e->index);
        DbRemove(dbFills, old);
        if (old < at) at--;
    }
    e->index = at;
    if (FillEditForeign(e)) p->fxRate[e->fill.currency] = e->fill.fxRate;
    p->lastPayment = e->fill.payment;
    p->lastCurrency = e->fill.currency;
    if (indexOut) *indexOut = at;
    return feSaved;
}

Int32 FillEditDelete(FillEdit *e)
{
    Int32 err;
    if (e->index == fillEditNew) return dbErrRange;
    err = DbDelete(dbFills, e->index);
    if (err == dbOk) e->index = fillEditNew;
    return err;
}
