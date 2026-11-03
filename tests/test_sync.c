/* The sync rules and the bundle framing. */
#include <stdio.h>
#include <string.h>
#include "../src/sync.h"
#include "../src/records.h"

static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

int main(void)
{
    /* Newer wins; ties and unknowns keep the local copy. */
    check("newer replaces", SyncNewer(100, 200), 1);
    check("older keeps", SyncNewer(200, 100), 0);
    check("equal keeps", SyncNewer(150, 150), 0);
    check("unknown vs unknown keeps", SyncNewer(0, 0), 0);
    check("known beats unknown", SyncNewer(0, 1), 1);
    check("unknown never beats known", SyncNewer(1, 0), 0);
    /* A deletion applies unless the record was edited after it. */
    check("delete older record", SyncDeletes(100, 200), 1);
    check("delete at the same second", SyncDeletes(200, 200), 1);
    check("edit after delete survives", SyncDeletes(300, 200), 0);
    check("never-edited record deletes", SyncDeletes(0, 1), 1);
    /* A local deletion refuses the incoming record it deleted. */
    check("refused when deleted later", SyncRefused(100, 200), 1);
    check("refused at the same second", SyncRefused(200, 200), 1);
    check("accepted when edited after the deletion", SyncRefused(300, 200), 0);

    /* Framing: header, then [db][len][bytes]. */
    {
        UInt8 b[64];
        SyncReader r;
        UInt16 kind, count, len;
        UInt8 db;
        const UInt8 *rec;
        memcpy(b, "FUEL", 4);
        b[4] = 0; b[5] = recSchemaVersion; b[6] = 0; b[7] = syncKindSync; b[8] = 0; b[9] = 2;
        b[10] = 1; b[11] = 0; b[12] = 3; b[13] = 'a'; b[14] = 'b'; b[15] = 'c';     /* db 1, 3 bytes */
        b[16] = 5; b[17] = 0; b[18] = 1; b[19] = 'z';                               /* db 5, 1 byte */
        check("open", SyncOpen(&r, b, 20, &kind, &count), 0);
        check("kind", kind, syncKindSync);
        check("count", count, 2);
        check("first record", SyncNext(&r, &db, &rec, &len), 1);
        check("first db", db, 1);
        check("first len", len, 3);
        check("first bytes", rec[2], 'c');
        check("second record", SyncNext(&r, &db, &rec, &len), 1);
        check("second db", db, 5);
        check("end", SyncNext(&r, &db, &rec, &len), 0);
        check("truncated bundle", (SyncOpen(&r, b, 19, &kind, &count), SyncNext(&r, &db, &rec, &len), SyncNext(&r, &db, &rec, &len)), -1);
        b[5] = recSchemaVersion + 1;
        check("newer schema refused", SyncOpen(&r, b, 20, &kind, &count), -2);
        b[0] = 'X'; b[5] = recSchemaVersion;
        check("not a bundle", SyncOpen(&r, b, 20, &kind, &count), -1);
        check("too short", SyncOpen(&r, b, 4, &kind, &count), -1);
    }
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
