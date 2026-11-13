/* Discord Rich Presence over the Discord client's local IPC (discord_presence.h).
 *
 * The protocol (Discord's RPC, what its Social SDK's unauthenticated path
 * and every Rich Presence client speak to the desktop app): frames of
 *     int32 opcode, int32 length (little-endian), UTF-8 JSON
 * opcode 0 HANDSHAKE {"v":1,"client_id":...}, 1 FRAME (commands and their
 * answers; the first answer is the READY dispatch), 2 CLOSE, 3 PING, 4 PONG.
 * Commands used: SET_ACTIVITY {pid, activity} and SUBSCRIBE ACTIVITY_JOIN.
 * Checked against the project machine's Discord on 2026-11-12: the handshake
 * answered READY, SET_ACTIVITY was accepted with an https large image (Discord
 * proxies it as mp:external/...), a party and a join secret, and the
 * ACTIVITY_JOIN subscription was accepted.
 *
 * Windows: \\.\pipe\discord-ipc-0..9.  Linux: <dir>/discord-ipc-0..9 with
 * <dir> from XDG_RUNTIME_DIR, TMPDIR, TMP, TEMP or /tmp, and the Flatpak and
 * Snap Discord's subfolders. */
#include "discord_presence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define RETRY_S   8.0                   /* not connected: try again after */
#define MIN_GAP_S 4.0                   /* Discord: 5 updates per 20 s */

static int    g_on = 1;
static int    g_ready;                  /* READY received */
static double g_next_try;
static double g_last_send = -1e9;
static int    g_dirty;                  /* the activity changed, not sent yet */
static int    g_have;                   /* an activity to show (else cleared) */
static struct discord_activity g_act;
static char   g_join[160];
static int    g_join_new;
static unsigned g_nonce;
static unsigned char g_in[65536];
static int    g_in_n;

#ifdef _WIN32
static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static int is_open(void) { return g_pipe != INVALID_HANDLE_VALUE; }
#else
static int g_sock = -1;
static int is_open(void) { return g_sock >= 0; }
#endif

static void close_pipe(void)
{
#ifdef _WIN32
    if (g_pipe != INVALID_HANDLE_VALUE)
        CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
#else
    if (g_sock >= 0)
        close(g_sock);
    g_sock = -1;
#endif
    g_ready = 0;
    g_in_n = 0;
}

static int try_open(void)
{
    int i;
#ifdef _WIN32
    for (i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof name, "\\\\.\\pipe\\discord-ipc-%d", i);
        g_pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (g_pipe != INVALID_HANDLE_VALUE)
            return 1;
    }
    return 0;
#else
    static const char *subs[] = { "", "app/com.discordapp.Discord/", "snap.discord/",
                                  ".flatpak/com.discordapp.Discord/xdg-run/" };
    const char *envs[] = { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" };
    const char *dirs[5];
    int nd = 0, d, s;
    for (d = 0; d < 4; d++) {
        const char *e = getenv(envs[d]);
        if (e && e[0]) dirs[nd++] = e;
    }
    dirs[nd++] = "/tmp";
    for (d = 0; d < nd; d++)
        for (s = 0; s < (int)(sizeof subs / sizeof subs[0]); s++)
            for (i = 0; i < 10; i++) {
                struct sockaddr_un a;
                int fd;
                memset(&a, 0, sizeof a);
                a.sun_family = AF_UNIX;
                if (snprintf(a.sun_path, sizeof a.sun_path, "%s/%sdiscord-ipc-%d", dirs[d], subs[s], i)
                    >= (int)sizeof a.sun_path)
                    continue;
                fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd < 0)
                    return 0;
                if (connect(fd, (struct sockaddr *)&a, sizeof a) == 0) {
                    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
                    { int one = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one); }
#endif
                    g_sock = fd;
                    return 1;
                }
                close(fd);
            }
    return 0;
#endif
}

