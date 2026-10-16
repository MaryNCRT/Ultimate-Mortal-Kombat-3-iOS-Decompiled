/*
 * Win32 + WGL backend.
 *
 * Chosen for the vertical slice because it needs nothing installed and because
 * the engine is OpenGL ES 1.1 fixed function -- glMatrixMode, glVertexPointer,
 * glTexEnvf, glDrawElements -- which desktop GL 1.1 provides directly. The 18
 * calls RenderMesh.cpp makes map almost one to one.
 *
 * That is a deliberate shortcut for a slice, not the shipping plan. A release
 * renderer should target a core profile and emulate the fixed-function bits it
 * needs, because compatibility contexts are not guaranteed everywhere. See
 * docs/LIME-ENGINE.md for the full 77-entry-point surface.
 */
#include "platform.h"
#include "gl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static HWND      g_wnd;
static DEVMODEA  g_mode;         /* see plat_fullscreen_exclusive */
static char      g_mode_dev[32]; /* its monitor's device, "" the main one */
static HDC       g_dc;
static HGLRC     g_rc;
static bool      g_quit;
static LARGE_INTEGER g_freq, g_start;
static bool      g_mouse_down;
static bool      g_focused = true;
static int       g_mouse_x, g_mouse_y;
static unsigned char g_key[256];
/* A key pressed since the game last ticked: a tap shorter than one tick
 * (down and up in the same frame) still reaches the game once. */
static unsigned char g_tap[256];
/* Typed characters, UTF-16 as WM_CHAR gives them, for plat_text_take. */
static wchar_t g_text[64];
static int     g_text_n;

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
    case WM_DESTROY:
        g_quit = true;
        return 0;
    case WM_KEYDOWN:
        if (wp < 256) g_key[wp] = g_tap[wp] = 1;
        return 0;
    case WM_CHAR:                       /* typed text: plat_text_take */
        if (g_text_n < (int)(sizeof g_text / sizeof g_text[0]))
            g_text[g_text_n++] = (wchar_t)wp;
        return 0;
    case WM_KEYUP:
        if (wp < 256) g_key[wp] = 0;
        return 0;
    /* The pointer, which the front end reads as a finger. The position comes
     * from the message rather than from GetCursorPos so that it is already in
     * client coordinates, and the capture stops a drag that leaves the window
     * from silently leaving the button down. */
    case WM_LBUTTONDOWN:
        g_mouse_down = true;
        SetCapture(h);
        g_mouse_x = (short)LOWORD(lp);
        g_mouse_y = (short)HIWORD(lp);
        return 0;
    case WM_LBUTTONUP:
        g_mouse_down = false;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
        g_mouse_x = (short)LOWORD(lp);
        g_mouse_y = (short)HIWORD(lp);
        return 0;
    case WM_ACTIVATE:
        /* LOWORD: WA_INACTIVE (0), WA_ACTIVE or WA_CLICKACTIVE */
        g_focused = LOWORD(wp) != WA_INACTIVE;
        if (g_mode.dmSize) {            /* exclusive fullscreen: see g_mode */
            if (!g_focused) {
                ChangeDisplaySettingsExA(g_mode_dev[0] ? g_mode_dev : NULL, NULL, NULL, 0, NULL);
                ShowWindow(h, SW_MINIMIZE);
            } else {
                ChangeDisplaySettingsExA(g_mode_dev[0] ? g_mode_dev : NULL, &g_mode, NULL,
                                         CDS_FULLSCREEN, NULL);
                ShowWindow(h, SW_RESTORE);
                plat_fullscreen();
            }
        }
        if (!g_focused) {               /* no key stays held across a focus loss */
            memset(g_key, 0, sizeof g_key);
            g_mouse_down = false;
        }
        return 0;
    case WM_SIZE: {
        int w = LOWORD(lp), h2 = HIWORD(lp);
        if (w > 0 && h2 > 0) glViewport(0, 0, w, h2);
        return 0;
    }
    default:
        return DefWindowProcA(h, msg, wp, lp);
    }
}

/* ------------------------------------------------------------ monitors */

static int  g_monitor;                  /* monitor=: 0 the window's */

struct mon_find { int want, at; HMONITOR found; };

static BOOL CALLBACK mon_enum(HMONITOR m, HDC dc, LPRECT r, LPARAM lp)
{
    struct mon_find *f = (struct mon_find *)lp;

    (void)dc;
    (void)r;
    if (++f->at == f->want)
        f->found = m;
    return TRUE;
}

/* Monitor i (1..n), or NULL. */
static HMONITOR mon_nth(int i)
{
    struct mon_find f = { i, 0, NULL };

    EnumDisplayMonitors(NULL, NULL, mon_enum, (LPARAM)&f);
    return f.found;
}

int plat_monitor_count(void)
{
    struct mon_find f = { -1, 0, NULL };

    EnumDisplayMonitors(NULL, NULL, mon_enum, (LPARAM)&f);
    return f.at;
}

void plat_monitor_name(int i, char *out, int n)
{
    HMONITOR m = mon_nth(i);
    MONITORINFOEXA mi;

    if (n <= 0)
        return;
    mi.cbSize = sizeof mi;
    if (!m || !GetMonitorInfoA(m, (MONITORINFO *)&mi)) {
        snprintf(out, (size_t)n, "%d", i);
        return;
    }
    snprintf(out, (size_t)n, "%d: %ldx%ld%s", i,
             mi.rcMonitor.right - mi.rcMonitor.left,
             mi.rcMonitor.bottom - mi.rcMonitor.top,
             (mi.dwFlags & MONITORINFOF_PRIMARY) ? " *" : "");
}

void plat_monitor_list(void)
{
    int i, n = plat_monitor_count();
    char name[64];

    for (i = 1; i <= n; i++) {
        plat_monitor_name(i, name, sizeof name);
        printf("%d\t%s\n", i, name);
    }
}

void plat_monitor_select(int i) { g_monitor = i >= 0 ? i : 0; }
int  plat_monitor(void) { return g_monitor; }

/* Where the game goes: the chosen monitor, else the window's (the main
 * one before there is a window). */
static HMONITOR mon_target(void)
{
    HMONITOR m = g_monitor ? mon_nth(g_monitor) : NULL;
    POINT o = { 0, 0 };

    if (m)
        return m;
    return g_wnd ? MonitorFromWindow(g_wnd, MONITOR_DEFAULTTOPRIMARY)
                 : MonitorFromPoint(o, MONITOR_DEFAULTTOPRIMARY);
}

bool plat_open(const char *title, int width, int height)
{
    HINSTANCE inst = GetModuleHandleA(NULL);

    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "LimeWindow";
    if (!RegisterClassA(&wc)) return false;

    RECT r = { 0, 0, width, height };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    if (plat_open_borderless) {
        /* born borderless and covering the monitor: never shown as a
         * window first (see plat_fullscreen for the extra row) */
        RECT m;
        MONITORINFO mi;

        mi.cbSize = sizeof mi;
        GetMonitorInfoA(mon_target(), &mi);
        m = mi.rcMonitor;
        g_wnd = CreateWindowA("LimeWindow", title, WS_POPUP,
                              m.left, m.top, m.right - m.left,
                              m.bottom - m.top + 1, NULL, NULL, inst, NULL);
    } else {
        /* centred on the chosen monitor; Windows' own place without one */
        int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
        MONITORINFO mi;

        mi.cbSize = sizeof mi;
        if (g_monitor && GetMonitorInfoA(mon_target(), &mi)) {
            x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - (r.right - r.left)) / 2;
            y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - (r.bottom - r.top)) / 2;
            if (x < mi.rcWork.left) x = mi.rcWork.left;
            if (y < mi.rcWork.top) y = mi.rcWork.top;
        }
        g_wnd = CreateWindowA("LimeWindow", title, WS_OVERLAPPEDWINDOW,
                              x, y, r.right - r.left, r.bottom - r.top,
                              NULL, NULL, inst, NULL);
    }
    if (!g_wnd) return false;

    g_dc = GetDC(g_wnd);

    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int fmt = ChoosePixelFormat(g_dc, &pfd);
    if (!fmt || !SetPixelFormat(g_dc, fmt, &pfd)) return false;

    g_rc = wglCreateContext(g_dc);
    if (!g_rc || !wglMakeCurrent(g_dc, g_rc)) return false;

    ShowWindow(g_wnd, SW_SHOW);
    glViewport(0, 0, width, height);

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    return true;
}

int plat_open_borderless;

/* Borderless fullscreen is one row taller than the monitor. A window that
 * covers it exactly is promoted by the driver to exclusive fullscreen, and
 * then every Alt+Tab switches modes and the screen flickers; one row more
 * keeps it an ordinary window the desktop composes. Exclusive fullscreen
 * (g_mode set) wants the exact size. */
