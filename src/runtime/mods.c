/*
 * mods.c -- the player's mods, laid over res/. Port, not in the binary.
 *
 * A mod is a folder in <game>/mods/. Whatever it holds replaces the game file
 * of the same NAME, wherever the game keeps it and wherever the mod puts it:
 * mods/HD Kano/KANO_DIFFUSE.png stands in for res/Textures/KANO_DIFFUSE.pvr,
 * mods/Music/MainMenu.mp3 for res/audio/MainMenu.mp3. Sub-folders inside a
 * mod are only for its author's order.
 *
 * Which mods are on, and which wins when two have the same file, is the
 * launcher's MOD MANAGER, written to mods/mods.ini:
 *
 *     ; highest priority first
 *     on=HD Kano
 *     off=Old blood
 *     on=Music
 *
 * A folder the file does not name is on, below the named ones (a mod copied
 * in by hand works without the launcher). Everything is indexed once, at
 * start: the engine asks for thousands of files and a folder walk per ask
 * would show.
 *
 * Two kinds of file are answered:
 *
 *   - by full name (mods_find), from lime_platform.c's resolve(): every
 *     file the engine loads -- models (.meshset .skin .bones .skinanim),
 *     scenes, events, sounds (.wav), music (.mp3);
 *   - textures by stem (mods_find_texture), from draw_gl.c: the data names a
 *     sheet ".PNG" that is ".pvr" on disk, and a modder paints a .png, so a
 *     texture matches on its name without the extension.
 */
#include "mods.h"
#include "skin_mats.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define MOD_MAX      256
#define NAME_MAX_    128
#define PATH_MAX_    1100

typedef struct Entry {
    char *key;                  /* the file's name, lower case */
    char *path;
} Entry;

typedef struct Map {
    Entry *slot;
    size_t cap, used;
} Map;

static Map g_files;             /* "kano.skin" -> path */
static Map g_tex;               /* "kano_diffuse" -> path, pictures only */
static int g_mods_on;
static int g_online;            /* an online fight: the game as it ships */
static char g_on[MOD_MAX][PATH_MAX_];   /* the mods that are on, highest first */

static void lower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

