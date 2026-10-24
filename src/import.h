/* CSV import in two passes: scan and show, then write with the merge rule
 * (a fill with the same vehicle, date and odometer is already in the log).
 * Device only, io section. */
#ifndef IMPORT_H
#define IMPORT_H
#include <PalmOS.h>
#include "sections.h"
#include "card.h"

#define impMaxErrors 6
#define impMaxNewVeh 4

typedef struct {
    char   file[cardNameLen];
    UInt32 rows, ready, errors, duplicates, badHeader;
    UInt16 newVehicles;
    char   newVehicleNames[impMaxNewVeh][24];
    UInt16 newProgrammes;
    Boolean kbPoints;            /* a KB Points column maps to the 76 preset (or a new programme) */
    char   kbTarget[24];
    char   errorLines[impMaxErrors][44];
    UInt32 written, skipped;
} ImportSummary;

extern ImportSummary gImport;

Err ImportScan(UInt16 vol, const char *file) IO_SEC;    /* pass 1: fills gImport */
Err ImportWrite(UInt16 vol) IO_SEC;                     /* pass 2 on gImport.file */
Boolean ChooseFileFormHandleEvent(EventType *event) IO_SEC;
Boolean ImportFormHandleEvent(EventType *event) IO_SEC;

#endif
