#include "import.h"
#include "csv.h"
#include "units.h"
#include "app.h"
#include "db.h"
#include "datecalc.h"
#include "vehicles.h"
#include "programmes.h"
#include "resources.h"

ImportSummary gImport;
static CardIn gIn;
static char *gRecord;                     /* csvMaxRecord bytes, heap during a pass */
static char *gFields[csvMaxFields];
static CsvMap gMap;
static FillRecord gImpFill, gImpScan;
static VehicleRecord gImpVeh;
static ProgrammeRecord gImpPgm;
static UInt8 gPgmIdFor[csvMaxPgms];       /* header programme index -> programme id (0 = none yet) */
static UInt8 gKbId;
static char gLastVehName[impMaxNewVeh + 12][24];   /* vehicles seen in the file, for odometer checks */
static UInt32 gLastOdo[impMaxNewVeh + 12];
static UInt32 gLastLine[impMaxNewVeh + 12];
static UInt8 gSeenCount;

/* Section placement: see sections.h. */
static Int16 Getc(void *ctx) IO_SEC;
static Boolean NameEq(const char *a, const char *b) IO_SEC;
static Int16 VehIndexByName(const char *name) IO_SEC;
static UInt8 PgmIdByName(const char *name) IO_SEC;
static void AddError(UInt32 line, const char *why) IO_SEC;
static Boolean IsDuplicate(const FillRecord *f) IO_SEC;
static UInt8 ResolveVehicle(const char *name, Boolean write) IO_SEC;
static UInt8 ResolveProgramme(const char *name, Boolean write) IO_SEC;
static void ResolveProgrammes(Boolean write) IO_SEC;
static Err Pass(UInt16 vol, Boolean write) IO_SEC;
static void Line(const char *s, Coord *y, Boolean bold) IO_SEC;
static void DrawImport(void) IO_SEC;

static Int16 Getc(void *ctx) { return CardInGetc((CardIn *)ctx); }

static Boolean NameEq(const char *a, const char *b) { return StrCaselessCompare(a, b) == 0; }

static Int16 VehIndexByName(const char *name)
{
    UInt16 n = DbCount(dbVehicles), i;
    for (i = 0; i < n; i++) if (DbGetVehicle(i, &gImpVeh) == errNone && NameEq(gImpVeh.name, name)) return (Int16)i;
    return -1;
}

/* A programme by name, or by one of its comma-separated aliases. */
static UInt8 PgmIdByName(const char *name)
{
    UInt16 n = DbCount(dbProgrammes), i;
    for (i = 0; i < n; i++) {
        const char *a;
        if (PgmGetAt(i, &gImpPgm) != errNone) continue;
        if (NameEq(gImpPgm.name, name)) return gImpPgm.id;
        a = gImpPgm.aliases;
        while (*a) {
            char one[pgmAliasLen]; UInt16 k = 0;
            while (*a == ' ') a++;
            while (*a && *a != ',' && k < pgmAliasLen - 1) one[k++] = *a++;
            while (k && one[k - 1] == ' ') k--;
            one[k] = 0;
            if (*a == ',') a++;
            if (k && NameEq(one, name)) return gImpPgm.id;
        }
    }
    return 0;
}

static void AddError(UInt32 line, const char *why)
{
    char *b;
    gImport.errors++;
    if (gImport.errors > impMaxErrors) return;
    b = gImport.errorLines[gImport.errors - 1];
    StrCopy(b, "Line "); StrIToA(b + 5, (Int32)line); StrCat(b, ": ");
    StrNCat(b, why, 43 - StrLen(b));
}

static Boolean IsDuplicate(const FillRecord *f)
{
    UInt16 n = DbCount(dbFills), i;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gImpScan) != errNone) continue;
        if (gImpScan.date > f->date) break;              /* sorted by date */
        if (gImpScan.date == f->date && gImpScan.vehicle == f->vehicle && gImpScan.odometer == f->odometer) return true;
    }
    return false;
}

