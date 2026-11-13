/* The Pocket PC's Prefs as bytes, for the registry value they are kept in
 * (HKEY_CURRENT_USER\Software\FuelLog, "Prefs"). Big-endian and versioned,
 * not the struct as the compiler lays it out; a later version only appends,
 * so an older build reads the part it knows. Plain C, host-tested. */
#ifndef SETTINGS_H
#define SETTINGS_H
#include "../../src/ptypes.h"
#include "../../src/prefs.h"

#define settingsVersion 1
#define settingsSize 48

UInt32 SettingsPack(const FuelPrefs *p, UInt8 *buf);      /* buf: settingsSize bytes; returns the length */
/* Reads over the defaults already in *p. A field out of range keeps its
 * default. Returns 0, or -1 (and leaves *p alone) when the bytes are too short. */
Int32  SettingsUnpack(const UInt8 *buf, UInt32 len, FuelPrefs *p);

#endif
