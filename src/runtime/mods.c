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
static Map *g_into_files = &g_files, *g_into_tex = &g_tex;  /* walk()'s target */
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
    map_put(g_into_files, key, path);
    if (is_picture(key)) {
        *strrchr(key, '.') = 0;
        map_put(g_into_tex, key, path);
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


/* ------------------------------------------------------------ added skins
 *
 * A mod whose mod.ini says
 *
 *     type=skin
 *
 * replaces nothing. It is another outfit for one fighter, beside the
 * game's: the select screen's SKIN box (FrontEnd.c) picks it, and its two
 * colours are the COLOR box's 1 and 2. Its files are indexed apart and are
 * only ever asked for through a fighter loaded with it (Players.c,
 * PortSkinLoad), under the name the game asks for with "@S<n>@" in front:
 * "@S1@SONYA_STANDARD.skin", "@S1@SONYA_DIFFUSE2.PNG". Whatever the skin
 * does not have is the game's own file (and a replacing mod's over it), so
 * a skin can be a whole model or two sheets for the game's.
 *
 * Its pictures and model can have any name; mod.ini says which is which:
 *
 *     skin=MKX              the skin's own name, the SKIN box's word; it
 *                           can change at will (else name=, the mod's,
 *                           else the folder's)
 *     character=SONYA       the game's name for the fighter's files (else
 *                           taken from the model's, SONYA_STANDARD.skin)
 *     model=MKX_SONYA.skin  (else the only .skin in the folder)
 *     color1=MKX_A.png      the COLOR 1 sheet (else <NAME>_DIFFUSE)
 *     color2=MKX_B.png      COLOR 2 (else <NAME>_DIFFUSE2, else colour 1)
 *     ice=MKX_ICE.png       frozen (else <NAME>_DIFFUSE_ICE, else colour 1)
 *
 * A file with the game's name in the skin comes before the one mod.ini
 * names (the launcher writes the game's names when it replaces a sheet or
 * the model, and the newest should win). The model's .mats and .bones go
 * beside it as <model>.mats / .bones; the part sheets a .mats names are
 * looked for in the skin first. */
#define SKIN_MAX 64

typedef struct Skin {
    char dir[PATH_MAX_];
    char title[48];
    long sig2;                      /* colour 1's bytes */
    char character[48];             /* lower case: "sonya", "" unknown */
    char model[NAME_MAX_];          /* lower case file name, "" none */
    char color[3][NAME_MAX_];       /* color1, color2, ice: lower-case stems */
    long sig;                       /* the model's (else colour 1's) bytes */
    Map files, tex;
} Skin;

static Skin *g_skins[SKIN_MAX];
static int g_nskins;

static void skins_free(void)
{
    int i;
    for (i = 0; i < g_nskins; i++) {
        map_free(&g_skins[i]->files);
        map_free(&g_skins[i]->tex);
        free(g_skins[i]);
        g_skins[i] = NULL;
    }
    g_nskins = 0;
}

static void trim(char *v)
{
    char *e;
    size_t lead = strspn(v, " \t");
    if (lead)
        memmove(v, v + lead, strlen(v + lead) + 1);
    e = v + strlen(v);
    while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
}

static const char *skin_tex(const Skin *k, const char *key);

/* A skin's sheets when mod.ini does not say which is which and they do
 * not have the game's names: by their own names -- one with "ice" in it is
 * the frozen one, one ending in 2 (COLOR2, _B2...) colour 2, the first of
 * the others colour 1. The model's part sheets (<model>_M1...) are left. */
static void skin_guess_colors(Skin *k)
{
    char conv[NAME_MAX_ + 16], mstem[NAME_MAX_];
    const char *dot;
    size_t i;

    if (!k->character[0] && !k->model[0])
        return;
    snprintf(conv, sizeof conv, "%s_diffuse", k->character);
    if (k->color[0][0] || (k->character[0] && map_get(&k->tex, conv)))
        return;
    snprintf(mstem, sizeof mstem, "%s", k->model);
    if ((dot = strchr(mstem, '.')) != NULL)
        mstem[dot - mstem] = 0;
    for (i = 0; i < k->tex.cap; i++) {
        const char *s = k->tex.slot[i].key;
        size_t l = s ? strlen(s) : 0;
        int w;
        if (!s || (mstem[0] && strncmp(s, mstem, strlen(mstem)) == 0
                   && s[strlen(mstem)] == '_' && s[strlen(mstem) + 1] == 'm'))
            continue;
        if (strstr(s, "_lite") || strstr(s, "_low"))
            continue;
        w = strstr(s, "ice") ? 2 : (l && s[l - 1] == '2') ? 1 : 0;
        if (!k->color[w][0] || strcmp(s, k->color[w]) < 0)
            snprintf(k->color[w], sizeof k->color[w], "%s", s);
    }
    if (k->color[0][0])
        printf("mods: skin %s, sheets by their names: %s, %s, %s\n", k->title,
               k->color[0], k->color[1][0] ? k->color[1] : "-",
               k->color[2][0] ? k->color[2] : "-");
}

/* 1 when the folder is a skin that adds (it is then taken here, not laid
 * over the game) */
static int skin_take(const char *dir, const char *folder)
{
    char path[PATH_MAX_ + 16], line[512];
    Skin *k;
    FILE *f;
    int is_skin = 0, named = 0;
    size_t i;

    snprintf(path, sizeof path, "%s/mod.ini", dir);
    if (!(f = fopen(path, "r")))
        return 0;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = 0;
        trim(line);
        lower(line);
        trim(eq + 1);
        lower(eq + 1);
        if (strcmp(line, "type") == 0 && strcmp(eq + 1, "skin") == 0)
            is_skin = 1;
    }
    if (!is_skin) {
        fclose(f);
        return 0;
    }
    if (g_nskins >= SKIN_MAX || !(k = (Skin *)calloc(1, sizeof *k))) {
        fclose(f);
        return 1;
    }
    snprintf(k->dir, sizeof k->dir, "%s", dir);
    snprintf(k->title, sizeof k->title, "%s", folder);
    rewind(f);
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), key[32], *v;
        if (line[0] == ';' || line[0] == '#' || !eq)
            continue;
        *eq = 0;
        v = eq + 1;
        snprintf(key, sizeof key, "%s", line);
        trim(key);
        lower(key);
        trim(v);
        if (!*v)
            continue;
        if (strcmp(key, "skin") == 0)
            snprintf(k->title, sizeof k->title, "%s", v), named = 1;
        else if (strcmp(key, "name") == 0 && !named)
            snprintf(k->title, sizeof k->title, "%s", v);
        else if (strcmp(key, "character") == 0)
            snprintf(k->character, sizeof k->character, "%s", v);
        else if (strcmp(key, "model") == 0)
            snprintf(k->model, sizeof k->model, "%s", v);
        else if (strcmp(key, "color1") == 0)
            snprintf(k->color[0], sizeof k->color[0], "%s", v);
        else if (strcmp(key, "color2") == 0)
            snprintf(k->color[1], sizeof k->color[1], "%s", v);
        else if (strcmp(key, "ice") == 0)
            snprintf(k->color[2], sizeof k->color[2], "%s", v);
    }
    fclose(f);
    lower(k->character);
    lower(k->model);
    for (i = 0; i < 3; i++) {       /* a sheet named by its stem or its file */
        lower(k->color[i]);
        if (is_picture(k->color[i]))
            *strrchr(k->color[i], '.') = 0;
    }

    g_into_files = &k->files;
    g_into_tex = &k->tex;
    walk(dir, 1, 0);
    g_into_files = &g_files;
    g_into_tex = &g_tex;

    /* the model: the one named, else the folder's only .skin */
    if (!k->model[0] || !map_get(&k->files, k->model)) {
        int n = 0;
        k->model[0] = 0;
        for (i = 0; i < k->files.cap; i++) {
            const char *key = k->files.slot[i].key;
            size_t l = key ? strlen(key) : 0;
            if (l > 5 && strcmp(key + l - 5, ".skin") == 0 && !n++)
                snprintf(k->model, sizeof k->model, "%s", key);
        }
        if (n > 1)
            k->model[0] = 0;        /* which one? the game's name decides */
    }
    skin_guess_colors(k);

    /* the fighter: from the model's name, sonya_standard.skin -> sonya */
    if (!k->character[0] && k->model[0]) {
        const char *u = strchr(k->model, '_'), *d = strchr(k->model, '.');
        size_t l = (size_t)((u && u < d ? u : d) - k->model);
        if (l < sizeof k->character) {
            memcpy(k->character, k->model, l);
            k->character[l] = 0;
        }
    }
    /* what tells two machines' copies of a skin apart: its model's size,
     * or its colour 1's when it has no model (mods_skin_key) */
    {
        char c1[NAME_MAX_];
        const char *sp;
        FILE *sf;
        snprintf(c1, sizeof c1, "%s_diffuse", k->character);
        sp = k->model[0] ? map_get(&k->files, k->model) : NULL;
        sf = sp ? fopen(sp, "rb") : NULL;
        if (sf) {
            fseek(sf, 0, SEEK_END);
            k->sig = ftell(sf);
            fclose(sf);
        }
        sp = skin_tex(k, c1);
        sf = sp ? fopen(sp, "rb") : NULL;
        if (sf) {
            fseek(sf, 0, SEEK_END);
            k->sig2 = ftell(sf);
            fclose(sf);
        }
    }
    g_skins[g_nskins++] = k;
    printf("mods: skin \"%s\" for %s (%s, model %s)\n", k->title,
           k->character[0] ? k->character : "?", folder,
           k->model[0] ? k->model : "the game's");
    return 1;
}

