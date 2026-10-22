#include "outstream.h"

UInt32 OutStrLen(const char *str)
{
    UInt32 n = 0;
    while (str[n]) n++;
    return n;
}

void OutWrite(OutStream *s, const void *buf, UInt32 len)
{
    if (s->err || len == 0) return;
    s->err = s->write(s, buf, len);
    s->pos += len;
}

void OutStr(OutStream *s, const char *str)
{
    OutWrite(s, str, OutStrLen(str));
}

void OutU16LE(OutStream *s, UInt16 v)
{
    UInt8 b[2];
    b[0] = (UInt8)v; b[1] = (UInt8)(v >> 8);
    OutWrite(s, b, 2);
}

void OutU32LE(OutStream *s, UInt32 v)
{
    UInt8 b[4];
    b[0] = (UInt8)v; b[1] = (UInt8)(v >> 8); b[2] = (UInt8)(v >> 16); b[3] = (UInt8)(v >> 24);
    OutWrite(s, b, 4);
}

void OutDec(OutStream *s, Int32 v)
{
    char buf[12];
    int i = 11;
    UInt32 u = (v < 0) ? (UInt32)(-v) : (UInt32)v;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) buf[--i] = '-';
    OutStr(s, buf + i);
}

void OutFixed(OutStream *s, Int32 value, UInt8 decimals)
{
    char buf[16];
    int i = 15, d;
    UInt32 u = (value < 0) ? (UInt32)(-value) : (UInt32)value;
    buf[i] = 0;
    for (d = 0; d < decimals; d++) { buf[--i] = (char)('0' + u % 10); u /= 10; }
    if (decimals) buf[--i] = '.';
    do { buf[--i] = (char)('0' + u % 10); u /= 10; } while (u);
    if (value < 0) buf[--i] = '-';
    OutStr(s, buf + i);
}

Int32 OutSeek(OutStream *s, UInt32 pos)
{
    if (!s->seek) return -1;
    if (s->err) return s->err;
    s->err = s->seek(s, pos);
    if (!s->err) s->pos = pos;
    return s->err;
}
