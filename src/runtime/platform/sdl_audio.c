/*
 * sdl_audio.c -- the SDL2 half of platform.h's audio.
 *
 * SDL2_mixer owns the single output device for both streamed MP3 music and
 * one-shot effects. Opening a second SDL audio device prevents the mixer from
 * opening on backends that allow only one device (including Emscripten).
 *
 * Every entry point is safe without a device, as platform.h requires.
 */

#include "platform.h"

#include <SDL.h>
#ifndef UMK3_NO_SDL_MIXER
#include <SDL_mixer.h>
#endif

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef UMK3_NO_SDL_MIXER

#define VOICES      16
#define MIX_RATE 44100

typedef struct {
    Mix_Chunk *chunk;
    Uint8     *buffer;
    float      gain;
    const unsigned char *src;   /* the sound it plays: one voice a sound */
} voice;

static int               g_open;
static int               g_rate = 16000;
static int               g_channels;
static voice             g_voice[VOICES];
static int               g_mixer_initialized;
static int               g_music_open;
static Mix_Music         *g_music;

int plat_audio_open(int rate)
{
    if (g_open)
        return 1;

    g_rate = rate > 0 ? rate : 16000;

    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "audio: %s, running silent\n", SDL_GetError());
        return 0;
    }

    if (!g_mixer_initialized) {
        int formats = Mix_Init(MIX_INIT_MP3);
        g_mixer_initialized = 1;
        if ((formats & MIX_INIT_MP3) == 0)
            fprintf(stderr, "music: MP3 decoder unavailable: %s\n",
                    Mix_GetError());
    }
    memset(g_voice, 0, sizeof g_voice);
    if (Mix_OpenAudio(MIX_RATE, AUDIO_S16SYS, 2, 1024) != 0) {
        fprintf(stderr, "audio: %s, running without audio or music\n",
                Mix_GetError());
        if (g_mixer_initialized) {
            Mix_Quit();
            g_mixer_initialized = 0;
        }
        return 0;
    }
    g_music_open = 1;
    g_channels = Mix_AllocateChannels(VOICES);
    if (g_channels <= 0) {
        fprintf(stderr, "audio: could not allocate mixer channels: %s\n",
                Mix_GetError());
        Mix_CloseAudio();
        Mix_Quit();
        g_music_open = 0;
        g_mixer_initialized = 0;
        return 0;
    }
    if (g_channels > VOICES)
        g_channels = VOICES;
    g_open = 1;
    return 1;
}

void plat_audio_close(void)
{
    plat_music_stop();
    if (g_open) {
        Mix_HaltChannel(-1);
        for (int i = 0; i < VOICES; i++) {
            if (g_voice[i].chunk)
                Mix_FreeChunk(g_voice[i].chunk);
            SDL_free(g_voice[i].buffer);
        }
        memset(g_voice, 0, sizeof g_voice);
    }
    if (g_music_open) {
        Mix_CloseAudio();
        g_music_open = 0;
    }
    if (g_mixer_initialized) {
        Mix_Quit();
        g_mixer_initialized = 0;
    }
    g_channels = 0;
    g_open = 0;
}

/* Each voice plays its own converted copy, so a caller freeing `pcm` is safe
 * already; nothing to do. */
void plat_audio_stop_pcm(const unsigned char *pcm)
{
    (void)pcm;
}

void plat_audio_stop_all(void)
{
    if (g_open)
        Mix_HaltChannel(-1);
}

