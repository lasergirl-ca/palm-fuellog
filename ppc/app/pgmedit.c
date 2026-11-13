/* Rewards programmes without the window (pgmedit.h), after src/programmes.c. */
#include "pgmedit.h"
#include "../store/db.h"
#include "../../src/calc.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/points.h"
#include "../../src/presets.h"
#include "../../src/datecalc.h"
#include <string.h>

static ProgrammeRecord gScan;
static FillRecord gScanFill;
static PointsAge gAge;

/* ---- lookups ---- */

UInt8 PgmNextId(void)
{
    UInt16 used = 0, n = DbCount(dbProgrammes), i;
    UInt8 id;
    for (i = 0; i < n; i++)
        if (DbGetProgramme(i, &gScan) == dbOk && gScan.id >= 1 && gScan.id <= 16) used |= (UInt16)(1 << (gScan.id - 1));
    for (id = 1; id <= pgmMaxProgrammes; id++) if (!(used & (1 << (id - 1)))) return id;
    return 0;
}

UInt16 PgmUseCount(UInt8 id)
{
    UInt16 n = DbCount(dbFills), i, count = 0;
    UInt8 k;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFill) != dbOk) continue;
        for (k = 0; k < gScanFill.pointCount; k++) if (gScanFill.points[k].programme == id) { count++; break; }
    }
    return count;
}

Int32 PgmBalance(UInt8 id, Int32 opening)
{
    UInt16 n = DbCount(dbFills), i;
    UInt8 k;
    Int32 bal = opening;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFill) != dbOk) continue;
        for (k = 0; k < gScanFill.pointCount; k++)
            if (gScanFill.points[k].programme == id) bal += gScanFill.points[k].earned - gScanFill.points[k].redeemed;
    }
    return bal;
}

Int32 PgmExpiring(const ProgrammeRecord *p, UInt16 today, UInt16 *when)
{
    UInt16 n = DbCount(dbFills), i, until, y;
    UInt8 m, d, k;
    *when = 0;
    if (p->expiryRule == pgmExpiryNone) return 0;
    PointsBegin(&gAge, p->expiryRule, p->expiryMonths);
    if (p->openingBalance > 0) PointsAdd(&gAge, 0, p->openingBalance, 0);   /* the opening balance ages like an old lot */
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFill) != dbOk) continue;
        for (k = 0; k < gScanFill.pointCount; k++)
            if (gScanFill.points[k].programme == p->id) PointsAdd(&gAge, gScanFill.date, gScanFill.points[k].earned, gScanFill.points[k].redeemed);
    }
    DateDaysToYmd(DateAddMonths(today, 1), &y, &m, &d);
    until = DateYmdToDays(y, m, DateDaysInMonth(y, m));    /* end of next month */
    return PointsExpiringBy(&gAge, today, until, when);
}

int PgmIsMoney(const ProgrammeRecord *p) { return p->valuePerPoint == 1000; }

void PgmValueText(char *buf, const ProgrammeRecord *p, Int32 points, char point, char thousands)
{
    if (PgmIsMoney(p)) {
        strcpy(buf, points < 0 ? "-$" : "$");
        TextFixed(buf + strlen(buf), points < 0 ? -points : points, 2, point, thousands);
    } else {
        TextFixed(buf, points, 0, point, thousands);
        strcat(buf, " pts");
    }
}

void PgmRateText(char *buf, UInt32 earnRate, char point)
{
    if (earnRate % 100 == 0) TextFixed(buf, (Int32)(earnRate / 100), 0, point, 0);
    else if (earnRate % 10 == 0) TextFixed(buf, (Int32)(earnRate / 10), 1, point, 0);
    else TextFixed(buf, (Int32)earnRate, 2, point, 0);
}

UInt8 PgmInstallPresets(UInt8 homeCurrency)
{
    UInt8 buf[pgmFixedSize + pgmAliasLen + 4 + recTrailerSize], i, n = 0;
    UInt16 k, fills = DbCount(dbFills);
    if (DbCount(dbProgrammes) != 0) return 0;
    for (k = 0; k < fills; k++)
        if (DbGetFill(k, &gScanFill) == dbOk && gScanFill.pointCount) return 0;   /* points exist: the user removed them on purpose */
    for (i = 0; i < pgmPresetCount; i++) {
        PgmPreset(i, &gScan);
        if (i == pgmPresetCount - 1) gScan.currency = homeCurrency;   /* card points: any currency */
        if (DbAppend(dbProgrammes, buf, ProgrammePack(&gScan, buf)) == dbOk) n++;
    }
    return n;
}

