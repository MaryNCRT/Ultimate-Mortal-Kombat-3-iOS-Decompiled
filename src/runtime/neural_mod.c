/*
 * neural_mod.c -- DLSS 5 (community neural rendering) inside the game. Port,
 * not in the binary. docs/DLSS5-MOD.md is the player's side of it.
 *
 * The chain: ReShade (our own build, tools/neural/build_reshade.py), the
 * DLSS5 ReShade AIO add-on (standalone-dlssnr, Apache-2.0: NVIDIA's neural
 * rendering and DLSS in one, with its caller bridge nvngx.dll) and its guide
 * shader DLSS5_AIO_Feed.fx -- ours, made from its own (launcher/neural/) to
 * read the game's layers -- and NVIDIA's two runtimes. The game ships all
 * but NVIDIA's in graphics\dlss5\ (launcher/make_release.py,
 * tools/neural/stage_dlss5.py); the player adds nvngx_dlss.dll and
 * nvngx_dlssnr.dll (the launcher's VIDEO > GRAPHICS > DLSS 5 imports them),
 * which nobody here may redistribute. GRAPHICS > DLSS 5 (umk3.ini dlss5=1)
 * turns it on; a mod whose mod.ini says type=dlss5, the way it began, still
 * works when the setting is off.
 *
 * The layers: with the chain loaded, rgl_layers (render/rgl.h) makes every
 * Direct3D 12 draw leave a code in the picture's alpha -- 0 the HUD, menus
 * and 2D, 0.5 a fighter, 1 the stage -- and DLSS5_AIO_Feed.fx turns it into
 * the neural pass's control mask: the HUD and menus bypass it, the fighters
 * and the stage get dlss5_fighters= and dlss5_stage= of it (umk3.ini).
 *
 * When it is on and the game draws with Direct3D 12 on an NVIDIA card,
 * ReShade is loaded from that folder before the device is made, so it hooks
 * the device the game makes next. Nothing is copied beside the exe:
 *
 *   - RESHADE_BASE_PATH_OVERRIDE points ReShade at the folder: its
 *     ReShade.ini, its log, its add-ons (AddonPath .\) and shaders are
 *     there, and a player's other ReShade add-ons are never picked up;
 *   - the folder is on the DLL search path; the AIO add-on looks for
 *     nvngx_dlss.dll and nvngx_dlssnr.dll beside itself;
 *   - those two are loaded only with a valid Authenticode signature and
 *     NVIDIA as their company (version resource);
 *   - a ReShade of the player's own beside the exe (dxgi.dll, d3d12.dll or
 *     ReShade.ini there) is a conflict: nothing is loaded, the log says why.
 *
 * ReShade.ini and the preset are written when missing; afterwards the game
 * sets only two lines of it (set_key). The DLSS5-Feeder chain of the first
 * builds is set aside when found (migrate). The game's data, rules and
 * online sync are not touched: this is drawing only.
 */
#include "neural_mod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int neural_mod_enabled = 1;             /* umk3.ini dlss5_mod= */
int neural_dlss5;                       /* umk3.ini dlss5= (GRAPHICS > DLSS 5) */
float neural_stage = 1.0f, neural_fighters = 1.0f;   /* umk3.ini dlss5_stage= / dlss5_fighters= */
int neural_layers = 1;                  /* umk3.ini dlss5_layers= (menus and HUD left out) */

/* GRAPHICS > DLSS 5 > PRESET (umk3.ini dlss5_preset=): the neural model --
 * NVIDIA's three networks, DLSSNR.Style 0..2, AIO's Model 1..3 -- and its
 * intensity, local tone and local structure (AIO's 0..2, 1 neutral). The
 * names say what each aims at; which network looks how is Mary's to judge
 * in the game. */
