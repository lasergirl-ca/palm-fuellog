/* The six preset rewards programmes from the spec, as plain data so the
 * host can test them. Installed on first run by PgmInstallPresets. */
#ifndef PRESETS_H
#define PRESETS_H
#include "ptypes.h"
#include "records.h"
#include "sections.h"

#define pgmPresetCount 6

/* Fill r with preset i (0..5); its id is i + 1. */
void PgmPreset(UInt8 i, ProgrammeRecord *r) EXPORT_SEC;

#endif
