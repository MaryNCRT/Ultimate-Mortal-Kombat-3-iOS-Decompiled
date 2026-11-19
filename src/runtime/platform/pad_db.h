/*
 * pad_db.h -- pad layouts from SDL_GameControllerDB. Port, not in the binary.
 *
 * gamecontrollerdb.txt (https://github.com/mdqinc/SDL_GameControllerDB, zlib
 * licence, beside this file and in every package's src/) names, for some two
 * thousand pads, which of their buttons, axes and hat is which: one line a
 * pad and a system,
 *
 *     <GUID>,<name>,a:b1,b:b2,x:b0,y:b3,...,dpup:h0.1,lefttrigger:a2,platform:Windows,
 *
 * the GUID holding the USB vendor at hex digits 8..11 and the product at
 * 16..19, both little-endian. The Linux build gives the file to SDL itself
 * (sdl_gl.c); the Windows build reads WinMM pads -- everything XInput does
 * not cover: PlayStation, Switch, 8BitDo in D-input, arcade sticks, generic
 * USB pads -- through the line of its vendor and product (win32_gl.c), and
 * falls back on its own tables when the pad has none.
 *
 * A gamecontrollerdb.txt beside the game (the SDL2 Gamepad Tool writes one,
 * and SDL_GAMECONTROLLERCONFIG lines too) is read first: the player's own
 * mappings win over the bundled ones.
 *
 * A header, not a .c: the launchers out there compile a fixed list of files
 * (online_rules.c says the same), so a new one would not link.
 */
#ifndef UMK3_PAD_DB_H
#define UMK3_PAD_DB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

/* Where a pad's control comes from: a button, an axis (all of it, or the
 * half past its middle on one side), or a direction of the hat. */
enum { PDB_NONE, PDB_BUTTON, PDB_AXIS, PDB_HAT };

typedef struct {
    signed char kind;                   /* PDB_* */
    signed char idx;                    /* button, axis or hat number */
    signed char half;                   /* PDB_AXIS: +1, -1, 0 the whole axis */
    signed char inv;                    /* PDB_AXIS: '~', the axis read backwards */
    unsigned char hat;                  /* PDB_HAT: 1 up, 2 right, 4 down, 8 left */
} PDB_SRC;

typedef struct {
    PDB_SRC b[PB_COUNT];                /* by PB_* */
    PDB_SRC up, down, left, right;      /* the cross */
    PDB_SRC lx, ly, rx;                 /* the sticks */
    char name[64];
} PDB_MAP;

/* The files' text, read once: the player's file, then the bundled one. */
static char *pdb_text;

static void pdb_add_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *t;

    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0 && n < 16 * 1024 * 1024
        && (t = (char *)realloc(pdb_text, *len + (size_t)n + 2)) != NULL) {
        pdb_text = t;
        if (fread(pdb_text + *len, 1, (size_t)n, f) == (size_t)n) {
            *len += (size_t)n;
            pdb_text[(*len)++] = '\n';
            pdb_text[*len] = 0;
            printf("pad: mappings from %s\n", path);
        }
    }
    fclose(f);
}

/* `dir`: the game's folder, ending in a slash. */
static void pdb_load(const char *dir)
{
    static const char *const where[] = {
        "gamecontrollerdb.txt",                         /* the player's */
        "src/runtime/platform/gamecontrollerdb.txt",    /* the package's */
        "resources/src/runtime/platform/gamecontrollerdb.txt",  /* Linux's */
    };
    char path[1200];
    size_t len = 0;
    const char *env = getenv("SDL_GAMECONTROLLERCONFIG");
    int i;

    if (pdb_text)
        return;
    pdb_text = (char *)calloc(1, 1);
    if (!pdb_text)
        return;
    if (env && *env) {                  /* Steam's, or the SDL2 Gamepad Tool's */
        size_t n = strlen(env);
        char *t = (char *)realloc(pdb_text, n + 2);
        if (t) {
            pdb_text = t;
            memcpy(pdb_text, env, n);
            pdb_text[n] = '\n';
            pdb_text[n + 1] = 0;
            len = n + 1;
        }
    }
    for (i = 0; i < (int)(sizeof where / sizeof where[0]); i++) {
        snprintf(path, sizeof path, "%s%s", dir ? dir : "", where[i]);
        pdb_add_file(path, &len);
        if (i == 1 && len > 1000)       /* the package's found: not Linux's too */
            break;
    }
}

