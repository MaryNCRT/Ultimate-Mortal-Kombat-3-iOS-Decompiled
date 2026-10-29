/*
 * SDL2 backend -- the portable path, and the reason platform.h never exposes
 * an HWND.
 *
 * It asks for a compatibility context on purpose. The engine is OpenGL ES 1.1
 * fixed function throughout (77 entry points, no shaders anywhere -- see
 * docs/LIME-ENGINE.md), so the slice draws with glMatrixMode, glVertexPointer
 * and friends, which only exist in a compatibility profile. A core-profile
 * renderer that emulates the fixed-function pipeline is the shipping plan;
 * this is not it.
 *
 * The window is resizable and reports its drawable size rather than the
 * requested one, so high-DPI displays get the right viewport.
 */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE                     /* popen, for fontconfig */
#endif
#include <math.h>
#include <stdio.h>
#include "platform.h"
#include "gl.h"
#include "ui_alert.h"
#include "../render/rgl.h"

#include <SDL.h>
#ifndef __EMSCRIPTEN__
#include <SDL_vulkan.h>
#endif
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static SDL_Window   *g_wnd;
static SDL_GLContext g_ctx;
static bool          g_quit;
static bool          g_focused = true;
static int           g_wheel;           /* notches not yet taken */
static Uint64        g_start, g_freq;
static SDL_GameController *g_controller;
/* Typed text for plat_text_take, one character (UTF-8) per entry, with
 * Backspace, Enter, Esc and Ctrl+V as 8, 13, 27 and 22, as Windows' WM_CHAR
 * gives them. Port, not in the binary. */
static char g_text[64][8];
static int  g_text_n;

static void text_push(const char *c, int len)
{
    if (g_text_n >= (int)(sizeof g_text / sizeof g_text[0]) || len <= 0 || len > 7)
        return;
    memcpy(g_text[g_text_n], c, (size_t)len);
    g_text[g_text_n][len] = 0;
    g_text_n++;
}

/* Vulkan (umk3.ini renderer=vulkan, render/rgl.h): the window is made for
 * it and SDL gives its surface; without it the window is made again for
 * OpenGL. */
static int g_vk;

static const char *const *vk_exts(unsigned *n)
{
    static const char *names[16];
    unsigned cnt = 16;
    if (!SDL_Vulkan_GetInstanceExtensions(g_wnd, &cnt, names))
        cnt = 0;
    *n = cnt;
    return names;
}

static int vk_surface(void *window, void *instance, void *surface)
{
    return SDL_Vulkan_CreateSurface((SDL_Window *)window, (VkInstance)instance,
                                    (VkSurfaceKHR *)surface) == SDL_TRUE;
}

static void drawable(int *w, int *h)
{
    if (g_vk)
        SDL_Vulkan_GetDrawableSize(g_wnd, w, h);
    else
        SDL_GL_GetDrawableSize(g_wnd, w, h);
}

bool plat_open(const char *title, int width, int height)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }

#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

#ifndef __EMSCRIPTEN__
    if (rgl_renderer == RGL_VULKAN) {
        g_wnd = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                 SDL_WINDOW_ALLOW_HIGHDPI);
        if (g_wnd) {
            g_vk = 1;
            rgl_vk_instance_exts = vk_exts;
            rgl_vk_make_surface = vk_surface;
            if (rgl_open(g_wnd, width, height)) {
                SDL_StartTextInput();
                g_quit = false;
                g_focused = true;
                g_freq = SDL_GetPerformanceFrequency();
                g_start = SDL_GetPerformanceCounter();
                return true;
            }
            g_vk = 0;
            SDL_DestroyWindow(g_wnd);
            g_wnd = NULL;
        }
        rgl_renderer = RGL_OPENGL;
    }