/* ---- the list ---- */

/* The earn rule and flags in one line: "10 pts/L; Card only; auto". */
static void RuleText(char *buf, const ProgrammeRecord *p, char point)
{
    char num[16];
    static const char *const unit[] = { "", "L", "gal", "$" };
    if (p->earnBasis == pgmEarnNone || p->earnRate == 0) strcpy(buf, "no earn rule");
    else {
        PgmRateText(num, p->earnRate, point);
        strcpy(buf, num);
        strcat(buf, PgmIsMoney(p) ? "c/" : " pts/");
        strcat(buf, unit[p->earnBasis < 4 ? p->earnBasis : 0]);
    }
    if (p->requiresPayment == pgmPaymentCard) strcat(buf, "; Card only");
    if (p->requiresPayment == pgmPaymentCash) strcat(buf, "; Cash only");
    if (p->autoApply) strcat(buf, "; auto");
}

UInt16 PgmListCollect(UInt16 *index, UInt16 max)
{
    UInt16 n = DbCount(dbProgrammes), i, j, count = 0;
    UInt16 key[pgmMaxProgrammes];
    for (i = 0; i < n && count < max && count < pgmMaxProgrammes; i++) {
        UInt16 k;
        if (DbGetProgramme(i, &gScan) != dbOk) continue;
        k = gScan.archived ? 0 : (UInt16)(PgmUseCount(gScan.id) + 1);
        for (j = count; j > 0 && key[j - 1] < k; j--) { key[j] = key[j - 1]; index[j] = index[j - 1]; }
        key[j] = k; index[j] = i;
        count++;
    }
    return count;
}

int PgmListRow(UInt16 dbIndex, UInt16 today, char point, char thousands, char *name, char *balance, char *note)
{
    ProgrammeRecord p;
    Int32 bal, exp;
    UInt16 when;
    if (DbGetProgramme(dbIndex, &p) != dbOk) return 0;
    strcpy(name, p.name);
    if (p.archived) strcat(name, " (archived)");
    bal = PgmBalance(p.id, p.openingBalance);
    PgmValueText(balance, &p, bal, point, thousands);
    exp = PgmExpiring(&p, today, &when);
    if (exp > 0) {
        /* the warning that costs money comes first */
        TextFixed(note, exp, 0, point, thousands);
        strcat(note, " expire ");
        DateShortText(note + strlen(note), when);
        return 1;
    }
    RuleText(note, &p, point);
    if (!PgmIsMoney(&p) && p.valuePerPoint && bal > 0) {
        /* points x thousandths of a cent / 1000 = cents */
        strcat(note, "; about $");
        TextFixed(note + strlen(note), (Int32)CalcMulDiv((UInt32)bal, p.valuePerPoint, 1000), 2, point, thousands);
    }
    return 1;
}

/* ---- the form ---- */

/* A whole or fixed-point number with an optional leading minus. 0 empty, 1 ok, -1 bad. */
static Int32 ParseSigned(const char *t, UInt8 decimals, char point, char thousands, Int32 *out)
{
    int neg = 0;
    UInt32 v;
    Int32 r;
    while (*t == ' ') t++;
    if (*t == '-') { neg = 1; t++; }
    r = TextParseFixed(t, decimals, point, thousands, &v);
    if (r == 1) *out = neg ? -(Int32)v : (Int32)v;
    return r;
}

