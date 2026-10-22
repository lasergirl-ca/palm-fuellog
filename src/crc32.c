#include "crc32.h"

void Crc32Init(UInt32 *table)
{
    UInt32 i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
}

UInt32 Crc32Update(const UInt32 *table, UInt32 crc, const UInt8 *p, UInt32 len)
{
    while (len--)
        crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc;
}
