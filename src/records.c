#include "records.h"

/* ---- byte helpers ---- */

static UInt8 *Put8(UInt8 *p, UInt8 v) EXPORT_SEC;
static UInt8 *Put16(UInt8 *p, UInt16 v) EXPORT_SEC;
static UInt8 *Put32(UInt8 *p, UInt32 v) EXPORT_SEC;
static UInt8 *PutStr(UInt8 *p, const char *s, UInt32 n) EXPORT_SEC;
static const UInt8 *Get8(const UInt8 *p, UInt8 *v) EXPORT_SEC;
static const UInt8 *Get16(const UInt8 *p, UInt16 *v) EXPORT_SEC;
static const UInt8 *Get32(const UInt8 *p, UInt32 *v) EXPORT_SEC;
static const UInt8 *GetStr(const UInt8 *p, char *s, UInt32 n) EXPORT_SEC;
static UInt32 CStrLen(const char *s, UInt32 max) EXPORT_SEC;

static UInt8 *Put8(UInt8 *p, UInt8 v) { *p++ = v; return p; }
static UInt8 *Put16(UInt8 *p, UInt16 v) { *p++ = (UInt8)(v >> 8); *p++ = (UInt8)v; return p; }
static UInt8 *Put32(UInt8 *p, UInt32 v)
{
    *p++ = (UInt8)(v >> 24); *p++ = (UInt8)(v >> 16); *p++ = (UInt8)(v >> 8); *p++ = (UInt8)v;
    return p;
}
/* Fixed-width string field: copied up to n-1 bytes, the rest zero. */
static UInt8 *PutStr(UInt8 *p, const char *s, UInt32 n)
{
    UInt32 i = 0;
    while (i < n - 1 && s[i]) { p[i] = (UInt8)s[i]; i++; }
    while (i < n) p[i++] = 0;
    return p + n;
}
static const UInt8 *Get8(const UInt8 *p, UInt8 *v) { *v = *p; return p + 1; }
static const UInt8 *Get16(const UInt8 *p, UInt16 *v) { *v = (UInt16)((p[0] << 8) | p[1]); return p + 2; }
static const UInt8 *Get32(const UInt8 *p, UInt32 *v)
{
    *v = ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) | ((UInt32)p[2] << 8) | p[3];
    return p + 4;
}
static const UInt8 *GetStr(const UInt8 *p, char *s, UInt32 n)
{
    UInt32 i;
    for (i = 0; i < n; i++) s[i] = (char)p[i];
    s[n - 1] = 0;
    return p + n;
}
static UInt32 CStrLen(const char *s, UInt32 max)
{
    UInt32 n = 0;
    while (n < max - 1 && s[n]) n++;
    return n;
}

/* ---- fill ---- */

UInt32 FillSize(const FillRecord *r)
{
    UInt8 np = r->pointCount > fillMaxPoints ? fillMaxPoints : r->pointCount;
    return fillHeaderSize + (UInt32)np * fillPointSize + CStrLen(r->notes, fillMaxNotes) + 1;
}

UInt32 FillPack(const FillRecord *r, UInt8 *buf)
{
    UInt8 *p = buf, np = r->pointCount > fillMaxPoints ? fillMaxPoints : r->pointCount, i;
    UInt32 n;
    p = Put16(p, r->date);
    p = Put16(p, r->time);
    p = Put16(p, r->flags);
    p = Put8(p, r->vehicle);
    p = Put8(p, r->grade);
    p = Put32(p, r->odometer);
    p = Put32(p, r->trip);
    p = Put32(p, r->volume);
    p = Put32(p, r->price);
    p = Put32(p, r->total);
    p = Put32(p, r->discount);
    p = Put8(p, r->currency);
    p = Put32(p, r->fxRate);
    p = Put8(p, r->payment);
    p = Put8(p, np);
    for (i = 0; i < np; i++) {
        p = Put8(p, r->points[i].programme);
        p = Put32(p, (UInt32)r->points[i].earned);
        p = Put32(p, (UInt32)r->points[i].redeemed);
    }
    n = CStrLen(r->notes, fillMaxNotes);
    for (i = 0; i < n; i++) *p++ = (UInt8)r->notes[i];
    *p++ = 0;
    return (UInt32)(p - buf);
}