void plat_fullscreen(void)
{
    MONITORINFO mi;

    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoA(mon_target(), &mi))
        return;
    SetWindowLongA(g_wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(g_wnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top + (g_mode.dmSize ? 0 : 1),
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

/* The mode exclusive fullscreen asked for; dmSize 0 while not in it. Given
 * back to Windows when the game loses focus (and the window minimised), and
 * taken again when it comes back. CDS_FULLSCREEN makes the change temporary:
 * Windows restores the desktop's mode if the game exits any way at all. */
void plat_fullscreen_exclusive(void)
{
    RECT r;
    MONITORINFOEXA mi;

    mi.cbSize = sizeof mi;
    g_mode_dev[0] = 0;
    if (GetMonitorInfoA(mon_target(), (MONITORINFO *)&mi))
        snprintf(g_mode_dev, sizeof g_mode_dev, "%s", mi.szDevice);
    GetClientRect(g_wnd, &r);
    ZeroMemory(&g_mode, sizeof g_mode);
    g_mode.dmSize       = sizeof g_mode;
    g_mode.dmPelsWidth  = (DWORD)(r.right - r.left);
    g_mode.dmPelsHeight = (DWORD)(r.bottom - r.top);
    g_mode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;
    if (ChangeDisplaySettingsExA(g_mode_dev[0] ? g_mode_dev : NULL, &g_mode, NULL,
                                 CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
        g_mode.dmSize = 0;
        plat_fullscreen();          /* no such mode: borderless instead */
        return;
    }
    plat_fullscreen();              /* now the monitor is the window's size */
}

void plat_display(int mode, int w, int h)
{
    if (g_mode.dmSize) {                /* out of exclusive fullscreen */
        ChangeDisplaySettingsExA(g_mode_dev[0] ? g_mode_dev : NULL, NULL, NULL, 0, NULL);
        g_mode.dmSize = 0;
    }
    if (mode == 1) {
        plat_fullscreen();
        return;
    }
    if (mode == 2) {
        MONITORINFO mi;
        int x = 0, y = 0;

        mi.cbSize = sizeof mi;
        if (GetMonitorInfoA(mon_target(), &mi)) {
            x = mi.rcMonitor.left;
            y = mi.rcMonitor.top;
        }
        SetWindowLongA(g_wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_wnd, HWND_TOP, x, y, w, h,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        plat_fullscreen_exclusive();
        return;
    }
    {
        RECT r = { 0, 0, w, h };
        MONITORINFO mi;
        int ww, wh, x, y;

        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        ww = r.right - r.left;
        wh = r.bottom - r.top;
        mi.cbSize = sizeof mi;
        GetMonitorInfoA(mon_target(), &mi);
        x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2;
        y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
        SetWindowLongA(g_wnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(g_wnd, HWND_NOTOPMOST, x, y, ww, wh,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

void plat_quit(void)
{
    g_quit = true;
}

int plat_text_take(char *out, int n)
{
    wchar_t w[2];
    int k = 1, len;

    if (g_text_n == 0 || n < 5)
        return 0;
    w[0] = g_text[0];
    if (w[0] >= 0xd800 && w[0] < 0xdc00 && g_text_n > 1) {
        w[1] = g_text[1];               /* a surrogate pair */
        k = 2;
    }
    g_text_n -= k;
    memmove(g_text, g_text + k, (size_t)g_text_n * sizeof g_text[0]);
    len = WideCharToMultiByte(CP_UTF8, 0, w, k, out, n - 1, NULL, NULL);
    if (len < 0)
        len = 0;
    out[len] = 0;
    return len;
}

void plat_text_clear(void)
{
    g_text_n = 0;
}

void *plat_gl_proc(const char *name)
{
    PROC p = wglGetProcAddress(name);

    /* some drivers answer 1, 2, 3 or -1 for "no" */
    if ((uintptr_t)p <= 3 || (intptr_t)p == -1)
        return NULL;
    return (void *)p;
}

void (*plat_ask_hook)(int begin);

bool plat_poll(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return !g_quit;
}

/* After a crash, before the message box: the desktop's own display mode
 * back (exclusive fullscreen changed it) and the game window out of the
 * way, so the box is not hidden behind a frozen fullscreen frame. */
void plat_crash_restore(void)
{
    if (g_mode.dmSize)
        ChangeDisplaySettingsExA(g_mode_dev[0] ? g_mode_dev : NULL, NULL, NULL, 0, NULL);
    if (g_wnd)
        ShowWindow(g_wnd, SW_HIDE);
}

/* glFinish after the swap: without it the driver may queue two or three
 * frames ahead, and every one of them is input lag. Waiting here keeps the
 * frame the player sees one tick behind the keys at most. */
bool plat_swap(void)
{
    bool ok = SwapBuffers(g_dc) != FALSE;

    glFinish();
    return ok;
}

void plat_size(int *width, int *height)
{
    RECT r;
    GetClientRect(g_wnd, &r);
    if (width)  *width  = r.right - r.left;
    if (height) *height = r.bottom - r.top;
}

bool plat_focused(void)
{
    return g_focused;
}

double plat_time(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_start.QuadPart) / (double)g_freq.QuadPart;
}

void plat_close(void)
{
    if (g_rc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_rc); g_rc = NULL; }
    if (g_dc) { ReleaseDC(g_wnd, g_dc); g_dc = NULL; }
    if (g_wnd) { DestroyWindow(g_wnd); g_wnd = NULL; }
}

/* Where the pointer is, and whether it is pressed. The front end has no mouse:
 * it reads a touch position, with -1 for "nothing is down", so the caller maps
 * one onto the other. */
int plat_mouse(int *x, int *y)
{
    if (x) *x = g_mouse_x;
    if (y) *y = g_mouse_y;
    return g_mouse_down ? 1 : 0;
}


/* ------------------------------------------------------------------- input
 *
 * The keyboard map. Player one is the left hand plus the number row, player
 * two is the numeric keypad -- so two people can share one keyboard, which is
 * the only way to test a fight without two pads.
 *
 * These are the DEFAULTS and they are here, in the backend, rather than in the
 * engine, because the engine has no idea what a key is. It takes ten bits.
 */
static int g_vk[PK_COUNT] = {
    'W', 'S', 'A', 'D',                 /* P1 directions */
    'U', 'I', 'O', 'J', 'K', 'L',       /* P1  HP LP BL HK LK RUN */
    VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,  /* P2 directions */
    VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, /* P2  HP LP BL */
    VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, /* P2  HK LK RUN */
    VK_F5,                              /* reset the scene */
    VK_F1, VK_RETURN, VK_RIGHT, VK_LEFT, /* the debug selector */
    VK_F3,                              /* leave the scene */
    VK_F2,                              /* enter the test mode */
    VK_F9, VK_F10, VK_F11, VK_F12,      /* fight debug: KO p2/p1, win/lose */
    'P', 'M',                           /* the pause menu, the moves list */
    VK_F6, VK_F7, VK_F8,                /* debug: screen back/next, main menu */
    'H', VK_NUMPAD0,                    /* the special button, P1 / P2 */
    'U', 'O', 'J', 'L',                 /* five buttons: P B K R */
    'Q', 'E',                           /* debug menu: page back / on */
    VK_NUMPAD7, VK_NUMPAD9,             /* P2 five buttons: P B */
    VK_NUMPAD4, VK_NUMPAD6,             /* P2 five buttons: K R */
    VK_ESCAPE,                          /* the settings menu */
};

void plat_key_label(int code, char *out, int n)
{
    int vk = (code >= 0 && code < PK_COUNT) ? g_vk[code] : 0;
    LONG lp = (LONG)(MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC) << 16);
    char *p;

    switch (vk) {                       /* the extended keys need bit 24 */
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
    case VK_PRIOR: case VK_NEXT:
        lp |= 1 << 24;
    }
    if (n <= 0)
        return;
    if (!vk || !GetKeyNameTextA(lp, out, n))
        snprintf(out, (size_t)n, "#%d", vk);
    out[n - 1] = 0;
    for (p = out; *p; p++)
        if (*p >= 'a' && *p <= 'z')
            *p = (char)(*p - 32);
}

static int al_glyph_have(const char *name);

/* The Vintage keyboard pictures' names (aditional textures/glyphs/kb). */
static const char *key_glyph_file(int vk, char *tmp)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        tmp[0] = (char)vk;
        tmp[1] = 0;
        return tmp;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        tmp[0] = (char)('0' + vk - VK_NUMPAD0);
        tmp[1] = 0;
        return tmp;
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        snprintf(tmp, 8, "F%d", vk - VK_F1 + 1);
        return tmp;
    }
    switch (vk) {
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_BACK: return "BackSpace";
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "Shift";
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "Crtl";
    case VK_MENU: case VK_LMENU: case VK_RMENU: return "Alt";
    case VK_CAPITAL: return "CapsLock";
    case VK_DELETE: case VK_DECIMAL: return "Del";
    case VK_INSERT: return "Ins";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_OEM_MINUS: case VK_SUBTRACT: return "Minus";
    case VK_OEM_PLUS: case VK_ADD: return "Plus";
    case VK_MULTIPLY: return "Asterisk";
    case VK_OEM_1: return "Semicolon";
    case VK_OEM_2: case VK_DIVIDE: return "Slash";
    case VK_OEM_3: return "Tilde";
    case VK_OEM_4: return "Brackets_L";
    case VK_OEM_6: return "Brackets_R";
    case VK_OEM_7: return "Quotation";
    case VK_SNAPSHOT: return "PrtScrn";
    case VK_NUMLOCK: return "NumLock";
    }
    return NULL;
}

void plat_key_glyph(int code, char *out, int n)
{
    int vk = (code >= 0 && code < PK_COUNT) ? g_vk[code] : 0;
    char tmp[8], name[64];
    const char *f = key_glyph_file(vk, tmp);

    if (n <= 0)
        return;
    if (f) {
        snprintf(name, sizeof name, "kb/T_%s_Key_Vintage", f);
        if (al_glyph_have(name)) {
            /* the keypad's digits are told from the row's by a NUM */
            snprintf(out, (size_t)n, "%s\001%s\001",
                     vk >= VK_NUMPAD0 && vk <= VK_DIVIDE ? "NUM " : "", name);
            return;
        }
    }
    plat_key_label(code, out, n);
}

static int g_glyph_style[2];

int plat_pad_sony_kind(unsigned product)
{
    /* the DualShock 4 (both models) and its USB adaptor */
    return product == 0x05c4 || product == 0x09cc || product == 0x0ba0 ? 4 : 2;
}

int plat_pad_glyph_style(int player, int style)
{
    player = player ? 1 : 0;
    if (style >= 0 && style <= 4)
        g_glyph_style[player] = style;
    return g_glyph_style[player];
}

int plat_pad_glyph_kind(int player, int kind)
{
    int st = g_glyph_style[player ? 1 : 0];
    return st ? st : kind;
}

/* Free Input Prompts' Retro pads (aditional textures/glyphs/xb, ps, ns). */
void plat_pad_glyph(int kind, int pb, char *out, int n)
{
    static const char *const xb[PB_COUNT] = {
        "T_X_A_Color_Retro", "T_X_B_Color_Retro", "T_X_X_Color_Retro",
        "T_X_Y_Color_Retro", "T_X_LB_Retro", "T_X_RB_Retro", "T_X_LT_Retro",
        "T_X_RT_Retro", "T_X_Share_Retro", NULL, "T_X_Left_Stick_Click_Retro",
        "T_X_Right_Stick_Click_Retro", NULL,
    };
    static const char *const ps[PB_COUNT] = {
        "T_P5_Cross_Color_Retro", "T_P5_Circle_Color_Retro",
        "T_P5_Square_Color_Retro", "T_P5_Triangle_Color_Retro", "T_P5_L1_Retro",
        "T_P5_R1_Retro", "T_P5_L2_Retro", "T_P5_R2_Retro", "T_P5_Share_Retro",
        "T_P5_Options_Retro", "T_P5_L3_Retro", "T_P5_R3_Retro", NULL,
    };
    /* by position: the south button of a Nintendo pad is its B (the
     * Default set, so they are not taken for the Xbox letters) */
    static const char *const ns[PB_COUNT] = {
        "T_S_B", "T_S_A", "T_S_Y", "T_S_X", "T_S_LB", "T_S_RB", "T_S_LT",
        "T_S_RT", "T_S_Minus", "T_S_Plus", "T_S_L", "T_S_R", "T_S_Home",
    };
    static const char *const p4[PB_COUNT] = {
        "T_P4_Cross_Color_Stylized", "T_P4_Circle_Color_Stylized",
        "T_P4_Square_Color_Stylized", "T_P4_Triangle_Color_Stylized",
        "T_P4_L1_Stylized", "T_P4_R1_Stylized", "T_P4_L2_Stylized",
        "T_P4_R2_Stylized", "T_P4_Share_Stylized", "T_P4_Options_Stylized",
        "T_P4_L3_Stylized", "T_P4_R3_Stylized", NULL,
    };
    static const char *const *const sets[5] = { xb, xb, ps, ns, p4 };
    static const char *const dir[5] = { "xb", "xb", "ps", "ns", "p4" };
    static const char *const names[PB_COUNT] = {
        "SOUTH", "EAST", "WEST", "NORTH", "L1", "R1", "L2", "R2",
        "SELECT", "START", "L3", "R3", "HOME",
    };
    char name[64];
    const char *f;

    if (n <= 0)
        return;
    if (pb < 0 || pb >= PB_COUNT) {
        out[0] = 0;
        return;
    }
    if (kind < 0 || kind > 4)
        kind = 0;
    f = sets[kind][pb];
    if (f) {
        snprintf(name, sizeof name, "%s/%s", dir[kind], f);
        if (al_glyph_have(name)) {
            snprintf(out, (size_t)n, "\001%s\001", name);
            return;
        }
    }
    snprintf(out, (size_t)n, "%s", (kind == 2 || kind == 4) && pb == PB_START ? "OPTIONS"
                                   : kind <= 1 && pb == PB_START ? "MENU" : names[pb]);
}

void plat_bind_key(int code, int key)
{
    if (code >= 0 && code < PK_COUNT && key > 0 && key < 256)
        g_vk[code] = key;
}

int plat_key_bound(int code)
{
    return code >= 0 && code < PK_COUNT ? g_vk[code] : 0;
}

int plat_key_any(void)
{
    int vk;

    for (vk = 8; vk < 256; vk++)        /* not the mouse buttons */
        if (g_key[vk])
            return vk;
    return 0;
}

int plat_key(int code)
{
    if (code < 0 || code >= PK_COUNT)
        return 0;
    return (g_key[g_vk[code]] || g_tap[g_vk[code]]) ? 1 : 0;
}

int plat_refresh_hz(void)
{
    int hz = g_dc ? GetDeviceCaps(g_dc, VREFRESH) : 0;
    return hz > 1 ? hz : 0;            /* 0 and 1 mean the default */
}

void plat_input_used(void)
{
    memset(g_tap, 0, sizeof g_tap);
}

/* Sleep until plat_time() reaches t: a high-resolution waitable timer for
 * all but the last millisecond, then a spin, so the wake-up is on time to
 * a few microseconds. */
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
void plat_wait_until(double t)
{
    static HANDLE timer;
    static int tried;
    double left = t - plat_time();

    if (!tried) {
        tried = 1;
        timer = CreateWaitableTimerExW(NULL, NULL,
                                       CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                       TIMER_ALL_ACCESS);
    }
    if (left > 0.0015 && timer) {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)((left - 0.001) * 1e7);  /* 100 ns units */
        if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
            WaitForSingleObject(timer, 100);
    }
    while (plat_time() < t)
        YieldProcessor();
}

/* The gamepad: XInput (loaded at run time) and the WinMM joystick API.
 *
 * XInput covers the Xbox pads and anything a driver shows as one (Steam,
 * DS4Windows). WinMM covers the rest -- PlayStation 4 and 5 pads, the Switch
 * Pro controller, generic USB pads -- each read through the layout its
 * vendor uses (pad_map). Both end up in one PLAT_PAD, the face buttons named
 * by position.
 *
 * Not DirectInput: on some machines its EnumDevices takes 40 seconds (one
 * slow HID device is enough), and finding the pad must never hold the game
 * up. WinMM reads a known pad in microseconds; looking for one -- trying the
 * 16 joystick ids -- runs on a thread of its own, again every two seconds
 * while none is found, so a pad plugged in later is picked up. A missing
 * driver reads as no pad, never as a crash. Port only. */
#include <mmsystem.h>

#define XI_A       0x1000
#define XI_B       0x2000
#define XI_X       0x4000
#define XI_Y       0x8000
#define XI_LB      0x0100
#define XI_RB      0x0200
#define XI_BACK    0x0020
#define XI_START   0x0010
#define XI_LTHUMB  0x0040
#define XI_RTHUMB  0x0080
#define XI_DUP     0x0001
#define XI_DDOWN   0x0002
#define XI_DLEFT   0x0004
#define XI_DRIGHT  0x0008

typedef struct { unsigned long packet; unsigned short buttons;
                 unsigned char lt, rt; short lx, ly, rx, ry; } XI_STATE;
typedef unsigned long (__stdcall *XIGET)(unsigned long, XI_STATE *);

static XIGET          g_xiget;
static int            g_pad_tried;
static HANDLE         g_pad_thread;

/* One per player: pad= for player one, p2pad= for player two (port). A
 * pad one player holds -- a WinMM id or an XInput slot -- is never the
 * other's, so "auto" on both gives each its own pad. */
typedef struct {
    char          want[32];
    volatile LONG jid;                  /* the WinMM pad in use, or -1 */
    unsigned      vidpid;               /* its vendor | product << 16 */
    char          name[64];
    volatile LONG xi;                   /* the XInput slot last read, or -1 */
} PAD_SLOT;

static PAD_SLOT g_slot[2] = {
    { "auto", -1, 0, "", -1 },
    { "auto", -1, 0, "", -1 },
};

static unsigned want_vidpid(const PAD_SLOT *ps)
{
    unsigned v, p;

    if (sscanf(ps->want, "%x:%x", &v, &p) == 2)
        return (v & 0xffff) | (p & 0xffff) << 16;
    return 0;
}

/* The name Windows gives a pad (Game Controllers in the control panel):
 * OEMName under the joystick OEM key for its VID/PID; the driver's generic
 * name when there is none. */
static void pad_name(unsigned vp, const char *fallback, char *out, int n)
{
    char key[160];
    DWORD size = (DWORD)n, type;
    HKEY h;
    int i;

    snprintf(key, sizeof key, "System\\CurrentControlSet\\Control\\"
             "MediaProperties\\PrivateProperties\\Joystick\\OEM\\"
             "VID_%04X&PID_%04X", vp & 0xffff, vp >> 16);
    for (i = 0; i < 2; i++) {
        if (RegOpenKeyExA(i ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, key, 0,
                          KEY_READ, &h) == ERROR_SUCCESS) {
            size = (DWORD)n;
            if (RegQueryValueExA(h, "OEMName", NULL, &type, (BYTE *)out,
                                 &size) == ERROR_SUCCESS && type == REG_SZ
                && size > 1) {
                out[n - 1] = 0;
                RegCloseKey(h);
                return;
            }
            RegCloseKey(h);
        }
    }
    snprintf(out, (size_t)n, "%s", fallback);
}

/* Joystick id `id`: attached? Its vendor | product << 16 and name. */
static int joy_probe(UINT id, unsigned *vp, char *name, int n)
{
    JOYINFOEX j;
    JOYCAPSA c;

    memset(&j, 0, sizeof j);
    j.dwSize = sizeof j;
    j.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(id, &j) != JOYERR_NOERROR)
        return 0;
    if (joyGetDevCapsA(id, &c, sizeof c) != JOYERR_NOERROR)
        return 0;
    *vp = (unsigned)c.wMid | (unsigned)c.wPid << 16;
    if (name)
        pad_name(*vp, c.szPname, name, n);
    return 1;
}

/* The search, off the game's thread: player one first, so with "auto" on
 * both player one gets the first pad found. */
static DWORD WINAPI pad_search(LPVOID arg)
{
    (void)arg;
    for (;;) {
        int s;

        for (s = 0; s < 2; s++) {
            PAD_SLOT *ps = &g_slot[s];
            unsigned want = want_vidpid(ps), vp;
            char name[64];
            UINT id;

            if (ps->jid >= 0 || strcmp(ps->want, "off") == 0
                || strcmp(ps->want, "keyboard") == 0
                || strcmp(ps->want, "xinput") == 0
                || (!want && ps->xi >= 0))      /* auto, on an XInput pad */
                continue;
            for (id = 0; id < 16 && ps->jid < 0; id++) {
                if ((LONG)id == g_slot[1 - s].jid)
                    continue;
                if (!joy_probe(id, &vp, name, sizeof name))
                    continue;
                if (want ? vp != want
                         : (vp & 0xffff) == 0x045e)  /* Xbox: XInput */
                    continue;
                ps->vidpid = vp;
                snprintf(ps->name, sizeof ps->name, "%s", name);
                InterlockedExchange(&ps->jid, (LONG)id);
            }
        }
        Sleep(2000);
    }
    return 0;
}

static void pad_select(PAD_SLOT *ps, const char *which)
{
    snprintf(ps->want, sizeof ps->want, "%s",
             which && *which ? which : "auto");
    InterlockedExchange(&ps->jid, -1);          /* look again */
    InterlockedExchange(&ps->xi, -1);
}

void plat_pad_select(const char *which)  { pad_select(&g_slot[0], which); }
void plat_pad2_select(const char *which) { pad_select(&g_slot[1], which); }

static void pad_load(void)
{
    static const char *xi[] = { "xinput1_4.dll", "xinput1_3.dll",
                                "xinput9_1_0.dll", "xinput1_2.dll" };
    int i;

    g_pad_tried = 1;
    for (i = 0; i < 4 && !g_xiget; i++) {
        HMODULE m = LoadLibraryA(xi[i]);
        if (m)
            g_xiget = (XIGET)(void *)GetProcAddress(m, "XInputGetState");
    }
}

/* Which WinMM button is which, by vendor. Index = PB_*, value = the button
 * number (bit of dwButtons), -1 none. The triggers are buttons on all
 * three. */
static const signed char g_map_sony[PB_COUNT] =      /* PS4 / PS5 */
    { 1, 2, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
static const signed char g_map_nintendo[PB_COUNT] =  /* Switch Pro */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
static const signed char g_map_generic[PB_COUNT] =   /* most USB pads */
    { 1, 2, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };

static int pad_winmm(PAD_SLOT *ps, PLAT_PAD *out)
{
    JOYINFOEX j;
    const signed char *map;
    LONG id = ps->jid;
    DWORD pov;
    int i;

    if (!g_pad_thread)
        g_pad_thread = CreateThread(NULL, 0, pad_search, NULL, 0, NULL);
    if (id < 0)
        return 0;
    memset(&j, 0, sizeof j);
    j.dwSize = sizeof j;
    j.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx((UINT)id, &j) != JOYERR_NOERROR) {
        InterlockedCompareExchange(&ps->jid, -1, id);  /* unplugged */
        return 0;
    }
    switch (ps->vidpid & 0xffff) {
    case 0x054c: map = g_map_sony; break;
    case 0x057e: map = g_map_nintendo; break;
    default:     map = g_map_generic; break;
    }
    for (i = 0; i < PB_COUNT; i++)
        if (map[i] >= 0 && (j.dwButtons & (1u << map[i])))
            out->buttons |= 1u << i;
    pov = j.dwPOV;                              /* hundredths of a degree */
    if (pov <= 35900) {
        if (pov >= 31500 || pov <= 4500)  out->up = 1;
        if (pov >= 4500 && pov <= 13500)  out->right = 1;
        if (pov >= 13500 && pov <= 22500) out->down = 1;
        if (pov >= 22500 && pov <= 31500) out->left = 1;
    }
    /* the stick: 0..65535, a direction past a third of the way out */
    if (j.dwYpos < 20000) out->up = 1;
    if (j.dwYpos > 45535) out->down = 1;
    if (j.dwXpos < 20000) out->left = 1;
    if (j.dwXpos > 45535) out->right = 1;
    snprintf(out->name, sizeof out->name, "%s", ps->name);
    out->kind = (ps->vidpid & 0xffff) == 0x054c                 /* Sony */
                ? plat_pad_sony_kind(ps->vidpid >> 16)
              : (ps->vidpid & 0xffff) == 0x057e ? 3 : 0;    /* Nintendo */
    return out->connected = 1;
}

/* The first XInput slot attached that the other player is not using. */
static int pad_xinput(PAD_SLOT *ps, PLAT_PAD *out)
{
    XI_STATE s;
    LONG other = g_slot[ps == g_slot ? 1 : 0].xi;
    unsigned long i;

    if (!g_xiget)
        return 0;
    for (i = 0; i < 4; i++) {
        memset(&s, 0, sizeof s);
        if ((LONG)i != other && g_xiget(i, &s) == 0)
            break;
    }
    if (i == 4) {
        InterlockedExchange(&ps->xi, -1);
        return 0;
    }
    InterlockedExchange(&ps->xi, (LONG)i);
    if (s.buttons & XI_A)      out->buttons |= 1u << PB_SOUTH;
    if (s.buttons & XI_B)      out->buttons |= 1u << PB_EAST;
    if (s.buttons & XI_X)      out->buttons |= 1u << PB_WEST;
    if (s.buttons & XI_Y)      out->buttons |= 1u << PB_NORTH;
    if (s.buttons & XI_LB)     out->buttons |= 1u << PB_L1;
    if (s.buttons & XI_RB)     out->buttons |= 1u << PB_R1;
    if (s.lt > 64)             out->buttons |= 1u << PB_L2;
    if (s.rt > 64)             out->buttons |= 1u << PB_R2;
    if (s.buttons & XI_BACK)   out->buttons |= 1u << PB_SELECT;
    if (s.buttons & XI_START)  out->buttons |= 1u << PB_START;
    if (s.buttons & XI_LTHUMB) out->buttons |= 1u << PB_L3;
    if (s.buttons & XI_RTHUMB) out->buttons |= 1u << PB_R3;
    /* The stick counts as a direction past a third of its range, so the pad
     * feels like the arcade's switch rather than like an analogue axis. */
    out->up    = (s.buttons & XI_DUP)    || s.ly >  10000;
    out->down  = (s.buttons & XI_DDOWN)  || s.ly < -10000;
    out->left  = (s.buttons & XI_DLEFT)  || s.lx < -10000;
    out->right = (s.buttons & XI_DRIGHT) || s.lx >  10000;
    snprintf(out->name, sizeof out->name, "XInput pad %lu", i + 1);
    out->kind = 1;
    return out->connected = 1;
}

static int pad_read(PAD_SLOT *ps, PLAT_PAD *out)
{
    memset(out, 0, sizeof *out);
    if (!g_pad_tried)
        pad_load();
    if (strcmp(ps->want, "off") == 0 || strcmp(ps->want, "keyboard") == 0)
        return 0;
    if (strcmp(ps->want, "xinput") == 0)
        return pad_xinput(ps, out);
    if (want_vidpid(ps))
        return pad_winmm(ps, out);
    if (pad_xinput(ps, out)) {                  /* auto: XInput first */
        LONG id = ps->jid;
        if (id >= 0)                            /* let the other have it */
            InterlockedCompareExchange(&ps->jid, -1, id);
        return 1;
    }
    return pad_winmm(ps, out);
}

int plat_pad_read(PLAT_PAD *out)  { return pad_read(&g_slot[0], out); }
int plat_pad2_read(PLAT_PAD *out) { return pad_read(&g_slot[1], out); }

/* --list-pads: every WinMM pad by the name Windows gives it (an Xbox pad
 * listed as "xinput", since that is how the game reads it), then any XInput
 * slot not already named that way. */
void plat_pad_list(void)
{
    XI_STATE s;
    unsigned vp;
    char name[64];
    int xbox = 0;
    UINT id;
    unsigned long i;

    if (!g_pad_tried)
        pad_load();
    for (id = 0; id < 16; id++) {
        if (!joy_probe(id, &vp, name, sizeof name))
            continue;
        if ((vp & 0xffff) == 0x045e) {
            xbox++;
            printf("xinput\t%s\n", name);
        } else {
            printf("%04x:%04x\t%s\n", vp & 0xffff, vp >> 16, name);
        }
    }
    for (i = 0; g_xiget && i < 4; i++)
        if (g_xiget(i, &s) == 0 && xbox-- <= 0)
            printf("xinput\tXInput pad %lu\n", i + 1);
    fflush(stdout);
}

/* The old ten-bit read (runtime/fight_scene.c): the same pad, the layout a
 * six-button fighter gets on a four-button pad. */
int plat_pad(int which)
{
    PLAT_PAD p;
    int bits = 0;

    if (which != 0 || !plat_pad_read(&p))
        return -1;
    if (p.up)    bits |= 1 << 0;
    if (p.down)  bits |= 1 << 1;
    if (p.left)  bits |= 1 << 2;
    if (p.right) bits |= 1 << 3;
    if (p.buttons & (1u << PB_WEST))  bits |= 1 << 4;      /* HP  */
    if (p.buttons & (1u << PB_SOUTH)) bits |= 1 << 5;      /* LP  */
    if (p.buttons & (1u << PB_R1))    bits |= 1 << 6;      /* BL  */
    if (p.buttons & (1u << PB_NORTH)) bits |= 1 << 7;      /* HK  */
    if (p.buttons & (1u << PB_EAST))  bits |= 1 << 8;      /* LK  */
    if (p.buttons & (1u << PB_L1))    bits |= 1 << 9;      /* RUN */
    return bits;
}


/* ------------------------------------------------------------------ alerts
 *
 * The PC's UIAlertView, drawn inside the game window. On the device
 * +[modalAlert askFull:textOK:textCANCEL:] / infoFull:textOK: (0xb5444 /
 * 0xb541c) build one UIAlertView -- initWithTitle: the whole text (both
 * GameText lines joined by "\n"), message: nil, the OK text as the
 * cancelButtonTitle (index 0, on the left) and CANCEL as the other button
 * (index 1, on the right) -- and spin a run loop until a button is pressed.
 * The game is frozen meanwhile, so it is here too: this loop owns the window
 * until the answer is in.
 *
 * The look is iPhone OS 3's: the screen dimmed, a translucent dark-blue box
 * with a light border and a gloss across the top, bold white centred text,
 * and glossy buttons. It is drawn in software into one texture (the shapes
 * anti-aliased by their distance to the edge, the text rasterised by GDI so
 * the game's accented UTF-16 shows as it is), over a copy of the frame the
 * game had drawn when it asked. Mouse: press and release on a button.
 * Keyboard: Enter is the left button (OK), Esc the right one (CANCEL), or
 * OK when there is only one. */

#include <math.h>
#include <stdlib.h>

typedef struct { float r, g, b, a; } al_px;     /* premultiplied */

typedef struct {
    int    w, h;            /* the box texture, pixels (box plus shadow margin) */
    al_px *px;
    float  s;               /* pixels per iPhone point */
    float  bx0, by0, bx1, by1;          /* the box inside the texture */
    float  btn[2][4];       /* x0 y0 x1 y1 of each button, texture pixels */
    int    nbtn;
} al_box;

static void al_over(al_px *d, float r, float g, float b, float a)
{
    d->r = r * a + d->r * (1 - a);
    d->g = g * a + d->g * (1 - a);
    d->b = b * a + d->b * (1 - a);
    d->a = a + d->a * (1 - a);
}

/* coverage of pixel (x, y) by a rounded rectangle, anti-aliased */
static float al_rr(float x, float y, float x0, float y0, float x1, float y1,
                   float rad)
{
    float px = x + 0.5f, py = y + 0.5f;
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float qx = fabsf(px - cx) - ((x1 - x0) * 0.5f - rad);
    float qy = fabsf(py - cy) - ((y1 - y0) * 0.5f - rad);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = (qx > qy ? qx : qy);
    float d = sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - rad;
    float c = 0.5f - d;
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

/* a GDI coverage mask of `text`, white on black, centred and word-wrapped in
 * `tw` pixels; returns the mask (w = tw, *th rows), malloc'd. The wrapping
 * is done here, at spaces and newlines only: DrawText's own broke words at
 * accented letters ("PÁ / GINA"). */
static unsigned char *al_text(const wchar_t *text, int tw, int fontpx, int *th)
{
    HDC dc = CreateCompatibleDC(g_dc);
    HFONT f = CreateFontW(-fontpx, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial");
    HFONT of = (HFONT)SelectObject(dc, f);
    BITMAPINFO bi;
    void *bits = NULL;
    HBITMAP bm, ob;
    unsigned char *mask;
    int h, i, n = 0, lh, len = (int)wcslen(text);
    int ls[32], ll[32];
    TEXTMETRICW tm;
    SIZE sz;

    GetTextMetricsW(dc, &tm);
    lh = tm.tmHeight;
    for (i = 0; i <= len && n < 32; ) {
        int start = i, brk = -1, j = i;
        /* the longest run of words from `start` that fits */
        while (j < len && text[j] != '\n') {
            if (text[j] == ' ') {
                GetTextExtentPoint32W(dc, text + start, j - start, &sz);
                if (sz.cx > tw && brk > start)
                    break;
                brk = j;
            }
            j++;
        }
        if (j >= len || text[j] == '\n') {
            GetTextExtentPoint32W(dc, text + start, j - start, &sz);
            if (sz.cx > tw && brk > start)
                j = brk;
        } else {
            j = brk;
        }
        ls[n] = start;
        ll[n] = j - start;
        n++;
        i = j + 1;                      /* past the space or the newline */
        if (j >= len)
            break;
    }
    h = n * lh > 1 ? n * lh : 1;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = tw;
    bi.bmiHeader.biHeight = -h;             /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ob = (HBITMAP)SelectObject(dc, bm);
    memset(bits, 0, (size_t)tw * h * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (i = 0; i < n; i++) {
        GetTextExtentPoint32W(dc, text + ls[i], ll[i], &sz);
        TextOutW(dc, (tw - sz.cx) / 2, i * lh, text + ls[i], ll[i]);
    }
    GdiFlush();
    mask = (unsigned char *)malloc((size_t)tw * h);
    for (i = 0; i < tw * h; i++)
        mask[i] = ((unsigned char *)bits)[i * 4 + 1];
    SelectObject(dc, ob);
    SelectObject(dc, of);
    DeleteObject(bm);
    DeleteObject(f);
    DeleteDC(dc);
    *th = h;
    return mask;
}

/* white text with the iPhone's dark shadow one point above it */
static void al_put_text(al_box *b, const unsigned char *m, int tw, int th,
                        int x, int y)
{
    int i, j, sh = (int)(b->s + 0.5f);

    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j - sh;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], 0, 0, 0, c * 0.55f);
        }
    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], 1, 1, 1, c);
        }
}