#endif
    g_wnd = SDL_CreateWindow(title,
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             width, height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                             SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_wnd) {
        SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    g_ctx = SDL_GL_CreateContext(g_wnd);
    if (!g_ctx) {
        SDL_Log("SDL_GL_CreateContext: %s", SDL_GetError());
        SDL_DestroyWindow(g_wnd);
        g_wnd = NULL;
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(g_wnd, g_ctx);
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput();               /* SDL_TEXTINPUT: plat_text_take */

    int dw, dh;
    SDL_GL_GetDrawableSize(g_wnd, &dw, &dh);
    glViewport(0, 0, dw, dh);

    g_quit  = false;
    g_focused = true;
    g_freq  = SDL_GetPerformanceFrequency();
    g_start = SDL_GetPerformanceCounter();
    return true;
}

bool plat_focused(void)
{
    return g_focused;
}

void plat_fullscreen(void)
{
    SDL_SetWindowFullscreen(g_wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

int plat_open_borderless;

void plat_fullscreen_exclusive(void)
{
    if (SDL_SetWindowFullscreen(g_wnd, SDL_WINDOW_FULLSCREEN) != 0)
        plat_fullscreen();
}

void plat_display(int mode, int w, int h)
{
    if (mode == 0) {
        SDL_SetWindowFullscreen(g_wnd, 0);
        SDL_SetWindowSize(g_wnd, w, h);
        SDL_SetWindowPosition(g_wnd, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
    } else if (mode == 1)
        plat_fullscreen();
    else {
        SDL_SetWindowSize(g_wnd, w, h);
        plat_fullscreen_exclusive();
    }
}

void plat_quit(void)
{
    g_quit = true;
}

int plat_text_take(char *out, int n)
{
    int len;

    if (n > 0)
        out[0] = 0;
    if (g_text_n == 0 || n < 5)
        return 0;
    len = (int)strlen(g_text[0]);
    if (len > n - 1)
        len = n - 1;
    memcpy(out, g_text[0], (size_t)len);
    out[len] = 0;
    g_text_n--;
    memmove(g_text, g_text + 1, (size_t)g_text_n * sizeof g_text[0]);
    return len;
}

void plat_text_clear(void)
{
    g_text_n = 0;
}

int plat_clipboard_text(char *out, int n)
{
    char *c;
    int len = 0;

    if (n <= 0)
        return 0;
    out[0] = 0;
    c = SDL_GetClipboardText();
    if (c) {
        snprintf(out, (size_t)n, "%s", c);
        len = (int)strlen(out);
        SDL_free(c);
    }
    return len;
}

int plat_clipboard_set(const char *utf8)
{
    return SDL_SetClipboardText(utf8) == 0;
}

void *plat_gl_proc(const char *name)
{
    if (rgl_renderer != RGL_OPENGL)
        return rgl_proc(name);
    return SDL_GL_GetProcAddress(name);
}

void (*plat_ask_hook)(int begin);

bool plat_poll(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            g_quit = true;
            break;
        case SDL_KEYDOWN: {
            SDL_Keycode k = ev.key.keysym.sym;
            if (k == SDLK_BACKSPACE)
                text_push("\b", 1);
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                text_push("\r", 1);
            else if (k == SDLK_ESCAPE)
                text_push("\033", 1);
            else if (k == SDLK_v && (ev.key.keysym.mod & KMOD_CTRL))
                text_push("\026", 1);
            break;
        }
        case SDL_TEXTINPUT: {
            /* one entry per character: split the UTF-8 */
            const unsigned char *t = (const unsigned char *)ev.text.text;
            while (*t) {
                int len = *t < 0x80 ? 1 : *t < 0xe0 ? 2 : *t < 0xf0 ? 3 : 4;
                text_push((const char *)t, len);
                t += len;
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            g_wheel += ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                       ? -ev.wheel.y : ev.wheel.y;
            break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                g_quit = true;
            } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                g_focused = false;
            } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                g_focused = true;
            } else if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                int dw, dh;
                drawable(&dw, &dh);
                if (dw > 0 && dh > 0) glViewport(0, 0, dw, dh);
            }
            break;
        default:
            break;
        }
        if (ev.type == SDL_CONTROLLERDEVICEADDED && !g_controller &&
            SDL_IsGameController(ev.cdevice.which))
            g_controller = SDL_GameControllerOpen(ev.cdevice.which);
        else if (ev.type == SDL_CONTROLLERDEVICEREMOVED && g_controller &&
                 SDL_JoystickInstanceID(
                     SDL_GameControllerGetJoystick(g_controller)) ==
                     ev.cdevice.which) {
            SDL_GameControllerClose(g_controller);
            g_controller = NULL;
        }
    }
#ifdef __EMSCRIPTEN__
    emscripten_sleep(16);
#endif
    return !g_quit;
}