static int send_frame(int op, const char *json)
{
    unsigned char h[8];
    int len = (int)strlen(json);
    h[0] = (unsigned char)op; h[1] = (unsigned char)(op >> 8); h[2] = (unsigned char)(op >> 16); h[3] = (unsigned char)(op >> 24);
    h[4] = (unsigned char)len; h[5] = (unsigned char)(len >> 8); h[6] = (unsigned char)(len >> 16); h[7] = (unsigned char)(len >> 24);
#ifdef _WIN32
    {
        DWORD w;
        if (!WriteFile(g_pipe, h, 8, &w, NULL) || w != 8 || !WriteFile(g_pipe, json, (DWORD)len, &w, NULL) || (int)w != len) {
            close_pipe();
            return 0;
        }
    }
#else
    {
        unsigned char buf[8 + 4096];
        int n = 8 + len, off = 0;
        if (len > 4096) return 0;
        memcpy(buf, h, 8);
        memcpy(buf + 8, json, (size_t)len);
        while (off < n) {
#ifdef MSG_NOSIGNAL
            ssize_t k = send(g_sock, buf + off, (size_t)(n - off), MSG_NOSIGNAL);
#else
            ssize_t k = send(g_sock, buf + off, (size_t)(n - off), 0);
#endif
            if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;   /* tiny frames: brief */
            if (k <= 0) { close_pipe(); return 0; }
            off += (int)k;
        }
    }
#endif
    return 1;
}

/* JSON string body, escaped */
static int put_str(char *out, int n, int at, const char *s)
{
    for (; *s && at < n - 8; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { out[at++] = '\\'; out[at++] = (char)c; }
        else if (c < 0x20) at += snprintf(out + at, (size_t)(n - at), "\\u%04x", c);
        else out[at++] = (char)c;
    }
    out[at] = 0;
    return at;
}

#define CAT(...) do { at += snprintf(out + at, (size_t)(at < n ? n - at : 0), __VA_ARGS__); if (at >= n) return -1; } while (0)
#define STR(s)   do { at = put_str(out, n, at, (s)); } while (0)

int discord_activity_json(const struct discord_activity *a, int pid, const char *nonce, char *out, int n)
{
    int at = 0;
    CAT("{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%s\",\"args\":{\"pid\":%d", nonce, pid);
    if (a) {
        CAT(",\"activity\":{\"type\":0");
        if (a->details[0]) { CAT(",\"details\":\""); STR(a->details); CAT("\""); }
        if (a->state[0])   { CAT(",\"state\":\"");   STR(a->state);   CAT("\""); }
        if (a->start > 0)  CAT(",\"timestamps\":{\"start\":%lld}", a->start);
        CAT(",\"assets\":{\"large_image\":\"%s\"", DISCORD_LARGE_IMAGE);
        if (a->large_text[0]) { CAT(",\"large_text\":\""); STR(a->large_text); CAT("\""); }
        CAT("}");
        if (a->party_id[0]) {
            CAT(",\"party\":{\"id\":\""); STR(a->party_id); CAT("\"");
            if (a->party_max > 0)
                CAT(",\"size\":[%d,%d]", a->party_size > 0 ? a->party_size : 1, a->party_max);
            CAT("}");
        }
        if (a->join[0]) { CAT(",\"secrets\":{\"join\":\""); STR(a->join); CAT("\"}"); }
        CAT(",\"instance\":false}");
    }
    CAT("}}");
    return at;
}

/* "secret":"..." of an ACTIVITY_JOIN dispatch */
int discord_find_join_secret(const char *json, char *secret, int n)
{
    const char *p;
    int k = 0;
    if (!strstr(json, "\"ACTIVITY_JOIN\"") || strstr(json, "\"ACTIVITY_JOIN_REQUEST\""))
        return 0;
    if (!strstr(json, "\"DISPATCH\""))
        return 0;
    p = strstr(json, "\"secret\"");
    if (!p) return 0;
    p = strchr(p + 8, '"');
    if (!p) return 0;
    for (p++; *p && *p != '"' && k < n - 1; p++) {
        if (*p == '\\' && p[1]) p++;
        secret[k++] = *p;
    }
    secret[k] = 0;
    return k > 0;
}

