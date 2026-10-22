/* A byte sink the export writers write to. On the host it wraps a FILE*;
 * on the device it wraps VFSFileWrite (seekable) or ExgSend (not seekable).
 * Errors are sticky in err; callers check once at the end. */
#ifndef OUTSTREAM_H
#define OUTSTREAM_H
#include "ptypes.h"
#include "sections.h"

typedef struct OutStream OutStream;
struct OutStream {
    Int32 (*write)(OutStream *s, const void *buf, UInt32 len); /* 0 = ok */
    Int32 (*seek)(OutStream *s, UInt32 pos);                   /* NULL = cannot */
    UInt32 pos;
    Int32 err;
    void *ctx;
};

void   OutWrite(OutStream *s, const void *buf, UInt32 len) EXPORT_SEC;
void   OutStr(OutStream *s, const char *str) EXPORT_SEC;
void   OutU16LE(OutStream *s, UInt16 v) EXPORT_SEC;
void   OutU32LE(OutStream *s, UInt32 v) EXPORT_SEC;
void   OutDec(OutStream *s, Int32 v) EXPORT_SEC;
/* value / 10^decimals with a decimal point, e.g. 8462,2 -> 84.62 */
void   OutFixed(OutStream *s, Int32 value, UInt8 decimals) EXPORT_SEC;
UInt32 OutStrLen(const char *str) EXPORT_SEC;
Int32  OutSeek(OutStream *s, UInt32 pos) EXPORT_SEC;   /* -1 when unseekable */

#endif
