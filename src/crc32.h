/* Table-driven CRC-32 (IEEE, as zip and PNG use it). The 1 KB table is
 * caller-owned so it can live on the heap only while needed. */
#ifndef CRC32_H
#define CRC32_H
#include "ptypes.h"
#include "sections.h"

void   Crc32Init(UInt32 *table) EXPORT_SEC;                   /* 256 entries */
UInt32 Crc32Update(const UInt32 *table, UInt32 crc, const UInt8 *p, UInt32 len) EXPORT_SEC;

#endif
