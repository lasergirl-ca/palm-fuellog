#include "report.h"
#include "calc.h"
#include "datecalc.h"

void ReportBegin(Report *r, UInt8 mode, UInt16 year)
{
    UInt8 *p = (UInt8 *)r;
    UInt32 i;
    for (i = 0; i < sizeof *r; i++) p[i] = 0;
    r->mode = mode;
    r->year = year;
}

void ReportSetRange(Report *r, UInt16 from, UInt16 to) { r->from = from; r->to = to; }

Int16 ReportRowFor(const Report *r, UInt16 date)
{
    UInt16 y; UInt8 m, d;
    if (r->mode == rptRange) return date >= r->from && date <= r->to ? 0 : -1;
    DateDaysToYmd(date, &y, &m, &d);
    if (r->mode == rptMonthly) return y == r->year ? (Int16)(m - 1) : -1;
    if (y < r->year || y >= r->year + rptMaxRows) return -1;
    return (Int16)(y - r->year);
}

static void RowAdd(ReportRow *row, const FillRecord *f, UInt32 home, UInt32 dkm) EXPORT_SEC;
static void RowAdd(ReportRow *row, const FillRecord *f, UInt32 home, UInt32 dkm)
{
    UInt8 k;
    row->fills++;
    if (f->flags & fillFlagCharge) row->wh += f->volume; else row->ml += f->volume;
    row->homeCents += home;
    row->dkm += dkm;
    for (k = 0; k < f->pointCount && k < fillMaxPoints; k++) row->points += f->points[k].earned - f->points[k].redeemed;
}

void ReportAdd(Report *r, const FillRecord *f)
{
    UInt32 home = CalcHomeCents(f->total, f->fxRate ? f->fxRate : 1000000UL), dkm = 0;
    Int16 row = ReportRowFor(r, f->date);
    UInt8 charge = (f->flags & fillFlagCharge) ? 1 : 0, v = f->vehicle < rptMaxId ? f->vehicle : 0;

    /* Distance since this vehicle's previous fill goes to the later fill's period. */
    if (f->odometer) {
        if (r->veh[v].seen && f->odometer > r->veh[v].lastOdo) dkm = f->odometer - r->veh[v].lastOdo;
        r->veh[v].seen = 1; r->veh[v].lastOdo = f->odometer;
    }
    if (row >= 0) {
        RowAdd(&r->rows[row], f, home, dkm);
        RowAdd(&r->total, f, home, dkm);
        RowAdd(&r->byVehicle[v], f, home, dkm);
        if (f->payment == fillPaymentCard) r->cardCents += home; else r->cashCents += home;
        if (f->currency < 4) r->curCents[f->currency] += f->total;
    }
    /* Full-to-full chain runs over every fill; a segment counts in the period it closes in. */
    {
        UInt8 c = charge;
        if (f->flags & fillFlagMissedPrev) { r->chain[v][c].haveFull = 0; r->chain[v][c].pending = 0; }
        r->chain[v][c].pending += f->volume;
        if (f->flags & fillFlagFullTank) {
            if (r->chain[v][c].haveFull && f->odometer > r->chain[v][c].lastFullOdo && !charge && row >= 0) {
                r->rows[row].ftfMl += r->chain[v][c].pending; r->rows[row].ftfDkm += f->odometer - r->chain[v][c].lastFullOdo;
                r->total.ftfMl += r->chain[v][c].pending; r->total.ftfDkm += f->odometer - r->chain[v][c].lastFullOdo;
                r->byVehicle[v].ftfMl += r->chain[v][c].pending; r->byVehicle[v].ftfDkm += f->odometer - r->chain[v][c].lastFullOdo;
            }
            r->chain[v][c].lastFullOdo = f->odometer; r->chain[v][c].haveFull = 1; r->chain[v][c].pending = 0;
        }
    }
}

UInt32 ReportRowEco(const ReportRow *row) { return row->ftfDkm ? CalcL100kmX10(row->ftfMl, row->ftfDkm) : 0; }
UInt32 ReportRowPrice(const ReportRow *row) { return row->ml ? CalcMulDiv(row->homeCents, 100000UL, row->ml) : 0; }