static size_t hash(const char *s)
{
    size_t h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

static void map_free(Map *m)
{
    size_t i;
    for (i = 0; i < m->cap; i++)
        if (m->slot[i].key) {
            free(m->slot[i].key);
            free(m->slot[i].path);
        }
    free(m->slot);
    memset(m, 0, sizeof(*m));
}

static const char *map_get(const Map *m, const char *key)
{
    size_t i;
    if (!m->cap)
        return NULL;
    for (i = hash(key) & (m->cap - 1); m->slot[i].key; i = (i + 1) & (m->cap - 1))
        if (strcmp(m->slot[i].key, key) == 0)
            return m->slot[i].path;
    return NULL;
}

/* the first mod to put a name in keeps it: they are walked highest first */
static void map_put(Map *m, const char *key, const char *path)
{
    size_t i;
    if (map_get(m, key))
        return;
    if ((m->used + 1) * 2 > m->cap) {
        Map big;
        big.cap = m->cap ? m->cap * 2 : 256;
        big.used = 0;
        big.slot = (Entry *)calloc(big.cap, sizeof(Entry));
        if (!big.slot)
            return;
        for (i = 0; i < m->cap; i++)
            if (m->slot[i].key) {
                size_t j = hash(m->slot[i].key) & (big.cap - 1);
                while (big.slot[j].key)
                    j = (j + 1) & (big.cap - 1);
                big.slot[j] = m->slot[i];
                big.used++;
            }
        free(m->slot);
        *m = big;
    }
    for (i = hash(key) & (m->cap - 1); m->slot[i].key; i = (i + 1) & (m->cap - 1))
        ;
    m->slot[i].key = (char *)malloc(strlen(key) + 1);
    m->slot[i].path = (char *)malloc(strlen(path) + 1);
    if (!m->slot[i].key || !m->slot[i].path) {
        free(m->slot[i].key);
        free(m->slot[i].path);
        m->slot[i].key = NULL;
        return;
    }
    strcpy(m->slot[i].key, key);
    strcpy(m->slot[i].path, path);
    m->used++;
}

static int is_picture(const char *lname)
{
    const char *dot = strrchr(lname, '.');
    return dot && (strcmp(dot, ".png") == 0 || strcmp(dot, ".pvr") == 0);
}

static void add_file(const char *dir, const char *name, int top)
{
    char key[NAME_MAX_], path[PATH_MAX_];
    if (strlen(name) >= sizeof(key))
        return;
    strcpy(key, name);
    lower(key);
    /* the mod's own card: its description and its picture in the launcher */
    if (top && (strcmp(key, "mod.ini") == 0 || strcmp(key, "data.ini") == 0
                || strncmp(key, "preview.", 8) == 0 || strncmp(key, "cover.", 6) == 0))
        return;
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    map_put(&g_files, key, path);
    if (is_picture(key)) {
        *strrchr(key, '.') = 0;
        map_put(&g_tex, key, path);
    }
}

static void walk(const char *dir, int top, int depth)
{
    char sub[PATH_MAX_];
    if (depth > 8)
        return;
#ifdef _WIN32
    {
        WIN32_FIND_DATAA fd;
        HANDLE h;
        snprintf(sub, sizeof(sub), "%s/*", dir);
        h = FindFirstFileA(sub, &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;
        do {
            if (fd.cFileName[0] == '.')
                continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                snprintf(sub, sizeof(sub), "%s/%s", dir, fd.cFileName);
                walk(sub, 0, depth + 1);
            } else
                add_file(dir, fd.cFileName, top);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir);
        struct dirent *e;
        struct stat st;
        if (!d)
            return;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.')
                continue;
            snprintf(sub, sizeof(sub), "%s/%s", dir, e->d_name);
            if (stat(sub, &st) != 0)
                continue;
            if (S_ISDIR(st.st_mode))
                walk(sub, 0, depth + 1);
            else
                add_file(dir, e->d_name, top);
        }
        closedir(d);
    }
#endif
}

/* the mod folders, for the ones mods.ini does not name */
static int list_dirs(const char *root, char names[][NAME_MAX_], int max)
{
    int n = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pat[PATH_MAX_];
    snprintf(pat, sizeof(pat), "%s/*", root);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if (fd.cFileName[0] != '.' && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                && strlen(fd.cFileName) < NAME_MAX_ && n < max)
            strcpy(names[n++], fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(root);
    struct dirent *e;
    struct stat st;
    char sub[PATH_MAX_];
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL && n < max) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= NAME_MAX_)
            continue;
        snprintf(sub, sizeof(sub), "%s/%s", root, e->d_name);
        if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode))
            strcpy(names[n++], e->d_name);
    }
    closedir(d);
#endif
    return n;
}

