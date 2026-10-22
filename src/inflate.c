#include "inflate.h"
#include "crc32.h"

#define kMaxBits 15
#define kFastSize (1 << inflateFastBits)

static const UInt16 kLenBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const UInt8 kLenExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const UInt16 kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const UInt8 kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const UInt8 kClOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static Int32 Need(Inflate *z, UInt16 n) EXPORT_SEC;
static Int32 Bits(Inflate *z, UInt16 need) EXPORT_SEC;
static Int32 Decode(Inflate *z, const InflateHuff *h) EXPORT_SEC;
static Int32 Construct(InflateHuff *h, const UInt8 *length, UInt16 n) EXPORT_SEC;
static Int32 Flush(Inflate *z, UInt32 len) EXPORT_SEC;
static Int32 Stored(Inflate *z) EXPORT_SEC;
static Int32 Codes(Inflate *z) EXPORT_SEC;
static Int32 Fixed(Inflate *z) EXPORT_SEC;
static Int32 Dynamic(Inflate *z) EXPORT_SEC;

void InflateInit(Inflate *z, InflateReadFn read, InflateWriteFn write, void *ctx,
                 UInt8 *inBuf, UInt32 inSize, UInt8 *window, const UInt32 *crcTable)
{
    z->read = read; z->write = write; z->ctx = ctx;
    z->in = inBuf; z->inSize = inSize; z->inLen = 0; z->inPos = 0; z->inEof = 0;
    z->window = window; z->winPos = 0; z->outTotal = 0;
    z->crcTable = crcTable;
    z->crc = 0xFFFFFFFFUL;
    z->bitBuf = 0; z->bitCnt = 0; z->err = 0;
}

/* Make at least n bits available when input remains. Returns -1 only on a
 * read error; at the end of input it returns 0 with bitCnt possibly < n. */
static Int32 Need(Inflate *z, UInt16 n)
{
    while (z->bitCnt < n) {
        if (z->inPos >= z->inLen) {
            Int32 got;
            if (z->inEof) return 0;
            got = z->read(z->ctx, z->in, z->inSize);
            if (got < 0) { z->err = inflateErrInput; return -1; }
            if (got == 0) { z->inEof = 1; return 0; }
            z->inLen = (UInt32)got; z->inPos = 0;
        }
        z->bitBuf |= (UInt32)z->in[z->inPos++] << z->bitCnt;
        z->bitCnt += 8;
    }
    return 0;
}

/* Return the next `need` bits (LSB first), or -1 on input failure. */
static Int32 Bits(Inflate *z, UInt16 need)
{
    UInt32 val;
    if (need == 0) return 0;
    if (Need(z, need) < 0) return -1;
    if (z->bitCnt < need) { z->err = inflateErrInput; return -1; }
    val = z->bitBuf & ((1UL << need) - 1);
    z->bitBuf >>= need;
    z->bitCnt -= need;
    return (Int32)val;
}

/* Table lookup for codes up to inflateFastBits long; canonical bit-by-bit
 * decoding (puff's method) for the rest. */
