/* Host tests for the Pocket PC build's programme rules (app/pgmedit.c),
 * against a working copy of the fixture set tools/mkpdb writes (argv[1])
 * and an empty folder (argv[2]). The fixture has one programme,
 * Petro-Points (id 2, CAD, 10 points a litre, auto-apply, 1,000 points take
 * $1 off), and one fill that earned 452 of them on Jul 29, 2026. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../app/pgmedit.h"
#include "../../src/currency.h"
#include "../../src/datecalc.h"
#include "../../src/presets.h"

static int failures;

static void check(const char *name, long got, long want)
{
    if (got == want) printf("ok   %s = %ld\n", name, got);
    else { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
}
static void checkStr(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) printf("ok   %s = \"%s\"\n", name, got);
    else { printf("FAIL %s: got \"%s\" want \"%s\"\n", name, got, want); failures++; }
}

static UInt32 Now(void) { return 3877000000UL; }

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work4", *empty = argc > 2 ? argv[2] : "build/host/empty4";
    DbClock clock;
    PgmEdit e;
    PgmEditForm f;
    ProgrammeRecord p;
    UInt16 idx[pgmMaxProgrammes], when, stones, today = DateYmdToDays(2026, 8, 20);
    char name[48], bal[48], note[48], status[64], buf[48];

    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    DbEnsureUids();

    /* Lookups and the list. */
    check("next free id", PgmNextId(), 1);
    check("Petro-Points on one fill", PgmUseCount(2), 1);
    check("  balance", (long)PgmBalance(2, 0), 452);
    check("presets: not over a log with programmes", PgmInstallPresets(curCAD), 0);
    check("one row", PgmListCollect(idx, pgmMaxProgrammes), 1);
    check("row reads", PgmListRow(idx[0], today, '.', ',', name, bal, note), 1);
    checkStr("  name", name, "Petro-Points");
    checkStr("  balance", bal, "452 pts");
    checkStr("  rule and worth", note, "10 pts/L; auto; about $0.45");

    /* Edit it. */
    check("begin on Petro-Points", PgmEditBegin(&e, 0, curCAD, '.', ',', &f), 1);
    checkStr("  name", f.name, "Petro-Points");
    check("  CAD", f.currency, curCAD);
    check("  counts in points", f.money, 0);
    checkStr("  earn", f.earn, "10");
    check("  per litre", f.basis, pgmEarnPerLitre);
    checkStr("  premium", f.premium, "100");
    check("  any payment", f.payment, pgmPaymentAny);
    check("  auto", f.autoApply, 1);
    check("  money off", f.redeem, pgmRedeemMoneyOff);
    checkStr("  1,000 points", f.block, "1,000");
    checkStr("  = $1.00", f.value, "1.00");
    checkStr("  opening shown for a stored programme", f.opening, "0");
    checkStr("  aliases", f.aliases, "PP,Petro");
    checkStr("  earn label", PgmEditEarnLabel(f.money), "Earn pts:");
    checkStr("  value label", PgmEditValueLabel(f.redeem), "= $:");
    PgmEditStatus(&e, '.', ',', status);
    checkStr("  status", status, "Balance now 452 pts, about $0.45");
    checkStr("  it has fills: archive", PgmEditArchiveLabel(&e), "Archive");
    strcpy(f.earn, "x");
    check("an unreadable rate", PgmEditSave(&e, &f, '.', ','), peBadNumber);
    DbGetProgramme(0, &p);
    check("  nothing stored", (long)p.earnRate, 1000);
    check("  nor kept in memory", (long)e.pgm.earnRate, 1000);
    strcpy(f.earn, "2.5"); strcpy(f.opening, "1,000"); f.expiry = pgmExpiryMonthAfter;
    check("save", PgmEditSave(&e, &f, '.', ','), peSaved);
    DbGetProgramme(0, &p);
    check("  rate 2.5", (long)p.earnRate, 250);
    check("  opening", (long)p.openingBalance, 1000);
    check("  worth a tenth of a cent", (long)p.valuePerPoint, 100);
    check("  uid kept", (long)(p.uid != 0 && p.uid == e.pgm.uid), 1);
    check("  balance", (long)PgmBalance(2, p.openingBalance), 1452);
    PgmRateText(buf, 250, '.');
    checkStr("rate text", buf, "2.5");
    PgmRateText(buf, 125, ',');
    checkStr("  two decimals, comma", buf, "1,25");

    /* Expiry: the month after the month earned; the opening balance never expires. */
    check("452 expire by the end of next month", (long)PgmExpiring(&p, today, &when), 452);
    check("  on Aug 31", when, DateYmdToDays(2026, 8, 31));
    PgmListRow(0, today, '.', ',', name, bal, note);
    checkStr("  the list warns", note, "452 expire Aug 31");
    check("  gone by September", (long)PgmExpiring(&p, DateYmdToDays(2026, 9, 2), &when), 0);

    /* Archive and restore. */
    check("archive", PgmEditArchive(&e), dbOk);
    DbGetProgramme(0, &p);
    check("  archived", p.archived, 1);
    checkStr("  label", PgmEditArchiveLabel(&e), "Restore");
    PgmListRow(0, today, '.', ',', name, bal, note);
    checkStr("  marked in the list", name, "Petro-Points (archived)");
    check("restore", PgmEditArchive(&e), dbOk);
    DbGetProgramme(0, &p);
    check("  in use again", p.archived, 0);

    /* A new one, counted in money. */
    check("begin a new programme", PgmEditBegin(&e, pgmEditNew, curUSD, '.', ',', &f), 1);
    check("  id 1", e.pgm.id, 1);
    check("  home currency", f.currency, curUSD);
    check("  auto by default", f.autoApply, 1);
    checkStr("  no opening shown", f.opening, "");
    check("  no archive action", PgmEditArchiveLabel(&e) == 0, 1);
    PgmEditStatus(&e, '.', ',', status);
    checkStr("  no status", status, "");
    strcpy(f.name, "  ");
    check("a blank name", PgmEditSave(&e, &f, '.', ','), peNeedName);
    strcpy(f.name, "Shell Go+"); f.money = 1; strcpy(f.earn, "3"); f.basis = pgmEarnPerCurrency; strcpy(f.opening, "12.50");
    f.redeem = pgmRedeemPerVolume; strcpy(f.block, "100"); strcpy(f.value, "5"); f.volUnit = 1; strcpy(f.maxVol, "20"); strcpy(f.cap, "50");
    check("  saves", PgmEditSave(&e, &f, '.', ','), peSaved);
    check("  two programmes", DbCount(dbProgrammes), 2);
    DbGetProgramme(1, &p);
    check("  counted in money", (long)p.valuePerPoint, 1000);
    check("  opening in cents", (long)p.openingBalance, 1250);
    check("  5 c off a gallon", (long)p.redeemValue, 50);
    check("  US gallons", p.redeemVolUnit, pgmVolUSGallon);
    check("  up to 20 gallons in mL", (long)p.redeemMaxVol, 75708);
    check("  capped at 50.0 c", (long)p.redeemMaxPerUnit, 500);
    PgmEditBegin(&e, 1, curUSD, '.', ',', &f);
    checkStr("  read back: 20 gallons", f.maxVol, "20");
    checkStr("  5.0 c", f.value, "5.0");
    checkStr("  value label", PgmEditValueLabel(f.redeem), "= c off:");
    checkStr("  opening label", PgmEditOpeningLabel(f.money), "Opening $:");
    checkStr("  opening", f.opening, "12.50");
    PgmListCollect(idx, pgmMaxProgrammes);
    check("the used one first", idx[0], 0);
    PgmListRow(1, today, '.', ',', name, bal, note);
    checkStr("  balance in dollars", bal, "$12.50");
    checkStr("  rule", note, "3c/$; auto");
    stones = DbCount(dbDeleted);
    checkStr("  no fills: delete", PgmEditArchiveLabel(&e), "Delete");
    check("  delete", PgmEditArchive(&e), dbOk);
    check("  one programme", DbCount(dbProgrammes), 1);
    check("  a tombstone", DbCount(dbDeleted), stones + 1);

    /* Sixteen is the limit. */
    while (PgmNextId()) {
        PgmEditBegin(&e, pgmEditNew, curCAD, '.', ',', &f);
        strcpy(f.name, "Card");
        if (PgmEditSave(&e, &f, '.', ',') != peSaved) break;
    }
    check("sixteen programmes", DbCount(dbProgrammes), 16);
    check("  a seventeenth is refused", PgmEditBegin(&e, pgmEditNew, curCAD, '.', ',', &f), 0);
    check("save the set", DbSave(), dbOk);
    DbCloseAll();

    /* An empty log gets the Palm's six presets. */
    check("open an empty folder", DbOpenAll(empty, &clock), dbOk);
    check("presets go in", PgmInstallPresets(curUSD), pgmPresetCount);
    check("  once", PgmInstallPresets(curUSD), 0);
    DbGetProgramme(pgmPresetCount - 1, &p);
    check("  card points in the home currency", p.currency, curUSD);
    DbGetProgramme(1, &p);
    checkStr("  Petro-Points among them", p.name, "Petro-Points");
    check("  written", DbSave(), dbOk);
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