const neural_preset neural_presets[NEURAL_PRESETS] = {
    { "realistic", "REALISTA",        "REALISTIC",  1, 1.00f, 1.00f, 1.00f },
    { "cinematic", "CINEMATOGRAFICO", "CINEMATIC",  2, 1.00f, 1.25f, 0.85f },
    { "vivid",     "INTENSO",         "VIVID",      3, 1.20f, 1.10f, 1.25f },
    { "subtle",    "SUAVE",           "SUBTLE",     1, 0.60f, 0.90f, 0.80f },
};
int neural_preset_index;

int neural_preset_parse(const char *v)
{
    int i;
    for (i = 0; i < NEURAL_PRESETS; i++)
        if (strcmp(v, neural_presets[i].key) == 0)
            return i;
    return 0;
}
static int g_loaded;
static char g_state[160] = "off";

int neural_mod_loaded(void) { return g_loaded; }
const char *neural_mod_state(void) { return g_state; }

const char *neural_status_name(int st)
{
    switch (st) {
    case NEURAL_ACTIVE:          return "active";
    case NEURAL_READY:           return "ready";
    case NEURAL_NEED_D3D12:      return "needs Direct3D 12";
    case NEURAL_NEED_NVIDIA_GPU: return "needs an NVIDIA card";
    case NEURAL_CONFLICT:        return "another ReShade is installed";
    case NEURAL_NOT_INSTALLED:   return "not installed";
    case NEURAL_NEED_FILES:      return "needs NVIDIA's files";
    case NEURAL_READY_DLAA:      return "ready, DLSS (DLAA) alone: no neural rendering files";
    }
    return "?";
}

#ifndef _WIN32

int neural_mod_load(const char *game_dir, int renderer_is_d3d12)
{
    (void)game_dir;
    (void)renderer_is_d3d12;
    return 0;                           /* Windows and Direct3D 12 only */
}

int neural_dlss5_status(const char *game_dir, int renderer_is_d3d12)
{
    (void)game_dir;
    (void)renderer_is_d3d12;
    return NEURAL_NEED_D3D12;
}

#else

#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <dxgi.h>

#include "mods.h"
#include "render/rgl.h"

/* IID_IDXGIFactory1, kept here so this file defines no GUIDs of its own
 * that rgl_d3d12.c (INITGUID) defines too */
static const GUID k_iid_factory1 =
    { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };

/* the reasons go to the log, except when the menu or the launcher asks */
static int g_quiet;
#define SAY(...) do { if (!g_quiet) { printf("dlss5: " __VA_ARGS__); printf("\n"); } } while (0)