/* Resolve the vehicle name to an id; in the write pass unknown names are created. */
static UInt8 ResolveVehicle(const char *name, Boolean write)
{
    Int16 i;
    UInt8 k;
    if (!name[0]) return VehDefaultForNewFill();
    i = VehIndexByName(name);
    if (i >= 0 && DbGetVehicle((UInt16)i, &gImpVeh) == errNone) return gImpVeh.id;
    for (k = 0; k < gImport.newVehicles && k < impMaxNewVeh; k++) if (NameEq(gImport.newVehicleNames[k], name)) break;
    if (k == gImport.newVehicles || k >= impMaxNewVeh) {
        if (gImport.newVehicles < impMaxNewVeh) StrCopy(gImport.newVehicleNames[gImport.newVehicles], name);
        gImport.newVehicles++;
    }
    if (!write) return 0;
    {
        UInt8 buf[vehRecordSize];
        MemSet(&gImpVeh, sizeof gImpVeh, 0);
        gImpVeh.id = VehNextId();
        if (gImpVeh.id == 0) return 0;
        StrNCopy(gImpVeh.name, name, vehNameLen - 1); gImpVeh.name[vehNameLen - 1] = 0;
        gImpVeh.defaultCurrency = vehCurrencyUsePrefs; gImpVeh.tankSize = 60000;
        gImpVeh.odoUnit = gMap.distUnit == unitDistMiles ? vehOdoMiles : vehOdoKm;
        VehiclePack(&gImpVeh, buf);
        if (DbAdd(dbVehicles, buf, vehRecordSize, NULL)) return 0;
        return gImpVeh.id;
    }
}

static UInt8 ResolveProgramme(const char *name, Boolean write)
{
    UInt8 id = PgmIdByName(name);
    if (id || !write) { if (!id) gImport.newProgrammes++; return id; }
    {
        static UInt8 buf[pgmFixedSize + pgmAliasLen + 4];
        MemSet(&gImpPgm, sizeof gImpPgm, 0);
        gImpPgm.id = PgmNextId();
        if (gImpPgm.id == 0) return 0;
        StrNCopy(gImpPgm.name, name, pgmNameLen - 1); gImpPgm.name[pgmNameLen - 1] = 0;
        gImpPgm.premiumMult = 100; gImpPgm.currency = gPrefs.homeCurrency;
        if (DbAdd(dbProgrammes, buf, ProgrammePack(&gImpPgm, buf), NULL)) return 0;
        return gImpPgm.id;
    }
}

static void ResolveProgrammes(Boolean write)
{
    UInt8 j;
    gImport.newProgrammes = 0;
    for (j = 0; j < gMap.pgmCount; j++) gPgmIdFor[j] = ResolveProgramme(gMap.pgmNames[j], write);
    gKbId = 0;
    if (gMap.kbPoints >= 0) {
        gKbId = PgmIdByName("KB Points");
        if (!gKbId) gKbId = PgmIdByName("76 / Phillips 66");
        if (gKbId) { PgmGet(gKbId, &gImpPgm); StrCopy(gImport.kbTarget, gImpPgm.name); }
        else { StrCopy(gImport.kbTarget, "a new KB Points programme"); if (write) gKbId = ResolveProgramme("KB Points", true); else gImport.newProgrammes++; }
        gImport.kbPoints = true;
    }
}