int mods_skin_list(const char *texbase, int *ids, int max)
{
    char want[48];
    int i, n = 0;

    if (!texbase || strlen(texbase) >= sizeof want)
        return 0;
    strcpy(want, texbase);
    lower(want);
    for (i = 0; i < g_nskins && n < max; i++)
        if (strcmp(g_skins[i]->character, want) == 0)
            ids[n++] = i + 1;
    return n;
}

/* An online match: a skin is named to the other machine by its fighter and
 * the sizes of its model and colour 1, "sonya|540648|1411748" -- not by
 * its name, which each player may change; the other shows it when it has
 * a skin that answers to the same three, else the game's. */
void mods_skin_key(int id, char *out, size_t n)
{
    const Skin *k = id >= 1 && id <= g_nskins ? g_skins[id - 1] : NULL;
    if (!k) {
        if (n)
            out[0] = 0;
        return;
    }
    snprintf(out, n, "%s|%ld|%ld", k->character, k->sig, k->sig2);
}

int mods_skin_by_key(const char *key)
{
    char mine[160];
    int i;
    if (!key || !*key)
        return 0;
    for (i = 1; i <= g_nskins; i++) {
        mods_skin_key(i, mine, sizeof mine);
        if (strcmp(mine, key) == 0)
            return i;
    }
    return 0;
}