static int exists(const char *dir, const char *rel)
{
    char p[1200];
    snprintf(p, sizeof p, "%s\\%s", dir, rel);
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

/* a file matching `pattern` in `dir`, its name in `out` */
static int match(const char *dir, const char *pattern, char *out, size_t n)
{
    char p[1200];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(p, sizeof p, "%s\\%s", dir, pattern);
    h = FindFirstFileA(p, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    snprintf(out, n, "%s", fd.cFileName);
    FindClose(h);
    return 1;
}

/* an Authenticode signature Windows trusts (wintrust.dll, loaded here so
 * the game links nothing new) */
static int signed_ok(const char *path)
{
    LONG (WINAPI *verify)(HWND, GUID *, LPVOID);
    HMODULE wt = LoadLibraryA("wintrust.dll");
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_FILE_INFO fi;
    WINTRUST_DATA wd;
    WCHAR wpath[1200];
    LONG r;

    if (!wt)
        return 0;
    verify = (LONG (WINAPI *)(HWND, GUID *, LPVOID))(void *)GetProcAddress(wt, "WinVerifyTrust");
    if (!verify)
        return 0;
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, 1200);
    memset(&fi, 0, sizeof fi);
    fi.cbStruct = sizeof fi;
    fi.pcwszFilePath = wpath;
    memset(&wd, 0, sizeof wd);
    wd.cbStruct = sizeof wd;
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    r = verify(NULL, &action, &wd);
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    verify(NULL, &action, &wd);
    return r == 0;
}

/* the version resource's CompanyName and FileVersion (version.dll) */
static void file_info(const char *path, char *company, size_t cn, char *ver, size_t vn)
{
    DWORD (WINAPI *size)(LPCSTR, LPDWORD);
    BOOL (WINAPI *get)(LPCSTR, DWORD, DWORD, LPVOID);
    BOOL (WINAPI *query)(LPCVOID, LPCSTR, LPVOID *, PUINT);
    HMODULE v = LoadLibraryA("version.dll");
    DWORD h, n;
    static unsigned char buf[65536];

    company[0] = ver[0] = 0;
    if (!v)
        return;
    size = (DWORD (WINAPI *)(LPCSTR, LPDWORD))(void *)GetProcAddress(v, "GetFileVersionInfoSizeA");
    get = (BOOL (WINAPI *)(LPCSTR, DWORD, DWORD, LPVOID))(void *)GetProcAddress(v, "GetFileVersionInfoA");
    query = (BOOL (WINAPI *)(LPCVOID, LPCSTR, LPVOID *, PUINT))(void *)GetProcAddress(v, "VerQueryValueA");
    if (!size || !get || !query)
        return;
    n = size(path, &h);
    if (!n || n > sizeof buf || !get(path, 0, n, buf))
        return;
    {
        struct { WORD lang, cp; } *tr;
        UINT len;
        char key[80];
        void *val;
        if (!query(buf, "\\VarFileInfo\\Translation", (LPVOID *)&tr, &len) || len < 4)
            return;
        snprintf(key, sizeof key, "\\StringFileInfo\\%04x%04x\\CompanyName", tr->lang, tr->cp);
        if (query(buf, key, &val, &len) && len)
            snprintf(company, cn, "%s", (const char *)val);
        snprintf(key, sizeof key, "\\StringFileInfo\\%04x%04x\\FileVersion", tr->lang, tr->cp);
        if (query(buf, key, &val, &len) && len)
            snprintf(ver, vn, "%s", (const char *)val);
    }
}

/* one of NVIDIA's runtimes: present, signed, NVIDIA's */
static int nvidia_dll(const char *dir, const char *name)
{
    char p[1200], co[128], ver[64];
    snprintf(p, sizeof p, "%s\\%s", dir, name);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) {
        SAY("%s is missing (you supply it: the launcher's VIDEO > DLSS 5)", name);
        return 0;
    }
    file_info(p, co, sizeof co, ver, sizeof ver);
    if (!strstr(co, "NVIDIA")) {
        SAY("%s is not NVIDIA's (company \"%s\"): not loaded", name, co);
        return 0;
    }
    if (!signed_ok(p)) {
        SAY("%s has no valid signature: not loaded", name);
        return 0;
    }
    SAY("%s %s, signed, %s", name, ver[0] ? ver : "(no version)", co);
    return 1;
}

/* an NVIDIA card among the adapters (vendor 0x10DE) */
static int nvidia_gpu(char *name, size_t n)
{
    HMODULE dx = LoadLibraryA("dxgi.dll");
    HRESULT (WINAPI *mk)(REFIID, void **) =
        dx ? (HRESULT (WINAPI *)(REFIID, void **))(void *)GetProcAddress(dx, "CreateDXGIFactory1") : NULL;
    IDXGIFactory1 *f = NULL;
    IDXGIAdapter1 *ad;
    UINT i;
    int hit = 0;

    if (!mk || FAILED(mk(&k_iid_factory1, (void **)&f)))
        return 0;
    for (i = 0; !hit && IDXGIFactory1_EnumAdapters1(f, i, &ad) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        IDXGIAdapter1_GetDesc1(ad, &d);
        if (d.VendorId == 0x10DE && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, (int)n, NULL, NULL);
            hit = 1;
        }
        IDXGIAdapter1_Release(ad);
    }
    IDXGIFactory1_Release(f);
    return hit;
}