bool plat_swap(void)
{
    if (!g_wnd) return false;
    if (rgl_renderer != RGL_OPENGL) {
        rgl_present();
        return true;
    }
    {   /* VSYNC (rgl_vsync), when it changes */
        static int set = 1;
        if (set != rgl_vsync)
            SDL_GL_SetSwapInterval((set = rgl_vsync) ? 1 : 0);
    }
    SDL_GL_SwapWindow(g_wnd);
    return true;
}

void plat_size(int *width, int *height)
{
    int dw = 0, dh = 0;
    if (g_wnd) drawable(&dw, &dh);
    if (width)  *width  = dw;
    if (height) *height = dh;
}

int plat_mouse(int *x, int *y)
{
    int mx, my, ww, wh, dw, dh;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    SDL_GetWindowSize(g_wnd, &ww, &wh);
    drawable(&dw, &dh);
    if (ww <= 0) ww = 1;
    if (wh <= 0) wh = 1;
    if (x) *x = mx * dw / ww;
    if (y) *y = my * dh / wh;
    return (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
}

int plat_mouse_wheel(void)
{
    int n = g_wheel;

    g_wheel = 0;
    return n;
}

static SDL_Scancode g_scancode[PK_COUNT] = {
    SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
    SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O,
    SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
    SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6,
    SDL_SCANCODE_F5,
    SDL_SCANCODE_F1, SDL_SCANCODE_RETURN, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_LEFT, SDL_SCANCODE_F3, SDL_SCANCODE_F2,
    SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
    SDL_SCANCODE_P, SDL_SCANCODE_M,
    SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8,
    SDL_SCANCODE_H, SDL_SCANCODE_KP_0,
    SDL_SCANCODE_U, SDL_SCANCODE_O, SDL_SCANCODE_J, SDL_SCANCODE_L,
    SDL_SCANCODE_Q, SDL_SCANCODE_E,
    [PK_SETTINGS] = SDL_SCANCODE_ESCAPE,
    [PK_P2_PAUSE] = SDL_SCANCODE_KP_DIVIDE,
    [PK_P2_MOVES] = SDL_SCANCODE_KP_MULTIPLY,
    [PK_P2_OK] = SDL_SCANCODE_KP_1,
};

void plat_key_label(int code, char *out, int n)
{
    const char *s = (code >= 0 && code < PK_COUNT)
                    ? SDL_GetScancodeName(g_scancode[code]) : "";
    char *p;

    if (n <= 0)
        return;
    snprintf(out, (size_t)n, "%s", s);
    for (p = out; *p; p++)
        if (*p >= 'a' && *p <= 'z')
            *p = (char)(*p - 32);
}

/* The Vintage keyboard pictures' names (aditional textures/glyphs/kb), by
 * SDL scancode -- the USB HID usage, the same on every system. */
static const char *key_glyph_file(int sc, char *tmp)
{
    if (sc >= 4 && sc <= 29) {                  /* A..Z */
        tmp[0] = (char)('A' + sc - 4);
        tmp[1] = 0;
        return tmp;
    }
    if (sc >= 30 && sc <= 39) {                 /* 1..9, 0 */
        tmp[0] = (char)(sc == 39 ? '0' : '1' + sc - 30);
        tmp[1] = 0;
        return tmp;
    }
    if (sc >= 89 && sc <= 98) {                 /* the keypad's 1..9, 0 */
        tmp[0] = (char)(sc == 98 ? '0' : '1' + sc - 89);
        tmp[1] = 0;
        return tmp;
    }
    if (sc >= 58 && sc <= 69) {                 /* F1..F12 */
        snprintf(tmp, 8, "F%d", sc - 58 + 1);
        return tmp;
    }
    switch (sc) {
    case 82: return "Up";
    case 81: return "Down";
    case 80: return "Left";
    case 79: return "Right";
    case 40: case 88: return "Enter";
    case 41: return "Esc";
    case 44: return "Space";
    case 43: return "Tab";
    case 42: return "BackSpace";
    case 225: case 229: return "Shift";
    case 224: case 228: return "Crtl";
    case 226: case 230: return "Alt";
    case 57: return "CapsLock";
    case 76: case 99: return "Del";
    case 73: return "Ins";
    case 74: return "Home";
    case 77: return "End";
    case 75: return "PageUp";
    case 78: return "PageDown";
    case 45: case 86: return "Minus";
    case 46: case 87: return "Plus";
    case 85: return "Asterisk";
    case 51: return "Semicolon";
    case 56: case 84: return "Slash";
    case 53: return "Tilde";
    case 47: return "Brackets_L";
    case 48: return "Brackets_R";
    case 52: return "Quotation";
    case 70: return "PrtScrn";
    case 83: return "NumLock";
    }
    return NULL;
}

void plat_key_glyph(int code, char *out, int n)
{
    int sc = (code >= 0 && code < PK_COUNT) ? (int)g_scancode[code] : 0;
    char tmp[8], name[64];
    const char *f = key_glyph_file(sc, tmp);

    if (n <= 0)
        return;
    if (f) {
        snprintf(name, sizeof name, "kb/T_%s_Key_Vintage", f);
        if (ui_glyph_have(name)) {
            /* the keypad's keys are told from the row's by a NUM */
            snprintf(out, (size_t)n, "%s\001%s\001",
                     sc >= 84 && sc <= 99 ? "NUM " : "", name);
            return;
        }
    }
    plat_key_label(code, out, n);
}

void plat_monitor_select(int i) { (void)i; }
int  plat_monitor(void) { return 0; }
int  plat_monitor_count(void) { return 1; }
void plat_monitor_name(int i, char *out, int n)
{
    if (n > 0)
        snprintf(out, (size_t)n, "%d", i);
}
void plat_monitor_list(void) { printf("1\t1\n"); }

/* The keys in umk3.ini, the launcher's CONTROLS (the browser's keyCode) and
 * the settings menu's defaults are Windows virtual-key codes on every system.
 * Read here as SDL scancodes, 'A' (65) was F8 and every key the launcher
 * bound on Linux / Steam Deck landed on another one. So this backend takes
 * and gives virtual-key codes and keeps the scancode inside. */
static int vk_to_scancode(int vk)
{
    static const unsigned char other[][2] = {
        { 0x0d, 40 }, { 0x1b, 41 }, { 0x08, 42 }, { 0x09, 43 }, { 0x20, 44 },
        { 0x25, 80 }, { 0x26, 82 }, { 0x27, 79 }, { 0x28, 81 },
        { 0x2d, 73 }, { 0x2e, 76 }, { 0x24, 74 }, { 0x23, 77 },
        { 0x21, 75 }, { 0x22, 78 },
        { 0x6a, 85 }, { 0x6b, 87 }, { 0x6d, 86 }, { 0x6e, 99 }, { 0x6f, 84 },
        { 0x10, 225 }, { 0x11, 224 }, { 0x12, 226 },
        { 0xa0, 225 }, { 0xa1, 229 }, { 0xa2, 224 }, { 0xa3, 228 },
        { 0xa4, 226 }, { 0xa5, 230 },
        { 0xba, 51 }, { 0xbb, 46 }, { 0xbc, 54 }, { 0xbd, 45 }, { 0xbe, 55 },
        { 0xbf, 56 }, { 0xc0, 53 }, { 0xdb, 47 }, { 0xdc, 49 }, { 0xdd, 48 },
        { 0xde, 52 },
        { 0x14, 57 }, { 0x13, 72 }, { 0x91, 71 }, { 0x90, 83 }, { 0x2c, 70 },
    };
    size_t i;

    if (vk >= 'A' && vk <= 'Z') return 4 + vk - 'A';
    if (vk >= '1' && vk <= '9') return 30 + vk - '1';
    if (vk == '0') return 39;
    if (vk >= 0x70 && vk <= 0x7b) return 58 + vk - 0x70;     /* F1..F12 */
    if (vk >= 0x61 && vk <= 0x69) return 89 + vk - 0x61;     /* keypad 1..9 */
    if (vk == 0x60) return 98;                               /* keypad 0 */
    for (i = 0; i < sizeof other / sizeof other[0]; i++)
        if (other[i][0] == vk)
            return other[i][1];
    return 0;
}

static int scancode_to_vk(int sc)
{
    int vk;

    for (vk = 1; vk < 256; vk++)
        if (vk_to_scancode(vk) == sc)
            return vk;
    return 0;
}

void plat_bind_key(int code, int key)
{
    int sc = vk_to_scancode(key);

    if (code >= 0 && code < PK_COUNT && sc > 0)
        g_scancode[code] = (SDL_Scancode)sc;
}

int plat_key_bound(int code)
{
    return code >= 0 && code < PK_COUNT ? scancode_to_vk((int)g_scancode[code]) : 0;
}

int plat_key_backspace(void)
{
    return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_BACKSPACE] ? 1 : 0;
}