int plat_audio_play_at(const unsigned char *pcm, int frames, int rate, float gain)
{
    SDL_AudioCVT cvt;
    Uint8 *buffer;
    Mix_Chunk *chunk;
    int i, quietest = -1, converted, buffer_len;
    float lowest = 1e30f;

    if (!g_open || pcm == NULL || frames <= 0)
        return 0;
    if (rate <= 0)
        rate = g_rate;

    for (i = 0; i < g_channels; i++) {
        if (g_voice[i].chunk && !Mix_Playing(i)) {
            Mix_FreeChunk(g_voice[i].chunk);
            SDL_free(g_voice[i].buffer);
            memset(&g_voice[i], 0, sizeof g_voice[i]);
        }
        if (!g_voice[i].chunk)
            break;
        if (g_voice[i].gain < lowest) {
            lowest = g_voice[i].gain;
            quietest = i;
        }
    }
    /* One sound, one voice, as on the device and as win32_audio.c: -[Sound
     * play] (0xada08) stops a sound still playing and starts it over -- it
     * never doubles over itself. Here every play took a channel of its own,
     * so on Linux a long scream played again (Liu Kang's, twice anyone
     * else's length) sounded on top of itself. */
    for (int k = 0; k < g_channels; k++)
        if (g_voice[k].chunk && g_voice[k].src == pcm && Mix_Playing(k)) {
            Mix_HaltChannel(k);
            Mix_FreeChunk(g_voice[k].chunk);
            SDL_free(g_voice[k].buffer);
            memset(&g_voice[k], 0, sizeof g_voice[k]);
            i = k;
            break;
        }
    if (i == g_channels) {
        /* every voice busy: steal the quietest only for a louder newcomer */
        if (quietest < 0 || gain <= lowest)
            return 0;
        i = quietest;
        Mix_HaltChannel(i);
        Mix_FreeChunk(g_voice[i].chunk);
        SDL_free(g_voice[i].buffer);
        memset(&g_voice[i], 0, sizeof g_voice[i]);
    }

    converted = SDL_BuildAudioCVT(&cvt, AUDIO_U8, 1, rate,
                                  AUDIO_S16SYS, 2, MIX_RATE);
    if (converted < 0 || cvt.len_mult <= 0 ||
        frames > INT_MAX / cvt.len_mult) {
        fprintf(stderr, "audio: cannot convert sound (%s)\n", SDL_GetError());
        return 0;
    }
    buffer_len = frames * cvt.len_mult;
    buffer = (Uint8 *)SDL_malloc((size_t)buffer_len);
    if (!buffer) {
        fprintf(stderr, "audio: cannot allocate converted sound (%s)\n",
                SDL_GetError());
        return 0;
    }
    memcpy(buffer, pcm, (size_t)frames);
    cvt.buf = buffer;
    cvt.len = frames;
    if (SDL_ConvertAudio(&cvt) < 0) {
        fprintf(stderr, "audio: cannot convert sound (%s)\n", SDL_GetError());
        SDL_free(buffer);
        return 0;
    }

    chunk = Mix_QuickLoad_RAW(buffer, (Uint32)cvt.len_cvt);
    if (!chunk) {
        fprintf(stderr, "audio: cannot create sound chunk (%s)\n", Mix_GetError());
        SDL_free(buffer);
        return 0;
    }

    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 1.0f)
        gain = 1.0f;
    Mix_Volume(i, (int)(gain * MIX_MAX_VOLUME + 0.5f));
    if (Mix_PlayChannel(i, chunk, 0) < 0) {
        fprintf(stderr, "audio: cannot play sound (%s)\n", Mix_GetError());
        Mix_FreeChunk(chunk);
        SDL_free(buffer);
        return 0;
    }
    g_voice[i].chunk = chunk;
    g_voice[i].buffer = buffer;
    g_voice[i].gain = gain;
    g_voice[i].src = pcm;
    return 1;
}

int plat_audio_play(const unsigned char *pcm, int frames, float gain)
{
    return plat_audio_play_at(pcm, frames, g_rate, gain);
}

void plat_audio_update(void)
{
    int i;

    if (!g_open)
        return;
    for (i = 0; i < g_channels; i++) {
        if (g_voice[i].chunk && !Mix_Playing(i)) {
            Mix_FreeChunk(g_voice[i].chunk);
            SDL_free(g_voice[i].buffer);
            memset(&g_voice[i], 0, sizeof g_voice[i]);
        }
    }
}

/* ------------------------------------------------------------------- music */

void plat_music_play(const char *path, int loop)
{
    Mix_Music *music;

    plat_music_stop();
    if (!g_music_open || path == NULL)
        return;

    music = Mix_LoadMUS(path);
    if (music == NULL) {
        fprintf(stderr, "music: cannot load %s: %s\n", path, Mix_GetError());
        return;
    }

    g_music = music;
    if (Mix_PlayMusic(g_music, loop ? -1 : 0) != 0) {
        fprintf(stderr, "music: cannot play %s: %s\n", path, Mix_GetError());
        Mix_FreeMusic(g_music);
        g_music = NULL;
    }
}

