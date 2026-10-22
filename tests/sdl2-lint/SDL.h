/*
 * SDL.h -- a LINT FIXTURE, not SDL2. Never in the build path.
 *
 * ## Why this exists
 *
 * `runtime/platform/sdl_gl.c` is the Linux backend. On a Windows machine CMake
 * never compiles it, so it sat completely unchecked: a typo in it would survive
 * every local build and only surface on somebody else's Linux box. That is the
 * same shape as the call-site scan that reported zero `glBindTexture` callers --
 * a check that cannot detect a known positive is not evidence of anything.
 *
 * So `tools/check.sh` syntax-checks that file against this header when real
 * SDL2 is absent.
 *
 * ## What a pass here does and does not prove
 *
 * PROVES  -- our own mistakes are gone: typos, undeclared variables, wrong
 *            argument COUNTS, missing returns, unused variables, bad struct
 *            member names on SDL_Event.
 *
 * DOES NOT PROVE -- that these declarations match real SDL2. They are written
 *            from the documented SDL2 API, which makes them a CLAIM, not a
 *            reading of an installed header. If one is wrong, this lint passes
 *            and the real build still fails.
 *
 * Which is why `tools/check.sh` prefers a real SDL2 whenever one is installed
 * and only falls back here, saying loudly which of the two it used. Install
 * SDL2 and the claim stops mattering.
 *
 * The constant VALUES below are placeholders chosen only to be distinct.
 * `-fsyntax-only` never looks at them. Do not copy them anywhere, and do not
 * link anything against this file -- the guard beneath makes that fail loudly.
 */
#ifndef UMK3_SDL2_LINT_FIXTURE_H
#define UMK3_SDL2_LINT_FIXTURE_H

#ifndef UMK3_SDL2_LINT
#error "tests/sdl2-lint/SDL.h is a lint fixture, not SDL2. Install SDL2 to build."
#endif

#include <stddef.h>
#include <stdint.h>

typedef uint8_t  Uint8;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int32_t  Sint32;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

typedef struct SDL_Window SDL_Window;
typedef void *SDL_GLContext;

/* placeholder values -- see the header comment */
#define SDL_INIT_VIDEO            0x1u
#define SDL_WINDOWPOS_CENTERED    0x2
#define SDL_WINDOW_OPENGL         0x4u
#define SDL_WINDOW_RESIZABLE      0x8u
#define SDL_WINDOW_ALLOW_HIGHDPI  0x10u
#define SDLK_ESCAPE               0x20
#define SDLK_BACKSPACE            0x21
#define SDLK_RETURN               0x22
#define SDLK_KP_ENTER             0x23
#define SDLK_v                    0x24
#define KMOD_CTRL                 0x25

typedef enum {
    SDL_GL_RED_SIZE, SDL_GL_GREEN_SIZE, SDL_GL_BLUE_SIZE, SDL_GL_ALPHA_SIZE,
    SDL_GL_DEPTH_SIZE, SDL_GL_DOUBLEBUFFER, SDL_GL_CONTEXT_PROFILE_MASK
} SDL_GLattr;

#define SDL_GL_CONTEXT_PROFILE_COMPATIBILITY 0x0002

typedef enum { SDL_QUIT = 0x100, SDL_WINDOWEVENT = 0x200, SDL_KEYDOWN = 0x300,
               SDL_TEXTINPUT = 0x303, SDL_MOUSEWHEEL = 0x403 } SDL_EventType;
typedef enum { SDL_MOUSEWHEEL_NORMAL, SDL_MOUSEWHEEL_FLIPPED } SDL_MouseWheelDirection;
typedef enum {
    SDL_WINDOWEVENT_SIZE_CHANGED = 6,
    SDL_WINDOWEVENT_FOCUS_GAINED = 12,
    SDL_WINDOWEVENT_FOCUS_LOST   = 13,
    SDL_WINDOWEVENT_CLOSE        = 14
} SDL_WindowEventID;

typedef Sint32 SDL_Keycode;
typedef struct { Sint32 sym; Uint32 mod; } SDL_Keysym;
typedef struct { Uint32 type; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint32 type; Uint8 event; Sint32 data1, data2; } SDL_WindowEvent;
typedef struct { Uint32 type; char text[32]; } SDL_TextInputEvent;
typedef struct { Uint32 type; Sint32 x, y; Uint32 direction; } SDL_MouseWheelEvent;
typedef struct {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
} SDL_ControllerDeviceEvent;