int PgmEditBegin(PgmEdit *e, UInt16 index, UInt8 homeCurrency, char point, char thousands, PgmEditForm *f)
{
    ProgrammeRecord *p = &e->pgm;
    int isNew;
    memset(e, 0, sizeof *e);
    memset(f, 0, sizeof *f);
    e->index = pgmEditNew;
    if (index != pgmEditNew && DbGetProgramme(index, p) == dbOk) {
        e->index = index;
    } else {
        memset(p, 0, sizeof *p);
        p->id = PgmNextId();
        if (!p->id) return 0;
        p->premiumMult = 100;
        p->currency = homeCurrency;
        p->earnBasis = pgmEarnPerLitre;
        p->autoApply = 1;
    }
    isNew = e->index == pgmEditNew;
    strcpy(f->name, p->name);
    f->currency = p->currency < curCount ? p->currency : 0;
    f->money = (UInt8)PgmIsMoney(p);
    if (p->earnRate) PgmRateText(f->earn, p->earnRate, point);
    f->basis = p->earnBasis < 4 ? p->earnBasis : 0;
    TextFixed(f->premium, p->premiumMult ? p->premiumMult : 100, 0, point, 0);
    f->payment = p->requiresPayment < 3 ? p->requiresPayment : 0;
    f->autoApply = p->autoApply ? 1 : 0;
    f->redeem = p->redeemKind < 3 ? p->redeemKind : 0;
    if (p->redeemBlock) TextFixed(f->block, (Int32)p->redeemBlock, 0, point, thousands);
    if (p->redeemValue) {
        /* Money off: thousandths of a dollar per step shown as dollars; per volume: shown as cents per unit. */
        if (p->redeemKind == pgmRedeemPerVolume) TextFixed(f->value, (Int32)p->redeemValue, 1, point, 0);
        else TextFixed(f->value, (Int32)(p->redeemValue / 10), 2, point, 0);
    }
    f->volUnit = p->redeemVolUnit == pgmVolUSGallon ? 1 : 0;
    if (p->redeemMaxVol) {
        UInt32 units = p->redeemVolUnit == pgmVolUSGallon ? CalcMulDiv(p->redeemMaxVol, 1000000UL, 3785411784UL) : (p->redeemMaxVol + 500) / 1000;
        TextFixed(f->maxVol, (Int32)units, 0, point, 0);
    }
    if (p->redeemMaxPerUnit) TextFixed(f->cap, (Int32)p->redeemMaxPerUnit, 1, point, 0);
    f->expiry = p->expiryRule < 3 ? p->expiryRule : 0;
    if (p->expiryMonths) TextFixed(f->months, p->expiryMonths, 0, point, 0);
    if (p->openingBalance || !isNew) TextFixed(f->opening, p->openingBalance, f->money ? 2 : 0, point, 0);
    strcpy(f->aliases, p->aliases);
    return 1;
}

const char *PgmEditEarnLabel(int money) { return money ? "Earn c:" : "Earn pts:"; }
const char *PgmEditOpeningLabel(int money) { return money ? "Opening $:" : "Opening pts:"; }
const char *PgmEditValueLabel(UInt8 redeem) { return redeem == pgmRedeemPerVolume ? "= c off:" : "= $:"; }

void PgmEditStatus(const PgmEdit *e, char point, char thousands, char *buf)
{
    char val[24];
    Int32 bal;
    buf[0] = 0;
    if (e->index == pgmEditNew) return;
    bal = PgmBalance(e->pgm.id, e->pgm.openingBalance);
    PgmValueText(val, &e->pgm, bal, point, thousands);
    strcpy(buf, "Balance now ");
    strcat(buf, val);
    if (!PgmIsMoney(&e->pgm) && e->pgm.valuePerPoint && bal > 0) {
        strcat(buf, ", about $");
        TextFixed(buf + strlen(buf), (Int32)CalcMulDiv((UInt32)bal, e->pgm.valuePerPoint, 1000), 2, point, thousands);
    }
}

const char *PgmEditArchiveLabel(const PgmEdit *e)
{
    if (e->index == pgmEditNew) return 0;
    if (e->pgm.archived) return "Restore";
    return PgmUseCount(e->pgm.id) ? "Archive" : "Delete";
}