const char *mods_skin_title(int id)
{
    return id >= 1 && id <= g_nskins ? g_skins[id - 1]->title : "";
}

void mods_skin_prefix(int id, char *out, size_t n)
{
    snprintf(out, n, "@S%d@", id);
}

/* "@S3@X" -> 3 and *rest "X"; 0 for a plain name */
static int virt(const char *name, const char **rest)
{
    const char *p;
    int id = 0;

    if (!name || name[0] != '@' || name[1] != 'S')
        return 0;
    for (p = name + 2; *p >= '0' && *p <= '9' && id < 100000; p++)
        id = id * 10 + (*p - '0');
    if (*p != '@' || p == name + 2)
        return 0;
    *rest = p + 1;
    return id;
}

const char *mods_real_name(const char *name)
{
    const char *base, *rest;
    if (!name)
        return name;
    base = name + strlen(name);
    while (base > name && base[-1] != '/' && base[-1] != '\\')
        base--;
    return virt(base, &rest) ? rest : name;
}

/* A file of skin k by the name the game asks for ("sonya_standard.skin",
 * "....skin.mats", "....skin.bones"): the same name in the folder, else the
 * model under its own name. */
static const char *skin_file(const Skin *k, const char *key)
{
    const char *hit = map_get(&k->files, key), *s;
    char alt[NAME_MAX_ * 2];

    if (hit || !k->model[0])
        return hit;
    s = strstr(key, ".skin");
    if (!s || (s[5] && s[5] != '.'))
        return NULL;
    snprintf(alt, sizeof alt, "%s%s", k->model, s + 5);
    return map_get(&k->files, alt);
}