int plat_music_playing(void) { return g_music != NULL && Mix_PlayingMusic(); }

void plat_music_stop(void)
{
    if (g_music == NULL)
        return;
    Mix_HaltMusic();
    Mix_FreeMusic(g_music);
    g_music = NULL;
}

void plat_music_volume(float gain)
{
    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 1.0f)
        gain = 1.0f;
    if (g_music_open)
        Mix_VolumeMusic((int)(gain * MIX_MAX_VOLUME + 0.5f));
}

#else /* UMK3_NO_SDL_MIXER */

/* Without SDL2_mixer -- the Linux package, which needs no -dev packages and
 * no library beyond SDL2 -- the device is SDL2's own and this file mixes:
 * up to VOICES effects (the same conversion and voice stealing as the mixer
 * path above) and one music stream. Every entry point used to be empty here,
 * so the Linux game had no sound at all (issue #104).
 *
 * The music is MP3. It is decoded with libmpg123, loaded when the first song
 * plays: every desktop distribution has it (Fedora and Bazzite through
 * mpg123-libs), and the game does not link it, so without it there is only
 * the music missing and the log says why. */

#include <dlfcn.h>
#include <stdlib.h>

#define VOICES      16
#define MIX_RATE 44100

typedef struct {
    Sint16 *buf;                        /* stereo frames, MIX_RATE */
    int     frames, pos;
    float   gain;
} nvoice;

static SDL_AudioDeviceID g_dev;
static int    g_rate = 16000;
static nvoice g_v[VOICES];
static Sint16 *g_mus;                   /* the whole song, stereo, MIX_RATE */
static int    g_mus_frames, g_mus_pos, g_mus_loop;
static float  g_mus_gain = 1.0f;

static void mix(void *ud, Uint8 *out, int len)
{
    Sint16 *o = (Sint16 *)out;
    int n = len / 4, i, k;

    (void)ud;
    for (i = 0; i < n; i++) {
        int l = 0, r = 0;

        for (k = 0; k < VOICES; k++) {
            nvoice *v = &g_v[k];
            if (!v->buf || v->pos >= v->frames)
                continue;
            l += (int)(v->buf[v->pos * 2] * v->gain);
            r += (int)(v->buf[v->pos * 2 + 1] * v->gain);
            v->pos++;
        }
        if (g_mus && g_mus_frames > 0) {
            if (g_mus_pos >= g_mus_frames && g_mus_loop)
                g_mus_pos = 0;
            if (g_mus_pos < g_mus_frames) {
                l += (int)(g_mus[g_mus_pos * 2] * g_mus_gain);
                r += (int)(g_mus[g_mus_pos * 2 + 1] * g_mus_gain);
                g_mus_pos++;
            }
        }
        o[i * 2]     = (Sint16)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        o[i * 2 + 1] = (Sint16)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
}

int plat_audio_open(int rate)
{
    SDL_AudioSpec want, have;

    if (g_dev)
        return 1;
    g_rate = rate > 0 ? rate : 16000;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "audio: %s, running silent\n", SDL_GetError());
        return 0;
    }
    memset(&want, 0, sizeof want);
    want.freq = MIX_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        fprintf(stderr, "audio: %s, running silent\n", SDL_GetError());
        return 0;
    }
    memset(g_v, 0, sizeof g_v);
    SDL_PauseAudioDevice(g_dev, 0);
    printf("audio: SDL2 device, %d Hz stereo, %d voices (no SDL2_mixer)\n", MIX_RATE, VOICES);
    return 1;
}

void plat_audio_close(void)
{
    int i;

    plat_music_stop();
    if (!g_dev)
        return;
    SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
    for (i = 0; i < VOICES; i++)
        SDL_free(g_v[i].buf);
    memset(g_v, 0, sizeof g_v);
}

void plat_audio_stop_pcm(const unsigned char *pcm)
{
    (void)pcm;                          /* each voice plays its own copy */
}