/* One pass over the file. write = false counts; write = true adds. */
static Err Pass(UInt16 vol, Boolean write)
{
    char path[80], veh[24], why[40];
    UInt32 line = 0, recLine;
    Int32 len;
    Int16 n;
    Err err;
    StrCopy(path, cardImport); StrCat(path, "/"); StrCat(path, gImport.file);
    gRecord = MemPtrNew(csvMaxRecord);
    if (!gRecord) return memErrNotEnoughSpace;
    err = CardInOpen(&gIn, vol, path);
    if (err) { MemPtrFree(gRecord); gRecord = NULL; return err; }
    gSeenCount = 0;
    /* header */
    do { len = CsvNextRecord(Getc, &gIn, gRecord, csvMaxRecord, &line); } while (len == 0);
    if (len < 0) { CardInClose(&gIn); MemPtrFree(gRecord); gRecord = NULL; gImport.badHeader = 1; return errNone; }
    n = CsvSplit(gRecord, gFields, csvMaxFields);
    if (CsvMapHeader(gFields, n, &gMap) == 0) { CardInClose(&gIn); MemPtrFree(gRecord); gRecord = NULL; gImport.badHeader = 1; return errNone; }
    ResolveProgrammes(write);
    for (;;) {
        UInt8 k, vid;
        recLine = line + 1;
        len = CsvNextRecord(Getc, &gIn, gRecord, csvMaxRecord, &line);
        if (len < 0) break;
        if (len == 0) continue;
        n = CsvSplit(gRecord, gFields, csvMaxFields);
        if (!write) gImport.rows++;
        if (CsvRowToFill(&gMap, gFields, n, &gImpFill, veh, why) != 0) { if (!write) AddError(recLine, why); continue; }
        /* odometer must not fall for the same vehicle within the file */
        if (!write && gImpFill.odometer) {
            for (k = 0; k < gSeenCount; k++) if (NameEq(gLastVehName[k], veh)) break;
            if (k == gSeenCount && gSeenCount < impMaxNewVeh + 12) { StrCopy(gLastVehName[k], veh); gLastOdo[k] = 0; gLastLine[k] = 0; gSeenCount++; }
            if (k < gSeenCount) {
                if (gLastOdo[k] && gImpFill.odometer < gLastOdo[k]) {
                    char w[40]; StrCopy(w, "odometer lower than line "); StrIToA(w + StrLen(w), (Int32)gLastLine[k]);
                    AddError(recLine, w); continue;
                }
                gLastOdo[k] = gImpFill.odometer; gLastLine[k] = recLine;
            }
        }
        vid = ResolveVehicle(veh, write);
        if (write && vid == 0) { continue; }
        gImpFill.vehicle = vid;
        /* programme indexes to ids */
        {
            UInt8 kept = 0;
            for (k = 0; k < gImpFill.pointCount; k++) {
                UInt8 ref = gImpFill.points[k].programme, id = ref == 255 ? gKbId : (ref >= 1 && ref <= gMap.pgmCount ? gPgmIdFor[ref - 1] : 0);
                if (!id) continue;
                gImpFill.points[kept] = gImpFill.points[k]; gImpFill.points[kept].programme = id; kept++;
            }
            gImpFill.pointCount = kept;
        }
        if (write) {
            if (IsDuplicate(&gImpFill)) { gImport.skipped++; continue; }
            if (DbAddFill(&gImpFill, NULL) == errNone) gImport.written++;
        } else {
            if (vid && IsDuplicate(&gImpFill)) gImport.duplicates++; else gImport.ready++;
        }
    }
    CardInClose(&gIn);
    MemPtrFree(gRecord); gRecord = NULL;
    return errNone;
}

Err ImportScan(UInt16 vol, const char *file)
{
    MemSet(&gImport, sizeof gImport, 0);
    StrNCopy(gImport.file, file, cardNameLen - 1);
    return Pass(vol, false);
}

Err ImportWrite(UInt16 vol)
{
    gImport.written = gImport.skipped = 0;
    gImport.newVehicles = 0;
    return Pass(vol, true);
}

/* ---- Choose a file ---- */

static char gFiles[16][cardNameLen];
static char *gFilePtrs[16];
static UInt16 gFileCount;

