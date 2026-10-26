#include "export.h"
#include "card.h"
#include "csv.h"
#include "app.h"
#include "db.h"
#include "datecalc.h"
#include "vehicles.h"
#include "programmes.h"

static CsvCtx gCsv;
static FillRecord gExpFill;
static VehicleRecord gExpVeh;
static ProgrammeRecord gExpPgm;
static CardOut gOut;
UInt8 gExportPgm = 255;

/* Section placement: see sections.h. */
static void BuildCtx(void) IO_SEC;

static void BuildCtx(void)
{
    UInt16 n = DbCount(dbProgrammes), i;
    MemSet(&gCsv, sizeof gCsv, 0);
    gCsv.distUnit = gPrefs.distanceUnit; gCsv.volUnit = gPrefs.volumeUnit; gCsv.priceUnit = gPrefs.priceUnit;
    gCsv.priceCents = gPrefs.priceFormat == 1; gCsv.homeCurrency = gPrefs.homeCurrency;
    for (i = 0; i < n && gCsv.pgmCount < csvMaxPgms; i++) {
        if (PgmGetAt(i, &gExpPgm) != errNone) continue;
        if (gExportPgm != 255 && gExpPgm.id != gExportPgm) continue;
        gCsv.pgmIds[gCsv.pgmCount] = gExpPgm.id;
        StrCopy(gCsv.pgmNames[gCsv.pgmCount], gExpPgm.name);
        gCsv.pgmCount++;
    }
}

void ExportCsvTo(OutStream *out, UInt8 vehicle, UInt16 year)
{
    UInt16 n = DbCount(dbFills), i;
    UInt8 lastVeh = 255;
    BuildCtx();
    CsvBom(out);
    CsvHeader(out, &gCsv);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gExpFill) != errNone || !ExportWants(&gExpFill, vehicle, year)) continue;
        if (gExpFill.vehicle != lastVeh) { VehGet(gExpFill.vehicle, &gExpVeh); lastVeh = gExpFill.vehicle; }
        CsvFillRow(out, &gCsv, &gExpFill, gExpVeh.name);
        if (out->err) break;
    }
}

Boolean ExportWants(const FillRecord *f, UInt8 vehicle, UInt16 year)
{
    if (vehicle != 255 && f->vehicle != vehicle) return false;
    if (gExportPgm != 255) {
        UInt8 k; Boolean has = false;
        for (k = 0; k < f->pointCount; k++) if (f->points[k].programme == gExportPgm) has = true;
        if (!has) return false;
    }
    if (year) { UInt16 y; UInt8 m, d; DateDaysToYmd(f->date, &y, &m, &d); if (y != year) return false; }
    return true;
}

Err ExportCsv(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes)
{
    char path[80];
    UInt16 n = DbCount(dbFills), i;
    UInt8 lastVeh = 255;
    Err err;
    CardExportName(nameOut, ".csv");
    StrCopy(path, cardExport); StrCat(path, "/"); StrCat(path, nameOut);
    err = CardEnsureDirs(vol);
    if (err) return err;
    err = CardOutOpen(&gOut, vol, path);
    if (err) return err;
    ExportCsvTo(&gOut.out, vehicle, year);
    err = CardOutClose(&gOut);
    (void)n; (void)i; (void)lastVeh;
    *bytes = CardFileSize(vol, path);
    return err;
}