static void al_button(al_box *b, int k, int pressed, int light,
                      const wchar_t *label)
{
    float x0 = b->btn[k][0], y0 = b->btn[k][1];
    float x1 = b->btn[k][2], y1 = b->btn[k][3];
    float rad = 6 * b->s, mid = (y0 + y1) * 0.5f;
    int x, y, tw, th;
    unsigned char *m;

    for (y = (int)y0 - 1; y <= (int)y1 + 1; y++)
        for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
            float edge, fill, t, top, bot, r, g, bl;
            al_px *d;
            if (x < 0 || y < 0 || x >= b->w || y >= b->h)
                continue;
            d = &b->px[y * b->w + x];
            edge = al_rr((float)x, (float)y, x0, y0, x1, y1, rad);
            fill = al_rr((float)x, (float)y, x0 + b->s, y0 + b->s,
                         x1 - b->s, y1 - b->s, rad - b->s);
            if (edge <= 0)
                continue;
            al_over(d, 0.03f, 0.05f, 0.12f, (edge - fill > 0 ? edge - fill : 0) * 0.85f);   /* the dark rim */
            if (fill <= 0)
                continue;
            /* a lighter upper half with a gloss, a darker lower half */
            t = (y + 0.5f - y0) / (y1 - y0);
            top = light ? 0.56f : 0.44f;
            bot = light ? 0.30f : 0.22f;
            if (y + 0.5f < mid) {
                r = top + 0.10f * (1 - t * 2);
                g = r + 0.02f;
                bl = r + 0.08f;
            } else {
                r = bot - 0.06f * (t * 2 - 1);
                g = r + 0.02f;
                bl = r + 0.08f;
            }
            if (pressed) {                  /* the iPhone's blue press */
                r = r * 0.4f;
                g = g * 0.55f + 0.12f;
                bl = bl * 0.6f + 0.38f;
            }
            al_over(d, r, g, bl, fill * 0.82f);
            if (y < (int)(y0 + 2 * b->s))   /* the highlight along the top */
                al_over(d, 1, 1, 1, fill * 0.25f);
        }

    tw = (int)(x1 - x0);
    m = al_text(label, tw, (int)(17 * b->s), &th);
    al_put_text(b, m, tw, th, (int)x0, (int)(mid - th * 0.5f));
    free(m);
}