int plat_key_any(void)
{
    int count, i;
    const Uint8 *keys = SDL_GetKeyboardState(&count);

    for (i = 1; i < count; i++)
        if (keys[i] && scancode_to_vk(i))
            return scancode_to_vk(i);   /* as a virtual-key code */
    return 0;
}

int plat_key(int code)
{
    const Uint8 *keys;
    int count;

    if (code < 0 || code >= PK_COUNT)
        return 0;
    keys = SDL_GetKeyboardState(&count);
    return g_scancode[code] < count && keys[g_scancode[code]] != 0;
}

int plat_refresh_hz(void)
{
    SDL_DisplayMode m;
    if (g_wnd && SDL_GetWindowDisplayMode(g_wnd, &m) == 0)
        return m.refresh_rate;
    return 0;
}

void plat_input_used(void)
{
}

void plat_wait_until(double t)
{
    double left = t - plat_time();
    if (left > 0.002)
        SDL_Delay((Uint32)((left - 0.001) * 1000.0));
    while (plat_time() < t)
        ;
}

int plat_pad(int which)
{
    SDL_GameController *pad = g_controller;
    Sint16 x, y, trigger;
    int bits = 0;

    if (which != 0)
        return -1;
    if (!pad) {
        int i;
        for (i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                pad = SDL_GameControllerOpen(i);
                if (pad) {
                    g_controller = pad;
                    break;
                }
            }
        }
    }
    if (!pad || !SDL_GameControllerGetAttached(pad))
        return -1;

    x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    trigger = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) ||
        y < -10000)
        bits |= 1 << 0;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
        y > 10000)
        bits |= 1 << 1;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
        x < -10000)
        bits |= 1 << 2;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
        x > 10000)
        bits |= 1 << 3;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X))
        bits |= 1 << 4;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
        bits |= 1 << 5;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        bits |= 1 << 6;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y))
        bits |= 1 << 7;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
        bits |= 1 << 8;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ||
        trigger > 8000)
        bits |= 1 << 9;
    return bits;
}