typedef union SDL_Event {
    Uint32                    type;
    SDL_KeyboardEvent         key;
    SDL_WindowEvent           window;
    SDL_TextInputEvent        text;
    SDL_MouseWheelEvent       wheel;
    SDL_ControllerDeviceEvent cdevice;
} SDL_Event;

int         SDL_Init(Uint32 flags);
void        SDL_Quit(void);
const char *SDL_GetError(void);
void        SDL_Log(const char *fmt, ...);

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags);
void        SDL_DestroyWindow(SDL_Window *window);

int           SDL_GL_SetAttribute(SDL_GLattr attr, int value);
SDL_GLContext SDL_GL_CreateContext(SDL_Window *window);
void          SDL_GL_DeleteContext(SDL_GLContext context);
int           SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context);
int           SDL_GL_SetSwapInterval(int interval);
void          SDL_GL_SwapWindow(SDL_Window *window);
void          SDL_GL_GetDrawableSize(SDL_Window *window, int *w, int *h);

Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
int    SDL_PollEvent(SDL_Event *event);

/* ---- window, mouse, keyboard and game controller: what sdl_gl.c's input
 *      code uses, from the documented SDL2 API (SDL_video.h, SDL_mouse.h,
 *      SDL_keyboard.h, SDL_scancode.h, SDL_joystick.h, SDL_gamecontroller.h).
 *      Same caveat as everything above. */
#define SDL_INIT_GAMECONTROLLER     0x2000u
#define SDL_CONTROLLERDEVICEADDED   0x653
#define SDL_CONTROLLERDEVICEREMOVED 0x654

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h);

#define SDL_BUTTON(X)   (1 << ((X) - 1))
#define SDL_BUTTON_LEFT 1
Uint32 SDL_GetMouseState(int *x, int *y);

/* SDL2 numbers scancodes by USB HID usage ID; only the ones used are listed */
typedef enum {
    SDL_SCANCODE_A = 4,  SDL_SCANCODE_D = 7,  SDL_SCANCODE_E = 8,
    SDL_SCANCODE_H = 11, SDL_SCANCODE_I = 12,
    SDL_SCANCODE_J = 13, SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16, SDL_SCANCODE_O = 18, SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20, SDL_SCANCODE_S = 22, SDL_SCANCODE_U = 24,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2 = 59, SDL_SCANCODE_F3 = 60,
    SDL_SCANCODE_F5 = 62, SDL_SCANCODE_F6 = 63, SDL_SCANCODE_F7 = 64,
    SDL_SCANCODE_F8 = 65, SDL_SCANCODE_F9 = 66, SDL_SCANCODE_F10 = 67,
    SDL_SCANCODE_F11 = 68, SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,  SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_4 = 92, SDL_SCANCODE_KP_5 = 93, SDL_SCANCODE_KP_6 = 94,
    SDL_SCANCODE_KP_7 = 95, SDL_SCANCODE_KP_8 = 96, SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;
const Uint8 *SDL_GetKeyboardState(int *numkeys);
const char  *SDL_GetScancodeName(SDL_Scancode scancode);

#define SDL_WINDOW_FULLSCREEN         0x00000001u
#define SDL_WINDOW_FULLSCREEN_DESKTOP (SDL_WINDOW_FULLSCREEN | 0x00001000u)
typedef struct {
    Uint32 format;
    int w, h, refresh_rate;
    void *driverdata;
} SDL_DisplayMode;
int   SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags);
void  SDL_SetWindowSize(SDL_Window *window, int w, int h);
void  SDL_SetWindowPosition(SDL_Window *window, int x, int y);
int   SDL_GetWindowDisplayMode(SDL_Window *window, SDL_DisplayMode *mode);
void *SDL_GL_GetProcAddress(const char *proc);
void  SDL_Delay(Uint32 ms);

typedef struct _SDL_Joystick SDL_Joystick;
typedef Sint32 SDL_JoystickID;
int            SDL_NumJoysticks(void);
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick);

typedef struct _SDL_GameController SDL_GameController;
typedef enum {
    SDL_CONTROLLER_AXIS_INVALID = -1,
    SDL_CONTROLLER_AXIS_LEFTX,
    SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,
    SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
    SDL_CONTROLLER_AXIS_MAX
} SDL_GameControllerAxis;
/* SDL 2.0.14 appends MISC1, PADDLE1-4 and TOUCHPAD after DPAD_RIGHT */
typedef enum {
    SDL_CONTROLLER_BUTTON_INVALID = -1,
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT
} SDL_GameControllerButton;

