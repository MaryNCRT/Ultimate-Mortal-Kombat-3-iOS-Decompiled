/*
 * neural_mod.c -- the DLSS 5 layer, as a mod. Port, not in the binary.
 * docs/DLSS5-MOD.md is the player's side of it.
 *
 * A folder in mods\ whose mod.ini says `type=dlss5` holds a community DLSS 5
 * chain: ReShade (with add-on support), the DLSS5-Feeder add-on and its
 * shader, a neural consumer add-on, and NVIDIA's two runtimes, which the
 * player supplies. When that mod is on and the game draws with Direct3D 12
 * on an NVIDIA card, ReShade is loaded from the mod's folder before the
 * device is made, so it hooks the device the game makes next. Nothing is
 * copied beside the exe:
 *
 *   - RESHADE_BASE_PATH_OVERRIDE points ReShade at the mod's folder: its
 *     ReShade.ini, its log, its add-ons (AddonPath .\) and shaders are there,
 *     and a player's other ReShade add-ons are never picked up;
 *   - the folder is added to the DLL search path, so the add-ons find
 *     nvngx_dlss.dll and nvngx_dlssnr.dll in it;
 *   - those two are loaded only with a valid Authenticode signature and
 *     NVIDIA as their company (version resource);
 *   - a ReShade of the player's own beside the exe (dxgi.dll, d3d12.dll or
 *     ReShade.ini there) is a conflict: the mod is left out and the log
 *     says why.
 *
 * The mod's ReShade.ini and preset are written when missing and never
 * overwritten. What happens after the load is the community components'
 * business; their logs are ReShade.log in the mod's folder and
 * dlss5-feed.log beside the exe. The whole frame is processed, the HUD
 * included (the feeder's own behaviour) -- a HUD drawn after the neural pass
 * is later work (docs/research/GRAPHICS-OVERHAUL.md section 4.5).
 *
 * The game's own data, rules and online sync are not touched: this is
 * drawing only. umk3.ini dlss5_mod=0 keeps it off whatever the mods say.
 */
#include "neural_mod.h"

#include <stdio.h>
#include <string.h>

int neural_mod_enabled = 1;             /* umk3.ini dlss5_mod= */
static int g_loaded;
static char g_state[160] = "off";

int neural_mod_loaded(void) { return g_loaded; }
const char *neural_mod_state(void) { return g_state; }

#ifndef _WIN32

int neural_mod_load(const char *game_dir, int renderer_is_d3d12)
{
    (void)game_dir;
    (void)renderer_is_d3d12;
    return 0;                           /* Windows and Direct3D 12 only */
}

#else

#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <dxgi.h>

#include "mods.h"

/* IID_IDXGIFactory1, kept here so this file defines no GUIDs of its own
 * that rgl_d3d12.c (INITGUID) defines too */
static const GUID k_iid_factory1 =
    { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };

#define SAY(...) do { printf("dlss5 mod: " __VA_ARGS__); printf("\n"); } while (0)

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
        SAY("%s is missing from the mod's folder (you supply it; docs/DLSS5-MOD.md)", name);
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

/* the mod's ReShade.ini and preset, when the mod brings none */
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

static const char k_reshade_ini[] =
    "; Written by UMK3 PC for the DLSS 5 mod (neural_mod.c). Edit freely: the\n"
    "; game never overwrites this file. Paths are relative to this folder.\n"
    "[INSTALL]\n"
    "Logging=1\n"
    "\n"
    "[ADDON]\n"
    "AddonPath=.\\\n"
    "\n"
    "[GENERAL]\n"
    "EffectSearchPaths=.\\reshade-shaders\\Shaders\\**\n"
    "TextureSearchPaths=.\\reshade-shaders\\Textures\\**\n"
    "PresetPath=.\\DLSS5-preset.ini\n"
    "PreprocessorDefinitions=DLSS5_MV_PROVIDER=5\n"
    "\n"
    "[OVERLAY]\n"
    "TutorialProgress=4\n";