/* the box itself: drop shadow, light border ring, translucent dark blue,
 * the gloss across the top */
static void al_draw_bg(al_box *b)
{
    float s = b->s, rad = 10 * s, bw = 2 * s;
    float gcx = (b->bx0 + b->bx1) * 0.5f;
    float grx = (b->bx1 - b->bx0) * 0.75f, gry = 34 * s;
    int x, y;

    memset(b->px, 0, sizeof(al_px) * b->w * b->h);
    for (y = 0; y < b->h; y++)
        for (x = 0; x < b->w; x++) {
            al_px *d = &b->px[y * b->w + x];
            float sh = al_rr((float)x, (float)y - 3 * s, b->bx0, b->by0,
                             b->bx1, b->by1, rad + 2 * s);
            float out = al_rr((float)x, (float)y, b->bx0, b->by0, b->bx1,
                              b->by1, rad);
            float in = al_rr((float)x, (float)y, b->bx0 + bw, b->by0 + bw,
                             b->bx1 - bw, b->by1 - bw, rad - bw);
            float ex, ey;
            if (sh > 0)
                al_over(d, 0, 0, 0, sh * 0.25f);        /* the drop shadow */
            if (out <= 0)
                continue;
            al_over(d, 0.86f, 0.88f, 0.93f, (out - in > 0 ? out - in : 0) * 0.9f);   /* the border ring */
            if (in <= 0)
                continue;
            /* translucent dark blue, a touch lighter at the top */
            al_over(d, 0.03f, 0.09f, 0.24f,
                    in * (0.62f + 0.08f * (1 - (y - b->by0) / (b->by1 - b->by0))));
            /* the gloss: an ellipse whose lower edge crosses the top */
            ex = (x + 0.5f - gcx) / grx;
            ey = (y + 0.5f - (b->by0 + gry - gry * 1.6f)) / (gry * 1.6f);
            if (ex * ex + ey * ey < 1)
                al_over(d, 1, 1, 1, in * 0.16f);
        }
}

