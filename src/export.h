/* Export of the log to the card. Device only, in the io section. */
#ifndef EXPORT_H
#define EXPORT_H
#include <PalmOS.h>
#include "sections.h"
/* vehicle 255 = all; year 0 = all. Writes Export/FuelLog-YYYYMMDD.csv;
 * nameOut (40) gets the file name and *bytes its size. */
Err ExportCsv(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes) IO_SEC;
#endif
