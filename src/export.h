/* Export of the log to the card. Device only, in the io section. */
#ifndef EXPORT_H
#define EXPORT_H
#include <PalmOS.h>
#include "sections.h"
#include "records.h"
/* vehicle 255 = all; year 0 = all. Writes Export/FuelLog-YYYYMMDD.csv;
 * nameOut (40) gets the file name and *bytes its size. */
Err ExportCsv(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes) IO_SEC;
Err ExportXlsx(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes) XP_SEC;
Err ExportPdf(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes) XP_SEC;
/* True when the fill passes the vehicle and year filter (shared by the exporters). */
Boolean ExportWants(const FillRecord *f, UInt8 vehicle, UInt16 year) IO_SEC;
/* Programme filter for exports: 255 = all; else only fills carrying that programme, and only its columns. */
extern UInt8 gExportPgm;
#include "outstream.h"
/* Write an export (0 CSV, 1 XLSX, 2 PDF) to any stream, for beaming. */
void ExportToStream(OutStream *out, UInt8 fmt, UInt8 vehicle, UInt16 year) XP_SEC;
void ExportCsvTo(OutStream *out, UInt8 vehicle, UInt16 year) IO_SEC;
void ExportXlsxTo(OutStream *out, UInt8 vehicle, UInt16 year) XP_SEC;
void ExportPdfTo(OutStream *out, UInt8 vehicle, UInt16 year) XP_SEC;
#endif