/* The PLAT_PAD read through SDL's game controller layer, which already names
 * the face buttons by position (SDL's A is the south one on every pad).
 * pad= picks "off" or a vendor:product; anything else takes the first pad. */
static char g_pad_want[32] = "auto";

void plat_pad_select(const char *which)
{
    snprintf(g_pad_want, sizeof g_pad_want, "%s",
             which && *which ? which : "auto");
}

/* Player two's pad: none on this backend (port). */
void plat_pad2_select(const char *which)
{
    (void)which;
}

int plat_pad2_read(PLAT_PAD *out)
{
    memset(out, 0, sizeof *out);
    return 0;
}

void plat_pad_list(void)
{
    int i;

    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    for (i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) {
            SDL_GameController *c = SDL_GameControllerOpen(i);
            if (!c)
                continue;
            printf("%04x:%04x\t%s\n", SDL_GameControllerGetVendor(c),
                   SDL_GameControllerGetProduct(c),
                   SDL_GameControllerName(c) ? SDL_GameControllerName(c)
                                             : "pad");
            SDL_GameControllerClose(c);
        }
    fflush(stdout);
}

int plat_pad_read(PLAT_PAD *out)
{
    static const SDL_GameControllerButton b[PB_COUNT] = {
        SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        SDL_CONTROLLER_BUTTON_GUIDE,
    };
    SDL_GameController *pad;
    unsigned v, p;
    int i;

    memset(out, 0, sizeof *out);
    if (strcmp(g_pad_want, "off") == 0 || strcmp(g_pad_want, "keyboard") == 0)
        return 0;
    if (plat_pad(0) < 0)                /* opens the first pad if needed */
        return 0;
    pad = g_controller;
    if (sscanf(g_pad_want, "%x:%x", &v, &p) == 2
        && (SDL_GameControllerGetVendor(pad) != v
            || SDL_GameControllerGetProduct(pad) != p))
        return 0;
    for (i = 0; i < PB_COUNT; i++)
        if (b[i] != SDL_CONTROLLER_BUTTON_INVALID
            && SDL_GameControllerGetButton(pad, b[i]))
            out->buttons |= 1u << i;
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000)
        out->buttons |= 1u << PB_L2;
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000)
        out->buttons |= 1u << PB_R2;
    i = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
    out->rx = i < -20000 ? -1 : i > 20000 ? 1 : 0;
    i = plat_pad(0);
    out->up = (i >> 0) & 1;
    out->down = (i >> 1) & 1;
    out->left = (i >> 2) & 1;
    out->right = (i >> 3) & 1;
    snprintf(out->name, sizeof out->name, "%s",
             SDL_GameControllerName(pad) ? SDL_GameControllerName(pad) : "pad");
    v = SDL_GameControllerGetVendor(pad);       /* the pictures to show */
    out->kind = v == 0x054c ? plat_pad_sony_kind(SDL_GameControllerGetProduct(pad))
              : v == 0x057e ? 3 : 1;
    return out->connected = 1;
}

