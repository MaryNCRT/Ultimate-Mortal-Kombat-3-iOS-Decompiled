/*
 * win32_audio.c -- the Win32 half of platform.h's audio.
 *
 * `waveOut`, plus Windows' own ACM codec for the MP3 music: no XAudio2, no
 * DirectSound, no dependency that has to be installed. The same reasoning as `win32_gl.c` -- the port should
 * build with the toolchain that is already here.
 *
 * ## How it works
 *
 * One output stream at the assets' own rate, and a ring of small buffers that
 * are refilled and re-queued as the device finishes them. `plat_audio_update`
 * does that refill, once a frame, mixing whatever voices are active into 16-bit
 * mono.
 *
 * There is no callback. A `waveOut` callback runs on the driver's thread and
 * everything it touches needs a lock; refilling from the game loop instead
 * costs one buffer of latency -- about 32 ms here -- and removes the entire
 * question. For sounds triggered by a 60 Hz simulation that is the right
 * trade.
 *
 * ## The sources
 *
 * The game's .wav files are **unsigned 8-bit mono PCM**, so a source sample is
 * 0..255 with 128 as silence and the mix is `(s - 128) << 8` scaled by gain.
 *
 * **They are not all one sample rate.** 496 of the 497 are 16 kHz and
 * `Arrowhit.wav` is 44,100 -- surveyed, not assumed, and the one exception is
 * why each voice carries a 16.16 step instead of the mixer taking the device
 * rate for granted. One odd file is enough to make the assumption wrong.
 *
 * ## When the device is missing
 *
 * Every entry point checks `g_open` and returns quietly. A machine with no
 * sound card, or one where `waveOutOpen` fails because something else holds
 * the device exclusively, still runs the game in silence -- it does not crash
 * and it does not print an error every frame.
 */

#include "platform.h"

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOICES      16          /* one-shot sounds at once */
#define BUFFERS      4          /* ring depth */
#define BUF_FRAMES 512          /* 32 ms at 16 kHz */

typedef struct {
    const unsigned char *pcm;   /* caller-owned, unsigned 8-bit mono */
    int                  frames;
    unsigned             pos;   /* 16.16 into pcm, so a source rate that is
                                 * not the device rate still plays at the
                                 * right SPEED */
    unsigned             step;  /* 16.16: src_rate / device_rate */
    float                gain;
    int                  active;
} voice;

static HWAVEOUT g_dev;
static int      g_open;
static int      g_rate = 16000;
static voice    g_voice[VOICES];
static WAVEHDR  g_hdr[BUFFERS];
static short    g_buf[BUFFERS][BUF_FRAMES];
static int      g_next;         /* the buffer to refill next */
static int      g_music;
static float    g_music_gain = 1.0f;