/* ReShade.ini and the preset, when the folder has none */
static void write_if_missing(const char *dir, const char *name, const char *text)
{
    char p[1200];
    FILE *f;
    snprintf(p, sizeof p, "%s\\%s", dir, name);
    if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES)
        return;
    if ((f = fopen(p, "w")) != NULL) {
        fputs(text, f);
        fclose(f);
        SAY("wrote %s", name);
    }
}

/* ReShade.ini for the DLSS5 ReShade AIO chain. The game writes it when
 * missing (or when it is the DLSS5-Feeder era's), and afterwards keeps only
 * two of its lines up to date (set_key, neural_mod_load): the neural pass on
 * or off as nvngx_dlssnr.dll is there, and the layers' strengths. */
static const char k_reshade_ini[] =
    "; Written by UMK3 PC for DLSS 5 (neural_mod.c). Edit freely, except\n"
    "; [Standalone.DLSSNR] NeuralRendering and [GENERAL] PreprocessorDefinitions,\n"
    "; which the game sets at every start. Paths are relative to this folder.\n"
    "[INSTALL]\n"
    "Logging=1\n"
    "\n"
    "[ADDON]\n"
    "AddonPath=.\\\n"
    "DisabledAddons=DLSS 5 Feed 2.0.0@dlss5-feed.addon64\n"
    "\n"
    "[GENERAL]\n"
    "EffectSearchPaths=.\\reshade-shaders\\Shaders\\**\n"
    "TextureSearchPaths=.\\reshade-shaders\\Textures\\**\n"
    "PresetPath=.\\DLSS5-preset.ini\n"
    "PreprocessorDefinitions=RESHADE_DEPTH_INPUT_IS_REVERSED=0,UMK3_NR_STAGE=1.0,UMK3_NR_FIGHTER=1.0\n"
    "\n"
    "[INPUT]\n"
    "KeyOverlay=0,0,0,0\n"
    "KeyScreenshot=0,0,0,0\n"
    "KeyEffects=0,0,0,0\n"
    "KeyReload=0,0,0,0\n"
    "KeyNextPreset=0,0,0,0\n"
    "KeyPreviousPreset=0,0,0,0\n"
    "KeyFPS=0,0,0,0\n"
    "\n"
    "[OVERLAY]\n"
    "TutorialProgress=4\n"
    "ShowClock=0\n"
    "ShowFPS=0\n"
    "ShowScreenshotMessage=0\n"
    "\n"
    /* the AIO addon (standalone-dlssnr): its guide pass -- VORT's motion
     * (vort_Motion.fx) and our DLSS5_AIO_Feed.fx with the game's layers --
     * runs only with VortGuides, and the layers' mask only with
     * NrRejectionMask; no frame generation and no counters on the picture.
     * The game sets these at every start (managed, neural_mod_load). */
    "[Standalone.DLSSNR]\n"
    "Enabled=1\n"
    "NeuralRendering=1\n"
    "NrRejectionMask=1\n"
    "NrRejectionStrength=1.0\n"
    "VortGuides=1\n"
    "NvidiaOpticalFlowMotion=0\n"
    "NvidiaOpticalFlowDepth=0\n"
    "FrameGeneration=0\n"
    "ShowProxyFps=0\n"
    "PerformanceTelemetry=0\n";

/* the addon runs DLSS5_AIO_Feed itself at Present: nothing on in the list */
static const char k_preset_ini[] =
    "Techniques=\n"
    "TechniqueSorting=DLSS5_AIO_Feed@DLSS5_AIO_Feed.fx\n";

static void slashes(char *s)
{
    for (; *s; s++)
        if (*s == '/')
            *s = '\\';
}

/* one key of an ini file set: replaced in its section, else added to it,
 * else the section added at the end */
