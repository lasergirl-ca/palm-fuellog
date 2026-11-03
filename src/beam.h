/* IR and Bluetooth transfer through the Exchange Manager. Sending and the
 * merge live in the exp2 section; receiving (which may run without
 * globals) lives in main.c and only stores the bytes. */
#ifndef BEAM_H
#define BEAM_H
#include <PalmOS.h>
#include "sections.h"

#define beamInboxName "FuelInbox-ZFlg"
#define beamKindRecords 1
#define beamKindFile    2
#define beamKindSync    3      /* every record with its trailer, then the deletion log */

/* Send the whole log (or one vehicle's) as a .fuel bundle: beamKindRecords
 * for Merge/Replace on the other side, beamKindSync to make both logs the same. */
Err  BeamSendLog(UInt8 vehicle, UInt16 kind) XP_SEC;
/* The sync bundle as a file in Sync/ on the card (nameOut, 40), and reading one back. */
Err  BeamWriteSyncFile(UInt16 vol, char *nameOut) XP_SEC;
Err  BeamImportSyncFile(UInt16 vol, const char *name) XP_SEC;
/* Send an export (0 CSV, 1 XLSX, 2 PDF) inside a .fuel file envelope. */
Err  BeamSendExport(UInt8 fmt, UInt8 vehicle, UInt16 year) XP_SEC;
/* At launch, or right after a receive while running: offer Merge / Replace
 * for any bundles in the inbox database. Returns true when the log changed. */
Boolean BeamProcessInbox(void) XP_SEC;
void BeamRegister(void) XP_SEC;

#endif
