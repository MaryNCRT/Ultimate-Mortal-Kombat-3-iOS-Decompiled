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
#include <stdio.h>
#include "platform.h"
#include "gl.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static SDL_Window   *g_wnd;
static SDL_GLContext g_ctx;
static bool          g_quit;
static bool          g_focused = true;
static Uint64        g_start, g_freq;
static SDL_GameController *g_controller;

static int g_monitor;
static char g_typed[64];
static int  g_ntyped;
int plat_open_borderless;

static int monitor_idx(void)
{
    int n = SDL_GetNumVideoDisplays();
    if (n <= 0)
        return 0;
    if (g_monitor >= 1 && g_monitor <= n)
        return g_monitor - 1;
    return 0;
}

bool plat_open(const char *title, int width, int height)
{
    Uint32 flags;
    int disp;

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

    disp = monitor_idx();
    flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (plat_open_borderless)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    else
        flags |= SDL_WINDOW_RESIZABLE;

    g_wnd = SDL_CreateWindow(title,
                             SDL_WINDOWPOS_CENTERED_DISPLAY(disp),
                             SDL_WINDOWPOS_CENTERED_DISPLAY(disp),
                             width, height, flags);
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
    if (!g_wnd)
        return;
    SDL_SetWindowFullscreen(g_wnd, 0);
    SDL_SetWindowPosition(g_wnd,
                          SDL_WINDOWPOS_CENTERED_DISPLAY(monitor_idx()),
                          SDL_WINDOWPOS_CENTERED_DISPLAY(monitor_idx()));
    SDL_SetWindowFullscreen(g_wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void plat_fullscreen_exclusive(void)
{
    if (!g_wnd)
        return;
    SDL_SetWindowFullscreen(g_wnd, 0);
    SDL_SetWindowPosition(g_wnd,
                          SDL_WINDOWPOS_CENTERED_DISPLAY(monitor_idx()),
                          SDL_WINDOWPOS_CENTERED_DISPLAY(monitor_idx()));
    if (SDL_SetWindowFullscreen(g_wnd, SDL_WINDOW_FULLSCREEN) != 0)
        plat_fullscreen();
}

void plat_display(int mode, int w, int h)
{
    int disp;

    if (!g_wnd)
        return;
    disp = monitor_idx();
    if (mode == 0) {
        SDL_SetWindowFullscreen(g_wnd, 0);
        SDL_SetWindowBordered(g_wnd, SDL_TRUE);
        SDL_SetWindowResizable(g_wnd, SDL_TRUE);
        SDL_SetWindowSize(g_wnd, w, h);
        SDL_SetWindowPosition(g_wnd,
                              SDL_WINDOWPOS_CENTERED_DISPLAY(disp),
                              SDL_WINDOWPOS_CENTERED_DISPLAY(disp));
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

unsigned char *plat_ui_badge(const char *name, const unsigned char *icon,
                             int icon_px, float s, int *w, int *h)
{
    (void)name; (void)icon; (void)icon_px; (void)s; (void)w; (void)h;
    return NULL;
}

int plat_text_take(char *out, int n)
{
    int k = g_ntyped;

    if (n <= 0)
        return 0;
    if (k >= n)
        k = n - 1;
    memcpy(out, g_typed, (size_t)k);
    out[k] = 0;
    g_ntyped = 0;
    return k;
}

void plat_text_clear(void)
{
    g_ntyped = 0;
}

int plat_clipboard_text(char *out, int n)
{
    char *s;
    int k = 0;

    if (n <= 0)
        return 0;
    out[0] = 0;
    if (!SDL_HasClipboardText())
        return 0;
    s = SDL_GetClipboardText();
    if (!s)
        return 0;
    k = (int)snprintf(out, (size_t)n, "%s", s);
    if (k >= n)
        k = n - 1;
    SDL_free(s);
    return k > 0 ? k : 0;
}

int plat_clipboard_set(const char *utf8)
{
    if (!utf8)
        return 0;
    return SDL_SetClipboardText(utf8) == 0 ? 1 : 0;
}

void *plat_gl_proc(const char *name)
{
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
        case SDL_TEXTINPUT: {
            size_t len = strlen(ev.text.text);
            if (g_ntyped + (int)len < (int)sizeof(g_typed) - 1) {
                memcpy(g_typed + g_ntyped, ev.text.text, len);
                g_ntyped += (int)len;
            }
            break;
        }
        case SDL_KEYDOWN:
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
                SDL_GL_GetDrawableSize(g_wnd, &dw, &dh);
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
    SDL_GL_SwapWindow(g_wnd);
    return true;
}

void plat_size(int *width, int *height)
{
    int dw = 0, dh = 0;
    if (g_wnd) SDL_GL_GetDrawableSize(g_wnd, &dw, &dh);
    if (width)  *width  = dw;
    if (height) *height = dh;
}

int plat_mouse(int *x, int *y)
{
    int mx, my, ww, wh, dw, dh;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    SDL_GetWindowSize(g_wnd, &ww, &wh);
    SDL_GL_GetDrawableSize(g_wnd, &dw, &dh);
    if (ww <= 0) ww = 1;
    if (wh <= 0) wh = 1;
    if (x) *x = mx * dw / ww;
    if (y) *y = my * dh / wh;
    return (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
}

/* Windows VK_* codes used in umk3.ini and settings_menu.c */
enum {
    WVK_BACK       = 0x08,
    WVK_TAB        = 0x09,
    WVK_RETURN     = 0x0D,
    WVK_SHIFT      = 0x10,
    WVK_CONTROL    = 0x11,
    WVK_MENU       = 0x12,
    WVK_PAUSE      = 0x13,
    WVK_CAPITAL    = 0x14,
    WVK_ESCAPE     = 0x1B,
    WVK_SPACE      = 0x20,
    WVK_PRIOR      = 0x21,
    WVK_NEXT       = 0x22,
    WVK_END        = 0x23,
    WVK_HOME       = 0x24,
    WVK_LEFT       = 0x25,
    WVK_UP         = 0x26,
    WVK_RIGHT      = 0x27,
    WVK_DOWN       = 0x28,
    WVK_SNAPSHOT   = 0x2C,
    WVK_INSERT     = 0x2D,
    WVK_DELETE     = 0x2E,
    WVK_NUMPAD0    = 0x60,
    WVK_NUMPAD1    = 0x61,
    WVK_NUMPAD2    = 0x62,
    WVK_NUMPAD3    = 0x63,
    WVK_NUMPAD4    = 0x64,
    WVK_NUMPAD5    = 0x65,
    WVK_NUMPAD6    = 0x66,
    WVK_NUMPAD7    = 0x67,
    WVK_NUMPAD8    = 0x68,
    WVK_NUMPAD9    = 0x69,
    WVK_MULTIPLY   = 0x6A,
    WVK_ADD        = 0x6B,
    WVK_SEPARATOR  = 0x6C,
    WVK_SUBTRACT   = 0x6D,
    WVK_DECIMAL    = 0x6E,
    WVK_DIVIDE     = 0x6F,
    WVK_F1         = 0x70,
    WVK_F2         = 0x71,
    WVK_F3         = 0x72,
    WVK_F4         = 0x73,
    WVK_F5         = 0x74,
    WVK_F6         = 0x75,
    WVK_F7         = 0x76,
    WVK_F8         = 0x77,
    WVK_F9         = 0x78,
    WVK_F10        = 0x79,
    WVK_F11        = 0x7A,
    WVK_F12        = 0x7B,
    WVK_NUMLOCK    = 0x90,
    WVK_SCROLL     = 0x91,
    WVK_LSHIFT     = 0xA0,
    WVK_RSHIFT     = 0xA1,
    WVK_LCONTROL   = 0xA2,
    WVK_RCONTROL   = 0xA3,
    WVK_LMENU      = 0xA4,
    WVK_RMENU      = 0xA5,
    WVK_OEM_1      = 0xBA,
    WVK_OEM_PLUS   = 0xBB,
    WVK_OEM_COMMA  = 0xBC,
    WVK_OEM_MINUS  = 0xBD,
    WVK_OEM_PERIOD = 0xBE,
    WVK_OEM_2      = 0xBF,
    WVK_OEM_3      = 0xC0,
    WVK_OEM_4      = 0xDB,
    WVK_OEM_5      = 0xDC,
    WVK_OEM_6      = 0xDD,
    WVK_OEM_7      = 0xDE,
};

static SDL_Scancode vk_to_scancode(int vk)
{
    if (vk >= 'A' && vk <= 'Z')
        return (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A'));
    if (vk == '0')
        return SDL_SCANCODE_0;
    if (vk >= '1' && vk <= '9')
        return (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '1'));
    if (vk == WVK_NUMPAD0)
        return SDL_SCANCODE_KP_0;
    if (vk >= WVK_NUMPAD1 && vk <= WVK_NUMPAD9)
        return (SDL_Scancode)(SDL_SCANCODE_KP_1 + (vk - WVK_NUMPAD1));
    if (vk >= WVK_F1 && vk <= WVK_F12)
        return (SDL_Scancode)(SDL_SCANCODE_F1 + (vk - WVK_F1));
    switch (vk) {
    case WVK_UP:         return SDL_SCANCODE_UP;
    case WVK_DOWN:       return SDL_SCANCODE_DOWN;
    case WVK_LEFT:       return SDL_SCANCODE_LEFT;
    case WVK_RIGHT:      return SDL_SCANCODE_RIGHT;
    case WVK_RETURN:     return SDL_SCANCODE_RETURN;
    case WVK_ESCAPE:     return SDL_SCANCODE_ESCAPE;
    case WVK_SPACE:      return SDL_SCANCODE_SPACE;
    case WVK_TAB:        return SDL_SCANCODE_TAB;
    case WVK_BACK:       return SDL_SCANCODE_BACKSPACE;
    case WVK_SHIFT:
    case WVK_LSHIFT:     return SDL_SCANCODE_LSHIFT;
    case WVK_RSHIFT:     return SDL_SCANCODE_RSHIFT;
    case WVK_CONTROL:
    case WVK_LCONTROL:   return SDL_SCANCODE_LCTRL;
    case WVK_RCONTROL:   return SDL_SCANCODE_RCTRL;
    case WVK_MENU:
    case WVK_LMENU:      return SDL_SCANCODE_LALT;
    case WVK_RMENU:      return SDL_SCANCODE_RALT;
    case WVK_CAPITAL:    return SDL_SCANCODE_CAPSLOCK;
    case WVK_INSERT:     return SDL_SCANCODE_INSERT;
    case WVK_DELETE:     return SDL_SCANCODE_DELETE;
    case WVK_HOME:       return SDL_SCANCODE_HOME;
    case WVK_END:        return SDL_SCANCODE_END;
    case WVK_PRIOR:      return SDL_SCANCODE_PAGEUP;
    case WVK_NEXT:       return SDL_SCANCODE_PAGEDOWN;
    case WVK_MULTIPLY:   return SDL_SCANCODE_KP_MULTIPLY;
    case WVK_ADD:        return SDL_SCANCODE_KP_PLUS;
    case WVK_SUBTRACT:   return SDL_SCANCODE_KP_MINUS;
    case WVK_DECIMAL:    return SDL_SCANCODE_KP_PERIOD;
    case WVK_DIVIDE:     return SDL_SCANCODE_KP_DIVIDE;
    case WVK_NUMLOCK:    return SDL_SCANCODE_NUMLOCKCLEAR;
    case WVK_SCROLL:     return SDL_SCANCODE_SCROLLLOCK;
    case WVK_PAUSE:      return SDL_SCANCODE_PAUSE;
    case WVK_SNAPSHOT:   return SDL_SCANCODE_PRINTSCREEN;
    case WVK_OEM_1:      return SDL_SCANCODE_SEMICOLON;
    case WVK_OEM_PLUS:   return SDL_SCANCODE_EQUALS;
    case WVK_OEM_COMMA:  return SDL_SCANCODE_COMMA;
    case WVK_OEM_MINUS:  return SDL_SCANCODE_MINUS;
    case WVK_OEM_PERIOD: return SDL_SCANCODE_PERIOD;
    case WVK_OEM_2:      return SDL_SCANCODE_SLASH;
    case WVK_OEM_3:      return SDL_SCANCODE_GRAVE;
    case WVK_OEM_4:      return SDL_SCANCODE_LEFTBRACKET;
    case WVK_OEM_5:      return SDL_SCANCODE_BACKSLASH;
    case WVK_OEM_6:      return SDL_SCANCODE_RIGHTBRACKET;
    case WVK_OEM_7:      return SDL_SCANCODE_APOSTROPHE;
    default:             return SDL_SCANCODE_UNKNOWN;
    }
}

static int scancode_to_vk(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return 'A' + (sc - SDL_SCANCODE_A);
    if (sc == SDL_SCANCODE_0)
        return '0';
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return '1' + (sc - SDL_SCANCODE_1);
    if (sc == SDL_SCANCODE_KP_0)
        return WVK_NUMPAD0;
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9)
        return WVK_NUMPAD1 + (sc - SDL_SCANCODE_KP_1);
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
        return WVK_F1 + (sc - SDL_SCANCODE_F1);
    switch (sc) {
    case SDL_SCANCODE_UP:           return WVK_UP;
    case SDL_SCANCODE_DOWN:         return WVK_DOWN;
    case SDL_SCANCODE_LEFT:         return WVK_LEFT;
    case SDL_SCANCODE_RIGHT:        return WVK_RIGHT;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:     return WVK_RETURN;
    case SDL_SCANCODE_ESCAPE:       return WVK_ESCAPE;
    case SDL_SCANCODE_SPACE:        return WVK_SPACE;
    case SDL_SCANCODE_TAB:          return WVK_TAB;
    case SDL_SCANCODE_BACKSPACE:    return WVK_BACK;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:       return WVK_SHIFT;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:        return WVK_CONTROL;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:         return WVK_MENU;
    case SDL_SCANCODE_CAPSLOCK:     return WVK_CAPITAL;
    case SDL_SCANCODE_INSERT:       return WVK_INSERT;
    case SDL_SCANCODE_DELETE:       return WVK_DELETE;
    case SDL_SCANCODE_HOME:         return WVK_HOME;
    case SDL_SCANCODE_END:          return WVK_END;
    case SDL_SCANCODE_PAGEUP:       return WVK_PRIOR;
    case SDL_SCANCODE_PAGEDOWN:     return WVK_NEXT;
    case SDL_SCANCODE_KP_MULTIPLY:  return WVK_MULTIPLY;
    case SDL_SCANCODE_KP_PLUS:      return WVK_ADD;
    case SDL_SCANCODE_KP_MINUS:     return WVK_SUBTRACT;
    case SDL_SCANCODE_KP_PERIOD:    return WVK_DECIMAL;
    case SDL_SCANCODE_KP_DIVIDE:    return WVK_DIVIDE;
    case SDL_SCANCODE_NUMLOCKCLEAR: return WVK_NUMLOCK;
    case SDL_SCANCODE_SCROLLLOCK:   return WVK_SCROLL;
    case SDL_SCANCODE_PAUSE:        return WVK_PAUSE;
    case SDL_SCANCODE_PRINTSCREEN:  return WVK_SNAPSHOT;
    case SDL_SCANCODE_SEMICOLON:    return WVK_OEM_1;
    case SDL_SCANCODE_EQUALS:       return WVK_OEM_PLUS;
    case SDL_SCANCODE_COMMA:        return WVK_OEM_COMMA;
    case SDL_SCANCODE_MINUS:        return WVK_OEM_MINUS;
    case SDL_SCANCODE_PERIOD:       return WVK_OEM_PERIOD;
    case SDL_SCANCODE_SLASH:        return WVK_OEM_2;
    case SDL_SCANCODE_GRAVE:        return WVK_OEM_3;
    case SDL_SCANCODE_LEFTBRACKET:  return WVK_OEM_4;
    case SDL_SCANCODE_BACKSLASH:    return WVK_OEM_5;
    case SDL_SCANCODE_RIGHTBRACKET: return WVK_OEM_6;
    case SDL_SCANCODE_APOSTROPHE:   return WVK_OEM_7;
    default:                        return 0;
    }
}

static int g_vk[PK_COUNT] = {
    'W', 'S', 'A', 'D',
    'U', 'I', 'O', 'J', 'K', 'L',
    WVK_UP, WVK_DOWN, WVK_LEFT, WVK_RIGHT,
    WVK_NUMPAD7, WVK_NUMPAD8, WVK_NUMPAD9,
    WVK_NUMPAD4, WVK_NUMPAD5, WVK_NUMPAD6,
    WVK_F5,
    WVK_F1, WVK_RETURN, WVK_RIGHT, WVK_LEFT,
    WVK_F3,
    WVK_F2,
    WVK_F9, WVK_F10, WVK_F11, WVK_F12,
    'P', 'M',
    WVK_F6, WVK_F7, WVK_F8,
    'H', WVK_NUMPAD0,
    'U', 'O', 'J', 'L',
    'Q', 'E',
    WVK_NUMPAD7, WVK_NUMPAD9,
    WVK_NUMPAD4, WVK_NUMPAD6,
    WVK_ESCAPE,
    WVK_DIVIDE, WVK_MULTIPLY, WVK_NUMPAD1,
};

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
    SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_9,
    SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
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

void plat_key_glyph(int code, char *out, int n)
{
    plat_key_label(code, out, n);
}

static int g_glyph_style[2];

int plat_pad_glyph_style(int player, int style)
{
    player = player ? 1 : 0;
    if (style >= 0 && style <= 4)
        g_glyph_style[player] = style;
    return g_glyph_style[player];
}

unsigned char *plat_ui_grid(const char *title,
                            const unsigned char *const *cells, int n,
                            int cols, int cpx, float cell, int sel,
                            const char *footer, float s, int *w, int *h)
{
    (void)title; (void)cells; (void)n; (void)cols; (void)cpx; (void)cell; (void)sel;
    (void)footer; (void)s; (void)w; (void)h;
    return NULL;
}

void plat_monitor_select(int i)
{
    g_monitor = i >= 1 ? i : 0;
}

int plat_monitor(void)
{
    return g_monitor;
}

int plat_monitor_count(void)
{
    int n;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
        SDL_InitSubSystem(SDL_INIT_VIDEO);
    n = SDL_GetNumVideoDisplays();
    return n > 0 ? n : 1;
}

void plat_monitor_name(int i, char *out, int n)
{
    const char *name;
    int count;

    if (n <= 0)
        return;
    count = plat_monitor_count();
    if (i >= 1 && i <= count) {
        name = SDL_GetDisplayName(i - 1);
        if (name && *name) {
            snprintf(out, (size_t)n, "%d: %s", i, name);
            return;
        }
    }
    snprintf(out, (size_t)n, "%d", i);
}

void plat_monitor_list(void)
{
    int i, count = plat_monitor_count();

    for (i = 1; i <= count; i++) {
        SDL_Rect r;
        const char *name = SDL_GetDisplayName(i - 1);
        if (SDL_GetDisplayBounds(i - 1, &r) == 0)
            printf("%d\t%d: %s (%dx%d)\n", i, i,
                   name && *name ? name : "Display", r.w, r.h);
        else
            printf("%d\t%d: %s\n", i, i,
                   name && *name ? name : "Display");
    }
    fflush(stdout);
}

int plat_pad_sony_kind(unsigned product)
{
    return product == 0x05c4 || product == 0x09cc || product == 0x0ba0 ? 4 : 2;
}

int plat_pad_glyph_kind(int player, int kind)
{
    int st = g_glyph_style[player ? 1 : 0];
    return st ? st : kind;
}

void plat_pad_glyph(int kind, int pb, char *out, int n)
{
    static const char *const names[PB_COUNT] = {
        "SOUTH", "EAST", "WEST", "NORTH", "L1", "R1", "L2", "R2",
        "SELECT", "START", "L3", "R3", "HOME",
    };
    (void)kind;
    if (n > 0)
        snprintf(out, (size_t)n, "%s", pb >= 0 && pb < PB_COUNT ? names[pb] : "");
}

void plat_bind_key(int code, int key)
{
    if (code >= 0 && code < PK_COUNT && key > 0 && key < 256) {
        SDL_Scancode sc = vk_to_scancode(key);
        g_vk[code] = key;
        if (sc != SDL_SCANCODE_UNKNOWN)
            g_scancode[code] = sc;
    }
}

int plat_key_bound(int code)
{
    return code >= 0 && code < PK_COUNT ? g_vk[code] : 0;
}

int plat_key_backspace(void)
{
    return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_BACKSPACE] ? 1 : 0;
}

int plat_key_any(void)
{
    int count, i, vk;
    const Uint8 *keys = SDL_GetKeyboardState(&count);

    for (i = 1; i < count; i++)
        if (keys[i]) {
            vk = scancode_to_vk((SDL_Scancode)i);
            if (vk > 0)
                return vk;
        }
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

unsigned char *plat_ui_menu(const char *title, const char *const *tabs,
                            int ntab, int tab, const char *const *label,
                            const char *const *value, const int *enabled,
                            int n, int sel, const char *footer, float width,
                            float s, int *w, int *h)
{
    (void)title; (void)tabs; (void)ntab; (void)tab; (void)label; (void)value; (void)enabled; (void)n;
    (void)sel; (void)footer; (void)width; (void)s; (void)w; (void)h;
    return NULL;                        /* the menu's own font */
}

int plat_ask(const unsigned short *msg, const unsigned short *ok,
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