void plat_audio_stop_all(void)
{
    int i;

    if (!g_dev)
        return;
    SDL_LockAudioDevice(g_dev);
    for (i = 0; i < VOICES; i++)
        g_v[i].pos = g_v[i].frames;
    SDL_UnlockAudioDevice(g_dev);
}

/* src (format, channels, rate) to stereo S16 at MIX_RATE, SDL_malloc'd */
static Sint16 *convert(const void *src, int bytes, SDL_AudioFormat fmt, int ch, int rate, int *frames)
{
    SDL_AudioCVT cvt;
    Uint8 *buf;

    if (SDL_BuildAudioCVT(&cvt, fmt, (Uint8)ch, rate, AUDIO_S16SYS, 2, MIX_RATE) < 0
        || cvt.len_mult <= 0 || bytes > INT_MAX / cvt.len_mult)
        return NULL;
    buf = (Uint8 *)SDL_malloc((size_t)bytes * (size_t)cvt.len_mult);
    if (!buf)
        return NULL;
    memcpy(buf, src, (size_t)bytes);
    cvt.buf = buf;
    cvt.len = bytes;
    if (SDL_ConvertAudio(&cvt) < 0) {
        SDL_free(buf);
        return NULL;
    }
    *frames = cvt.len_cvt / 4;
    return (Sint16 *)buf;
}

int plat_audio_play_at(const unsigned char *pcm, int frames, int rate, float gain)
{
    Sint16 *buf;
    int i, n, slot = -1, quietest = -1;
    float lowest = 1e30f;

    if (!g_dev || pcm == NULL || frames <= 0)
        return 0;
    if (rate <= 0)
        rate = g_rate;
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;

    buf = convert(pcm, frames, AUDIO_U8, 1, rate, &n);
    if (!buf) {
        fprintf(stderr, "audio: cannot convert sound (%s)\n", SDL_GetError());
        return 0;
    }

    SDL_LockAudioDevice(g_dev);
    for (i = 0; i < VOICES; i++) {
        if (!g_v[i].buf || g_v[i].pos >= g_v[i].frames) {
            slot = i;
            break;
        }
        if (g_v[i].gain < lowest) {
            lowest = g_v[i].gain;
            quietest = i;
        }
    }
    /* every voice busy: steal the quietest only for a louder newcomer */
    if (slot < 0 && quietest >= 0 && gain > lowest)
        slot = quietest;
    if (slot >= 0) {
        SDL_free(g_v[slot].buf);
        g_v[slot].buf = buf;
        g_v[slot].frames = n;
        g_v[slot].pos = 0;
        g_v[slot].gain = gain;
    }
    SDL_UnlockAudioDevice(g_dev);
    if (slot < 0)
        SDL_free(buf);
    return slot >= 0;
}

int plat_audio_play(const unsigned char *pcm, int frames, float gain)
{
    return plat_audio_play_at(pcm, frames, g_rate, gain);
}

void plat_audio_update(void)
{
}

/* ------------------------------------------------------- music: libmpg123 */

typedef struct mpg123_handle_struct mpg123_handle;
static struct {
    int tried, ok;
    int  (*init)(void);
    mpg123_handle *(*make)(const char *, int *);
    int  (*open)(mpg123_handle *, const char *);
    int  (*getformat)(mpg123_handle *, long *, int *, int *);
    int  (*format_none)(mpg123_handle *);
    int  (*format)(mpg123_handle *, long, int, int);
    int  (*read)(mpg123_handle *, void *, size_t, size_t *);
    int  (*close)(mpg123_handle *);
    void (*del)(mpg123_handle *);
} M;

#define MPG123_ENC_SIGNED_16 0xd0
#define MPG123_DONE        (-12)
#define MPG123_NEW_FORMAT  (-11)

