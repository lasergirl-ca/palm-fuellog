/* CSV export and import of fills: RFC 4180 quoting, UTF-8 with a BOM on the
 * way out (Palm Latin transcoded byte by byte), either encoding on the way
 * in. The header row names the units, so an export doubles as an import
 * template. Plain C, host-tested. */
#ifndef CSV_H
#define CSV_H
#include "ptypes.h"
#include "records.h"
#include "outstream.h"
#include "sections.h"

#define csvMaxPgms 16
#define csvMaxFields 56
#define csvMaxRecord 1024

typedef struct {
    UInt8 distUnit, volUnit, priceUnit, priceCents, homeCurrency;
    UInt8 pgmCount;
    UInt8 pgmIds[csvMaxPgms];
    char  pgmNames[csvMaxPgms][pgmNameLen];
} CsvCtx;

/* Writing. */
void CsvBom(OutStream *out) IO_SEC;
void CsvHeader(OutStream *out, const CsvCtx *c) IO_SEC;
void CsvFillRow(OutStream *out, const CsvCtx *c, const FillRecord *f, const char *vehicleName) IO_SEC;
/* One quoted, transcoded text field (no leading comma). */
void CsvText(OutStream *out, const char *palmText) IO_SEC;

UInt8 CsvUnicodeToPalm(UInt16 unicode) IO_SEC;   /* Windows-1252 and Latin-1 fold; else '?' */

/* Reading. A byte source: returns 0..255, or -1 at the end. */
typedef Int16 (*CsvGetcFn)(void *ctx);
/* Reads one record (quoted newlines kept) into buf, UTF-8 converted to Palm
 * Latin, the BOM skipped. Returns the length, 0 for an empty line, or -1 at
 * the end of the file. *lineNo counts physical lines. */
Int32 CsvNextRecord(CsvGetcFn getc, void *ctx, char *buf, UInt16 cap, UInt32 *lineNo) IO_SEC;
/* Splits a record in place into fields; returns the count. */
Int16 CsvSplit(char *record, char *fields[], Int16 maxFields) IO_SEC;

/* Header mapping: which column holds what, and the units it declares. -1 = absent. */
typedef struct {
    Int16 date, time, vehicle, odometer, trip, volume, price, total, currency, rate, payment, grade, full, missed, discount, notes;
    Int16 pgmEarned[csvMaxPgms], pgmRedeemed[csvMaxPgms];
    char  pgmNames[csvMaxPgms][pgmNameLen];   /* as the header names them */
    UInt8 pgmCount;
    UInt8 distUnit, volUnit, priceUnit, priceCents;
    UInt8 priceCurrency;      /* 255 when the header names none */
    Int16 kbPoints;           /* the paper form's KB Points column, or -1 */
    Int16 kbCumulative;       /* its cumulative half, or -1 */
} CsvMap;
/* Returns the number of recognised columns; 0 means this is not a fills header. */
Int16 CsvMapHeader(char *fields[], Int16 n, CsvMap *m) IO_SEC;
/* Builds a fill from a data row. Vehicle name is copied to vehicleName (24).
 * Programme points land in f->points with programme = the header index + 1
 * (the caller maps names to ids). Returns 0, or -1 with a short reason in err (40). */
Int32 CsvRowToFill(const CsvMap *m, char *fields[], Int16 n, FillRecord *f, char *vehicleName, char *err) IO_SEC;

#endif
