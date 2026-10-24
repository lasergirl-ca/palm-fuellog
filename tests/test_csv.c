/* CSV export round trip and the paper-log header. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/csv.h"
#include "../src/units.h"
#include "../src/currency.h"
#include "../src/datecalc.h"
#include "../src/calc.h"

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

/* An OutStream into memory. */
static char gMem[8192]; static UInt32 gMemLen;
static Int32 MemWrite(OutStream *s, const void *buf, UInt32 len) { (void)s; if (gMemLen + len >= sizeof gMem) return -1; memcpy(gMem + gMemLen, buf, len); gMemLen += len; gMem[gMemLen] = 0; return 0; }
/* A byte source over memory. */
static const char *gIn; static UInt32 gInPos, gInLen;
static Int16 MemGetc(void *ctx) { (void)ctx; return gInPos < gInLen ? (UInt8)gIn[gInPos++] : -1; }

int main(void)
{
    OutStream out; CsvCtx c; FillRecord f, g; char *fields[csvMaxFields]; char rec[csvMaxRecord], veh[24], err[40];
    CsvMap m; UInt32 line = 0; Int16 n;
    memset(&out, 0, sizeof out); out.write = MemWrite;
    memset(&c, 0, sizeof c);
    c.distUnit = unitDistKm; c.volUnit = unitVolLitres; c.priceUnit = unitPricePerLitre; c.priceCents = 1; c.homeCurrency = curCAD;
    c.pgmCount = 2; c.pgmIds[0] = 2; strcpy(c.pgmNames[0], "Petro-Points"); c.pgmIds[1] = 3; strcpy(c.pgmNames[1], "Triangle Rewards");
    memset(&f, 0, sizeof f);
    f.date = DateYmdToDays(2026, 10, 19); f.time = 14 * 60 + 5; f.vehicle = 1; f.odometer = 842120; f.trip = 5580; f.volume = 48200;
    f.price = 17490; f.total = 8430; f.currency = curCAD; f.fxRate = fxOne; f.payment = fillPaymentCard; f.grade = fillGradeRegular;
    f.flags = fillFlagFullTank | fillFlagTripEntered; f.discount = 100;
    f.pointCount = 2; f.points[0].programme = 2; f.points[0].earned = 482; f.points[0].redeemed = 1000; f.points[1].programme = 3; f.points[1].earned = 145;
    strcpy(f.notes, "Pump 6 \"card\" down, caf\xE9 ok");
    CsvBom(&out); CsvHeader(&out, &c); CsvFillRow(&out, &c, &f, "Outback");
    check("starts with a BOM", (UInt8)gMem[0], 0xEF);
    check("header names litres", strstr(gMem, "Volume (L)") != NULL, 1);
    check("row has the ISO date", strstr(gMem, "2026-10-19,14:05,\"Outback\",84212.0,558.0,48.20,1.7490,84.30,CAD,1.0000,84.30,Card,Regular,yes,,1.00,482,1000,145,,") != NULL, 1);
    check("notes quoted with doubled quotes and UTF-8 e-acute", strstr(gMem, "\"Pump 6 \"\"card\"\" down, caf\xC3\xA9 ok\"") != NULL, 1);

    /* Read it back. */
    gIn = gMem; gInPos = 0; gInLen = gMemLen;
    check("header record read", CsvNextRecord(MemGetc, NULL, rec, sizeof rec, &line) > 0, 1);
    n = CsvSplit(rec, fields, csvMaxFields);
    check("header has 21 fields", n, 21);
    check("header maps", CsvMapHeader(fields, n, &m) > 10, 1);
    check("map: price per litre", m.priceUnit, unitPricePerLitre);
    check("map: two programmes", m.pgmCount, 2);
    checkStr("map: programme 2 name", m.pgmNames[1], "Triangle Rewards");
    check("data record read", CsvNextRecord(MemGetc, NULL, rec, sizeof rec, &line) > 0, 1);
    n = CsvSplit(rec, fields, csvMaxFields);
    check("row parses", CsvRowToFill(&m, fields, n, &g, veh, err), 0);
    checkStr("vehicle name", veh, "Outback");
    check("date back", g.date, f.date);
    check("time back", g.time, f.time);
    check("odometer back", g.odometer, 842120);
    check("trip back", g.trip, 5580);
    check("volume back", g.volume, 48200);
    check("price back", g.price, 17490);
    check("total back", g.total, 8430);
    check("card back", g.payment, fillPaymentCard);
    check("full back", (g.flags & fillFlagFullTank) != 0, 1);
    check("discount back", g.discount, 100);
    check("points back: two", g.pointCount, 2);
    check("points back: Petro earned", g.points[0].earned, 482);
    check("points back: Petro redeemed", g.points[0].redeemed, 1000);
    checkStr("notes back in Palm Latin", g.notes, "Pump 6 \"card\" down, caf\xE9 ok");
    check("end of file", CsvNextRecord(MemGetc, NULL, rec, sizeof rec, &line), -1);

    /* The paper log typed into a spreadsheet: US gallons, $/gallon, KB Points, Mac line endings. */
    {
        const char *paper = "Date,Trip,Odometer (mi),Fuel Amt. (US gal),$/Gallon,KB Points,Cumulative,Notes\r"
                            "10/19/2026,346.7,52345,12.495,3.459,37,1250,\"76 on Meridian\"\r"
                            "13/31/2019,10,52400,5,3.4,,,\r";
        gIn = paper; gInPos = 0; gInLen = (UInt32)strlen(paper);
        line = 0;
        /* CR-only files: our reader treats CR as whitespace, so give it LF too */
        {
            static char fixed[400]; size_t i; for (i = 0; i < strlen(paper); i++) fixed[i] = paper[i] == '\r' ? '\n' : paper[i]; fixed[i] = 0;
            gIn = fixed; gInLen = (UInt32)strlen(fixed);
        }
        CsvNextRecord(MemGetc, NULL, rec, sizeof rec, &line);
        n = CsvSplit(rec, fields, csvMaxFields);
        check("paper header maps", CsvMapHeader(fields, n, &m) >= 6, 1);
        check("paper: miles", m.distUnit, unitDistMiles);
        check("paper: US gallons", m.volUnit, unitVolUSGal);
        check("paper: price per US gal", m.priceUnit, unitPricePerUSGal);
        check("paper: KB Points column", m.kbPoints, 5);
        CsvNextRecord(MemGetc, NULL, rec, sizeof rec, &line);
        n = CsvSplit(rec, fields, csvMaxFields);
        check("paper row parses", CsvRowToFill(&m, fields, n, &g, veh, err), 0);
        check("paper: 52,345 mi in km x10", g.odometer, UnitDistFromDisplay(523450, unitDistMiles));
        check("paper: 12.495 gal in mL", g.volume, UnitVolFromDisplay(12495, unitVolUSGal));
        check("paper: $3.459/gal per litre", g.price, UnitPriceFromDisplay(34590, unitPricePerUSGal));
        check("paper: total computed", g.total, CalcTotalCents(g.price, g.volume));
        check("paper: KB Points as alias entry", g.pointCount == 1 && g.points[0].programme == 255 && g.points[0].earned == 37, 1);
        CsvNextRecord(MemGetc, NULL, rec, sizeof rec, &line);
        n = CsvSplit(rec, fields, csvMaxFields);
        check("bad date rejected", CsvRowToFill(&m, fields, n, &g, veh, err), -1);
        checkStr("bad date reason", err, "date is not valid");
    }
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