Boolean ChooseFileFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { ChooseOpenButton, ChooseCancelButton };
    switch (event->eType) {
    case frmOpenEvent: {
        ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ChooseList));
        UInt16 vol, i, n = 0;
        AppFitForm(form, bottom, 2);
        if (CardFind(&vol) == errNone) { CardEnsureDirs(vol); n = CardList(vol, cardImport, false, gFiles, 16); }
        gFileCount = 0;
        for (i = 0; i < n; i++) {
            UInt16 l = StrLen(gFiles[i]);
            if (l > 4 && StrCaselessCompare(gFiles[i] + l - 4, ".csv") == 0) { if (gFileCount != i) StrCopy(gFiles[gFileCount], gFiles[i]); gFileCount++; }
        }
        if (gFileCount == 0) { StrCopy(gFiles[0], "(no CSV files)"); gFileCount = 1; }
        for (i = 0; i < gFileCount; i++) gFilePtrs[i] = gFiles[i];
        LstSetListChoices(lst, gFilePtrs, (Int16)gFileCount);
        LstSetSelection(lst, 0);
        FrmDrawForm(form);
        return true;
    }
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case winDisplayChangedEvent: AppFitForm(form, bottom, 2); WinEraseWindow(); FrmDrawForm(form); return true;
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == ChooseCancelButton) { FrmGotoForm(ExportForm); return true; }
        if (event->data.ctlSelect.controlID == ChooseOpenButton) {
            ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ChooseList));
            Int16 sel = LstGetSelection(lst);
            UInt16 vol;
            Err err;
            if (sel < 0 || gFiles[sel][0] == '(') { FrmAlert(NoImportFilesAlert); return true; }
            if (CardFind(&vol) != errNone) { FrmAlert(NoCardAlert); return true; }
            err = ImportScan(vol, gFiles[sel]);
            if (err) { char n[8]; StrIToA(n, err); FrmCustomAlert(ExportFailedAlert, "Import", n, ""); return true; }
            FrmGotoForm(ImportForm);
            return true;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Import preview ---- */

static RectangleType gImpArea;
static Int16 gImpScroll, gImpHeight;

static void Line(const char *s, Coord *y, Boolean bold)
{
    FntSetFont(bold ? boldFont : stdFont);
    WinDrawChars(s, StrLen(s), 4, *y);
    *y += 11;
}

static void DrawImport(void)
{
    char b[48], n[12];
    Coord y = (Coord)(17 - gImpScroll);
    UInt16 i;
    WinEraseRectangle(&gImpArea, 0);
    WinSetClip(&gImpArea);
    StrNCopy(b, gImport.file, 24); b[24] = 0; StrCat(b, ", "); StrIToA(n, (Int32)gImport.rows); StrCat(b, n); StrCat(b, gImport.rows == 1 ? " row read" : " rows read");
    Line(b, &y, false); y += 4;
    if (gImport.badHeader) {
        Line("The first line is not a fills header.", &y, true);
        Line("It needs Date, Odometer or Trip, and", &y, false);
        Line("Volume or Total. Export a blank CSV", &y, false);
        Line("from this screen for a template.", &y, false);
    } else {
        StrIToA(b, (Int32)gImport.ready); StrCat(b, gImport.ready == 1 ? " row ready to import" : " rows ready to import"); Line(b, &y, true);
        if (gImport.duplicates) { StrIToA(b, (Int32)gImport.duplicates); StrCat(b, " already in the log (skipped)"); Line(b, &y, false); }
        if (gImport.errors) { StrIToA(b, (Int32)gImport.errors); StrCat(b, gImport.errors == 1 ? " row has an error (below)" : " rows have errors (below)"); Line(b, &y, false); }
        if (gImport.newVehicles) {
            StrIToA(b, gImport.newVehicles); StrCat(b, gImport.newVehicles == 1 ? " new vehicle: " : " new vehicles: ");
            for (i = 0; i < gImport.newVehicles && i < impMaxNewVeh; i++) { if (i) StrCat(b, ", "); StrNCat(b, gImport.newVehicleNames[i], 47 - StrLen(b)); }
            Line(b, &y, false);
        }
        if (gImport.newProgrammes) { StrIToA(b, gImport.newProgrammes); StrCat(b, gImport.newProgrammes == 1 ? " new programme" : " new programmes"); Line(b, &y, false); }
        if (gImport.kbPoints) { StrCopy(b, "KB Points column to "); StrNCat(b, gImport.kbTarget, 47 - StrLen(b)); Line(b, &y, false); }
        if (gImport.errors) {
            y += 4; Line("Errors", &y, true);
            for (i = 0; i < gImport.errors && i < impMaxErrors; i++) Line(gImport.errorLines[i], &y, false);
            if (gImport.errors > impMaxErrors) Line("...and more; fix these and import again.", &y, false);
            else Line("These rows are skipped.", &y, false);
        }
        y += 4; Line("Nothing is written until you tap Import.", &y, false);
    }
    WinResetClip();
    gImpHeight = (Int16)(y + gImpScroll - gImpArea.topLeft.y);
    AppListScrollBar(FrmGetActiveForm(), ImpScrollBar, gImpScroll, (Int16)(gImpHeight - gImpArea.extent.y), gImpArea.extent.y,
                     gImpArea.topLeft.y, (Coord)(gImpArea.topLeft.y + gImpArea.extent.y));
}

