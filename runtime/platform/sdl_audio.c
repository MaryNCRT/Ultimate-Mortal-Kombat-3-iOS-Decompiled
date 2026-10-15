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
#include <SDL_mixer.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define VOICES      16
#define MIX_RATE 44100

typedef struct {
    Mix_Chunk *chunk;
    Uint8     *buffer;
    float      gain;
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