Int32 FillUnpack(const UInt8 *buf, UInt32 len, FillRecord *r)
{
    const UInt8 *p = buf, *end = buf + len;
    UInt8 i;
    UInt32 n;
    if (len < fillHeaderSize) return -1;
    p = Get16(p, &r->date);
    p = Get16(p, &r->time);
    p = Get16(p, &r->flags);
    p = Get8(p, &r->vehicle);
    p = Get8(p, &r->grade);
    p = Get32(p, &r->odometer);
    p = Get32(p, &r->trip);
    p = Get32(p, &r->volume);
    p = Get32(p, &r->price);
    p = Get32(p, &r->total);
    p = Get32(p, &r->discount);
    p = Get8(p, &r->currency);
    p = Get32(p, &r->fxRate);
    p = Get8(p, &r->payment);
    p = Get8(p, &r->pointCount);
    if (r->pointCount > fillMaxPoints) return -1;
    if ((UInt32)(end - p) < (UInt32)r->pointCount * fillPointSize) return -1;
    for (i = 0; i < r->pointCount; i++) {
        UInt32 v;
        p = Get8(p, &r->points[i].programme);
        p = Get32(p, &v); r->points[i].earned = (Int32)v;
        p = Get32(p, &v); r->points[i].redeemed = (Int32)v;
    }
    for (; i < fillMaxPoints; i++) { r->points[i].programme = 0; r->points[i].earned = 0; r->points[i].redeemed = 0; }
    n = 0;
    while (p < end && *p && n < fillMaxNotes - 1) r->notes[n++] = (char)*p++;
    r->notes[n] = 0;
    return 0;
}

Int32 FillCompare(const UInt8 *a, const UInt8 *b)
{
    UInt16 da, db;
    UInt32 oa, ob;
    Get16(a, &da); Get16(b, &db);
    if (da != db) return da < db ? -1 : 1;
    Get32(a + 8, &oa); Get32(b + 8, &ob);
    if (oa != ob) return oa < ob ? -1 : 1;
    return 0;
}

/* ---- vehicle ---- */

UInt32 VehiclePack(const VehicleRecord *r, UInt8 *buf)
{
    UInt8 *p = buf;
    p = Put8(p, r->id);
    p = PutStr(p, r->name, vehNameLen);
    p = Put8(p, r->powertrain);
    p = Put8(p, r->odoUnit);
    p = Put8(p, r->tripMode);
    p = Put32(p, r->startOdo);
    p = Put32(p, r->tankSize);
    p = Put32(p, r->batteryWh);
    p = Put32(p, r->homeRate);
    p = Put8(p, r->fuelDefault);
    p = Put8(p, r->defaultCurrency);
    p = Put8(p, r->archived);
    return (UInt32)(p - buf);
}

Int32 VehicleUnpack(const UInt8 *buf, UInt32 len, VehicleRecord *r)
{
    const UInt8 *p = buf;
    if (len < vehRecordSize) return -1;
    p = Get8(p, &r->id);
    p = GetStr(p, r->name, vehNameLen);
    p = Get8(p, &r->powertrain);
    p = Get8(p, &r->odoUnit);
    p = Get8(p, &r->tripMode);
    p = Get32(p, &r->startOdo);
    p = Get32(p, &r->tankSize);
    p = Get32(p, &r->batteryWh);
    p = Get32(p, &r->homeRate);
    p = Get8(p, &r->fuelDefault);
    p = Get8(p, &r->defaultCurrency);
    p = Get8(p, &r->archived);
    return 0;
}

/* ---- programme ---- */

UInt32 ProgrammeSize(const ProgrammeRecord *r)
{
    return pgmFixedSize + CStrLen(r->aliases, pgmAliasLen) + 1;
}

UInt32 ProgrammePack(const ProgrammeRecord *r, UInt8 *buf)
{
    UInt8 *p = buf;
    UInt32 n, i;
    p = Put8(p, r->id);
    p = PutStr(p, r->name, pgmNameLen);
    p = Put8(p, r->currency);
    p = Put8(p, r->earnBasis);
    p = Put32(p, r->earnRate);
    p = Put16(p, r->premiumMult);
    p = Put8(p, r->requiresPayment);
    p = Put32(p, (UInt32)r->openingBalance);
    p = Put32(p, r->valuePerPoint);
    p = Put8(p, r->autoApply);
    p = Put8(p, r->archived);
    p = Put8(p, r->redeemKind);
    p = Put32(p, r->redeemBlock);
    p = Put32(p, r->redeemValue);
    p = Put8(p, r->redeemVolUnit);
    p = Put32(p, r->redeemMaxVol);
    p = Put32(p, r->redeemMaxPerUnit);
    p = Put8(p, r->expiryRule);
    p = Put8(p, r->expiryMonths);
    n = CStrLen(r->aliases, pgmAliasLen);
    for (i = 0; i < n; i++) *p++ = (UInt8)r->aliases[i];
    *p++ = 0;
    return (UInt32)(p - buf);
}

