/* CSV, XLSX and PDF export of the log to a file (exportfile.c), the Palm
 * build's exporters ported over the Pocket PC store. Plain C, host-tested. */
#ifndef EXPORTFILE_H
#define EXPORTFILE_H
#include "../../src/ptypes.h"
#include "../../src/prefs.h"
#include "../../src/records.h"
#include "../../src/outstream.h"

#define exportCsv  0
#define exportXlsx 1
#define exportPdf  2
#define exportOk        0
#define exportErrFolder 1     /* the folder could not be made, or the file opened */
#define exportErrWrite  2     /* the storage filled up or refused a write */

/* The units, currency, separators and clock style the files are written in. */
void  ExportSetup(const FuelPrefs *p, char point, char thousands, int time24);
/* "FuelLog-20261112" plus ext, in buf (40). */
void  ExportName(char *buf, const char *ext, UInt16 today);
/* Writes FuelLog-YYYYMMDD.csv, .xlsx or .pdf into dir (made if missing): vehicle 255 = all,
 * year 0 = all, pgm 255 = all programmes, else only fills carrying it and only its columns.
 * nameOut (40) gets the name, *bytes the size. */
Int32 ExportFile(const char *dir, UInt8 fmt, UInt8 vehicle, UInt16 year, UInt8 pgm, UInt16 today, char *nameOut, UInt32 *bytes);

/* The Palm's own entry points, ported: any stream (0 CSV, 1 XLSX, 2 PDF). */
extern UInt8 gExportPgm;
void ExportToStream(OutStream *out, UInt8 fmt, UInt8 vehicle, UInt16 year);
void ExportCsvTo(OutStream *out, UInt8 vehicle, UInt16 year);
void ExportXlsxTo(OutStream *out, UInt8 vehicle, UInt16 year);
void ExportPdfTo(OutStream *out, UInt8 vehicle, UInt16 year);
int  ExportWants(const FillRecord *f, UInt8 vehicle, UInt16 year);

#endif