static void set_key(const char *path, const char *section, const char *key, const char *value)
{
    static char text[65536], out[65536 + 512];
    char head[96];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0, kl = strlen(key), o = 0;
    char *line, *next;
    int in = 0, done = 0;

    if (f)
        fclose(f);
    text[n] = 0;
    snprintf(head, sizeof head, "[%s]", section);
    for (line = text; *line; line = next) {
        size_t len;
        next = strchr(line, '\n');
        next = next ? next + 1 : line + strlen(line);
        len = (size_t)(next - line);
        if (line[0] == '[') {
            if (in && !done) {
                o += (size_t)snprintf(out + o, sizeof out - o, "%s=%s\n", key, value);
                done = 1;
            }
            in = strncmp(line, head, strlen(head)) == 0;
        } else if (in && !done && strncmp(line, key, kl) == 0 && line[kl] == '=') {
            o += (size_t)snprintf(out + o, sizeof out - o, "%s=%s\n", key, value);
            done = 1;
            continue;
        }
        if (o + len < sizeof out) {
            memcpy(out + o, line, len);
            o += len;
        }
    }
    if (!done) {
        if (!in)
            o += (size_t)snprintf(out + o, sizeof out - o, "%s%s\n", o && out[o - 1] != '\n' ? "\n" : "", head);
        o += (size_t)snprintf(out + o, sizeof out - o, "%s=%s\n", key, value);
    }
    if ((f = fopen(path, "wb")) != NULL) {
        fwrite(out, 1, o, f);
        fclose(f);
    }
}

/* the DLSS5-Feeder era's files (before AIO), set aside: its add-on and its
 * shader (which ReShade would load and compile, 25 s, for nothing), and its
 * ReShade.ini and preset, which say nothing of AIO */
static void migrate(const char *dir)
{
    static const char *const old[] = { "dlss5-feed.addon64", "reshade-shaders\\Shaders\\DLSS5_Feed.fx" };
    char a[1200], b[1200], line[256];
    FILE *f;
    int i, aio = 0;

    for (i = 0; i < 2; i++) {
        snprintf(a, sizeof a, "%s\\%s", dir, old[i]);
        snprintf(b, sizeof b, "%s.off", a);
        if (GetFileAttributesA(a) != INVALID_FILE_ATTRIBUTES && MoveFileExA(a, b, MOVEFILE_REPLACE_EXISTING))
            SAY("set aside %s (the DLSS5-Feeder chain; AIO replaces it)", old[i]);
    }
    snprintf(a, sizeof a, "%s\\ReShade.ini", dir);
    if ((f = fopen(a, "r")) == NULL)
        return;
    while (fgets(line, sizeof line, f))
        if (strncmp(line, "[Standalone.DLSSNR]", 19) == 0)
            aio = 1;
    fclose(f);
    if (!aio) {
        static const char *const ini[] = { "ReShade.ini", "DLSS5-preset.ini" };
        for (i = 0; i < 2; i++) {
            snprintf(a, sizeof a, "%s\\%s", dir, ini[i]);
            snprintf(b, sizeof b, "%s.old", a);
            MoveFileExA(a, b, MOVEFILE_REPLACE_EXISTING);
        }
        SAY("the DLSS5-Feeder era's ReShade.ini and preset kept as .old; written again for AIO");
    }
}

