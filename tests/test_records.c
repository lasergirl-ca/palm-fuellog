/* Byte-exact checks of the packed record layouts and pack/unpack round trips. */
#include <stdio.h>
#include <string.h>
#include "../src/records.h"

static int failures;

static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

static void checkStr(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want)) { printf("FAIL %s: got \"%s\" want \"%s\"\n", name, got, want); failures++; }
    else printf("ok   %s = \"%s\"\n", name, got);
}

int main(void)
{
    UInt8 buf[512];
    UInt32 n;

    /* Fill: the Oct 19 sample fill with Petro-Points and Triangle, packed and read back. */
    {
        FillRecord f, g;
        memset(&f, 0, sizeof f);
        f.date = 44852; f.time = 11 * 60 + 36; f.flags = fillFlagFullTank | fillFlagTripEntered; f.vehicle = 1; f.grade = fillGradeRegular;
        f.odometer = 842120; f.trip = 5720; f.volume = 48200; f.price = 17490; f.total = 8430; f.discount = 0;
        f.currency = 1; f.fxRate = 1000000; f.payment = fillPaymentCash; f.pointCount = 2;
        f.points[0].programme = 2; f.points[0].earned = 482; f.points[0].redeemed = 0;
        f.points[1].programme = 3; f.points[1].earned = 145; f.points[1].redeemed = -7;
        strcpy(f.notes, "Petro-Canada on 152 St.");
        n = FillPack(&f, buf);
        check("fill size", n, FillSize(&f));
        check("fill size value", n, 39 + 2 * 9 + 23 + 1);
        /* Byte offsets from the design spec's canonical layout. */
        check("fill date at 0", (buf[0] << 8) | buf[1], 44852);
        check("fill time at 2", (buf[2] << 8) | buf[3], 696);
        check("fill flags at 4", (buf[4] << 8) | buf[5], 5);
        check("fill vehicle at 6", buf[6], 1);
        check("fill odometer at 8", ((long)buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11], 842120);
        check("fill total at 24", ((long)buf[24] << 24) | (buf[25] << 16) | (buf[26] << 8) | buf[27], 8430);
        check("fill currency at 32", buf[32], 1);
        check("fill fxRate at 33", ((long)buf[33] << 24) | (buf[34] << 16) | (buf[35] << 8) | buf[36], 1000000);
        check("fill payment at 37", buf[37], 0);
        check("fill pointCount at 38", buf[38], 2);
        check("fill point 1 pgm at 39", buf[39], 2);
        check("fill point 2 redeemed at 53 (odd offset)", (long)(Int32)(((UInt32)buf[53] << 24) | (buf[54] << 16) | (buf[55] << 8) | buf[56]), -7);
        check("fill notes at 57", buf[57], 'P');
        check("fill terminator", buf[n - 1], 0);
        check("fill unpack", FillUnpack(buf, n, &g), 0);
        check("fill odometer back", g.odometer, 842120);
        check("fill time back", g.time, 696);
        check("fill redeemed back", g.points[1].redeemed, -7);
        checkStr("fill notes back", g.notes, "Petro-Canada on 152 St.");
        check("fill unpack short buffer", FillUnpack(buf, 20, &g), -1);
        buf[38] = 5;
        check("fill unpack bad pointCount", FillUnpack(buf, n, &g), -1);
        /* Sort order: date, then odometer. */
        {
            UInt8 a[64], b[64];
            FillRecord x = f, y = f;
            y.date = f.date + 1; y.odometer = 1;
            FillPack(&x, a); FillPack(&y, b);
            check("compare by date", FillCompare(a, b), -1);
            y.date = f.date; y.odometer = f.odometer + 10;
            FillPack(&y, b);
            check("compare by odometer", FillCompare(a, b), -1);
            check("compare equal", FillCompare(a, a), 0);
        }
        /* Notes longer than the field are cut to 255 bytes plus the terminator. */
        memset(f.notes, 'x', sizeof f.notes); f.notes[sizeof f.notes - 1] = 0;
        n = FillPack(&f, buf);
        check("fill long notes size", n, 39 + 18 + 255 + 1);
    }

    /* Vehicle: fixed 47 bytes, name padded with zeros. */
    {
        VehicleRecord v, w;
        memset(&v, 0, sizeof v);
        v.id = 1; strcpy(v.name, "Outback"); v.powertrain = vehPowerFuel; v.odoUnit = vehOdoKm; v.tripMode = vehTripReset;
        v.startOdo = 806500; v.tankSize = 60000; v.batteryWh = 0; v.homeRate = 0; v.fuelDefault = 0;
        v.defaultCurrency = vehCurrencyUsePrefs; v.archived = 0;
        n = VehiclePack(&v, buf);
        check("vehicle size", n, vehRecordSize);
        check("vehicle name padding", buf[1 + 7], 0);
        check("vehicle startOdo at 28", ((long)buf[28] << 24) | (buf[29] << 16) | (buf[30] << 8) | buf[31], 806500);
        check("vehicle defaultCurrency at 45", buf[45], 255);
        check("vehicle unpack", VehicleUnpack(buf, n, &w), 0);
        checkStr("vehicle name back", w.name, "Outback");
        check("vehicle tank back", w.tankSize, 60000);
        /* A field filled to its last byte with no terminator still packs and
         * unpacks as a 23-character string. */
        memcpy(v.name, "Twenty-four characters!!", vehNameLen);
        VehiclePack(&v, buf); VehicleUnpack(buf, vehRecordSize, &w);
        check("vehicle name cut to 23", strlen(w.name), 23);
        check("vehicle name terminator in packed bytes", buf[1 + 23], 0);
    }

    /* Programme: 64 fixed bytes plus aliases. */
    {
        ProgrammeRecord p, q;
        memset(&p, 0, sizeof p);
        p.id = 1; strncpy(p.name, "76 / Phillips 66 / Conoco", pgmNameLen - 1); p.currency = 0; p.earnBasis = pgmEarnPerUSGallon;
        p.earnRate = 300; p.premiumMult = 100; p.requiresPayment = pgmPaymentCard; p.openingBalance = -1250;
        p.valuePerPoint = 1000; p.autoApply = 1; p.redeemKind = pgmRedeemMoneyOff; p.redeemBlock = 1;
        p.redeemValue = 10; p.redeemVolUnit = pgmVolUSGallon; p.redeemMaxVol = 132489; p.redeemMaxPerUnit = 1000;
        p.expiryRule = pgmExpiryNone; strcpy(p.aliases, "KB Points,KB");
        n = ProgrammePack(&p, buf);
        check("programme size", n, pgmFixedSize + 12 + 1);
        check("programme size fn", n, ProgrammeSize(&p));
        check("programme earnRate at 27", ((long)buf[27] << 24) | (buf[28] << 16) | (buf[29] << 8) | buf[30], 300);
        check("programme aliases at 64", buf[64], 'K');
        check("programme unpack", ProgrammeUnpack(buf, n, &q), 0);
        check("programme opening back", q.openingBalance, -1250);
        checkStr("programme aliases back", q.aliases, "KB Points,KB");
        checkStr("programme name cut to 23", q.name, "76 / Phillips 66 / Cono");
    }

    /* Service type and visit. */
    {
        ServiceTypeRecord t, u;
        ServiceVisitRecord s, r;
        memset(&t, 0, sizeof t); memset(&s, 0, sizeof s);
        t.id = 0; t.vehicle = 1; strcpy(t.name, "Oil and filter"); t.intervalDist = 80000; t.intervalMonths = 6;
        t.warnDist = 8000; t.warnDays = 14; t.alarm = 1; strcpy(t.defaultDetail, "0W-20 synthetic, 4.8 L");
        n = ServiceTypePack(&t, buf);
        check("service type size", n, svtRecordSize);
        check("service type unpack", ServiceTypeUnpack(buf, n, &u), 0);
        check("service type interval back", u.intervalDist, 80000);
        checkStr("service type detail back", u.defaultDetail, "0W-20 synthetic, 4.8 L");

        s.date = 44855; s.vehicle = 1; s.odometer = 842120; s.services = 0x7UL | svlOtherBit;
        strncpy(s.other, "Battery terminals cleaned", svlOtherLen - 1); s.cost = 12995; s.currency = 1; s.fxRate = 1000000;
        s.payment = fillPaymentCard; strcpy(s.shop, "Corner Garage"); strcpy(s.detail, "0W-20 synthetic, 4.8 L");
        strcpy(s.notes, "Also topped up washer fluid.");
        n = ServiceVisitPack(&s, buf);
        check("service visit size", n, svlFixedSize + 28 + 1);
        check("service visit services at 7", ((long)buf[7] << 24) | (buf[8] << 16) | (buf[9] << 8) | buf[10], (long)(0x7UL | svlOtherBit));
        check("service visit unpack", ServiceVisitUnpack(buf, n, &r), 0);
        checkStr("service visit other cut to 23", r.other, "Battery terminals clean");
        checkStr("service visit notes back", r.notes, "Also topped up washer fluid.");
        {
            UInt8 a[160], b[160];
            ServiceVisitRecord later = s; later.date++;
            ServiceVisitPack(&s, a); ServiceVisitPack(&later, b);
            check("visit compare", ServiceVisitCompare(a, b), -1);
        }
    }

    /* AppInfo. */
    {
        AppInfoBlock a, b;
        a.schemaVersion = recSchemaVersion; a.reserved = 0;
        n = AppInfoPack(&a, buf);
        check("appinfo size", n, appInfoSize);
        check("appinfo version bytes", (buf[0] << 8) | buf[1], 1);
        check("appinfo unpack", AppInfoUnpack(buf, n, &b), 0);
        check("appinfo version back", b.schemaVersion, 1);
    }

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