static int pdb_hex4(const char *s, unsigned *out)
{
    unsigned v = 0;
    int i;

    for (i = 0; i < 4; i++) {
        int c = s[i], d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return 0;
        v = v << 4 | (unsigned)d;
    }
    *out = v;
    return 1;
}

/* "b3", "a2", "+a2", "-a2", "a2~", "h0.4" */
static void pdb_src(const char *v, int n, PDB_SRC *s)
{
    char buf[16];
    const char *p = buf;

    memset(s, 0, sizeof *s);
    if (n <= 0 || n >= (int)sizeof buf)
        return;
    memcpy(buf, v, (size_t)n);
    buf[n] = 0;
    if (*p == '+' || *p == '-') {
        s->half = *p == '+' ? 1 : -1;
        p++;
    }
    if (*p == 'b') {
        s->kind = PDB_BUTTON;
        s->idx = (signed char)atoi(p + 1);
    } else if (*p == 'a') {
        s->kind = PDB_AXIS;
        s->idx = (signed char)atoi(p + 1);
        s->inv = strchr(p, '~') != NULL;
    } else if (*p == 'h' && strchr(p, '.')) {
        s->kind = PDB_HAT;
        s->idx = (signed char)atoi(p + 1);
        s->hat = (unsigned char)atoi(strchr(p, '.') + 1);
    }
}

/* The line for vendor:product on `platform` ("Windows", "Linux"); 1 if one
 * was found. The first line wins: the player's file comes first. */
static int pdb_find(const char *platform, unsigned vid, unsigned pid, PDB_MAP *m)
{
    const char *line = pdb_text;
    char plat[40];

    if (!line)
        return 0;
    snprintf(plat, sizeof plat, "platform:%s", platform);
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t ll = end ? (size_t)(end - line) : strlen(line);
        unsigned v, p;

        if (ll > 40 && line[0] != '#' && line[32] == ','
            && pdb_hex4(line + 8, &v) && pdb_hex4(line + 16, &p)
            && ((v >> 8) | (v & 0xff) << 8) == vid && ((p >> 8) | (p & 0xff) << 8) == pid) {
            /* the platform field, inside this line */
            const char *q = line;
            int ok = 0;
            while ((q = strstr(q, "platform:")) != NULL && q < line + ll) {
                ok = strncmp(q, plat, strlen(plat)) == 0;
                break;
            }
            if (ok) {
                static const char *const names[PB_COUNT] = {
                    "a", "b", "x", "y", "leftshoulder", "rightshoulder",
                    "lefttrigger", "righttrigger", "back", "start",
                    "leftstick", "rightstick", "guide",
                };
                const char *f = line + 33, *e = line + ll;
                const char *c = memchr(f, ',', (size_t)(e - f));
                int i;

                memset(m, 0, sizeof *m);
                if (c) {
                    size_t nl = (size_t)(c - f) < sizeof m->name - 1
                                ? (size_t)(c - f) : sizeof m->name - 1;
                    memcpy(m->name, f, nl);
                    f = c + 1;
                }
                while (f < e) {
                    const char *colon = memchr(f, ':', (size_t)(e - f));
                    const char *comma = memchr(f, ',', (size_t)(e - f));
                    int kn, vn;

                    if (!comma)
                        comma = e;
                    if (!colon || colon > comma) {
                        f = comma + 1;
                        continue;
                    }
                    kn = (int)(colon - f);
                    vn = (int)(comma - colon - 1);
                    for (i = 0; i < PB_COUNT; i++)
                        if ((int)strlen(names[i]) == kn && !strncmp(f, names[i], (size_t)kn))
                            pdb_src(colon + 1, vn, &m->b[i]);
#define PDB_KEY(k, dst) if (kn == (int)sizeof(k) - 1 && !strncmp(f, k, (size_t)kn)) \
                            pdb_src(colon + 1, vn, &m->dst)
                    PDB_KEY("dpup", up);
                    PDB_KEY("dpdown", down);
                    PDB_KEY("dpleft", left);
                    PDB_KEY("dpright", right);
                    PDB_KEY("leftx", lx);
                    PDB_KEY("lefty", ly);
                    PDB_KEY("rightx", rx);
#undef PDB_KEY
                    f = comma + 1;
                }
                return 1;
            }
        }
        if (!end)
            break;
        line = end + 1;
    }
    return 0;
}

#endif