static void al_draw(al_box *b, const wchar_t *msg, const wchar_t *ok,
                    const wchar_t *cancel, int pressed)
{
    float s = b->s;
    int tw, th;
    unsigned char *m;

    al_draw_bg(b);
    tw = (int)(b->bx1 - b->bx0 - 24 * s);
    m = al_text(msg, tw, (int)(18 * s), &th);
    al_put_text(b, m, tw, th, (int)(b->bx0 + 12 * s), (int)(b->by0 + 18 * s));
    free(m);

    al_button(b, 0, pressed == 0, 0, ok);
    if (b->nbtn == 2)
        al_button(b, 1, pressed == 1, 1, cancel);
}

/* one line of text, its exact width: a coverage mask *w x *h, malloc'd */
static unsigned char *al_line(const char *s8, int fontpx, int bold,
                              int *w, int *h)
{
    wchar_t ws[256];
    HDC dc = CreateCompatibleDC(g_dc);
    HFONT f = CreateFontW(-fontpx, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0,
                          0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                          DEFAULT_PITCH, L"Arial");
    HFONT of = (HFONT)SelectObject(dc, f);
    BITMAPINFO bi;
    void *bits = NULL;
    HBITMAP bm, ob;
    unsigned char *mask;
    TEXTMETRICW tm;
    SIZE sz;
    int i, n;

    /* UTF-8 (a player's name, settings_menu.c); anything that is not
     * valid UTF-8 is read as Latin-1, as before */
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s8, -1, ws, 255);
    if (n > 0)
        n--;
    else
        for (n = 0; s8[n] && n < 255; n++)
            ws[n] = (unsigned char)s8[n];
    ws[n] = 0;
    GetTextMetricsW(dc, &tm);
    GetTextExtentPoint32W(dc, ws, n, &sz);
    *w = sz.cx > 0 ? sz.cx : 1;
    *h = tm.tmHeight;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = *w;
    bi.bmiHeader.biHeight = -*h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ob = (HBITMAP)SelectObject(dc, bm);
    memset(bits, 0, (size_t)*w * *h * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, 0, 0, ws, n);
    GdiFlush();
    mask = (unsigned char *)malloc((size_t)*w * *h);
    for (i = 0; i < *w * *h; i++)
        mask[i] = ((unsigned char *)bits)[i * 4 + 1];
    SelectObject(dc, ob);
    SelectObject(dc, of);
    DeleteObject(bm);
    DeleteObject(f);
    DeleteDC(dc);
    return mask;
}

