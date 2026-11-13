/* Palm database files (.pdb) read whole into memory and written whole back:
 * FuelLog's storage off the Palm. The same bytes a Palm backup set holds,
 * so a set copied from a Palm opens here and a set written here restores
 * on a Palm. Plain C: built for Windows Mobile and for the host tests. */
#ifndef PDBFILE_H
#define PDBFILE_H
#include "../../src/ptypes.h"

#define pdbOk            0
#define pdbErrMissing   -1     /* no such file */
#define pdbErrMalformed -2
#define pdbErrMemory    -3
#define pdbErrWrite     -4
#define pdbErrRange     -5

#define pdbAttrBackup 0x0008   /* the header attribute HotSync backs a database up by */
#define pdbMaxRecords 16000

typedef struct {
    UInt8 *data;
    UInt32 len;
    UInt8  attr;               /* the record's attribute byte, kept as found */
    UInt32 id;                 /* its 24-bit unique id */
} PdbRecord;

typedef struct {
    char   name[32];
    UInt16 attrs, version;
    UInt32 created, modified, backedUp, modNumber;   /* seconds since 1904, as the Palm keeps them */
    UInt32 type, creator, idSeed;
    UInt8 *appInfo;
    UInt32 appInfoLen;
    PdbRecord *recs;
    UInt16 count, cap;
} Pdb;

#define PdbTag(a, b, c, d) (((UInt32)(a) << 24) | ((UInt32)(b) << 16) | ((UInt32)(c) << 8) | (UInt32)(d))

void  PdbInit(Pdb *d, const char *name, UInt32 type, UInt32 creator);
void  PdbFree(Pdb *d);
Int32 PdbLoad(Pdb *d, const char *path);
/* Written beside the file as path~ first, then moved over it. */
Int32 PdbSave(const Pdb *d, const char *path);
Int32 PdbSetAppInfo(Pdb *d, const UInt8 *p, UInt32 len);
Int32 PdbInsert(Pdb *d, UInt16 index, const UInt8 *p, UInt32 len);   /* index >= count appends */
Int32 PdbReplace(Pdb *d, UInt16 index, const UInt8 *p, UInt32 len);
Int32 PdbRemove(Pdb *d, UInt16 index);

#endif