static Int32 Decode(Inflate *z, const InflateHuff *h)
{
    UInt16 e;
    Int32 code = 0, first = 0, index = 0, count;
    Int16 len;

    if (Need(z, inflateFastBits) < 0) return -1;
    e = h->fast[z->bitBuf & (kFastSize - 1)];
    if (e) {
        UInt16 n = e >> 12;
        if (n <= z->bitCnt) {
            z->bitBuf >>= n;
            z->bitCnt -= n;
            return e & 0x0FFF;
        }
    }
    for (len = 1; len <= kMaxBits; len++) {
        Int32 bit = Bits(z, 1);
        if (bit < 0) return -1;
        code |= bit;
        count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    z->err = inflateErrFormat;
    return -1;
}

/* Build count[], symbol[] and the fast table from code lengths. Returns 0
 * when complete, positive when incomplete (allowed for a single code),
 * negative when over-subscribed. */
static Int32 Construct(InflateHuff *h, const UInt8 *length, UInt16 n)
{
    Int16 offs[kMaxBits + 1];
    UInt16 nextCode[kMaxBits + 1];
    UInt16 sym, len, k;
    Int32 left;
    UInt16 code;

    for (len = 0; len <= kMaxBits; len++) h->count[len] = 0;
    for (sym = 0; sym < n; sym++) h->count[length[sym]]++;
    for (k = 0; k < kFastSize; k++) h->fast[k] = 0;
    if (h->count[0] == n) return 0;
    left = 1;
    for (len = 1; len <= kMaxBits; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return -1;
    }
    offs[1] = 0;
    for (len = 1; len < kMaxBits; len++) offs[len + 1] = offs[len] + h->count[len];
    for (sym = 0; sym < n; sym++)
        if (length[sym] != 0) h->symbol[offs[length[sym]]++] = (Int16)sym;

    /* Canonical codes (RFC 1951 3.2.2), bit-reversed for LSB-first lookup. */
    code = 0;
    nextCode[0] = 0;
    for (len = 1; len <= kMaxBits; len++) {
        code = (UInt16)((code + (len == 1 ? 0 : h->count[len - 1])) << 1);
        nextCode[len] = code;
    }
    for (sym = 0; sym < n; sym++) {
        len = length[sym];
        if (len != 0) {
            UInt16 c = nextCode[len]++;
            if (len <= inflateFastBits) {
                UInt16 rev = 0, i;
                for (i = 0; i < len; i++) rev |= (UInt16)(((c >> i) & 1) << (len - 1 - i));
                for (k = rev; k < kFastSize; k += (UInt16)(1 << len))
                    h->fast[k] = (UInt16)((len << 12) | sym);
            }
        }
    }
    return left;
}

static Int32 Flush(Inflate *z, UInt32 len)
{
    if (len == 0) return 0;
    z->crc = Crc32Update(z->crcTable, z->crc, z->window, len);
    if (z->write(z->ctx, z->window, len) != 0) { z->err = inflateErrOutput; return -1; }
    return 0;
}

#define EMIT(z, b) \
    do { (z)->window[(z)->winPos++] = (UInt8)(b); (z)->outTotal++; \
         if ((z)->winPos == inflateWindowSize) { if (Flush((z), inflateWindowSize) < 0) return -1; (z)->winPos = 0; } } while (0)

static Int32 Stored(Inflate *z)
{
    UInt32 len, nlen;
    Int32 a, b;
    z->bitBuf = 0; z->bitCnt = 0;             /* drop to a byte boundary */
    a = Bits(z, 16); if (a < 0) return -1;
    b = Bits(z, 16); if (b < 0) return -1;
    len = (UInt32)a; nlen = (UInt32)b;
    if (len != (~nlen & 0xFFFFUL)) { z->err = inflateErrFormat; return -1; }
    while (len--) {
        b = Bits(z, 8);
        if (b < 0) return -1;
        EMIT(z, b);
    }
    return 0;
}

static Int32 Codes(Inflate *z)
{
    UInt8 *win = z->window;
    for (;;) {
        Int32 sym = Decode(z, &z->lenCode);
        if (sym < 0) return -1;
        if (sym < 256) {
            EMIT(z, sym);
        } else if (sym == 256) {
            return 0;
        } else {
            UInt32 len, dist, from;
            Int32 e;
            sym -= 257;
            if (sym >= 29) { z->err = inflateErrFormat; return -1; }
            e = Bits(z, kLenExtra[sym]);
            if (e < 0) return -1;
            len = kLenBase[sym] + (UInt32)e;
            sym = Decode(z, &z->distCode);
            if (sym < 0) return -1;
            if (sym >= 30) { z->err = inflateErrFormat; return -1; }
            e = Bits(z, kDistExtra[sym]);
            if (e < 0) return -1;
            dist = kDistBase[sym] + (UInt32)e;
            if (dist > z->outTotal || dist > inflateWindowSize) { z->err = inflateErrTooFar; return -1; }
            from = (z->winPos + inflateWindowSize - dist) & (inflateWindowSize - 1);
            /* Fast path: the copy neither reads nor writes across the window end. */
            if (from + len <= inflateWindowSize && z->winPos + len < inflateWindowSize) {
                UInt8 *d = win + z->winPos;
                const UInt8 *s = win + from;
                UInt32 k = len;
                while (k--) *d++ = *s++;
                z->winPos += len;
                z->outTotal += len;
            } else {
                while (len--) {
                    UInt8 b = win[from];
                    EMIT(z, b);
                    from = (from + 1) & (inflateWindowSize - 1);
                }
            }
        }
    }
}

static Int32 Fixed(Inflate *z)
{
    UInt8 lengths[288];
    UInt16 sym;
    for (sym = 0; sym < 144; sym++) lengths[sym] = 8;
    for (; sym < 256; sym++) lengths[sym] = 9;
    for (; sym < 280; sym++) lengths[sym] = 7;
    for (; sym < 288; sym++) lengths[sym] = 8;
    Construct(&z->lenCode, lengths, 288);
    for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
    Construct(&z->distCode, lengths, 30);
    return Codes(z);
}

static Int32 Dynamic(Inflate *z)
{
    UInt8 lengths[320];
    UInt16 nlen, ndist, ncode, index;
    Int32 v;

    v = Bits(z, 5); if (v < 0) return -1; nlen = (UInt16)v + 257;
    v = Bits(z, 5); if (v < 0) return -1; ndist = (UInt16)v + 1;
    v = Bits(z, 4); if (v < 0) return -1; ncode = (UInt16)v + 4;
    if (nlen > 286 || ndist > 30) { z->err = inflateErrFormat; return -1; }

    for (index = 0; index < ncode; index++) {
        v = Bits(z, 3); if (v < 0) return -1;
        lengths[kClOrder[index]] = (UInt8)v;
    }
    for (; index < 19; index++) lengths[kClOrder[index]] = 0;
    if (Construct(&z->lenCode, lengths, 19) != 0) { z->err = inflateErrFormat; return -1; }

    index = 0;
    while (index < nlen + ndist) {
        Int32 sym = Decode(z, &z->lenCode);
        UInt8 len = 0;
        UInt16 rep;
        if (sym < 0) return -1;
        if (sym < 16) { lengths[index++] = (UInt8)sym; continue; }
        if (sym == 16) {
            if (index == 0) { z->err = inflateErrFormat; return -1; }
            len = lengths[index - 1];
            v = Bits(z, 2); if (v < 0) return -1; rep = 3 + (UInt16)v;
        } else if (sym == 17) {
            v = Bits(z, 3); if (v < 0) return -1; rep = 3 + (UInt16)v;
        } else {
            v = Bits(z, 7); if (v < 0) return -1; rep = 11 + (UInt16)v;
        }
        if (index + rep > nlen + ndist) { z->err = inflateErrFormat; return -1; }
        while (rep--) lengths[index++] = len;
    }
    if (lengths[256] == 0) { z->err = inflateErrFormat; return -1; }
    v = Construct(&z->lenCode, lengths, nlen);
    if (v < 0 || (v > 0 && nlen - z->lenCode.count[0] != 1)) { z->err = inflateErrFormat; return -1; }
    v = Construct(&z->distCode, lengths + nlen, ndist);
    if (v < 0 || (v > 0 && ndist - z->distCode.count[0] != 1)) { z->err = inflateErrFormat; return -1; }
    return Codes(z);
}

Int32 InflateRun(Inflate *z)
{
    Int32 last, type, r;
    do {
        last = Bits(z, 1);
        if (last < 0) return z->err;
        type = Bits(z, 2);
        if (type < 0) return z->err;
        if (type == 0) r = Stored(z);
        else if (type == 1) r = Fixed(z);
        else if (type == 2) r = Dynamic(z);
        else { z->err = inflateErrFormat; r = -1; }
        if (r < 0) return z->err;
    } while (!last);
    if (Flush(z, z->winPos) < 0) return z->err;
    z->winPos = 0;
    z->crc ^= 0xFFFFFFFFUL;
    return 0;
}