/* text in a colour, with the dark shadow above it, alpha `a` */
static void al_put_line(al_box *b, const unsigned char *m, int tw, int th,
                        int x, int y, float r, float g, float bl, float a)
{
    int i, j, sh = (int)(b->s + 0.5f);

    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j - sh;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], 0, 0, 0, c * 0.5f * a);
        }
    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], r, g, bl, c * a);
        }
}

/* ------------------------------------------------------------ glyphs
 * Port, not in the binary: key and button pictures in the menus' text
 * (plat_key_glyph, plat_pad_glyph). Read once from "aditional textures/
 * glyphs/<name>.png" beside res -- Julio Cacko's Free Input Prompts, CC0 --
 * and kept. */
const char *lime_platform_asset_root(void);
#include "lime/png.h"

typedef struct { char name[64]; uint8_t *rgba; int w, h; } al_glyph;
static al_glyph g_glyphs[128];
static int g_nglyphs;

static const al_glyph *al_glyph_get(const char *name, int len)
{
    char full[1200];
    al_glyph *g;
    int i;

    if (len <= 0 || len >= (int)sizeof g_glyphs[0].name)
        return NULL;
    for (i = 0; i < g_nglyphs; i++)
        if ((int)strlen(g_glyphs[i].name) == len
            && memcmp(g_glyphs[i].name, name, (size_t)len) == 0)
            return g_glyphs[i].rgba ? &g_glyphs[i] : NULL;
    if (g_nglyphs == (int)(sizeof g_glyphs / sizeof g_glyphs[0]))
        return NULL;
    g = &g_glyphs[g_nglyphs++];
    memcpy(g->name, name, (size_t)len);
    g->name[len] = 0;
    snprintf(full, sizeof full, "%s/../aditional textures/glyphs/%s.png",
             lime_platform_asset_root(), g->name);
    if (!lime_png_load(full, &g->rgba, &g->w, &g->h) || g->w <= 0 || g->h <= 0)
        g->rgba = NULL;
    return g->rgba ? g : NULL;
}

static int al_glyph_have(const char *name)
{
    return al_glyph_get(name, (int)strlen(name)) != NULL;
}

/* A glyph hp pixels tall at (x, y), averaged down from its picture. */
static void al_put_glyph(al_box *b, const al_glyph *g, int x, int y, int hp,
                         float a)
{
    int w = g->w * hp / g->h, i, j;

    for (j = 0; j < hp; j++)
        for (i = 0; i < w; i++) {
            int sx0 = i * g->w / w, sx1 = (i + 1) * g->w / w;
            int sy0 = j * g->h / hp, sy1 = (j + 1) * g->h / hp, u, v, k = 0;
            float pr = 0, pg = 0, pb = 0, pa = 0;
            int X = x + i, Y = y + j;

            if (X < 0 || X >= b->w || Y < 0 || Y >= b->h)
                continue;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (v = sy0; v < sy1 && v < g->h; v++)
                for (u = sx0; u < sx1 && u < g->w; u++) {
                    const uint8_t *p = g->rgba + ((size_t)v * g->w + u) * 4;
                    float al = p[3] / 255.0f;
                    pr += p[0] / 255.0f * al;
                    pg += p[1] / 255.0f * al;
                    pb += p[2] / 255.0f * al;
                    pa += al;
                    k++;
                }
            if (k == 0 || pa <= 0)
                continue;
            al_over(&b->px[Y * b->w + X], pr / pa, pg / pa, pb / pa, pa / k * a);
        }
}

/* align: 0 left at x, 1 centred on x, 2 right-aligned to x. "\001name\001"
 * in s is a glyph, a little taller than the text. */
static void al_say(al_box *b, const char *s, int fontpx, int bold, float x,
                   float ymid, int align, float r, float g, float bl, float a)
{
    struct { unsigned char *m; const al_glyph *gl; int w, h; } pc[16];
    int np = 0, tot = 0, gh = (int)(fontpx * 1.4f + 0.5f), gap = fontpx / 5, i;
    const char *p = s;
    float cx;

    if (!s || !s[0])
        return;
    while (*p && np < 16) {
        const char *q;
        if (*p == 1) {
            const al_glyph *gl;
            q = strchr(p + 1, 1);
            if (!q)
                break;
            gl = al_glyph_get(p + 1, (int)(q - p - 1));
            if (gl) {
                pc[np].m = NULL;
                pc[np].gl = gl;
                pc[np].h = gh;
                pc[np].w = gl->w * gh / gl->h;
                np++;
            }
            p = q + 1;
        } else {
            char t[256];
            size_t len;
            q = strchr(p, 1);
            len = q ? (size_t)(q - p) : strlen(p);
            if (len > sizeof t - 1)
                len = sizeof t - 1;
            memcpy(t, p, len);
            t[len] = 0;
            p += len;
            pc[np].gl = NULL;
            pc[np].m = al_line(t, fontpx, bold, &pc[np].w, &pc[np].h);
            np++;
        }
    }
    for (i = 0; i < np; i++)
        tot += pc[i].w + (i ? gap : 0);
    cx = align == 1 ? x - tot * 0.5f : align == 2 ? x - tot : x;
    for (i = 0; i < np; i++) {
        if (i)
            cx += gap;
        if (pc[i].gl)
            al_put_glyph(b, pc[i].gl, (int)cx, (int)(ymid - pc[i].h * 0.5f),
                         pc[i].h, a);
        else {
            al_put_line(b, pc[i].m, pc[i].w, pc[i].h, (int)cx,
                        (int)(ymid - pc[i].h * 0.5f), r, g, bl, a);
            free(pc[i].m);
        }
        cx += pc[i].w;
    }
}

/* The player's badge (settings_menu.c, port): the profile's icon and name
 * in a small box of the alerts' dress -- the border ring, the gloss, the
 * drop shadow -- but black instead of blue, darker at the bottom. `icon` is
 * icon_px square, straight RGBA; `s` pixels per point. Returns RGBA8,
 * premultiplied, *w x *h, malloc'd. */
unsigned char *plat_ui_badge(const char *name, const unsigned char *icon,
                             int icon_px, float s, int *ow, int *oh)
{
    al_box b;
    float m = 4 * s, h = 30 * s, isz = 24 * s, pad = 3 * s, rad = 8 * s,
          bw = 1.5f * s;
    int tw = 0, th = 0, x, y;
    unsigned char *tm = al_line(name && name[0] ? name : "-", (int)(12 * s), 1,
                                &tw, &th);
    unsigned char *out;

    memset(&b, 0, sizeof b);
    b.s = s;
    b.bx0 = m;
    b.by0 = m;
    b.bx1 = m + pad + isz + 6 * s + tw + 8 * s;
    b.by1 = m + h;
    b.w = (int)(b.bx1 + m + 1);
    b.h = (int)(b.by1 + m + 1);
    b.px = (al_px *)calloc((size_t)b.w * b.h, sizeof(al_px));
    if (!b.px) {
        free(tm);
        return NULL;
    }
    for (y = 0; y < b.h; y++)
        for (x = 0; x < b.w; x++) {
            al_px *d = &b.px[y * b.w + x];
            float sh = al_rr((float)x, (float)y - 2 * s, b.bx0, b.by0,
                             b.bx1, b.by1, rad + 2 * s);
            float out_ = al_rr((float)x, (float)y, b.bx0, b.by0, b.bx1,
                               b.by1, rad);
            float in = al_rr((float)x, (float)y, b.bx0 + bw, b.by0 + bw,
                             b.bx1 - bw, b.by1 - bw, rad - bw);
            float t = (y + 0.5f - b.by0) / (b.by1 - b.by0), ex, ey;

            if (sh > 0)
                al_over(d, 0, 0, 0, sh * 0.3f);         /* the drop shadow */
            if (out_ <= 0)
                continue;
            al_over(d, 0.86f, 0.88f, 0.93f, (out_ - in > 0 ? out_ - in : 0) * 0.85f);
            if (in <= 0)
                continue;
            /* black, a dark grey at the top */
            al_over(d, 0.16f * (1 - t), 0.16f * (1 - t), 0.18f * (1 - t),
                    in * (0.80f + 0.12f * t));
            ex = (x + 0.5f - (b.bx0 + b.bx1) * 0.5f) / ((b.bx1 - b.bx0) * 0.75f);
            ey = (y + 0.5f - (b.by0 - h * 0.35f)) / (h * 0.8f);
            if (ex * ex + ey * ey < 1)                  /* the gloss */
                al_over(d, 1, 1, 1, in * 0.18f);
        }
    if (icon && icon_px > 0) {                          /* the icon, scaled */
        int ix0 = (int)(b.bx0 + pad), iy0 = (int)(b.by0 + (h - isz) * 0.5f);
        int n = (int)isz;
        for (y = 0; y < n; y++)
            for (x = 0; x < n; x++) {
                int X = ix0 + x, Y = iy0 + y;
                const unsigned char *p = icon + ((size_t)(y * icon_px / n) * icon_px
                                                 + (size_t)(x * icon_px / n)) * 4;
                if (X >= 0 && X < b.w && Y >= 0 && Y < b.h && p[3])
                    al_over(&b.px[Y * b.w + X], p[0] / 255.0f, p[1] / 255.0f,
                            p[2] / 255.0f, p[3] / 255.0f);
            }
    }
    al_put_line(&b, tm, tw, th, (int)(b.bx0 + pad + isz + 6 * s),
                (int)((b.by0 + b.by1) * 0.5f - th * 0.5f), 1, 1, 1, 1);
    free(tm);

    out = (unsigned char *)malloc((size_t)b.w * b.h * 4);
    if (out)
        for (x = 0; x < b.w * b.h; x++) {
            out[x * 4 + 0] = (unsigned char)(b.px[x].r * 255 + 0.5f);
            out[x * 4 + 1] = (unsigned char)(b.px[x].g * 255 + 0.5f);
            out[x * 4 + 2] = (unsigned char)(b.px[x].b * 255 + 0.5f);
            out[x * 4 + 3] = (unsigned char)(b.px[x].a * 255 + 0.5f);
        }
    free(b.px);
    *ow = b.w;
    *oh = b.h;
    return out;
}