static const char k_preset_ini[] =
    "Techniques=DLSS5_Feed@DLSS5_Feed.fx\n"
    "TechniqueSorting=DLSS5_Feed@DLSS5_Feed.fx\n";

int neural_mod_load(const char *game_dir, int renderer_is_d3d12)
{
    char dir[1100], gpu[128], consumer[260], p[1200];
    WCHAR wdir[1100], wdll[1200];
    HMODULE rs;
    int ok = 1;

    if (!neural_mod_enabled || !mods_find_typed(game_dir, "dlss5", dir, sizeof dir))
        return 0;                       /* no such mod on: nothing at all */
    for (char *c = dir; *c; c++)
        if (*c == '/')
            *c = '\\';
    SAY("mod at %s", dir);
    snprintf(g_state, sizeof g_state, "not loaded (see the log)");

    if (!renderer_is_d3d12) {
        SAY("needs RENDERER Direct3D 12 (renderer=d3d12); not loaded");
        snprintf(g_state, sizeof g_state, "needs Direct3D 12");
        return 0;
    }
    if (!nvidia_gpu(gpu, sizeof gpu)) {
        SAY("needs an NVIDIA card (the neural model runs on RTX 50); not loaded");
        snprintf(g_state, sizeof g_state, "needs an NVIDIA card");
        return 0;
    }
    SAY("card: %s", gpu);
    if (GetModuleHandleA("ReShade64.dll") || exists(game_dir, "dxgi.dll")
        || exists(game_dir, "d3d12.dll") || exists(game_dir, "ReShade.ini")) {
        SAY("a ReShade of your own is beside the game (dxgi.dll, d3d12.dll or ReShade.ini): "
            "remove it or turn this mod off; not loaded");
        snprintf(g_state, sizeof g_state, "another ReShade is installed");
        return 0;
    }

    /* what the chain is made of */
    if (!exists(dir, "ReShade64.dll")) {
        SAY("ReShade64.dll is missing (ReShade with add-on support)");
        ok = 0;
    }
    if (!exists(dir, "dlss5-feed.addon64")) {
        SAY("dlss5-feed.addon64 is missing (DLSS5-Feeder)");
        ok = 0;
    }
    if (exists(dir, "dlss5-feed-helper.addon64")) {
        /* ReShade loads every .addon64 in the folder: the helper (32-bit
         * games' mode) beside the add-on would be two feeders at once */
        SAY("dlss5-feed-helper.addon64 is for 32-bit games: take it out of the mod's folder");
        ok = 0;
    }
    if (!exists(dir, "reshade-shaders\\Shaders\\DLSS5_Feed.fx")) {
        SAY("reshade-shaders\\Shaders\\DLSS5_Feed.fx is missing (DLSS5-Feeder)");
        ok = 0;
    }
    if (!exists(dir, "reshade-shaders\\Shaders\\ReShade.fxh")) {
        SAY("reshade-shaders\\Shaders\\ReShade.fxh is missing (ReShade's shaders)");
        ok = 0;
    }
    if (match(dir, "renodx-dlss5*.addon64", consumer, sizeof consumer)
        || match(dir, "deep-fried-chicken*.addon64", consumer, sizeof consumer))
        SAY("neural consumer: %s", consumer);
    else {
        SAY("no neural consumer add-on (renodx-dlss5*.addon64 or deep-fried-chicken.addon64)");
        ok = 0;
    }
    ok &= nvidia_dll(dir, "nvngx_dlss.dll");
    ok &= nvidia_dll(dir, "nvngx_dlssnr.dll");
    if (!ok) {
        snprintf(g_state, sizeof g_state, "files missing or refused (see the log)");
        return 0;
    }

    write_if_missing(dir, "ReShade.ini", k_reshade_ini);
    write_if_missing(dir, "DLSS5-preset.ini", k_preset_ini);

    /* ReShade's folder is the mod's; the runtimes are found in it */
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
    snprintf(g_state, sizeof g_state, "loaded");
    SAY("ReShade loaded from the mod; its log: %s\\ReShade.log, the feeder's: dlss5-feed.log", dir);
    return 1;
}

#endif
