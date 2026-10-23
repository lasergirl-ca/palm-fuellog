/* SP4: card (VFS) write speed, folder creation, database export and import. */
#ifndef VFSTEST_H
#define VFSTEST_H
#include <PalmOS.h>
#include "sections.h"

#define vfsTestMaxLines 14
#define vfsTestLineLen  40

/* Runs the whole test, filling lines[] with one result per line.
 * Returns the number of lines. Takes a few seconds on a real card. */
UInt16 VfsTestRun(char lines[vfsTestMaxLines][vfsTestLineLen]) SEC2;

#endif