/* The debug menu (runtime/debug_menu.c) in the alerts' iPhone OS 3 dress:
 * the same translucent box, the title on top, one row per entry -- the
 * label on the left, its value on the right -- the selected row in the
 * iPhone's blue selection bar, rows that cannot act now in grey, and the
 * key help at the bottom. Under the title, one tab per page, the current
 * one in the selection blue. `s` is pixels per point of the menu's
 * 320-high space.
 * Returns RGBA8, premultiplied, *w x *h, malloc'd. */
unsigned char *plat_ui_menu(const char *title, const char *const *tabs,
                            int ntab, int tab, const char *const *label,
                            const char *const *value, const int *enabled,
                            int n, int sel, const char *footer, float width,
                            float s, int *ow, int *oh)
{
    al_box b;
    float m = 6 * s, top, rowh = 15 * s, x0, x1;
    int side = ntab < 0, nt = side ? -ntab : ntab;
    unsigned char *out;
    int i;

    memset(&b, 0, sizeof b);
    b.s = s;
    b.bx0 = m;
    b.by0 = m;
    b.bx1 = m + (width > 0 ? width : 360) * s;
    top = b.by0 + 22 * s + (ntab > 0 ? 20 * s : 0) + 4 * s;  /* under the title and tabs */
    b.by1 = top + n * rowh + 26 * s;
    if (side && b.by1 < top + (nt * 24 + 8 + UI_SIDE) * s + 26 * s)
        b.by1 = top + (nt * 24 + 8 + UI_SIDE) * s + 26 * s;   /* room for the icon */
    b.w = (int)(b.bx1 + m + 1);
    b.h = (int)(b.by1 + m + 1);
    b.px = (al_px *)malloc(sizeof(al_px) * b.w * b.h);
    al_draw_bg(&b);

    al_say(&b, title, (int)(13 * s), 1, (b.bx0 + b.bx1) * 0.5f,
           b.by0 + 12 * s, 1, 1, 1, 1, 1);

    x0 = b.bx0 + 9 * s;
    x1 = b.bx1 - 9 * s;
    if (side) {                             /* the side panel */
        float px0 = b.bx0 + 6 * s, px1 = b.bx0 + (UI_SIDE - 4) * s;
        float py0 = top, py1 = b.by1 - 24 * s;
        int x, y;

        for (y = (int)py0; y <= (int)py1; y++)
            for (x = (int)px0; x <= (int)px1; x++) {
                float c = al_rr((float)x, (float)y, px0, py0, px1, py1, 6 * s);
                if (c > 0 && x >= 0 && x < b.w && y >= 0 && y < b.h)
                    al_over(&b.px[y * b.w + x], 0, 0, 0.04f, c * 0.45f);
            }
        for (i = 0; i < nt; i++) {
            float ty0 = top + (4 + i * 24) * s, ty1 = ty0 + 20 * s;
            if (i == tab)
                for (y = (int)ty0; y <= (int)ty1; y++)
                    for (x = (int)px0 + 2; x <= (int)px1 - 2; x++) {
                        float c = al_rr((float)x, (float)y, px0 + 3 * s, ty0, px1 - 3 * s, ty1, 5 * s);
                        float t = (y + 0.5f - ty0) / (ty1 - ty0);
                        if (c > 0 && x >= 0 && x < b.w && y >= 0 && y < b.h)
                            al_over(&b.px[y * b.w + x], 0.02f + 0.03f * (1 - t),
                                    0.36f + 0.20f * (1 - t), 0.90f + 0.06f * (1 - t), c * 0.95f);
                    }
            al_say(&b, tabs[i], (int)(10.5f * s), 1, px0 + 10 * s, (ty0 + ty1) * 0.5f,
                   0, 1, 1, 1, i == tab ? 1.0f : 0.7f);
        }
        x0 = b.bx0 + (UI_SIDE + 6) * s;
        nt = 0;
    }
    for (i = 0; i < nt; i++) {              /* the tab strip */
        float tw = (x1 - x0) / ntab, tx0 = x0 + i * tw + 1 * s,
              tx1 = x0 + (i + 1) * tw - 1 * s, ty0 = b.by0 + 22 * s,
              ty1 = ty0 + 18 * s;
        int x, y;
        for (y = (int)ty0; y < (int)ty1 + 1; y++)
            for (x = (int)tx0 - 1; x <= (int)tx1 + 1; x++) {
                float c = al_rr((float)x, (float)y, tx0, ty0, tx1, ty1, 5 * s);
                float t = (y + 0.5f - ty0) / (ty1 - ty0);
                if (c <= 0 || x < 0 || x >= b.w || y < 0 || y >= b.h)
                    continue;
                if (i == tab)
                    al_over(&b.px[y * b.w + x], 0.02f + 0.03f * (1 - t),
                            0.36f + 0.20f * (1 - t), 0.90f + 0.06f * (1 - t),
                            c * 0.95f);
                else
                    al_over(&b.px[y * b.w + x], 1, 1, 1, c * 0.12f);
            }
        al_say(&b, tabs[i], (int)((ntab > 4 ? 8.5f : 10) * s), 1, (tx0 + tx1) * 0.5f,
               (ty0 + ty1) * 0.5f, 1, 1, 1, 1, i == tab ? 1.0f : 0.75f);
    }
    for (i = 0; i < n; i++) {
        float y0 = top + i * rowh, a = enabled[i] ? 1.0f : 0.4f;
        if (i == sel) {                     /* the blue selection bar */
            int x, y;
            for (y = (int)y0; y < (int)(y0 + rowh); y++)
                for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
                    float c = al_rr((float)x, (float)y, x0, y0 + 0.5f * s,
                                    x1, y0 + rowh - 0.5f * s, 4 * s);
                    float t = (y + 0.5f - y0) / rowh;
                    if (c > 0 && x >= 0 && x < b.w)
                        al_over(&b.px[y * b.w + x],
                                0.02f + 0.03f * (1 - t), 0.36f + 0.20f * (1 - t),
                                0.90f + 0.06f * (1 - t), c * 0.92f);
                }
        }
        if (label[i][0] == 2) {             /* a section's heading */
            int x, y = (int)(y0 + rowh - 2 * s);
            al_say(&b, label[i] + 1, (int)(10 * s), 1, x0 + 4 * s, y0 + rowh * 0.55f,
                   0, 0.55f, 0.75f, 1.0f, 1);
            for (x = (int)(x0 + 4 * s); x < (int)(x1 - 4 * s) && y >= 0 && y < b.h; x++)
                al_over(&b.px[y * b.w + x], 0.55f, 0.75f, 1.0f,
                        0.45f * (1 - (x - x0) / (x1 - x0)));
            continue;
        }
        al_say(&b, label[i], (int)(11 * s), 1, x0 + 6 * s, y0 + rowh * 0.5f,
               0, 1, 1, 1, a);
        al_say(&b, value[i], (int)(11 * s), 0, x1 - 6 * s, y0 + rowh * 0.5f,
               2, 1, 1, 1, a);
    }
    al_say(&b, footer, (int)(9 * s), 0, (b.bx0 + b.bx1) * 0.5f,
           b.by1 - 13 * s, 1, 0.80f, 0.84f, 0.92f, 1);

    out = (unsigned char *)malloc((size_t)b.w * b.h * 4);
    for (i = 0; i < b.w * b.h; i++) {
        al_px *p = &b.px[i];
        out[i * 4 + 0] = (unsigned char)(p->r * 255 + 0.5f);
        out[i * 4 + 1] = (unsigned char)(p->g * 255 + 0.5f);
        out[i * 4 + 2] = (unsigned char)(p->b * 255 + 0.5f);
        out[i * 4 + 3] = (unsigned char)(p->a * 255 + 0.5f);
    }
    free(b.px);
    *ow = b.w;
    *oh = b.h;
    return out;
}

unsigned char *plat_ui_grid(const char *title,
                            const unsigned char *const *cells, int n,
                            int cols, int cpx, float cell, int sel,
                            const char *footer, float s, int *ow, int *oh)
{
    al_box b;
    float m = 6 * s, cs = cell * s, step = (cell + 4) * s, pad = 10 * s, top;
    int rows = cols > 0 ? (n + cols - 1) / cols : 0, i, x, y;
    unsigned char *out;

    if (n <= 0 || cols <= 0)
        return NULL;
    memset(&b, 0, sizeof b);
    b.s = s;
    b.bx0 = m;
    b.by0 = m;
    b.bx1 = m + 2 * pad + (cols - 1) * step + cs;
    top = b.by0 + 22 * s + 4 * s;
    b.by1 = top + (rows - 1) * step + cs + 26 * s;
    b.w = (int)(b.bx1 + m + 1);
    b.h = (int)(b.by1 + m + 1);
    b.px = (al_px *)malloc(sizeof(al_px) * b.w * b.h);
    if (!b.px)
        return NULL;
    al_draw_bg(&b);
    al_say(&b, title, (int)(13 * s), 1, (b.bx0 + b.bx1) * 0.5f,
           b.by0 + 12 * s, 1, 1, 1, 1, 1);
    for (i = 0; i < n; i++) {
        float x0 = b.bx0 + pad + (i % cols) * step, y0 = top + (i / cols) * step;
        al_glyph g;

        if (i == sel)                   /* the selection blue round it */
            for (y = (int)(y0 - 3 * s); y <= (int)(y0 + cs + 3 * s); y++)
                for (x = (int)(x0 - 3 * s); x <= (int)(x0 + cs + 3 * s); x++) {
                    float c = al_rr((float)x, (float)y, x0 - 2.5f * s, y0 - 2.5f * s,
                                    x0 + cs + 2.5f * s, y0 + cs + 2.5f * s, cs * 0.24f);
                    if (c > 0 && x >= 0 && x < b.w && y >= 0 && y < b.h)
                        al_over(&b.px[y * b.w + x], 0.15f, 0.55f, 1.0f, c * 0.95f);
                }
        if (!cells[i])
            continue;
        memset(&g, 0, sizeof g);
        g.rgba = (uint8_t *)cells[i];
        g.w = g.h = cpx;
        al_put_glyph(&b, &g, (int)x0, (int)y0, (int)cs, 1);
    }
    al_say(&b, footer, (int)(9 * s), 0, (b.bx0 + b.bx1) * 0.5f,
           top + (rows - 1) * step + cs + 13 * s, 1, 0.80f, 0.84f, 0.92f, 1);

    out = (unsigned char *)malloc((size_t)b.w * b.h * 4);
    for (i = 0; out && i < b.w * b.h; i++) {
        al_px *p = &b.px[i];
        out[i * 4 + 0] = (unsigned char)(p->r * 255 + 0.5f);
        out[i * 4 + 1] = (unsigned char)(p->g * 255 + 0.5f);
        out[i * 4 + 2] = (unsigned char)(p->b * 255 + 0.5f);
        out[i * 4 + 3] = (unsigned char)(p->a * 255 + 0.5f);
    }
    free(b.px);
    *ow = b.w;
    *oh = b.h;
    return out;
}

