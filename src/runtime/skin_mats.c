/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * skin_mats.c -- a modded skin's own textures, one per part. Port, not in
 * the binary.
 *
 * The game draws a skin block with one texture (the outfit's diffuse; the
 * second block with its own). A model brought in through REPLACE MODEL
 * (launcher ui/src/lib/mods/modelin.js) can have several materials, each
 * with its own picture, UVs that run past 0..1 (tiling) and transparency.
 * Instead of packing them onto one sheet, the launcher writes the skin's
 * triangles grouped by material and, beside it, `<skin>.mats` -- text, one
 * part a line:
 *
 *      block first count alpha texture
 *
 *   block     0 the body, 1 the second skin
 *   first     the part's first triangle in the block, count how many
 *   alpha     1: the picture has transparency (drawn after the rest,
 *             alpha-tested and blended)
 *   texture   the part's picture, or `-` for the block's own texture (the
 *             outfit chosen)
 *
 * The record LoadAnimatedCharacter bakes keeps the skin's triangle order
 * (RenderIndexes, three indices a face), so a part is a run of indices.
 * Frozen, each part has its iced copy (`<texture>_ICE`, the launcher writes
 * it); babality and the shadow draw the whole block with the texture the
 * game passes, as before. A file whose parts all use the block's own
 * texture, opaque, is no parts at all. SMOOTH MODELS leaves a skin with
 * parts unsmoothed (its subdivided triangles do not keep the runs). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods.h"
#include "platform/gl.h"
#include "../decomp/lime/lime.h"
#include "skin_mats.h"

#ifndef GL_REPEAT
#define GL_REPEAT 0x2901
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#endif

typedef struct {
    long first, count;
    int alpha;
    char name[96];          /* "" the block's own texture */
    TEXTURE *tex[2];        /* plain, iced: loaded on the first draw */
    int tried[2];
} Part;

typedef struct {
    const void *skin;       /* the block's SKININFO */
    int n;
    Part *p;
} Mats;

#define MATS_MAX 64
static Mats g_mats[MATS_MAX];
static const Mats *g_use;
static int g_ice;

static Mats *find(const void *skin)
{
    int i;
    if (!skin)
        return NULL;
    for (i = 0; i < MATS_MAX; i++)
        if (g_mats[i].skin == skin)
            return &g_mats[i];
    return NULL;
}

static void drop(Mats *m)
{
    int i;
    for (i = 0; i < m->n; i++) {
        if (m->p[i].tex[0])
            limeDeleteTexture(m->p[i].tex[0]);
        if (m->p[i].tex[1])
            limeDeleteTexture(m->p[i].tex[1]);
    }
    free(m->p);
    if (g_use == m)
        g_use = NULL;
    memset(m, 0, sizeof *m);
}

static void add(const void *skin, const Part *pt)
{
    Mats *m = find(skin);
    int i;
    if (!m)
        for (i = 0; i < MATS_MAX && !m; i++)
            if (!g_mats[i].skin) {
                m = &g_mats[i];
                m->skin = skin;
            }
    if (!m)
        return;
    m->p = (Part *)realloc(m->p, sizeof(Part) * (size_t)(m->n + 1));
    m->p[m->n++] = *pt;
}

void port_skin_mats_loaded(const void *skin, const char *filename)
{
    char rel[160], path[1024], line[256];
    const void *blk[2];
    FILE *f;
    int b;

    if (!skin || !filename)
        return;
    snprintf(rel, sizeof rel, "%s.mats", filename);
    if (!mods_find(rel, path, sizeof path) || !(f = fopen(path, "r")))
        return;
    blk[0] = skin;
    blk[1] = *(void *const *)skin;      /* SKININFO+0x00, the second block */
    while (fgets(line, sizeof line, f)) {
        Part pt;
        char tex[96];
        memset(&pt, 0, sizeof pt);
        if (line[0] == '#'
            || sscanf(line, "%d %ld %ld %d %95s", &b, &pt.first, &pt.count,
                      &pt.alpha, tex) != 5
            || b < 0 || b > 1 || !blk[b] || pt.first < 0 || pt.count <= 0)
            continue;
        if (strcmp(tex, "-") != 0)
            snprintf(pt.name, sizeof pt.name, "%s", tex);
        add(blk[b], &pt);
    }
    fclose(f);
    for (b = 0; b < 2; b++) {
        Mats *m = find(blk[b]);
        int i, own = 0;
        if (!m)
            continue;
        for (i = 0; i < m->n; i++)
            own |= m->p[i].name[0] || m->p[i].alpha;
        printf("skin_mats: %s block %d, %d parts%s\n", rel, b, m->n,
               own ? "" : " (all the block's own texture: none)");
        if (!own)
            drop(m);
    }
}

void port_skin_mats_forget(const void *skin)
{
    Mats *m;
    if (!skin)
        return;
    if ((m = find(skin)) != NULL)
        drop(m);
    if ((m = find(*(void *const *)skin)) != NULL)
        drop(m);
}

int port_skin_mats_has(const void *skin)
{
    return find(skin) != NULL;
}

void port_skin_mats_use(const void *skin, int ice)
{
    g_use = find(skin);
    g_ice = ice != 0;
}

/* "A.png" -> "A_ICE.png" */
static void iced(const char *name, char *out, size_t n)
{
    const char *dot = strrchr(name, '.');
    int stem = dot ? (int)(dot - name) : (int)strlen(name);
    snprintf(out, n, "%.*s_ICE%s", stem, name, dot ? dot : "");
}

static void part(const Part *pt, const unsigned short *indices, long nfaces,
                 unsigned tex_name)
{
    long first = pt->first, count = pt->count;
    if (first >= nfaces)
        return;
    if (first + count > nfaces)
        count = nfaces - first;
    if (pt->tex[g_ice]) {
        glBindTexture(GL_TEXTURE_2D, pt->tex[g_ice]->name);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else
        glBindTexture(GL_TEXTURE_2D, tex_name);
    glDrawElements(GL_TRIANGLES, (int)(count * 3), GL_UNSIGNED_SHORT,
                   indices + first * 3);
}

int port_skin_mats_draw(const unsigned short *indices, long nfaces,
                        unsigned tex_name)
{
    Mats *m = (Mats *)g_use;
    int i, any = 0;

    if (!m || !m->n)
        return 0;
    for (i = 0; i < m->n; i++) {
        Part *pt = &m->p[i];
        if (pt->name[0] && !pt->tried[g_ice]) {
            char nm[112];
            pt->tried[g_ice] = 1;
            if (g_ice)
                iced(pt->name, nm, sizeof nm);
            else
                snprintf(nm, sizeof nm, "%s", pt->name);
            pt->tex[g_ice] = limeLoadTexture(nm, 0, 0);
            if (!pt->tex[g_ice])
                printf("skin_mats: %s not found\n", nm);
        }
    }
    /* the opaque parts, then the ones that see through */
    for (i = 0; i < m->n; i++)
        if (!m->p[i].alpha)
            part(&m->p[i], indices, nfaces, tex_name);
        else
            any = 1;
    if (any) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.1f);
        limeEnableAlphaBlending_Basic();
        for (i = 0; i < m->n; i++)
            if (m->p[i].alpha)
                part(&m->p[i], indices, nfaces, tex_name);
        limeDisableAlphaBlending();
        glDisable(GL_ALPHA_TEST);
    }
    glBindTexture(GL_TEXTURE_2D, tex_name);
    return 1;
}