Int32 PgmEditSave(PgmEdit *e, const PgmEditForm *f, char point, char thousands)
{
    ProgrammeRecord p = e->pgm;      /* changed only when every field reads */
    UInt8 buf[pgmFixedSize + pgmAliasLen + 4 + recTrailerSize];
    const char *name = f->name;
    UInt32 v, n;
    Int32 r, s, err;

    while (*name == ' ') name++;
    if (!name[0]) return peNeedName;
    strncpy(p.name, name, pgmNameLen - 1); p.name[pgmNameLen - 1] = 0;
    p.currency = f->currency < curCount ? f->currency : 0;
    p.earnBasis = f->basis < 4 ? f->basis : 0;
    r = TextParseFixed(f->earn, 2, point, thousands, &v);
    if (r < 0) return peBadNumber;
    p.earnRate = r == 1 ? v : 0;
    r = TextParseFixed(f->premium, 0, point, thousands, &v);
    if (r < 0) return peBadNumber;
    p.premiumMult = (UInt16)(r == 1 && v ? v : 100);
    p.requiresPayment = f->payment < 3 ? f->payment : 0;
    p.autoApply = f->autoApply ? 1 : 0;
    p.redeemKind = f->redeem < 3 ? f->redeem : 0;
    r = TextParseFixed(f->block, 0, point, thousands, &v);
    if (r < 0) return peBadNumber;
    p.redeemBlock = r == 1 ? v : 0;
    r = TextParseFixed(f->value, p.redeemKind == pgmRedeemPerVolume ? 1 : 2, point, thousands, &v);
    if (r < 0) return peBadNumber;
    p.redeemValue = r == 1 ? (p.redeemKind == pgmRedeemPerVolume ? v : v * 10) : 0;
    p.redeemVolUnit = f->volUnit == 1 ? pgmVolUSGallon : pgmVolLitre;
    r = TextParseFixed(f->maxVol, 0, point, thousands, &v);
    if (r < 0) return peBadNumber;
    p.redeemMaxVol = r == 1 ? (p.redeemVolUnit == pgmVolUSGallon ? CalcMulDiv(v, 3785411784UL, 1000000UL) : v * 1000) : 0;
    r = TextParseFixed(f->cap, 1, point, thousands, &v);
    if (r < 0) return peBadNumber;
    p.redeemMaxPerUnit = r == 1 ? v : 0;
    p.expiryRule = f->expiry < 3 ? f->expiry : 0;
    r = TextParseFixed(f->months, 0, point, thousands, &v);
    if (r < 0 || (r == 1 && v > 255)) return peBadNumber;
    p.expiryMonths = (UInt8)(r == 1 ? v : 0);
    r = ParseSigned(f->opening, f->money ? 2 : 0, point, thousands, &s);
    if (r < 0) return peBadNumber;
    p.openingBalance = r == 1 ? s : 0;
    strncpy(p.aliases, f->aliases, pgmAliasLen - 1); p.aliases[pgmAliasLen - 1] = 0;
    /* What a point is worth: a cent for money, the money-off rate for points, else unknown. */
    if (f->money) p.valuePerPoint = 1000;
    else if (p.redeemKind == pgmRedeemMoneyOff && p.redeemBlock) p.valuePerPoint = CalcMulDiv(p.redeemValue, 100, p.redeemBlock);
    else if (p.valuePerPoint == 1000) p.valuePerPoint = 0;
    n = ProgrammePack(&p, buf);
    if (e->index == pgmEditNew) {
        err = DbAppend(dbProgrammes, buf, n);
        if (err == dbOk) e->index = (UInt16)(DbCount(dbProgrammes) - 1);
    } else {
        err = DbReplace(dbProgrammes, e->index, buf, n);
    }
    if (err != dbOk) return peSaveFailed;
    e->pgm = p;
    return peSaved;
}

Int32 PgmEditArchive(PgmEdit *e)
{
    UInt8 buf[pgmFixedSize + pgmAliasLen + 4 + recTrailerSize];
    ProgrammeRecord p;
    Int32 err;
    if (e->index == pgmEditNew || DbGetProgramme(e->index, &p) != dbOk) return dbErrRange;
    if (!p.archived && !PgmUseCount(p.id)) {
        err = DbDelete(dbProgrammes, e->index);
        if (err == dbOk) e->index = pgmEditNew;
        return err;
    }
    p.archived = p.archived ? 0 : 1;
    err = DbReplace(dbProgrammes, e->index, buf, ProgrammePack(&p, buf));
    if (err == dbOk) e->pgm = p;
    return err;
}
