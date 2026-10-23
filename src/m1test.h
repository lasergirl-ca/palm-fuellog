/* M1 device check: the five databases open with their schema version,
 * a fill round-trips through the packed layout, and Prefs persist. */
#ifndef M1TEST_H
#define M1TEST_H
#include "vfstest.h"
UInt16 M1TestRun(char lines[vfsTestMaxLines][vfsTestLineLen]) SEC2;
#endif