double plat_time(void)
{
    if (!g_freq) return 0.0;
    return (double)(SDL_GetPerformanceCounter() - g_start) / (double)g_freq;
}

void plat_close(void)
{
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
    rgl_close();
    if (g_ctx) { SDL_GL_DeleteContext(g_ctx); g_ctx = NULL; }
    if (g_wnd) { SDL_DestroyWindow(g_wnd); g_wnd = NULL; }
    SDL_Quit();
}


/* UTF-16 (the game's strings) to UTF-8 (SDL's), BMP only -- the game's text
 * has no surrogate pairs. */
static void utf16_to_utf8(const unsigned short *in, char *out, size_t n)
{
    size_t o = 0;

    for (; in && *in && o + 4 < n; in++) {
        unsigned c = *in;
        if (c < 0x80) {
            out[o++] = (char)c;
        } else if (c < 0x800) {
            out[o++] = (char)(0xc0 | (c >> 6));
            out[o++] = (char)(0x80 | (c & 0x3f));
        } else {
            out[o++] = (char)(0xe0 | (c >> 12));
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3f));
            out[o++] = (char)(0x80 | (c & 0x3f));
        }
    }
    out[o] = 0;
}

/* The system's own dialog: plat_ask (ui_alert.c) when there is no font. */
int ui_ask_native(const unsigned short *msg, const unsigned short *ok,
                  const unsigned short *cancel)
{
    char m[1024], a[128], b[128];
    SDL_MessageBoxButtonData btn[2];
    SDL_MessageBoxData box;
    int hit = -1;

    utf16_to_utf8(msg, m, sizeof m);
    utf16_to_utf8(ok, a, sizeof a);
    utf16_to_utf8(cancel, b, sizeof b);

    btn[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    btn[0].buttonid = 0;
    btn[0].text = a;
    btn[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    btn[1].buttonid = 1;
    btn[1].text = b;

    memset(&box, 0, sizeof box);
    box.flags = SDL_MESSAGEBOX_INFORMATION;
    box.window = g_wnd;
    box.title = "Ultimate Mortal Kombat 3";
    box.message = m;
    box.numbuttons = cancel ? 2 : 1;
    box.buttons = btn;

    if (SDL_ShowMessageBox(&box, &hit) != 0 || hit < 0)
        return cancel ? 1 : 0;
    return hit;
}


/* ------------------------------------------------------------------- font
 *
 * The alerts' and menus' text (ui_alert.c), through stb_truetype (Sean
 * Barrett, public domain / MIT, platform/stb/). The font is the first found
 * of: "aditional textures/fonts/ui_bold.ttf" and "ui.ttf" beside res (the
 * player's own), then Liberation Sans (Arial's metrics, as Windows draws
 * them), DejaVu Sans and Noto Sans where the distributions put them, then
 * whatever fontconfig gives for Arial. With none, ui_font_ok() is 0 and the
 * menus use their own 5x7 font. */
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#if defined(__GNUC__)
#pragma GCC diagnostic push               /* its unused parts */
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

const char *lime_platform_asset_root(void);

static struct ui_font {
    int tried, ok;
    unsigned char *data;
    stbtt_fontinfo info;
} g_font[2];                            /* regular, bold */

static unsigned char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char *d = NULL;
    long n;

    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0
        && fseek(f, 0, SEEK_SET) == 0 && (d = (unsigned char *)malloc((size_t)n)) != NULL
        && fread(d, 1, (size_t)n, f) != (size_t)n) {
        free(d);
        d = NULL;
    }
    fclose(f);
    return d;
}

