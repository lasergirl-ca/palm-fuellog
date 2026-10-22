#include "zipread.h"

static UInt16 Le16(const UInt8 *p) EXPORT_SEC;
static UInt32 Le32(const UInt8 *p) EXPORT_SEC;
static Int32 NameEq(const UInt8 *a, UInt16 alen, const char *b) EXPORT_SEC;

static UInt16 Le16(const UInt8 *p) { return (UInt16)(p[0] | (p[1] << 8)); }
static UInt32 Le32(const UInt8 *p) { return (UInt32)p[0] | ((UInt32)p[1] << 8) | ((UInt32)p[2] << 16) | ((UInt32)p[3] << 24); }

static Int32 NameEq(const UInt8 *a, UInt16 alen, const char *b)
{
    UInt16 i;
    for (i = 0; i < alen; i++) if (b[i] == 0 || (UInt8)b[i] != a[i]) return 0;
    return b[alen] == 0;
}

Int32 ZipFind(ZipReadAtFn readAt, void *ctx, UInt32 fileSize, const char *name,
              UInt8 *scratch, UInt32 scratchLen, ZipEntry *out)
{
    UInt32 tailLen, tailPos, cdOffset, cdSize, pos, end;
    UInt16 entries, i;
    Int32 got, k;

    /* End of central directory: within the last 22 + 65535 bytes; we look in
     * the last scratchLen bytes, which covers every zip FuelLog will meet. */
    tailLen = fileSize < scratchLen ? fileSize : scratchLen;
    tailPos = fileSize - tailLen;
    got = readAt(ctx, tailPos, scratch, tailLen);
    if (got != (Int32)tailLen) return zipErrRead;
    for (k = (Int32)tailLen - 22; k >= 0; k--)
        if (scratch[k] == 'P' && scratch[k + 1] == 'K' && scratch[k + 2] == 5 && scratch[k + 3] == 6)
            break;
    if (k < 0) return zipErrNotZip;
    entries = Le16(scratch + k + 10);
    cdSize = Le32(scratch + k + 12);
    cdOffset = Le32(scratch + k + 16);

    pos = cdOffset;
    end = cdOffset + cdSize;
    for (i = 0; i < entries && pos + 46 <= end; i++) {
        UInt16 nameLen, extraLen, commentLen;
        got = readAt(ctx, pos, scratch, 46);
        if (got != 46) return zipErrRead;
        if (Le32(scratch) != 0x02014b50UL) return zipErrNotZip;
        nameLen = Le16(scratch + 28);
        extraLen = Le16(scratch + 30);
        commentLen = Le16(scratch + 32);
        if (nameLen > scratchLen - 46) return zipErrNotZip;
        got = readAt(ctx, pos + 46, scratch + 46, nameLen);
        if (got != nameLen) return zipErrRead;
        if (NameEq(scratch + 46, nameLen, name)) {
            UInt32 local = Le32(scratch + 42);
            out->method = Le16(scratch + 10);
            out->crc = Le32(scratch + 16);
            out->csize = Le32(scratch + 20);
            out->usize = Le32(scratch + 24);
            got = readAt(ctx, local, scratch, 30);
            if (got != 30 || Le32(scratch) != 0x04034b50UL) return zipErrNotZip;
            out->dataOffset = local + 30 + Le16(scratch + 26) + Le16(scratch + 28);
            return 0;
        }
        pos += 46 + nameLen + extraLen + commentLen;
    }
    return zipErrNotFound;
}