static int al_pot(int n)
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

/* upload w x h RGBA8 into a power-of-two texture (GL 1.1) */
static GLuint al_upload(const unsigned char *rgba, int w, int h)
{
    GLuint t;
    int pw = al_pot(w), ph = al_pot(h);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba);
    return t;
}

static GLuint al_box_tex(al_box *b)
{
    unsigned char *p = (unsigned char *)malloc((size_t)b->w * b->h * 4);
    GLuint t;
    int i;

    for (i = 0; i < b->w * b->h; i++) {
        al_px *s = &b->px[i];
        p[i * 4 + 0] = (unsigned char)(s->r * 255 + 0.5f);
        p[i * 4 + 1] = (unsigned char)(s->g * 255 + 0.5f);
        p[i * 4 + 2] = (unsigned char)(s->b * 255 + 0.5f);
        p[i * 4 + 3] = (unsigned char)(s->a * 255 + 0.5f);
    }
    t = al_upload(p, b->w, b->h);
    free(p);
    return t;
}

static void al_quad(GLuint t, float x0, float y0, float x1, float y1,
                    float u1, float v1, int flip)
{
    glBindTexture(GL_TEXTURE_2D, t);
    glBegin(GL_QUADS);
    glTexCoord2f(0, flip ? v1 : 0);  glVertex2f(x0, y0);
    glTexCoord2f(u1, flip ? v1 : 0); glVertex2f(x1, y0);
    glTexCoord2f(u1, flip ? 0 : v1); glVertex2f(x1, y1);
    glTexCoord2f(0, flip ? 0 : v1);  glVertex2f(x0, y1);
    glEnd();
}

static void al_layout(al_box *b, int ww, int wh, const wchar_t *msg)
{
    float s = (float)ww / 480.0f, sy = (float)wh / 320.0f;
    float m, bw, top, by;
    int th;
    unsigned char *mask;

    if (sy < s)
        s = sy;
    s *= 0.8f;                  /* a little smaller than the device's */
    b->s = s;
    m = 8 * s;
    bw = 284 * s;
    mask = al_text(msg, (int)(bw - 24 * s), (int)(18 * s), &th);
    free(mask);
    b->bx0 = m;
    b->by0 = m;
    b->bx1 = m + bw;
    top = 18 * s + th + 16 * s;             /* the text, then the buttons */
    by = b->by0 + top;
    b->by1 = by + 43 * s + 14 * s;
    b->w = (int)(bw + 2 * m + 1);
    b->h = (int)(b->by1 + m + 1);
    if (b->nbtn == 2) {
        float half = (bw - 22 * s - 8 * s) * 0.5f;
        b->btn[0][0] = b->bx0 + 11 * s;  b->btn[0][2] = b->btn[0][0] + half;
        b->btn[1][0] = b->btn[0][2] + 8 * s;  b->btn[1][2] = b->bx1 - 11 * s;
    } else {
        b->btn[0][0] = b->bx0 + 11 * s;  b->btn[0][2] = b->bx1 - 11 * s;
    }
    b->btn[0][1] = b->btn[1][1] = by;
    b->btn[0][3] = b->btn[1][3] = by + 43 * s;
    b->px = (al_px *)malloc(sizeof(al_px) * b->w * b->h);
}

int plat_ask(const unsigned short *msg16, const unsigned short *ok16,
             const unsigned short *cancel16)
{
    const wchar_t *msg = (const wchar_t *)msg16;
    const wchar_t *ok = ok16 ? (const wchar_t *)ok16 : L"OK";
    const wchar_t *cancel = (const wchar_t *)cancel16;
    int ww, wh, bgw, bgh, pressed = -1, drawn = -2, result = -1;
    int armed_mouse = 0, armed_keys = 0, press_on = -1;
    GLint vp[4];
    unsigned char *bg;
    GLuint bgt, boxt = 0;
    al_box b;

    plat_size(&ww, &wh);
    if (ww <= 0 || wh <= 0)
        return 0;
    if (plat_ask_hook)
        plat_ask_hook(1);
    bgw = ww;
    bgh = wh;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glGetIntegerv(GL_VIEWPORT, vp);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    /* the frame the game had drawn so far: the back buffer, bottom-up */
    bg = (unsigned char *)malloc((size_t)bgw * bgh * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, bgw, bgh, GL_RGBA, GL_UNSIGNED_BYTE, bg);
    {
        int i;
        for (i = 0; i < bgw * bgh; i++)
            bg[i * 4 + 3] = 255;
    }
    bgt = al_upload(bg, bgw, bgh);
    free(bg);

    memset(&b, 0, sizeof b);
    b.nbtn = cancel ? 2 : 1;
    al_layout(&b, ww, wh, msg);

    for (;;) {
        int mx, my, down, cw, ch, over = -1, k;
        float ox, oy;

        if (!plat_poll()) {
            result = b.nbtn - 1;
            break;
        }
        plat_size(&cw, &ch);
        if (cw > 0 && ch > 0 && (cw != ww || ch != wh)) {
            ww = cw;
            wh = ch;
            free(b.px);
            {
                int n = b.nbtn;
                memset(&b, 0, sizeof b);
                b.nbtn = n;
            }
            al_layout(&b, ww, wh, msg);
            drawn = -2;
        }
        ox = (float)(int)((ww - b.w) * 0.5f);
        oy = (float)(int)((wh - b.h) * 0.5f);

        down = plat_mouse(&mx, &my) && g_focused;
        for (k = 0; k < b.nbtn; k++)
            if (mx >= ox + b.btn[k][0] && mx < ox + b.btn[k][2] &&
                my >= oy + b.btn[k][1] && my < oy + b.btn[k][3])
                over = k;
        if (!down)
            armed_mouse = 1;    /* the click that opened it does not count */
        if (armed_mouse) {
            if (down && press_on < 0)
                press_on = over >= 0 ? over : 9;
            if (!down && press_on >= 0) {
                if (press_on == over) {
                    result = over;
                    break;
                }
                press_on = -1;
            }
        }
        pressed = (down && press_on >= 0 && press_on == over) ? over : -1;

        if (!g_key[VK_RETURN] && !g_key[VK_ESCAPE] && !g_key[VK_SPACE])
            armed_keys = 1;
        if (armed_keys && (g_key[VK_RETURN] || g_key[VK_SPACE])) {
            result = 0;
            break;
        }
        if (armed_keys && g_key[VK_ESCAPE]) {
            result = b.nbtn - 1;
            break;
        }

        if (pressed != drawn) {
            if (boxt)
                glDeleteTextures(1, &boxt);
            al_draw(&b, msg, ok, cancel, pressed);
            boxt = al_box_tex(&b);
            drawn = pressed;
        }

        glViewport(0, 0, ww, wh);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, ww, wh, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_FOG);
        glDisable(GL_LIGHTING);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor4f(1, 1, 1, 1);
        al_quad(bgt, 0, 0, (float)ww, (float)wh,
                (float)bgw / al_pot(bgw), (float)bgh / al_pot(bgh), 1);
        glEnable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0, 0, 0, 0.3f);           /* the screen dims behind it */
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f((float)ww, 0);
        glVertex2f((float)ww, (float)wh); glVertex2f(0, (float)wh);
        glEnd();
        glEnable(GL_TEXTURE_2D);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   /* premultiplied */
        glColor4f(1, 1, 1, 1);
        al_quad(boxt, ox, oy, ox + b.w, oy + b.h,
                (float)b.w / al_pot(b.w), (float)b.h / al_pot(b.h), 0);
        /* UMK3_ALERT_SHOT=<file.ppm>: a scripted test saves the dialog and
         * answers OK, since nothing else runs while it is up */
        if (getenv("UMK3_ALERT_SHOT")) {
            FILE *f = fopen(getenv("UMK3_ALERT_SHOT"), "wb");
            unsigned char *p = (unsigned char *)malloc((size_t)ww * wh * 3);
            int r;
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, ww, wh, GL_RGB, GL_UNSIGNED_BYTE, p);
            if (f && p) {
                fprintf(f, "P6\n%d %d\n255\n", ww, wh);
                for (r = wh - 1; r >= 0; r--)
                    fwrite(p + (size_t)r * ww * 3, 1, (size_t)ww * 3, f);
            }
            if (f)
                fclose(f);
            free(p);
            result = 0;
            break;
        }
        plat_swap();
        Sleep(15);
    }

    /* Put the game's half-drawn frame back in the back buffer, so the rest
     * of this frame lands on it rather than on the dialog. */
    glViewport(0, 0, ww, wh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, ww, wh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
    al_quad(bgt, 0, 0, (float)ww, (float)wh,
            (float)bgw / al_pot(bgw), (float)bgh / al_pot(bgh), 1);

    glDeleteTextures(1, &bgt);
    if (boxt)
        glDeleteTextures(1, &boxt);
    free(b.px);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    if (plat_ask_hook)
        plat_ask_hook(0);
    return result < 0 ? 0 : result;
}

void plat_language(char *out, int n)
{
    if (n <= 0)
        return;
    out[0] = 0;
    GetLocaleInfoA(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT),
                   LOCALE_SISO639LANGNAME, out, n);
}
