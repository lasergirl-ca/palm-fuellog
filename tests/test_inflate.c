/* Host driver: inflate one entry of a zip to a file and report the CRC.
 * Usage: test_inflate <zip> <entry> <out>
 * Exit 0 when the inflated size and CRC match the central directory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/inflate.h"
#include "../src/zipread.h"
#include "../src/crc32.h"

typedef struct { FILE *f; UInt32 pos, left; FILE *out; } Ctx;

static Int32 ReadAt(void *c, UInt32 pos, UInt8 *buf, UInt32 len)
{
    Ctx *x = c;
    if (fseek(x->f, (long)pos, SEEK_SET)) return -1;
    return (Int32)fread(buf, 1, len, x->f);
}

static Int32 ReadNext(void *c, UInt8 *buf, UInt32 len)
{
    Ctx *x = c;
    size_t n;
    if (x->left == 0) return 0;
    if (len > x->left) len = x->left;
    if (fseek(x->f, (long)x->pos, SEEK_SET)) return -1;
    n = fread(buf, 1, len, x->f);
    x->pos += (UInt32)n; x->left -= (UInt32)n;
    return (Int32)n;
}

static Int32 WriteOut(void *c, const UInt8 *buf, UInt32 len)
{
    Ctx *x = c;
    return fwrite(buf, 1, len, x->out) == len ? 0 : -1;
}

int main(int argc, char **argv)
{
    Ctx x;
    ZipEntry e;
    Inflate z;
    UInt8 scratch[1024], inBuf[4096];
    UInt8 *window;
    UInt32 crcTable[256];
    long size;
    Int32 r;

    if (argc != 4) { fprintf(stderr, "usage: test_inflate zip entry out\n"); return 2; }
    x.f = fopen(argv[1], "rb");
    if (!x.f) { perror(argv[1]); return 2; }
    fseek(x.f, 0, SEEK_END); size = ftell(x.f);
    r = ZipFind(ReadAt, &x, (UInt32)size, argv[2], scratch, sizeof scratch, &e);
    if (r) { fprintf(stderr, "ZipFind: %ld\n", (long)r); return 1; }
    x.out = fopen(argv[3], "wb");
    x.pos = e.dataOffset; x.left = e.csize;
    window = malloc(inflateWindowSize);
    Crc32Init(crcTable);
    if (e.method == 0) {
        UInt32 crc = 0xFFFFFFFFUL, n = e.csize;
        while (n) {
            Int32 got = ReadNext(&x, inBuf, n < sizeof inBuf ? n : sizeof inBuf);
            if (got <= 0) break;
            crc = Crc32Update(crcTable, crc, inBuf, (UInt32)got);
            fwrite(inBuf, 1, (size_t)got, x.out);
            n -= (UInt32)got;
        }
        z.crc = crc ^ 0xFFFFFFFFUL; z.outTotal = e.usize - n;
    } else if (e.method == 8) {
        InflateInit(&z, ReadNext, WriteOut, &x, inBuf, sizeof inBuf, window, crcTable);
        r = InflateRun(&z);
        if (r) { fprintf(stderr, "InflateRun: %ld after %lu bytes\n", (long)r, (unsigned long)z.outTotal); return 1; }
    } else {
        fprintf(stderr, "method %u not supported\n", e.method); return 1;
    }
    fclose(x.out);
    printf("%s: method %u, %lu -> %lu bytes, crc %08lx (expected %08lx) %s\n", argv[2], e.method,
           (unsigned long)e.csize, (unsigned long)z.outTotal, (unsigned long)z.crc, (unsigned long)e.crc,
           (z.crc == e.crc && z.outTotal == e.usize) ? "ok" : "MISMATCH");
    return (z.crc == e.crc && z.outTotal == e.usize) ? 0 : 1;
}