/* what stands in the way, without loading anything: NEURAL_* */
static int check(const char *game_dir, const char *dir, int renderer_is_d3d12)
{
    static const char *const ours[] = {
        "ReShade64.dll",                                    /* ReShade, our build */
        "standalone-dlssnr.addon64",                        /* DLSS5 ReShade AIO */
        "nvngx.dll",                                        /* its caller bridge (not NVIDIA's) */
        "reshade-shaders\\Shaders\\DLSS5_AIO_Feed.fx",      /* its guides, ours (the layers) */
        "reshade-shaders\\Shaders\\StandaloneBoundary.fx",
        "reshade-shaders\\Shaders\\vort_Motion.fx",     /* VORT's motion (MIT) */
        "reshade-shaders\\Shaders\\Includes\\vort_MotionVectors.fxh",
        "reshade-shaders\\Shaders\\ReShade.fxh",
    };
    char gpu[128];
    int ok = 1, i;
    int probe = getenv("UMK3_DLSS5_PROBE") && atoi(getenv("UMK3_DLSS5_PROBE"));

    if (!renderer_is_d3d12) {
        SAY("needs RENDERER Direct3D 12 (renderer=d3d12); not loaded");
        return NEURAL_NEED_D3D12;
    }
    if (!nvidia_gpu(gpu, sizeof gpu)) {
        SAY("needs an NVIDIA card (the neural model runs on RTX 50); not loaded");
        return NEURAL_NEED_NVIDIA_GPU;
    }
    SAY("card: %s", gpu);
    if (GetModuleHandleA("ReShade64.dll") || exists(game_dir, "dxgi.dll")
        || exists(game_dir, "d3d12.dll") || exists(game_dir, "ReShade.ini")) {
        SAY("a ReShade of your own is beside the game (dxgi.dll, d3d12.dll or ReShade.ini): "
            "remove it to use DLSS 5; not loaded");
        return NEURAL_CONFLICT;
    }
    for (i = 0; i < (int)(sizeof ours / sizeof ours[0]); i++)
        if (!exists(dir, ours[i])) {
            SAY("%s is missing from %s (the launcher's compile puts it there)", ours[i], dir);
            ok = 0;
        }
    if (!ok)
        return NEURAL_NOT_INSTALLED;

    /* what the player supplies: nvngx_dlss.dll is DLSS (DLAA alone without
     * the neural model); nvngx_dlssnr.dll the neural rendering */
    if (!nvidia_dll(dir, "nvngx_dlss.dll")) {
        if (probe) {
            SAY("UMK3_DLSS5_PROBE: loading without NVIDIA's files (the hooks alone)");
            return NEURAL_READY;
        }
        return NEURAL_NEED_FILES;
    }
    if (!nvidia_dll(dir, "nvngx_dlssnr.dll")) {
        SAY("DLSS (DLAA) alone: the neural rendering needs nvngx_dlssnr.dll");
        return NEURAL_READY_DLAA;
    }
    return NEURAL_READY;
}

int neural_dlss5_status(const char *game_dir, int renderer_is_d3d12)
{
    char dir[1100];
    int st;

    if (g_loaded)
        return NEURAL_ACTIVE;
    snprintf(dir, sizeof dir, "%sgraphics\\dlss5", game_dir);
    slashes(dir);
    g_quiet = 1;
    st = check(game_dir, dir, renderer_is_d3d12);
    g_quiet = 0;
    return st;
}