void mods_init(const char *res_root)
{
    static char named[MOD_MAX][NAME_MAX_];
    static char dirs[MOD_MAX][NAME_MAX_];
    char root[PATH_MAX_], path[PATH_MAX_], line[512];
    int nnamed = 0, ndirs, i, j;
    FILE *f;

    map_free(&g_files);
    map_free(&g_tex);
    g_mods_on = 0;
    if (!res_root || !*res_root)
        return;
    snprintf(root, sizeof(root), "%s/../mods", res_root);

    snprintf(path, sizeof(path), "%s/mods.ini", root);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) && nnamed < MOD_MAX) {
            char *v, *end;
            int on;
            line[strcspn(line, "\r\n")] = 0;
            if (strncmp(line, "on=", 3) == 0)
                on = 1, v = line + 3;
            else if (strncmp(line, "off=", 4) == 0)
                on = 0, v = line + 4;
            else
                continue;
            end = v + strlen(v);
            while (end > v && end[-1] == ' ')
                *--end = 0;
            if (!*v || strlen(v) >= NAME_MAX_ - 1 || strstr(v, "..") || strchr(v, '/') || strchr(v, '\\'))
                continue;
            /* the first character says on or off; the name follows */
            named[nnamed][0] = on ? '+' : '-';
            strcpy(named[nnamed] + 1, v);
            nnamed++;
        }
        fclose(f);
    }

    for (i = 0; i < nnamed; i++) {
        if (named[i][0] != '+')
            continue;
        snprintf(path, sizeof(path), "%s/%s", root, named[i] + 1);
        walk(path, 1, 0);
        snprintf(g_on[g_mods_on++], PATH_MAX_, "%s", path);
    }
    /* folders the file does not name: on, after the named ones */
    ndirs = list_dirs(root, dirs, MOD_MAX);
    for (i = 0; i < ndirs; i++) {
        int known = 0;
        for (j = 0; j < nnamed && !known; j++)
            known = strcmp(named[j] + 1, dirs[i]) == 0;
        if (known)
            continue;
        snprintf(path, sizeof(path), "%s/%s", root, dirs[i]);
        walk(path, 1, 0);
        if (g_mods_on < MOD_MAX)
            snprintf(g_on[g_mods_on++], PATH_MAX_, "%s", path);
    }
    if (g_mods_on)
        printf("mods: %d on, %d files\n", g_mods_on, (int)g_files.used);
}

/* Online, only what is seen and changes nothing of the fight: models
 * (.skin .meshset, a skin's .mats) and textures. Animations, scenes, events, sounds and the
 * CHARACTER EDITOR's numbers stay the game's, the same on both machines. */
static int online_file(const char *key)
{
    const char *dot = strrchr(key, '.');
    return dot && (strcmp(dot, ".skin") == 0 || strcmp(dot, ".meshset") == 0
                   || strcmp(dot, ".mats") == 0 || is_picture(key));
}

int mods_find(const char *rel, char *out, size_t n)
{
    char key[NAME_MAX_];
    const char *base, *hit;
    if (!g_files.used || !rel)
        return 0;
    base = rel + strlen(rel);
    while (base > rel && base[-1] != '/' && base[-1] != '\\')
        base--;
    if (strlen(base) >= sizeof(key))
        return 0;
    strcpy(key, base);
    lower(key);
    if (g_online && !online_file(key))
        return 0;
    hit = map_get(&g_files, key);
    if (!hit)
        return 0;
    snprintf(out, n, "%s", hit);
    return 1;
}

int mods_find_texture(const char *stem, char *out, size_t n)
{
    char key[NAME_MAX_];
    const char *base, *hit;
    size_t k;
    if (!g_tex.used || !stem)
        return 0;
    base = stem + strlen(stem);
    while (base > stem && base[-1] != '/' && base[-1] != '\\')
        base--;
    if (strlen(base) >= sizeof(key))
        return 0;
    strcpy(key, base);
    lower(key);
    hit = map_get(&g_tex, key);
    /* The smaller copies the game takes by quality: X_LOW (the iPhone's
     * sheets), X_LITE (a fighter's 256 skin, same layout) and X2_LITE,
     * X_LITE2, X_LOW2 (the second outfit's): a mod's X or X2 serves them */
    k = strlen(key);
    if (!hit && k > 4 && (strcmp(key + k - 4, "_low") == 0)) {
        key[k - 4] = 0;
        hit = map_get(&g_tex, key);
    } else if (!hit && k > 5 && strcmp(key + k - 5, "_lite") == 0) {
        key[k - 5] = 0;
        hit = map_get(&g_tex, key);
    } else if (!hit && k > 6 && (strcmp(key + k - 6, "_lite2") == 0
                                 || strcmp(key + k - 5, "_low2") == 0)) {
        strcpy(key + k - (key[k - 5] == '_' ? 5 : 6), "2");
        hit = map_get(&g_tex, key);
    }
    if (!hit)
        return 0;
    snprintf(out, n, "%s", hit);
    return 1;
}

