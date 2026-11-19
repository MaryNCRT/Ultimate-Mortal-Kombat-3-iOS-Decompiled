/* pad_db_test.c -- src/runtime/platform/pad_db.h against the bundled
 * gamecontrollerdb.txt: a few pads' lines found and read as expected.
 *
 *     cc -I src/runtime/platform tests/pad_db_test.c -o pad_db_test && ./pad_db_test .
 *
 * The argument is the repository's root (the folder holding src/). */
#include <stdio.h>
#include <string.h>

/* platform.h's PB_* without the rest of it */
#define LIME_PLATFORM_H
enum {
    PB_SOUTH, PB_EAST, PB_WEST, PB_NORTH,
    PB_L1, PB_R1, PB_L2, PB_R2,
    PB_SELECT, PB_START, PB_L3, PB_R3, PB_HOME,
    PB_COUNT
};
#include "pad_db.h"

static int fails;

static void expect(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        fails++;
}

int main(int argc, char **argv)
{
    char dir[1024];
    PDB_MAP m;

    snprintf(dir, sizeof dir, "%s/", argc > 1 ? argv[1] : ".");
    pdb_load(dir);
    expect(pdb_text && strlen(pdb_text) > 100000, "the bundled file is read");

    /* DualShock 4 (054c:05c4) on Windows: square b0, cross b1, circle b2,
     * triangle b3, the hat for the cross */
    expect(pdb_find("Windows", 0x054c, 0x05c4, &m), "054c:05c4 found on Windows");
    expect(m.b[PB_SOUTH].kind == PDB_BUTTON && m.b[PB_SOUTH].idx == 1, "  cross is b1");
    expect(m.b[PB_WEST].kind == PDB_BUTTON && m.b[PB_WEST].idx == 0, "  square is b0");
    expect(m.up.kind == PDB_HAT && m.up.hat == 1, "  up is the hat's 1");
    printf("     \"%s\"\n", m.name);

    /* the Steam Deck's own controls on Linux */
    expect(pdb_find("Linux", 0x28de, 0x1205, &m), "28de:1205 (Steam Deck) found on Linux");
    expect(m.b[PB_SOUTH].kind == PDB_BUTTON, "  A is a button");
    expect(m.b[PB_L2].kind == PDB_AXIS, "  the left trigger is an axis");
    printf("     \"%s\"\n", m.name);

    /* a vendor:product with no line, and the wrong platform */
    expect(!pdb_find("Windows", 0x1234, 0x5678, &m), "1234:5678 not found");
    expect(!pdb_find("Plan9", 0x054c, 0x05c4, &m), "another platform not found");

    /* any 8BitDo line on Windows */
    {
        unsigned p;
        int any = 0;
        for (p = 0; p < 0x10000 && !any; p++)
            any = pdb_find("Windows", 0x2dc8, p, &m);
        expect(any, "an 8BitDo pad found on Windows");
        if (any)
            printf("     \"%s\" (2dc8:%04x)\n", m.name, p - 1);
    }
    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