static struct ui_font *ui_font_get(int bold)
{
    static const char *const sys[2][12] = {
        {
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            NULL,
        }, {
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/liberation-sans/LiberationSans-Bold.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/TTF/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
            "/usr/share/fonts/noto/NotoSans-Bold.ttf",
            NULL,
        },
    };
    struct ui_font *f = &g_font[bold ? 1 : 0];
    char path[1200];
    const char *from = path;
    int i;

    if (f->tried)
        return f->ok ? f : (bold ? ui_font_get(0) : NULL);
    f->tried = 1;
    snprintf(path, sizeof path, "%s/../aditional textures/fonts/%s",
             lime_platform_asset_root(), bold ? "ui_bold.ttf" : "ui.ttf");
    f->data = read_file(path);
    for (i = 0; !f->data && sys[bold ? 1 : 0][i]; i++)
        f->data = read_file(from = sys[bold ? 1 : 0][i]);
#if defined(__linux__) && !defined(UMK3_SDL2_LINT)
    if (!f->data) {                     /* fontconfig's choice */
        FILE *p = popen(bold ? "fc-match -f '%{file}' 'Arial:bold' 2>/dev/null"
                             : "fc-match -f '%{file}' 'Arial' 2>/dev/null", "r");
        if (p) {
            if (fgets(path, sizeof path, p))
                f->data = read_file(from = path);
            pclose(p);
        }
    }
#endif
    if (f->data && stbtt_InitFont(&f->info, f->data,
                                  stbtt_GetFontOffsetForIndex(f->data, 0)))
        f->ok = 1;
    else {
        free(f->data);
        f->data = NULL;
    }
    if (f->ok)
        printf("ui font (%s): %s\n", bold ? "bold" : "regular", from);
    return f->ok ? f : (bold ? ui_font_get(0) : NULL);
}

int ui_font_ok(void)
{
    return ui_font_get(1) != NULL;
}

/* `px` to the em, as GDI's CreateFont(-px) */
static float ui_font_scale(struct ui_font *f, int px)
{
    return stbtt_ScaleForMappingEmToPixels(&f->info, (float)px);
}

int ui_font_height(int px, int bold)
{
    struct ui_font *f = ui_font_get(bold);
    int asc, desc, gap;

    if (!f)
        return 0;
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
    return (int)ceilf((asc - desc) * ui_font_scale(f, px));
}