int mods_count(int *files)
{
    if (files)
        *files = (int)g_files.used;
    return g_mods_on;
}

/* ------------------------------------------------------------ data.ini
 *
 * A mod's numbers: the launcher's CHARACTER EDITOR writes them, one per line,
 *
 *     ; Kano's high kick: full damage 24 -> 30, chip 5
 *     stk_kano_hikick[5] = 0x1e05
 *
 * a table of the fight engine by its name in the binary (the registry
 * logic_tables.c ends with) and a word in it. Applied once at start, lowest
 * priority first so the highest wins. A word that holds an address is never
 * written: the editor offers only the number fields. Words are 32 bits in
 * both builds (the 64-bit one keeps the tables' words 32-bit, UMK3_LOW32). */
struct umk3_logic_name { const char *name; void *at; unsigned n; };
extern const struct umk3_logic_name umk3_logic_names[];
extern const unsigned umk3_logic_nnames;

static const struct umk3_logic_name *logic_name(const char *name)
{
    unsigned i;

    for (i = 0; i < umk3_logic_nnames; i++)
        if (strcmp(umk3_logic_names[i].name, name) == 0)
            return &umk3_logic_names[i];
    return NULL;
}

/* every word the mods changed and what the game had there, so an online
 * fight can have the game's own (mods_online) */
typedef struct Old { uint32_t *at; uint32_t v; } Old;
static Old *g_old;
static size_t g_nold, g_capold;

static void keep_old(uint32_t *at)
{
    size_t i;

    for (i = 0; i < g_nold; i++)
        if (g_old[i].at == at)
            return;
    if (g_nold == g_capold) {
        size_t cap = g_capold ? g_capold * 2 : 256;
        Old *o = realloc(g_old, cap * sizeof *o);
        if (!o)
            return;
        g_old = o;
        g_capold = cap;
    }
    g_old[g_nold].at = at;
    g_old[g_nold++].v = *at;
}

int mods_apply_data(void)
{
    char path[PATH_MAX_ + 16], line[512];
    int m, done = 0;

    for (m = g_mods_on - 1; m >= 0; m--) {
        FILE *f;

        snprintf(path, sizeof(path), "%s/data.ini", g_on[m]);
        f = fopen(path, "r");
        if (!f)
            continue;
        while (fgets(line, sizeof(line), f)) {
            char name[96], *p = line;
            const struct umk3_logic_name *t;
            unsigned long idx;
            long long v;
            int k = 0;

            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == ';' || *p == '#' || !*p)
                continue;
            while (*p && *p != '[' && k < (int)sizeof(name) - 1)
                name[k++] = *p++;
            name[k] = 0;
            while (k && name[k - 1] == ' ')
                name[--k] = 0;
            if (*p != '[')
                continue;
            idx = strtoul(p + 1, &p, 0);
            if (*p != ']' || !(p = strchr(p, '=')))
                continue;
            v = strtoll(p + 1, NULL, 0);
            t = logic_name(name);
            if (!t || idx >= t->n) {
                printf("mods: %s: no %s[%lu]\n", path, name, idx);
                continue;
            }
            keep_old((uint32_t *)t->at + idx);
            ((uint32_t *)t->at)[idx] = (uint32_t)v;
            done++;
        }
        fclose(f);
    }
    if (done)
        printf("mods: %d numbers changed\n", done);
    return done;
}

void mods_online(int on)
{
    size_t i;

    if (on == g_online)
        return;
    g_online = on;
    if (on) {
        port_skin_bones_online();       /* skin_mats.c: models' lengths */
        for (i = 0; i < g_nold; i++)
            *g_old[i].at = g_old[i].v;
        if (g_nold)
            printf("mods: online, the game's own numbers (%u put back)\n",
                   (unsigned)g_nold);
    } else if (g_nold)
        mods_apply_data();
}
