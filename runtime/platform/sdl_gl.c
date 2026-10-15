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
    SDL_SCANCODE_Q, SDL_SCANCODE_E
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

void plat_bind_key(int code, int key)
{
    if (code >= 0 && code < PK_COUNT && key > 0 && key < SDL_NUM_SCANCODES)
        g_scancode[code] = (SDL_Scancode)key;
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

unsigned char *plat_ui_menu(const char *title, const char *const *label,
                            const char *const *value, const int *enabled,
                            int n, int sel, const char *footer, float s,
                            int *w, int *h)
{
    (void)title; (void)label; (void)value; (void)enabled; (void)n;
    (void)sel; (void)footer; (void)s; (void)w; (void)h;
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