int ui_font_width(const unsigned short *s, int n, int px, int bold)
{
    struct ui_font *f = ui_font_get(bold);
    float sc, x = 0;
    int i, adv, lsb;

    if (!f)
        return 0;
    sc = ui_font_scale(f, px);
    for (i = 0; i < n; i++) {
        stbtt_GetCodepointHMetrics(&f->info, s[i], &adv, &lsb);
        x += adv * sc;
        if (i + 1 < n)
            x += stbtt_GetCodepointKernAdvance(&f->info, s[i], s[i + 1]) * sc;
    }
    return (int)ceilf(x);
}

unsigned char *ui_font_line(const unsigned short *s, int n, int px, int bold,
                            int *w, int *h)
{
    struct ui_font *f = ui_font_get(bold);
    unsigned char *mask;
    float sc, x = 0;
    int asc, desc, gap, base, i;

    if (!f)
        return NULL;
    sc = ui_font_scale(f, px);
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
    base = (int)ceilf(asc * sc);
    *w = ui_font_width(s, n, px, bold) + 1;
    *h = ui_font_height(px, bold);
    if (*h < 1)
        *h = 1;
    mask = (unsigned char *)calloc((size_t)*w * *h, 1);
    if (!mask)
        return NULL;
    for (i = 0; i < n; i++) {
        int adv, lsb, x0, y0, x1, y1, gw, gh, gx, gy, X, Y;
        float shift = x - (float)floor(x);
        unsigned char *g;

        stbtt_GetCodepointHMetrics(&f->info, s[i], &adv, &lsb);
        stbtt_GetCodepointBitmapBoxSubpixel(&f->info, s[i], sc, sc, shift, 0,
                                            &x0, &y0, &x1, &y1);
        gw = x1 - x0;
        gh = y1 - y0;
        if (gw > 0 && gh > 0 && (g = (unsigned char *)malloc((size_t)gw * gh)) != NULL) {
            stbtt_MakeCodepointBitmapSubpixel(&f->info, g, gw, gh, gw, sc, sc,
                                              shift, 0, s[i]);
            for (gy = 0; gy < gh; gy++)
                for (gx = 0; gx < gw; gx++) {
                    X = (int)floor(x) + x0 + gx;
                    Y = base + y0 + gy;
                    if (X >= 0 && X < *w && Y >= 0 && Y < *h) {
                        unsigned char c = g[gy * gw + gx], *d = &mask[Y * *w + X];
                        if (c > *d)
                            *d = c;
                    }
                }
            free(g);
        }
        x += adv * sc;
        if (i + 1 < n)
            x += stbtt_GetCodepointKernAdvance(&f->info, s[i], s[i + 1]) * sc;
    }
    return mask;
}

int ui_key(int k)
{
    const Uint8 *ks = SDL_GetKeyboardState(NULL);

    switch (k) {                        /* USB scancodes, as SDL's */
    case UI_K_OK:     return ks[40] || ks[88] || ks[44];    /* Enter, keypad's, Space */
    case UI_K_CANCEL: return ks[41];                        /* Esc */
    case UI_K_LEFT:   return ks[80];
    case UI_K_RIGHT:  return ks[79];
    }
    return 0;
}

int  ui_focused(void)    { return g_focused; }
void ui_sleep_ms(int ms) { SDL_Delay((Uint32)ms); }

/* POSIX locale variables, in the order setlocale(LC_MESSAGES) reads them:
 * "es_CO.UTF-8" gives "es". */
void plat_language(char *out, int n)
{
    static const char *vars[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    const char *v = NULL;
    int i;

    if (n <= 0)
        return;
    out[0] = 0;
    for (i = 0; i < 3 && (v == NULL || *v == 0); i++)
        v = getenv(vars[i]);
    if (v == NULL || strcmp(v, "C") == 0 || strcmp(v, "POSIX") == 0)
        return;
    for (i = 0; i < n - 1 && v[i] && v[i] != '_' && v[i] != '.' && v[i] != '@'; i++)
        out[i] = v[i];
    out[i] = 0;
}
