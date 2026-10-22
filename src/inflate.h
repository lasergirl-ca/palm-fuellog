/* Inflate (RFC 1951) in plain C for the 68k: no malloc, no recursion, a
 * caller-supplied 32 KB window, input buffer and CRC table, streaming in
 * and out through callbacks. Codes of up to 9 bits decode through a lookup
 * table; longer ones fall back to canonical decoding bit by bit, in the
 * manner of zlib's puff. */
#ifndef INFLATE_H
#define INFLATE_H
#include "ptypes.h"
#include "sections.h"

#define inflateWindowSize 32768UL
#define inflateFastBits   9

/* Fill buf with up to len compressed bytes; return the count, 0 at the end
 * of the entry, or a negative error. */
typedef Int32 (*InflateReadFn)(void *ctx, UInt8 *buf, UInt32 len);
/* Take len inflated bytes; return 0, or a negative error. */
typedef Int32 (*InflateWriteFn)(void *ctx, const UInt8 *buf, UInt32 len);

typedef struct {
    Int16  count[16];
    Int16  symbol[288];
    UInt16 fast[1 << inflateFastBits];   /* (len << 12) | symbol, or 0 */
} InflateHuff;

typedef struct {
    InflateReadFn read;
    InflateWriteFn write;
    void *ctx;
    UInt8 *in;            /* input buffer, inSize bytes, caller-owned */
    UInt32 inSize, inLen, inPos;
    UInt8 inEof;
    UInt8 *window;        /* inflateWindowSize bytes, caller-owned */
    UInt32 winPos;        /* next write position in the window */
    UInt32 outTotal;      /* bytes produced so far */
    const UInt32 *crcTable;
    UInt32 crc;           /* running CRC-32 of the output (final value after InflateRun) */
    UInt32 bitBuf;
    UInt16 bitCnt;
    Int32 err;
    InflateHuff lenCode;
    InflateHuff distCode;
} Inflate;

#define inflateErrInput   (-1)   /* read callback failed or data ran out */
#define inflateErrOutput  (-2)   /* write callback failed */
#define inflateErrFormat  (-3)   /* corrupt or unsupported deflate data */
#define inflateErrTooFar  (-4)   /* back-reference before the start of output */

void  InflateInit(Inflate *z, InflateReadFn read, InflateWriteFn write, void *ctx,
                  UInt8 *inBuf, UInt32 inSize, UInt8 *window, const UInt32 *crcTable) EXPORT_SEC;
/* Decodes one complete deflate stream. Returns 0 or an inflateErr code. */
Int32 InflateRun(Inflate *z) EXPORT_SEC;

#endif