Boolean ImportFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { ImpImportButton, ImpCancelButton };
    static char label[16];
    switch (event->eType) {
    case frmOpenEvent: {
        Coord w, h;
        AppFitForm(form, bottom, 2);
        WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
        gImpArea.topLeft.x = 0; gImpArea.topLeft.y = 15; gImpArea.extent.x = w; gImpArea.extent.y = (Coord)(h - 17 - 15);
        gImpScroll = 0;
        StrCopy(label, "Import "); StrIToA(label + 7, (Int32)gImport.ready);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ImpImportButton)), label);
        if (gImport.ready == 0) FrmHideObject(form, FrmGetObjectIndex(form, ImpImportButton));
        FrmDrawForm(form);
        DrawImport();
        return true;
    }
    case frmUpdateEvent: FrmDrawForm(form); DrawImport(); return true;
    case winDisplayChangedEvent: {
        Coord w, h;
        AppFitForm(form, bottom, 2);
        WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
        gImpArea.extent.y = (Coord)(h - 17 - 15); gImpScroll = 0;
        WinEraseWindow(); FrmDrawForm(form); DrawImport(); return true;
    }
    case sclRepeatEvent: case sclExitEvent: {
        Int16 v;
        if (AppListScrollEvent(event, &v)) { gImpScroll = v < 0 ? 0 : v; DrawImport(); return true; }
        break;
    }
    case keyDownEvent: {
        Boolean page; Int16 dir = AppScrollDirection(event, &page), step = page ? 60 : 22;
        Int16 room = gImpHeight - gImpArea.extent.y - gImpScroll;
        if (dir > 0 && room > 0) { gImpScroll += room < step ? room : step; DrawImport(); return true; }
        if (dir < 0 && gImpScroll > 0) { gImpScroll -= gImpScroll < step ? gImpScroll : step; DrawImport(); return true; }
        break;
    }
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == ImpCancelButton) { FrmGotoForm(ExportForm); return true; }
        if (event->data.ctlSelect.controlID == ImpImportButton) {
            UInt16 vol;
            char a[12], b2[12];
            if (CardFind(&vol) != errNone) { FrmAlert(NoCardAlert); return true; }
            if (ImportWrite(vol)) { FrmCustomAlert(ExportFailedAlert, "Import", "read", ""); return true; }
            StrIToA(a, (Int32)gImport.written); StrIToA(b2, (Int32)gImport.skipped);
            FrmCustomAlert(ImportDoneAlert, a, b2, "");
            FrmGotoForm(FillListForm);
            return true;
        }
        break;
    default: break;
    }
    return false;
}