SDL_bool            SDL_IsGameController(int joystick_index);
SDL_GameController *SDL_GameControllerOpen(int joystick_index);
void                SDL_GameControllerClose(SDL_GameController *gamecontroller);
SDL_bool            SDL_GameControllerGetAttached(SDL_GameController *gamecontroller);
SDL_Joystick       *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller);
Sint16              SDL_GameControllerGetAxis(SDL_GameController *gamecontroller,
                                              SDL_GameControllerAxis axis);
Uint8               SDL_GameControllerGetButton(SDL_GameController *gamecontroller,
                                                SDL_GameControllerButton button);
uint16_t            SDL_GameControllerGetVendor(SDL_GameController *gamecontroller);
uint16_t            SDL_GameControllerGetProduct(SDL_GameController *gamecontroller);
const char         *SDL_GameControllerName(SDL_GameController *gamecontroller);

/* ---- audio: what runtime/platform/sdl_audio.c uses, from the documented
 *      SDL2 API (SDL_audio.h). Same caveat as everything above. */
typedef uint16_t Uint16;
typedef Uint16   SDL_AudioFormat;
typedef Uint32   SDL_AudioDeviceID;
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

typedef struct SDL_AudioSpec {
    int               freq;
    SDL_AudioFormat   format;
    Uint8             channels;
    Uint8             silence;
    Uint16            samples;
    Uint16            padding;
    Uint32            size;
    SDL_AudioCallback callback;
    void             *userdata;
} SDL_AudioSpec;

#define SDL_INIT_AUDIO  0x10u
#define AUDIO_U8        0x0008
#define AUDIO_S16SYS    0x8010

/* rate/format conversion: sdl_audio.c turns the game's u8 mono into the
 * mixer's format with these before handing it to SDL_mixer */
struct SDL_AudioCVT;
typedef void (*SDL_AudioFilter)(struct SDL_AudioCVT *cvt, SDL_AudioFormat format);
#define SDL_AUDIOCVT_MAX_FILTERS 9
typedef struct SDL_AudioCVT {
    int             needed;
    SDL_AudioFormat src_format;
    SDL_AudioFormat dst_format;
    double          rate_incr;
    Uint8          *buf;
    int             len;
    int             len_cvt;
    int             len_mult;
    double          len_ratio;
    SDL_AudioFilter filters[SDL_AUDIOCVT_MAX_FILTERS + 1];
    int             filter_index;
} SDL_AudioCVT;

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt,
                      SDL_AudioFormat src_format, Uint8 src_channels, int src_rate,
                      SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate);
int SDL_ConvertAudio(SDL_AudioCVT *cvt);

void *SDL_malloc(size_t size);
void  SDL_free(void *mem);
void  SDL_StartTextInput(void);
char *SDL_GetClipboardText(void);
int   SDL_SetClipboardText(const char *text);

Uint32 SDL_WasInit(Uint32 flags);
int    SDL_InitSubSystem(Uint32 flags);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture,
                                      const SDL_AudioSpec *desired,
                                      SDL_AudioSpec *obtained,
                                      int allowed_changes);
void   SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void   SDL_CloseAudioDevice(SDL_AudioDeviceID dev);
int    SDL_QueueAudio(SDL_AudioDeviceID dev, const void *data, Uint32 len);
Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID dev);

/* SDL_messagebox.h -- plat_ask. */
#define SDL_MESSAGEBOX_INFORMATION                0x40u
#define SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT   0x1u
#define SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT   0x2u

typedef struct {
    Uint32      flags;
    int         buttonid;
    const char *text;
} SDL_MessageBoxButtonData;

typedef struct SDL_MessageBoxColorScheme SDL_MessageBoxColorScheme;

typedef struct {
    Uint32                           flags;
    SDL_Window                      *window;
    const char                      *title;
    const char                      *message;
    int                              numbuttons;
    const SDL_MessageBoxButtonData  *buttons;
    const SDL_MessageBoxColorScheme *colorScheme;
} SDL_MessageBoxData;

int SDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid);

#endif /* UMK3_SDL2_LINT_FIXTURE_H */