int plat_audio_open(int rate)
{
    WAVEFORMATEX wf;
    int i;

    if (g_open)
        return 1;

    g_rate = rate > 0 ? rate : 16000;

    memset(&wf, 0, sizeof wf);
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = (DWORD)g_rate;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = (WORD)(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    if (waveOutOpen(&g_dev, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        /* Said once, not once a frame. Silence is a working state. */
        fprintf(stderr, "audio: no output device, running silent\n");
        g_dev = NULL;
        return 0;
    }

    memset(g_hdr, 0, sizeof g_hdr);
    memset(g_buf, 0, sizeof g_buf);
    for (i = 0; i < BUFFERS; i++) {
        g_hdr[i].lpData         = (LPSTR)g_buf[i];
        g_hdr[i].dwBufferLength = (DWORD)(BUF_FRAMES * sizeof(short));
        waveOutPrepareHeader(g_dev, &g_hdr[i], sizeof g_hdr[i]);
        /* Mark them done so the first update fills every one. */
        g_hdr[i].dwFlags |= WHDR_DONE;
    }

    memset(g_voice, 0, sizeof g_voice);
    g_next = 0;
    g_open = 1;
    return 1;
}

void plat_audio_close(void)
{
    int i;

    if (!g_open)
        return;
    plat_music_stop();
    waveOutReset(g_dev);
    for (i = 0; i < BUFFERS; i++)
        waveOutUnprepareHeader(g_dev, &g_hdr[i], sizeof g_hdr[i]);
    waveOutClose(g_dev);
    g_dev  = NULL;
    g_open = 0;
}

int plat_audio_play_at(const unsigned char *pcm, int frames, int rate, float gain)
{
    int i, quietest = -1;
    float lowest = 1e30f;
    unsigned step;

    if (!g_open || pcm == NULL || frames <= 0)
        return 0;

    /* **The assets are NOT all one rate.** 496 of the game's 497 .wav files are
     * 16 kHz and `Arrowhit.wav` is 44,100 -- so a mixer that assumes the device
     * rate plays that one at a third of its speed. One 16.16 step per voice
     * fixes it for any file and costs a shift in the inner loop. */
    if (rate <= 0)
        rate = g_rate;
    step = (unsigned)(((double)rate / (double)g_rate) * 65536.0 + 0.5);
    if (step == 0)
        step = 1;

    for (i = 0; i < VOICES; i++) {
        if (!g_voice[i].active) {
            g_voice[i].pcm    = pcm;
            g_voice[i].frames = frames;
            g_voice[i].pos    = 0;
            g_voice[i].step   = step;
            g_voice[i].gain   = gain;
            g_voice[i].active = 1;
            return 1;
        }
        if (g_voice[i].gain < lowest) {
            lowest = g_voice[i].gain;
            quietest = i;
        }
    }

    /* Every voice busy. Steal the quietest only if the newcomer is louder --
     * so a burst of hits never silences the one the player is listening for,
     * and a background footstep never evicts a punch. */
    if (quietest >= 0 && gain > lowest) {
        g_voice[quietest].pcm    = pcm;
        g_voice[quietest].frames = frames;
        g_voice[quietest].pos    = 0;
        g_voice[quietest].step   = step;
        g_voice[quietest].gain   = gain;
        g_voice[quietest].active = 1;
        return 1;
    }
    return 0;
}

/* The voices play straight from the caller's buffer, so one still sounding
 * when its sound is deleted -- the announcer at the end of a fight, say --
 * would go on reading freed memory. Everything runs on the main thread (no
 * callback), so clearing the voice here is enough. */
void plat_audio_stop_pcm(const unsigned char *pcm)
{
    int i;

    for (i = 0; i < VOICES; i++)
        if (g_voice[i].active && g_voice[i].pcm == pcm)
            g_voice[i].active = 0;
}

/* Every voice off at once: a save state loaded (savestate.c) puts back the
 * sounds the voices were reading. */
void plat_audio_stop_all(void)
{
    int i;

    for (i = 0; i < VOICES; i++)
        g_voice[i].active = 0;
}

/* The old spelling, for callers whose source really is the device rate. */
int plat_audio_play(const unsigned char *pcm, int frames, float gain)
{
    return plat_audio_play_at(pcm, frames, g_rate, gain);
}

static void fill(short *out)
{
    int i, n;

    memset(out, 0, BUF_FRAMES * sizeof(short));

    for (i = 0; i < VOICES; i++) {
        voice *v = &g_voice[i];

        if (!v->active)
            continue;

        for (n = 0; n < BUF_FRAMES; n++) {
            unsigned idx = v->pos >> 16;
            int s, m;

            if ((int)idx >= v->frames) {
                v->active = 0;
                break;
            }
            /* unsigned 8-bit, 128 is silence */
            s = ((int)v->pcm[idx] - 128) << 8;
            m = out[n] + (int)((float)s * v->gain);
            if (m >  32767) m =  32767;
            if (m < -32768) m = -32768;
            out[n] = (short)m;

            v->pos += v->step;
        }
        if ((int)(v->pos >> 16) >= v->frames)
            v->active = 0;
    }
}

static void music_update(void);

void plat_audio_update(void)
{
    int spun = 0;

    music_update();
    if (!g_open)
        return;

    /* Refill and re-queue every buffer the device has finished with. The cap
     * stops a long stall from queueing the whole ring at once and then
     * starving. */
    while (spun < BUFFERS) {
        WAVEHDR *h = &g_hdr[g_next];

        if (!(h->dwFlags & WHDR_DONE))
            break;

        fill((short *)h->lpData);
        h->dwFlags &= ~WHDR_DONE;
        if (waveOutWrite(g_dev, h, sizeof *h) != MMSYSERR_NOERROR) {
            h->dwFlags |= WHDR_DONE;
            break;
        }
        g_next = (g_next + 1) % BUFFERS;
        spun++;
    }
}

/* ------------------------------------------------------------------- music
 *
 * **Decoded with the ACM MP3 codec and streamed through a second waveOut.**
 *
 * This used to go through MCI (`open ... type mpegvideo`), which decodes MP3
 * itself. In a 32-bit process that crashes the second time a tune is opened:
 * the first `open` plays, and the next one -- after `close` or not, the same
 * file or another -- takes the process down from inside msmpeg2adec.dll
 * (fail-fast 0xc0000602). Reproduced outside the game with a 30-line program
 * that only opens MainMenu.mp3 and then CharacterSelect.mp3; the 64-bit build
 * of the same program is fine. The game switches tunes on the way into the
 * character select, so every 32-bit build died at "Arcade".
 *
 * ACM's MP3 decoder ships with Windows in both widths, needs no install, and
 * survives any number of switches. A tune is decoded whole when it starts --
 * the 28 tunes are all MPEG-1 Layer III at 44.1 kHz, the longest about two
 * minutes, so that is a few MB and well under a frame's worth of a stall --
 * and fed to its own device a buffer at a time from plat_audio_update, the
 * same no-callback discipline the sound effects use.
 *
 * The volume is applied as the buffers are filled, so a change takes effect
 * within one buffer (about 90 ms).
 */
#define MBUFFERS       4
#define MBUF_FRAMES 4096

static HWAVEOUT g_mdev;
static WAVEHDR  g_mhdr[MBUFFERS];
static short    g_mbuf[MBUFFERS][MBUF_FRAMES * 2];
static int      g_mnext;
static short   *g_mpcm;         /* the whole tune, interleaved */
static long     g_mframes, g_mpos;
static int      g_mch, g_mloop;

/* MP3 bytes -> 16-bit PCM. NULL if the codec or the stream is not usable. */
static short *mp3_decode(const unsigned char *d, long n, int *rate, int *ch,
                         long *frames)
{
    static const int kbps_tab[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112,
                                      128, 160, 192, 224, 256, 320, 0 };
    static const int rate_tab[4] = { 44100, 48000, 32000, 0 };
    MPEGLAYER3WAVEFORMAT mf;
    WAVEFORMATEX pf;
    HACMSTREAM s;
    ACMSTREAMHEADER h;
    DWORD chunk = 8192, out_max = 0;
    unsigned char *src, *dst, *all;
    long o = 0, pos, len = 0, cap;
    int kbps, sr, nch;

    /* an ID3v2 tag first, then the first frame header */
    if (n > 10 && memcmp(d, "ID3", 3) == 0)
        o = 10 + ((d[6] & 0x7f) << 21 | (d[7] & 0x7f) << 14 |
                  (d[8] & 0x7f) << 7 | (d[9] & 0x7f));
    while (o + 4 < n && !(d[o] == 0xff && (d[o + 1] & 0xe0) == 0xe0))
        o++;
    if (o + 4 >= n)
        return NULL;
    kbps = kbps_tab[d[o + 2] >> 4];
    sr   = rate_tab[(d[o + 2] >> 2) & 3];
    nch  = (d[o + 3] >> 6) == 3 ? 1 : 2;
    if (kbps == 0 || sr == 0)
        return NULL;

    memset(&mf, 0, sizeof mf);
    mf.wfx.wFormatTag      = WAVE_FORMAT_MPEGLAYER3;
    mf.wfx.nChannels       = (WORD)nch;
    mf.wfx.nSamplesPerSec  = (DWORD)sr;
    mf.wfx.nAvgBytesPerSec = (DWORD)(kbps * 1000 / 8);
    mf.wfx.nBlockAlign     = 1;
    mf.wfx.cbSize          = MPEGLAYER3_WFX_EXTRA_BYTES;
    mf.wID                 = MPEGLAYER3_ID_MPEG;
    mf.fdwFlags            = MPEGLAYER3_FLAG_PADDING_OFF;
    mf.nBlockSize          = (WORD)(144 * kbps * 1000 / sr);
    mf.nFramesPerBlock     = 1;
    mf.nCodecDelay         = 1393;

    memset(&pf, 0, sizeof pf);
    pf.wFormatTag      = WAVE_FORMAT_PCM;
    pf.nChannels       = (WORD)nch;
    pf.nSamplesPerSec  = (DWORD)sr;
    pf.wBitsPerSample  = 16;
    pf.nBlockAlign     = (WORD)(nch * 2);
    pf.nAvgBytesPerSec = (DWORD)sr * pf.nBlockAlign;

    if (acmStreamOpen(&s, NULL, (LPWAVEFORMATEX)&mf, &pf, NULL, 0, 0, 0) != 0)
        return NULL;
    acmStreamSize(s, chunk, &out_max, ACM_STREAMSIZEF_SOURCE);

    src = (unsigned char *)malloc(chunk);
    dst = (unsigned char *)malloc(out_max);
    cap = (long)((double)(n - o) * 8.0 / (kbps * 1000.0) * sr * pf.nBlockAlign)
          + 4 * (long)out_max;
    all = (unsigned char *)malloc((size_t)cap);
    if (!src || !dst || !all) {
        free(src); free(dst); free(all);
        acmStreamClose(s, 0);
        return NULL;
    }

    memset(&h, 0, sizeof h);
    h.cbStruct    = sizeof h;
    h.pbSrc       = src;
    h.cbSrcLength = chunk;
    h.pbDst       = dst;
    h.cbDstLength = out_max;
    acmStreamPrepareHeader(s, &h, 0);
    for (pos = o; pos < n; ) {
        DWORD take = (DWORD)(n - pos < (long)chunk ? n - pos : (long)chunk);

        memcpy(src, d + pos, take);
        h.cbSrcLength = take;
        if (acmStreamConvert(s, &h, ACM_STREAMCONVERTF_BLOCKALIGN) != 0)
            break;
        if (h.cbSrcLengthUsed == 0 && h.cbDstLengthUsed == 0)
            break;
        pos += (long)h.cbSrcLengthUsed;
        if (len + (long)h.cbDstLengthUsed > cap) {
            unsigned char *more = (unsigned char *)realloc(all, (size_t)cap * 2);
            if (!more)
                break;
            all = more;
            cap *= 2;
        }
        memcpy(all + len, dst, h.cbDstLengthUsed);
        len += (long)h.cbDstLengthUsed;
    }
    h.cbSrcLength = chunk;
    acmStreamUnprepareHeader(s, &h, 0);
    acmStreamClose(s, 0);
    free(src);
    free(dst);

    if (len == 0) {
        free(all);
        return NULL;
    }
    *rate   = sr;
    *ch     = nch;
    *frames = len / pf.nBlockAlign;
    return (short *)all;
}

static void music_fill(short *out)
{
    long i, n = (long)MBUF_FRAMES * g_mch;
    long at = g_mpos * g_mch;
    long end = g_mframes * g_mch;

    for (i = 0; i < n; i++) {
        if (at >= end) {
            if (!g_mloop) {
                out[i] = 0;
                continue;
            }
            at = 0;
        }
        out[i] = (short)((float)g_mpcm[at++] * g_music_gain);
    }
    g_mpos = at / g_mch;
}

static void music_update(void)
{
    int spun = 0;

    if (!g_music)
        return;
    while (spun < MBUFFERS) {
        WAVEHDR *h = &g_mhdr[g_mnext];

        if (!(h->dwFlags & WHDR_DONE))
            break;
        music_fill((short *)h->lpData);
        h->dwFlags &= ~WHDR_DONE;
        if (waveOutWrite(g_mdev, h, sizeof *h) != MMSYSERR_NOERROR) {
            h->dwFlags |= WHDR_DONE;
            break;
        }
        g_mnext = (g_mnext + 1) % MBUFFERS;
        spun++;
    }
}

void plat_music_play(const char *path, int loop)
{
    FILE *f;
    long n;
    unsigned char *mp3;
    int rate, ch, i;
    WAVEFORMATEX wf;

    plat_music_stop();

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "music: cannot open %s\n", path);
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    mp3 = n > 0 ? (unsigned char *)malloc((size_t)n) : NULL;
    if (!mp3 || fread(mp3, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(mp3);
        return;
    }
    fclose(f);

    g_mpcm = mp3_decode(mp3, n, &rate, &ch, &g_mframes);
    free(mp3);
    if (!g_mpcm) {
        fprintf(stderr, "music: cannot decode %s\n", path);
        return;
    }

    memset(&wf, 0, sizeof wf);
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = (WORD)ch;
    wf.nSamplesPerSec  = (DWORD)rate;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = (WORD)(ch * 2);
    wf.nAvgBytesPerSec = (DWORD)rate * wf.nBlockAlign;
    if (waveOutOpen(&g_mdev, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        free(g_mpcm);
        g_mpcm = NULL;
        return;                         /* silent, as with no sound device */
    }

    memset(g_mhdr, 0, sizeof g_mhdr);
    for (i = 0; i < MBUFFERS; i++) {
        g_mhdr[i].lpData         = (LPSTR)g_mbuf[i];
        g_mhdr[i].dwBufferLength = (DWORD)(MBUF_FRAMES * ch * sizeof(short));
        waveOutPrepareHeader(g_mdev, &g_mhdr[i], sizeof g_mhdr[i]);
        g_mhdr[i].dwFlags |= WHDR_DONE;
    }
    g_mch   = ch;
    g_mpos  = 0;
    g_mloop = loop;
    g_mnext = 0;
    g_music = 1;
    music_update();                     /* start now, not a frame later */
}

/* Kept across tracks, as GBMusicTrack's gain is set once by limePlayTune and
 * then only by limeSetTuneVol. */
void plat_music_volume(float gain)
{
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    g_music_gain = gain;
}

void plat_music_stop(void)
{
    int i;

    if (!g_music)
        return;
    waveOutReset(g_mdev);
    for (i = 0; i < MBUFFERS; i++)
        waveOutUnprepareHeader(g_mdev, &g_mhdr[i], sizeof g_mhdr[i]);
    waveOutClose(g_mdev);
    g_mdev = NULL;
    free(g_mpcm);
    g_mpcm = NULL;
    g_music = 0;
}