static int get_pid(void)
{
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

static void send_activity(double now)
{
    char json[4096], nonce[16];
    snprintf(nonce, sizeof nonce, "a%u", ++g_nonce);
    if (discord_activity_json(g_have ? &g_act : NULL, get_pid(), nonce, json, sizeof json) < 0)
        return;
    if (send_frame(1, json)) {
        g_dirty = 0;
        g_last_send = now;
    }
}

static void on_message(int op, char *json)
{
    if (op == 2) {                      /* CLOSE: a bad id, or Discord quitting */
        printf("discord: closed by Discord: %.200s\n", json);
        close_pipe();
        return;
    }
    if (op == 3) {                      /* PING */
        send_frame(4, json);
        return;
    }
    if (op != 1)
        return;
    if (!g_ready && strstr(json, "\"READY\"")) {
        g_ready = 1;
        printf("discord: connected\n");
        send_frame(1, "{\"cmd\":\"SUBSCRIBE\",\"evt\":\"ACTIVITY_JOIN\",\"nonce\":\"s1\"}");
        g_dirty = 1;
        g_last_send = -1e9;
        return;
    }
    {
        char s[160];
        if (discord_find_join_secret(json, s, sizeof s)) {
            printf("discord: join from Discord\n");
            snprintf(g_join, sizeof g_join, "%s", s);
            g_join_new = 1;
        }
    }
    if (strstr(json, "\"evt\":\"ERROR\""))
        printf("discord: %.300s\n", json);
}

static void read_some(void)
{
    for (;;) {
        int got = 0;
#ifdef _WIN32
        DWORD avail = 0, r = 0;
        if (!PeekNamedPipe(g_pipe, NULL, 0, NULL, &avail, NULL)) { close_pipe(); return; }
        if (!avail) break;
        if (avail > (DWORD)(sizeof g_in - (size_t)g_in_n)) avail = (DWORD)(sizeof g_in - (size_t)g_in_n);
        if (!avail) { g_in_n = 0; break; }      /* a frame larger than the buffer: drop */
        if (!ReadFile(g_pipe, g_in + g_in_n, avail, &r, NULL)) { close_pipe(); return; }
        got = (int)r;
#else
        ssize_t k = recv(g_sock, g_in + g_in_n, sizeof g_in - (size_t)g_in_n, 0);
        if (k == 0) { close_pipe(); return; }
        if (k < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close_pipe();
            return;
        }
        got = (int)k;
#endif
        g_in_n += got;
        while (g_in_n >= 8) {
            int op = g_in[0] | g_in[1] << 8 | g_in[2] << 16 | g_in[3] << 24;
            int len = g_in[4] | g_in[5] << 8 | g_in[6] << 16 | g_in[7] << 24;
            char *msg;
            if (len < 0 || len > (int)sizeof g_in - 8) { close_pipe(); return; }
            if (g_in_n < 8 + len) break;
            msg = (char *)malloc((size_t)len + 1);
            if (!msg) { close_pipe(); return; }
            memcpy(msg, g_in + 8, (size_t)len);
            msg[len] = 0;
            memmove(g_in, g_in + 8 + len, (size_t)(g_in_n - 8 - len));
            g_in_n -= 8 + len;
            on_message(op, msg);
            free(msg);
            if (!is_open()) return;
        }
        if (!got) break;
    }
}

void discord_presence_enable(int on)
{
    const char *e = getenv("UMK3_DISCORD");
    if (e && e[0] == '0')
        on = 0;
    if (!on && is_open())
        close_pipe();
    g_on = on;
}

void discord_presence_tick(double now)
{
    if (!g_on)
        return;
    if (!is_open()) {
        char hs[128];
        if (now < g_next_try)
            return;
        g_next_try = now + RETRY_S;
        if (!try_open())
            return;
        snprintf(hs, sizeof hs, "{\"v\":1,\"client_id\":\"%s\"}", DISCORD_APP_ID);
        if (!send_frame(0, hs))
            return;
    }
    read_some();
    if (is_open() && g_ready && g_dirty && now - g_last_send >= MIN_GAP_S)
        send_activity(now);
}

void discord_presence_set(const struct discord_activity *a)
{
    if (g_have && !memcmp(&g_act, a, sizeof g_act))
        return;
    g_act = *a;
    g_have = 1;
    g_dirty = 1;
}

void discord_presence_clear(void)
{
    if (!g_have)
        return;
    g_have = 0;
    g_dirty = 1;
}

int discord_presence_take_join(char *secret, int n)
{
    if (!g_join_new)
        return 0;
    g_join_new = 0;
    snprintf(secret, (size_t)n, "%s", g_join);
    return 1;
}

int discord_presence_connected(void) { return is_open() && g_ready; }

void discord_presence_shutdown(void)
{
    if (is_open() && g_ready) {
        g_have = 0;
        send_activity(0.0);
    }
    close_pipe();
}
