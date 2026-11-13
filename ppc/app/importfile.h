/* Import of fills from a CSV, XLSX or XLS file (importfile.c), the Palm
 * build's src/import.c ported over the Pocket PC store. Plain C, host-tested. */
#ifndef IMPORTFILE_H
#define IMPORTFILE_H
#include "../../src/ptypes.h"
#include "../../src/prefs.h"
#include "../store/dirs.h"

#define cardNameLen dirNameLen
#define impMaxErrors 6
#define impMaxNewVeh 4

typedef struct {
    char   file[cardNameLen];
    UInt32 rows, ready, errors, duplicates, badHeader;
    UInt16 newVehicles;
    char   newVehicleNames[impMaxNewVeh][24];
    UInt16 newProgrammes;
    int    kbPoints;             /* a KB Points column maps to the 76 preset (or a new programme) */
    char   kbTarget[24];
    char   errorLines[impMaxErrors][44];
    UInt32 written, skipped;
} ImportSummary;

extern ImportSummary gImport;

/* The units and currency the new vehicles and programmes take. */
void  ImportSetup(const FuelPrefs *p);
/* Pass 1 over the file at path: fills gImport. XLSX and XLS parts are inflated to temporary
 * files in tmpDir and removed after. 0, or an error code. */
Int32 ImportScan(const char *path, const char *tmpDir);
/* Pass 2 on the same file: adds the fills not already in the log. */
Int32 ImportWrite(void);
/* The programme a KB Points column counts for (0 = by the KB Points or 76 alias). */
void  ImportSetKbTarget(UInt8 id);
UInt8 ImportKbDefault(void);

#endif
