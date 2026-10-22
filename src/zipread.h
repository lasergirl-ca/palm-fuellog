/* Find an entry in a zip file by name, using positioned reads so the file
 * can live on the card (VFSFileSeek + VFSFileRead) or on the host (stdio). */
#ifndef ZIPREAD_H
#define ZIPREAD_H
#include "ptypes.h"
#include "sections.h"

/* Read up to len bytes at pos; return the count (0 at end) or negative. */
typedef Int32 (*ZipReadAtFn)(void *ctx, UInt32 pos, UInt8 *buf, UInt32 len);

typedef struct {
    UInt32 dataOffset;   /* first byte of the entry's data */
    UInt32 csize, usize;
    UInt32 crc;
    UInt16 method;       /* 0 stored, 8 deflate */
} ZipEntry;

#define zipErrNotZip    (-10)
#define zipErrNotFound  (-11)
#define zipErrRead      (-12)

/* Locate `name` through the central directory. `scratch` must hold at least
 * 1,024 bytes. Returns 0 or a zipErr code. */
Int32 ZipFind(ZipReadAtFn readAt, void *ctx, UInt32 fileSize, const char *name,
              UInt8 *scratch, UInt32 scratchLen, ZipEntry *out) EXPORT_SEC;

#endif
