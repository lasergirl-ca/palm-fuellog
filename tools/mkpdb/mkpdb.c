/* Writes a FuelLog backup set (five .pdb files) with sample records into a
 * directory, using the app's own pack functions, so the Go reader in
 * tools/fuellogdump can be checked against them on the Mac. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/records.h"
#include "../../src/datecalc.h"

static void put16(FILE *f, unsigned v) { fputc((v >> 8) & 255, f); fputc(v & 255, f); }
static void put32(FILE *f, unsigned long v) { put16(f, (unsigned)(v >> 16)); put16(f, (unsigned)(v & 0xFFFF)); }

/* A PDB with one AppInfo block (schema 1) and the given records. */
static void WritePdb(const char *dir, const char *name, const char *type, UInt8 (*recs)[512], UInt32 *lens, int n)
{
    char path[512]; FILE *f; int i; unsigned long off;
    snprintf(path, sizeof path, "%s/%s.pdb", dir, name);
    f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
    { char nm[32]; memset(nm, 0, 32); strncpy(nm, name, 31); fwrite(nm, 1, 32, f); }
    put16(f, 0x0008); put16(f, 1); put32(f, 0); put32(f, 0); put32(f, 0); put32(f, 0);
    off = 78 + 8UL * n + 2;                 /* appInfo right after the record list and its pad */
    put32(f, off); put32(f, 0);
    fwrite(type, 1, 4, f); fwrite("ZFlg", 1, 4, f); put32(f, 0); put32(f, 0); put16(f, (unsigned)n);
    off += appInfoSize;
    for (i = 0; i < n; i++) { put32(f, off); fputc(0, f); fputc(0, f); fputc(0, f); fputc(i + 1, f); off += lens[i]; }
    put16(f, 0);
    { AppInfoBlock a; UInt8 buf[8]; a.schemaVersion = 1; a.reserved = 0; fwrite(buf, 1, AppInfoPack(&a, buf), f); }
    for (i = 0; i < n; i++) fwrite(recs[i], 1, lens[i], f);
    fclose(f);
}

int main(int argc, char **argv)
{
    static UInt8 recs[8][512]; UInt32 lens[8];
    const char *dir = argc > 1 ? argv[1] : "build/pdb";
    FillRecord f; VehicleRecord v; ProgrammeRecord p; ServiceTypeRecord t; ServiceVisitRecord s;

    memset(&v, 0, sizeof v); v.id = 1; strcpy(v.name, "Outback"); v.powertrain = 0; v.odoUnit = 0; v.startOdo = 806500; v.tankSize = 60000; v.defaultCurrency = 255;
    lens[0] = VehiclePack(&v, recs[0]);
    memset(&v, 0, sizeof v); v.id = 2; strcpy(v.name, "Ioniq 5"); v.powertrain = 3; v.batteryWh = 77000; v.homeRate = 1800; v.defaultCurrency = 255;
    lens[1] = VehiclePack(&v, recs[1]);
    WritePdb(dir, "FuelVeh-ZFlg", "Vehi", recs, lens, 2);

    memset(&p, 0, sizeof p); p.id = 2; strcpy(p.name, "Petro-Points"); p.currency = 1; p.earnBasis = 1; p.earnRate = 1000; p.premiumMult = 100; p.autoApply = 1; p.valuePerPoint = 100; p.redeemKind = 1; p.redeemBlock = 1000; p.redeemValue = 1000; strcpy(p.aliases, "PP,Petro");
    lens[0] = ProgrammePack(&p, recs[0]);
    WritePdb(dir, "FuelPgm-ZFlg", "Pgms", recs, lens, 1);

    memset(&f, 0, sizeof f); f.date = DateYmdToDays(2026, 7, 29); f.time = 9 * 60 + 5; f.flags = fillFlagFullTank; f.vehicle = 1; f.odometer = 806500; f.volume = 45200; f.price = 17560; f.total = 7937; f.currency = 1; f.fxRate = 1000000UL; f.payment = 1;
    f.pointCount = 1; f.points[0].programme = 2; f.points[0].earned = 452; strcpy(f.notes, "Petro-Canada on 152 St., caf\xE9");
    lens[0] = FillPack(&f, recs[0]);
    memset(&f, 0, sizeof f); f.date = DateYmdToDays(2026, 8, 10); f.time = fillTimeUnknown; f.flags = fillFlagFullTank | fillFlagTripEntered; f.vehicle = 1; f.odometer = 811900; f.trip = 5400; f.volume = 46000; f.price = 17690; f.total = 8137; f.currency = 1; f.fxRate = 1000000UL;
    lens[1] = FillPack(&f, recs[1]);
    memset(&f, 0, sizeof f); f.date = DateYmdToDays(2026, 8, 12); f.time = fillTimeUnknown; f.flags = fillFlagFullTank | fillFlagCharge; f.vehicle = 2; f.grade = fillGradeHomeAC; f.odometer = 184200; f.volume = 38600; f.price = 1800; f.total = 69; f.currency = 1; f.fxRate = 1000000UL;
    lens[2] = FillPack(&f, recs[2]);
    WritePdb(dir, "FuelLog-ZFlg", "Data", recs, lens, 3);

    memset(&t, 0, sizeof t); t.id = 0; t.vehicle = 1; strcpy(t.name, "Oil and filter"); t.intervalDist = 80000; t.intervalMonths = 6; t.warnDist = 8000; t.warnDays = 14; t.alarm = 1;
    lens[0] = ServiceTypePack(&t, recs[0]);
    WritePdb(dir, "FuelSvT-ZFlg", "SvcT", recs, lens, 1);

    memset(&s, 0, sizeof s); s.date = DateYmdToDays(2026, 5, 14); s.vehicle = 1; s.odometer = 768520; s.services = 1; s.cost = 11995; s.currency = 1; s.fxRate = 1000000UL; s.payment = 1; strcpy(s.shop, "Corner Garage"); strcpy(s.detail, "0W-20 synthetic, 4.8 L");
    lens[0] = ServiceVisitPack(&s, recs[0]);
    WritePdb(dir, "FuelSvL-ZFlg", "SvcL", recs, lens, 1);
    printf("wrote %s\n", dir);
    return 0;
}