static int mpg_load(void)
{
    static const char *const libs[] = { "libmpg123.so.0", "libmpg123.so" };
    void *h = NULL;
    size_t i;

    if (M.tried)
        return M.ok;
    M.tried = 1;
    for (i = 0; i < sizeof libs / sizeof libs[0] && !h; i++)
        h = dlopen(libs[i], RTLD_NOW);
    if (!h) {
        fprintf(stderr, "music: libmpg123 not found (install mpg123 / mpg123-libs); "
                        "playing without music\n");
        return 0;
    }
#define SYM(f, n) (*(void **)&M.f = dlsym(h, n))
    SYM(init, "mpg123_init");
    SYM(make, "mpg123_new");
    if (!SYM(open, "mpg123_open"))
        SYM(open, "mpg123_open_64");
    SYM(getformat, "mpg123_getformat");
    SYM(format_none, "mpg123_format_none");
    SYM(format, "mpg123_format");
    SYM(read, "mpg123_read");
    SYM(close, "mpg123_close");
    SYM(del, "mpg123_delete");
#undef SYM
    M.ok = M.init && M.make && M.open && M.getformat && M.format_none && M.format
           && M.read && M.close && M.del && M.init() == 0;
    if (!M.ok)
        fprintf(stderr, "music: libmpg123 is not usable; playing without music\n");
    return M.ok;
}

/* the whole file as interleaved S16, its rate and channels */
static Sint16 *mpg_decode(const char *path, long *rate, int *ch, int *bytes)
{
    mpg123_handle *h;
    int err = 0, enc = 0, r;
    size_t cap = 1 << 20, len = 0, done = 0;
    unsigned char *buf;

    h = M.make(NULL, &err);
    if (!h)
        return NULL;
    if (M.open(h, path) != 0 || M.getformat(h, rate, ch, &enc) != 0) {
        M.del(h);
        return NULL;
    }
    if (enc != MPG123_ENC_SIGNED_16) {      /* ask for S16 at the same rate */
        M.format_none(h);
        M.format(h, *rate, *ch, MPG123_ENC_SIGNED_16);
    }
    buf = (unsigned char *)malloc(cap);
    for (;;) {
        if (!buf)
            break;
        if (cap - len < 65536) {
            unsigned char *nb = (unsigned char *)realloc(buf, cap * 2);
            if (!nb) { free(buf); buf = NULL; break; }
            buf = nb;
            cap *= 2;
        }
        r = M.read(h, buf + len, cap - len, &done);
        len += done;
        if (r == MPG123_NEW_FORMAT) {
            M.getformat(h, rate, ch, &enc);
            continue;
        }
        if (r != 0)
            break;                      /* MPG123_DONE, or an error */
    }
    M.close(h);
    M.del(h);
    *bytes = (int)len;
    return (Sint16 *)buf;
}

void plat_music_play(const char *path, int loop)
{
    long rate = 0;
    int ch = 0, bytes = 0, frames = 0;
    Sint16 *pcm, *cvt;

    plat_music_stop();
    if (!g_dev || path == NULL || !mpg_load())
        return;
    pcm = mpg_decode(path, &rate, &ch, &bytes);
    if (!pcm || bytes <= 0 || ch < 1 || rate <= 0) {
        fprintf(stderr, "music: cannot decode %s\n", path);
        free(pcm);
        return;
    }
    cvt = convert(pcm, bytes, AUDIO_S16SYS, ch, (int)rate, &frames);
    free(pcm);
    if (!cvt) {
        fprintf(stderr, "music: cannot convert %s (%s)\n", path, SDL_GetError());
        return;
    }
    SDL_LockAudioDevice(g_dev);
    g_mus = cvt;
    g_mus_frames = frames;
    g_mus_pos = 0;
    g_mus_loop = loop;
    SDL_UnlockAudioDevice(g_dev);
}

int plat_music_playing(void) { return g_mus != NULL; }

void plat_music_stop(void)
{
    Sint16 *old;

    if (!g_dev) {
        SDL_free(g_mus);
        g_mus = NULL;
        return;
    }
    SDL_LockAudioDevice(g_dev);
    old = g_mus;
    g_mus = NULL;
    g_mus_frames = g_mus_pos = 0;
    SDL_UnlockAudioDevice(g_dev);
    SDL_free(old);
}

void plat_music_volume(float gain)
{
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    g_mus_gain = gain;
}

#endif /* UMK3_NO_SDL_MIXER */