Int32 ProgrammeUnpack(const UInt8 *buf, UInt32 len, ProgrammeRecord *r)
{
    const UInt8 *p = buf, *end = buf + len;
    UInt32 v, n;
    if (len < pgmFixedSize) return -1;
    p = Get8(p, &r->id);
    p = GetStr(p, r->name, pgmNameLen);
    p = Get8(p, &r->currency);
    p = Get8(p, &r->earnBasis);
    p = Get32(p, &r->earnRate);
    p = Get16(p, &r->premiumMult);
    p = Get8(p, &r->requiresPayment);
    p = Get32(p, &v); r->openingBalance = (Int32)v;
    p = Get32(p, &r->valuePerPoint);
    p = Get8(p, &r->autoApply);
    p = Get8(p, &r->archived);
    p = Get8(p, &r->redeemKind);
    p = Get32(p, &r->redeemBlock);
    p = Get32(p, &r->redeemValue);
    p = Get8(p, &r->redeemVolUnit);
    p = Get32(p, &r->redeemMaxVol);
    p = Get32(p, &r->redeemMaxPerUnit);
    p = Get8(p, &r->expiryRule);
    p = Get8(p, &r->expiryMonths);
    n = 0;
    while (p < end && *p && n < pgmAliasLen - 1) r->aliases[n++] = (char)*p++;
    r->aliases[n] = 0;
    return 0;
}

/* ---- service type ---- */

UInt32 ServiceTypePack(const ServiceTypeRecord *r, UInt8 *buf)
{
    UInt8 *p = buf;
    p = Put8(p, r->id);
    p = Put8(p, r->vehicle);
    p = PutStr(p, r->name, svtNameLen);
    p = Put32(p, r->intervalDist);
    p = Put8(p, r->intervalMonths);
    p = Put32(p, r->warnDist);
    p = Put16(p, r->warnDays);
    p = Put8(p, r->alarm);
    p = Put8(p, r->archived);
    p = PutStr(p, r->defaultDetail, svtDetailLen);
    return (UInt32)(p - buf);
}

Int32 ServiceTypeUnpack(const UInt8 *buf, UInt32 len, ServiceTypeRecord *r)
{
    const UInt8 *p = buf;
    if (len < svtRecordSize) return -1;
    p = Get8(p, &r->id);
    p = Get8(p, &r->vehicle);
    p = GetStr(p, r->name, svtNameLen);
    p = Get32(p, &r->intervalDist);
    p = Get8(p, &r->intervalMonths);
    p = Get32(p, &r->warnDist);
    p = Get16(p, &r->warnDays);
    p = Get8(p, &r->alarm);
    p = Get8(p, &r->archived);
    p = GetStr(p, r->defaultDetail, svtDetailLen);
    return 0;
}

/* ---- service visit ---- */

UInt32 ServiceVisitSize(const ServiceVisitRecord *r)
{
    return svlFixedSize + CStrLen(r->notes, fillMaxNotes) + 1;
}

UInt32 ServiceVisitPack(const ServiceVisitRecord *r, UInt8 *buf)
{
    UInt8 *p = buf;
    UInt32 n, i;
    p = Put16(p, r->date);
    p = Put8(p, r->vehicle);
    p = Put32(p, r->odometer);
    p = Put32(p, r->services);
    p = PutStr(p, r->other, svlOtherLen);
    p = Put32(p, r->cost);
    p = Put8(p, r->currency);
    p = Put32(p, r->fxRate);
    p = Put8(p, r->payment);
    p = PutStr(p, r->shop, svlShopLen);
    p = PutStr(p, r->detail, svlDetailLen);
    n = CStrLen(r->notes, fillMaxNotes);
    for (i = 0; i < n; i++) *p++ = (UInt8)r->notes[i];
    *p++ = 0;
    return (UInt32)(p - buf);
}

Int32 ServiceVisitUnpack(const UInt8 *buf, UInt32 len, ServiceVisitRecord *r)
{
    const UInt8 *p = buf, *end = buf + len;
    UInt32 n;
    if (len < svlFixedSize) return -1;
    p = Get16(p, &r->date);
    p = Get8(p, &r->vehicle);
    p = Get32(p, &r->odometer);
    p = Get32(p, &r->services);
    p = GetStr(p, r->other, svlOtherLen);
    p = Get32(p, &r->cost);
    p = Get8(p, &r->currency);
    p = Get32(p, &r->fxRate);
    p = Get8(p, &r->payment);
    p = GetStr(p, r->shop, svlShopLen);
    p = GetStr(p, r->detail, svlDetailLen);
    n = 0;
    while (p < end && *p && n < fillMaxNotes - 1) r->notes[n++] = (char)*p++;
    r->notes[n] = 0;
    return 0;
}

Int32 ServiceVisitCompare(const UInt8 *a, const UInt8 *b)
{
    UInt16 da, db;
    Get16(a, &da); Get16(b, &db);
    if (da != db) return da < db ? -1 : 1;
    return 0;
}

/* ---- app info ---- */

UInt32 AppInfoPack(const AppInfoBlock *a, UInt8 *buf)
{
    UInt8 *p = buf;
    p = Put16(p, a->schemaVersion);
    p = Put16(p, a->reserved);
    return (UInt32)(p - buf);
}

Int32 AppInfoUnpack(const UInt8 *buf, UInt32 len, AppInfoBlock *a)
{
    const UInt8 *p = buf;
    if (len < appInfoSize) return -1;
    p = Get16(p, &a->schemaVersion);
    p = Get16(p, &a->reserved);
    return 0;
}