int neural_mod_load(const char *game_dir, int renderer_is_d3d12)
{
    char dir[1100], p[1200], v[160];
    WCHAR wdir[1100], wdll[1200];
    HMODULE rs;
    int is_mod = 0, st;

    /* the game's own folder when GRAPHICS > DLSS 5 is on, else a mod with
     * type=dlss5 (the way it began); neither: nothing at all */
    if (neural_dlss5)
        snprintf(dir, sizeof dir, "%sgraphics\\dlss5", game_dir);
    else if (neural_mod_enabled && mods_find_typed(game_dir, "dlss5", dir, sizeof dir))
        is_mod = 1;
    else
        return 0;
    slashes(dir);
    SAY("%s, from %s", is_mod ? "the DLSS 5 mod" : "DLSS 5 on (GRAPHICS > DLSS 5)", dir);
    snprintf(g_state, sizeof g_state, "not loaded (see the log)");
    migrate(dir);
    st = check(game_dir, dir, renderer_is_d3d12);
    if (st != NEURAL_READY && st != NEURAL_READY_DLAA) {
        snprintf(g_state, sizeof g_state, "%s", neural_status_name(st));
        return 0;
    }

    write_if_missing(dir, "ReShade.ini", k_reshade_ini);
    write_if_missing(dir, "DLSS5-preset.ini", k_preset_ini);
    snprintf(p, sizeof p, "%s\\ReShade.ini", dir);
    {
        /* what the game owns of AIO's settings: the pipeline the layers need,
         * and GRAPHICS > DLSS 5's preset (umk3.ini dlss5_preset=): the
         * neural model (DLSSNR.Style 0..2 is AIO's Model 1..3) and its
         * strengths */
        /* the layers (dlss5_layers=1, experimental): VORT's motion and the
         * layers' mask. Off (the default, what Mary saw working): AIO's own
         * NVIDIA optical flow, no mask -- the whole picture processed. With
         * the layers on, Mary saw no neural effect on 2026-11-22 although
         * the logs had the mask bound; kept as an option to come back to */
        static const char *const fixed[][2] = {
            { "Enabled", "1" }, { "FrameGeneration", "0" }, { "ShowProxyFps", "0" },
            { "NrRejectionStrength", "1.0" }, { "NvidiaOpticalFlowDepth", "0" },
        };
        set_key(p, "Standalone.DLSSNR", "VortGuides", neural_layers ? "1" : "0");
        set_key(p, "Standalone.DLSSNR", "NrRejectionMask", neural_layers ? "1" : "0");
        set_key(p, "Standalone.DLSSNR", "NvidiaOpticalFlowMotion", neural_layers ? "0" : "1");
        const neural_preset *np = &neural_presets[neural_preset_index];
        char num[16];
        int k;
        for (k = 0; k < (int)(sizeof fixed / sizeof fixed[0]); k++)
            set_key(p, "Standalone.DLSSNR", fixed[k][0], fixed[k][1]);
        set_key(p, "Standalone.DLSSNR", "NeuralRendering", st == NEURAL_READY ? "1" : "0");
        snprintf(num, sizeof num, "%d", np->model);
        set_key(p, "Standalone.DLSSNR", "Model", num);
        snprintf(num, sizeof num, "%.2f", np->intensity);
        set_key(p, "Standalone.DLSSNR", "Intensity", num);
        snprintf(num, sizeof num, "%.2f", np->tone);
        set_key(p, "Standalone.DLSSNR", "LocalTone", num);
        snprintf(num, sizeof num, "%.2f", np->structure);
        set_key(p, "Standalone.DLSSNR", "LocalStructure", num);
        SAY("preset %s: model %d, intensity %.2f, local tone %.2f, local structure %.2f",
            np->key, np->model, np->intensity, np->tone, np->structure);
    }
    snprintf(v, sizeof v, "RESHADE_DEPTH_INPUT_IS_REVERSED=0,UMK3_NR_STAGE=%.2f,UMK3_NR_FIGHTER=%.2f",
             neural_stage, neural_fighters);
    set_key(p, "GENERAL", "PreprocessorDefinitions", v);
    if (neural_layers)
        SAY("layers (experimental): the stage %.2f, the fighters %.2f, the HUD and menus untouched",
            neural_stage, neural_fighters);
    else
        SAY("layers off: the whole picture processed (dlss5_layers=1 tries them)");

    /* ReShade's folder is this one; the runtimes are found in it */
    MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, 1100);
    SetEnvironmentVariableW(L"RESHADE_BASE_PATH_OVERRIDE", wdir);
    SetDllDirectoryW(wdir);
    snprintf(p, sizeof p, "%s\\ReShade64.dll", dir);
    MultiByteToWideChar(CP_ACP, 0, p, -1, wdll, 1200);
    rs = LoadLibraryW(wdll);
    if (!rs) {
        SAY("ReShade64.dll did not load (error %lu); see %s\\ReShade.log",
            (unsigned long)GetLastError(), dir);
        snprintf(g_state, sizeof g_state, "ReShade did not load");
        SetDllDirectoryW(NULL);
        return 0;
    }
    g_loaded = 1;
    rgl_layers = neural_layers;         /* the layer codes in the picture's alpha (render/rgl.h) */
    snprintf(g_state, sizeof g_state, "loaded");
    SAY("ReShade loaded; logs: %s\\ReShade.log and LOCALAPPDATA\\RHI\\Logs\\standalone-dlssnr.log", dir);
    return 1;
}

#endif