/* The sheet named `key` ("sonya_diffuse2_lite", lower case) in skin k. */
static const char *skin_tex(const Skin *k, const char *key)
{
    static const char *const conv[3] = { "_diffuse", "_diffuse2", "_diffuse_ice" };
    const char *d = strstr(key, "_diffuse"), *hit, *suf;
    char base[NAME_MAX_], name[NAME_MAX_ + 16];
    int which = -1;
    size_t bl;

    if (!d)
        return map_get(&k->tex, key);   /* a part's sheet (.mats) and others */
    while (strstr(d + 1, "_diffuse"))
        d = strstr(d + 1, "_diffuse");
    suf = d + 8;
    if (!*suf || !strcmp(suf, "_low") || !strcmp(suf, "_lite"))
        which = 0;
    else if (!strcmp(suf, "2") || !strcmp(suf, "2_low") || !strcmp(suf, "2_lite")
             || !strcmp(suf, "_lite2") || !strcmp(suf, "_low2"))
        which = 1;
    else if (!strcmp(suf, "_ice"))
        which = 2;
    bl = (size_t)(d - key);
    if (which < 0 || bl >= sizeof base)
        return map_get(&k->tex, key);
    memcpy(base, key, bl);
    base[bl] = 0;

    /* the game's name first -- what the launcher writes when a sheet is
     * replaced, so the newest wins -- then the one mod.ini names */
    if ((hit = map_get(&k->tex, key)) != NULL)          /* the size asked for */
        return hit;
    snprintf(name, sizeof name, "%s%s", base, conv[which]);
    if ((hit = map_get(&k->tex, name)) != NULL)
        return hit;
    if (k->color[which][0] && (hit = map_get(&k->tex, k->color[which])) != NULL)
        return hit;
    if (which == 0)
        return NULL;
    /* colour 2 and ice the skin does not have: its colour 1, not the
     * game's sheet, which is laid out for the game's model */
    if (k->color[0][0] && (hit = map_get(&k->tex, k->color[0])) != NULL)
        return hit;
    snprintf(name, sizeof name, "%s%s", base, conv[0]);
    return map_get(&k->tex, name);
}

static int skin_find(const char *name, int tex, char *out, size_t n)
{
    char key[NAME_MAX_];
    const char *rest, *hit;
    int id = virt(name, &rest);

    if (id < 1 || id > g_nskins || strlen(rest) >= sizeof key)
        return 0;
    strcpy(key, rest);
    lower(key);
    hit = tex ? skin_tex(g_skins[id - 1], key) : skin_file(g_skins[id - 1], key);
    if (!hit)
        return 0;
    snprintf(out, n, "%s", hit);
    return 1;
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
    skins_free();
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
        if (skin_take(path, named[i] + 1))
            continue;                   /* a skin that adds, not replaces */
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
        if (skin_take(path, dirs[i]))
            continue;
        walk(path, 1, 0);
        if (g_mods_on < MOD_MAX)
            snprintf(g_on[g_mods_on++], PATH_MAX_, "%s", path);
    }
    if (g_mods_on)
        printf("mods: %d on, %d files\n", g_mods_on, (int)g_files.used);
    if (g_nskins)
        printf("mods: %d added skins\n", g_nskins);
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
    if ((!g_files.used && !g_nskins) || !rel)
        return 0;
    base = rel + strlen(rel);
    while (base > rel && base[-1] != '/' && base[-1] != '\\')
        base--;
    if (skin_find(base, 0, out, n))
        return 1;
    base = mods_real_name(base);        /* not in the skin: the game's */
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
    if ((!g_tex.used && !g_nskins) || !stem)
        return 0;
    base = stem + strlen(stem);
    while (base > stem && base[-1] != '/' && base[-1] != '\\')
        base--;
    if (skin_find(base, 1, out, n))
        return 1;
    base = mods_real_name(base);
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
