/*
 * mp_net.c -- the network session under the game's multiplayer code. Port,
 * not in the binary.
 *
 * The iPhone game kept its multiplayer in two files. limeMPGameSpecific.m is
 * plain C -- the packets, the heartbeat, the sync, the sprite-list queue --
 * and is transcribed in decomp/gamecode/limeMPGameSpecific.c. limeMPSession.m
 * is an Objective-C class around GKSession, Apple's Bluetooth peer-to-peer
 * session, and that is what this file replaces: the C file asks "is there a
 * session", "am I the server", "send this packet", and the answers come from
 * here instead of from GameKit.
 *
 * What the original put on the wire is kept: every game packet is a 4-byte
 * big-endian type followed by its payload, sent reliably except the sprite
 * lists (5) and the joystick (6), which went unreliable. GKSession gave
 * reliable delivery itself; UDP does not, so reliable packets carry a
 * sequence number here, are acknowledged, sent again until they are, and
 * handed over in order.
 *
 * ## Finding the other player
 *
 * The ONLINE screen (FrontEnd.c, FE_Task_Online) opens a session. FIND MATCH
 * asks for a player by their code -- the id in their profiles.ini -- and the
 * game with that code, if its ONLINE screen is open, answers and hosts. The
 * question goes two ways at once: to the games on this computer and on the
 * local network (UDP ports 27560-27575, broadcast), and over the internet
 * through Epic Online Services (mp_eos.c), where the game with the ONLINE
 * screen open owns a lobby named after its code. Whichever answers is the
 * link, and the same messages go over either: over EOS they are P2P
 * datagrams, acknowledged here exactly as over UDP.
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mp_eos.h"
#include "mp_net.h"
#include "profiles.h"

/* limeMPGameSpecific.c */
void processPacket(const void *payload, long len, long type);
void disconnected(void);
void endMP(void);

#define PORT_FIRST    27560
#define PORT_COUNT    16
#define MAX_PACKET    1400
#define RESEND_MS     120
#define JOIN_MS       400
#define FIND_MS       10000             /* "not found" after ten seconds */
#define FIND_EOS_MS   30000             /* found over the internet: thirty */
#define KEEP_MS       1000
#define LINK_LOST_MS  15000
#define WINDOW        256               /* reliable packets in flight */

enum { K_JOIN = 1, K_ACCEPT, K_BUSY, K_DATA_U, K_DATA_R, K_ACK, K_BYE, K_KEEP };

static SOCKET   g_sock = INVALID_SOCKET;
static int      g_port;
static int      g_session;              /* gSession exists */
static int      g_connected;
static int      g_server;
static int      g_status = MPNET_IDLE;
/* Where a message came from or goes: a UDP address, or an EOS user. */
struct paddr { int eos; struct sockaddr_in in; char who[40]; };
static struct paddr g_peer_addr;
static char     g_eos_target[40];       /* the code's owner, from the lobby search */
static struct mpnet_peer g_peer;
static char     g_want[16];             /* the code FIND MATCH asked for */
static unsigned g_find_start, g_join_last, g_keep_last, g_heard;

/* reliable channel */
struct pend { int used; uint32_t seq; unsigned sent; int len;
              unsigned char buf[MAX_PACKET]; };
static struct pend g_out[WINDOW];
static uint32_t g_out_seq;              /* next to send */
static uint32_t g_in_next;              /* next to hand over */
struct held { int used; uint32_t seq; int len; unsigned char buf[MAX_PACKET]; };
static struct held g_in[WINDOW];

static unsigned now_ms(void)
{
#ifdef _WIN32
    return (unsigned)GetTickCount();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
#endif
}

static void put32(unsigned char *p, uint32_t v)
{ p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const unsigned char *p)
{ return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static void hdr(unsigned char *p, int kind)
{ p[0] = 'U'; p[1] = '3'; p[2] = (unsigned char)kind; p[3] = 1; }

/* ------------------------------------------------------------- socket */

static int sock_open(void)
{
    struct sockaddr_in a;
    int i, on = 1;

    if (g_sock != INVALID_SOCKET)
        return 1;
#ifdef _WIN32
    {
        static int up;
        WSADATA w;
        if (!up && WSAStartup(MAKEWORD(2, 2), &w) != 0)
            return 0;
        up = 1;
    }
#endif
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_sock == INVALID_SOCKET)
        return 0;
    setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof on);
#ifdef _WIN32
    {
        u_long nb = 1;
        ioctlsocket(g_sock, FIONBIO, &nb);
    }
#else
    fcntl(g_sock, F_SETFL, fcntl(g_sock, F_GETFL) | O_NONBLOCK);
#endif
    for (i = 0; i < PORT_COUNT; i++) {
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons((unsigned short)(PORT_FIRST + i));
        if (bind(g_sock, (struct sockaddr *)&a, sizeof a) == 0) {
            g_port = PORT_FIRST + i;
            printf("online: listening on UDP %d\n", g_port);
            return 1;
        }
    }
    printf("online: no free UDP port in %d-%d\n", PORT_FIRST,
           PORT_FIRST + PORT_COUNT - 1);
    closesocket(g_sock);
    g_sock = INVALID_SOCKET;
    return 0;
}

static void sock_close(void)
{
    if (g_sock != INVALID_SOCKET)
        closesocket(g_sock);
    g_sock = INVALID_SOCKET;
    g_port = 0;
}

static void send_in(const struct sockaddr_in *a, const void *p, int n)
{
    if (g_sock != INVALID_SOCKET)
        sendto(g_sock, (const char *)p, n, 0, (const struct sockaddr *)a,
               sizeof *a);
}

static void send_to(const struct paddr *a, const void *p, int n)
{
    if (a->eos)
        mpeos_send(a->who, p, n);
    else
        send_in(&a->in, p, n);
}

static void send_peer(const void *p, int n) { send_to(&g_peer_addr, p, n); }

/* ---------------------------------------------------------- profile */

#define INFO_BYTES (16 + 100 + 16)

static void info_put(unsigned char *p)
{
    const struct profile *pr = prof_get();

    memset(p, 0, INFO_BYTES);
    if (!pr)
        return;
    memcpy(p, pr->id, 15);
    snprintf((char *)p + 16, 100, "%s", pr->name);
    put32(p + 116, (uint32_t)pr->icon);
    put32(p + 120, pr->color);
    put32(p + 124, pr->color2);
    /* the gradient's kind, and above it the angle + 1 (0: none) and the
     * outfit: an older game reads the word as the kind and draws the icon
     * without them */
    put32(p + 128, ((uint32_t)pr->dark & 0xff) | ((uint32_t)(pr->angle + 1) & 0x1ff) << 8
                   | (uint32_t)(pr->alt ? 1 : 0) << 20);
}

static void info_get(const unsigned char *p, struct mpnet_peer *o)
{
    memset(o, 0, sizeof *o);
    memcpy(o->code, p, 15);
    memcpy(o->name, p + 16, 99);
    o->icon = (int)get32(p + 116);
    if (o->icon < 0 || o->icon >= PROF_ICONS)
        o->icon = 0;
    o->color = get32(p + 120);
    o->color2 = get32(p + 124);
    {
        uint32_t v = get32(p + 128);
        o->dark = (int)(v & 0xff);
        o->angle = (int)((v >> 8) & 0x1ff) - 1;
        if (o->angle >= 360)
            o->angle = -1;
        o->alt = (int)((v >> 20) & 1);
    }
}

static const char *my_code(void)
{
    const struct profile *pr = prof_get();
    return pr ? pr->id : "";
}

/* Codes compare without the dashes and the case, so "abcd efgh ijkl" finds
 * ABCD-EFGH-IJKL. */
static int same_code(const char *a, const char *b)
{
    for (;;) {
        while (*a && !((*a >= '0' && *a <= '9') || (*a >= 'A' && *a <= 'Z')
                       || (*a >= 'a' && *a <= 'z')))
            a++;
        while (*b && !((*b >= '0' && *b <= '9') || (*b >= 'A' && *b <= 'Z')
                       || (*b >= 'a' && *b <= 'z')))
            b++;
        if (!*a || !*b)
            return !*a && !*b;
        if ((*a | 0x20) != (*b | 0x20))
            return 0;
        a++;
        b++;
    }
}

/* ------------------------------------------------------- connection */

static void reliable_reset(void)
{
    memset(g_out, 0, sizeof g_out);
    memset(g_in, 0, sizeof g_in);
    g_out_seq = 0;
    g_in_next = 0;
}

static void connect_to(const struct paddr *a, const unsigned char *info,
                       int server)
{
    g_peer_addr = *a;
    info_get(info, &g_peer);
    g_server = server;
    g_connected = 1;
    g_status = MPNET_CONNECTED;
    g_heard = g_keep_last = now_ms();
    g_want[0] = 0;
    reliable_reset();
    if (a->eos)
        printf("online: connected to %s (%s) over the internet, %s\n",
               g_peer.name, g_peer.code, server ? "hosting" : "joining");
    else
        printf("online: connected to %s (%s) at %s:%d, %s\n", g_peer.name,
               g_peer.code, inet_ntoa(a->in.sin_addr), ntohs(a->in.sin_port),
               server ? "hosting" : "joining");
}

/* The link went: as when GKSession reported the peer gone. */
static void link_lost(void)
{
    if (!g_connected)
        return;
    printf("online: the other player is gone\n");
    g_connected = 0;
    g_status = MPNET_LOST;
    disconnected();
    endMP();
}

/* ------------------------------------------------- session interface */

int mpnet_session_exists(void) { return g_session; }

void mpnet_session_create(void)
{
    /* UMK3_ONLINE_NOLAN=1: the internet alone (testing EOS on one PC) */
    const char *nolan = getenv("UMK3_ONLINE_NOLAN");
    int lan = !(nolan && nolan[0] == '1') && sock_open();

    mpeos_start();
    if (!lan && (mpeos_state() == MPEOS_OFF || mpeos_state() == MPEOS_FAILED))
        return;
    mpeos_advertise(my_code());
    g_session = 1;
    g_connected = 0;
    g_server = 0;
    g_want[0] = 0;
    if (g_status != MPNET_LOST)
        g_status = MPNET_IDLE;
}

void mpnet_session_destroy(void)
{
    if (g_connected) {
        unsigned char b[4];
        hdr(b, K_BYE);
        send_peer(b, 4);
        send_peer(b, 4);
    }
    g_connected = 0;
    g_session = 0;
    g_want[0] = 0;
    mpeos_unadvertise();
    sock_close();
}

int mpnet_connection_state(void) { return g_connected ? 2 : 0; }
int mpnet_session_state(void)    { return g_connected ? 2 : 0; }
int mpnet_am_server(void)        { return g_connected && g_server; }
int mpnet_is_working(void)       { return g_session && g_want[0] != 0; }
int mpnet_peer_count(void)       { return 0; }
int mpnet_internet(void)         { return mpeos_state(); }

void mpnet_send(long type, const void *data, int len, int reliable)
{
    unsigned char b[MAX_PACKET];
    int n;

    if (!g_connected || len < 0 || len > MAX_PACKET - 12)
        return;
    if (!reliable) {
        hdr(b, K_DATA_U);
        b[4] = (unsigned char)(type >> 24); b[5] = (unsigned char)(type >> 16);
        b[6] = (unsigned char)(type >> 8);  b[7] = (unsigned char)type;
        memcpy(b + 8, data, (size_t)len);
        send_peer(b, len + 8);
        return;
    }
    {
        struct pend *p = &g_out[g_out_seq % WINDOW];
        if (p->used) {
            /* a whole window unacknowledged: the link is as good as gone */
            printf("online: send window full\n");
            return;
        }
        hdr(p->buf, K_DATA_R);
        put32(p->buf + 4, g_out_seq);
        p->buf[8] = (unsigned char)(type >> 24); p->buf[9] = (unsigned char)(type >> 16);
        p->buf[10] = (unsigned char)(type >> 8); p->buf[11] = (unsigned char)type;
        memcpy(p->buf + 12, data, (size_t)len);
        n = len + 12;
        p->len = n;
        p->seq = g_out_seq++;
        p->used = 1;
        p->sent = now_ms();
        send_peer(p->buf, n);
    }
}

/* ------------------------------------------------------- ONLINE screen */

void mpnet_find(const char *code)
{
    if (!g_session)
        mpnet_session_create();
    if (!g_session || g_connected)
        return;
    snprintf(g_want, sizeof g_want, "%s", code);
    g_eos_target[0] = 0;
    mpeos_lookup(code);
    g_find_start = now_ms();
    g_join_last = 0;
    g_status = MPNET_SEARCHING;
}

void mpnet_cancel_find(void)
{
    g_want[0] = 0;
    if (!g_connected)
        g_status = MPNET_IDLE;
}

int mpnet_status(void) { return g_status; }

const struct mpnet_peer *mpnet_peer(void)
{
    return g_peer.code[0] ? &g_peer : NULL;
}

static void send_join(void)
{
    unsigned char b[4 + 16 + INFO_BYTES];
    struct sockaddr_in a;
    struct paddr e;
    int i;

    hdr(b, K_JOIN);
    memset(b + 4, 0, 16);
    memcpy(b + 4, g_want, strlen(g_want) < 15 ? strlen(g_want) : 15);
    info_put(b + 20);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    for (i = 0; i < PORT_COUNT; i++) {
        if (PORT_FIRST + i == g_port)
            continue;
        a.sin_port = htons((unsigned short)(PORT_FIRST + i));
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        send_in(&a, b, sizeof b);
        a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        send_in(&a, b, sizeof b);
    }
    if (g_eos_target[0]) {
        memset(&e, 0, sizeof e);
        e.eos = 1;
        snprintf(e.who, sizeof e.who, "%s", g_eos_target);
        send_to(&e, b, sizeof b);
    }
}

/* -------------------------------------------------------------- poll */

static int from_peer(const struct paddr *a)
{
    if (!g_connected || a->eos != g_peer_addr.eos)
        return 0;
    if (a->eos)
        return !strcmp(a->who, g_peer_addr.who);
    return a->in.sin_addr.s_addr == g_peer_addr.in.sin_addr.s_addr
           && a->in.sin_port == g_peer_addr.in.sin_port;
}

static void deliver(const unsigned char *p, int n)
{
    long type = (long)((uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]);
    processPacket(p + 4, n - 4, type);
}

static void on_packet(const unsigned char *b, int n, const struct paddr *a)
{
    if (n < 4 || b[0] != 'U' || b[1] != '3')
        return;
    switch (b[2]) {
    case K_JOIN: {
        unsigned char r[4 + INFO_BYTES];
        char want[16];

        if (n < 4 + 16 + INFO_BYTES || !g_session)
            return;
        memcpy(want, b + 4, 15);
        want[15] = 0;
        if (!same_code(want, my_code()))
            return;
        {
            char theirs[16];
            memcpy(theirs, b + 20, 15);
            theirs[15] = 0;
            if (g_connected && !from_peer(a)) {
                /* the same player again by another route (the join goes
                 * to localhost and to the broadcast address) is not busy */
                if (!same_code(theirs, g_peer.code)) {
                    hdr(r, K_BUSY);
                    send_to(a, r, 4);
                }
                return;
            }
        }
        if (!g_connected) {
            char theirs[16];
            memcpy(theirs, b + 20, 15);
            theirs[15] = 0;
            /* both asked for each other at once: the lower code hosts,
             * the other waits for its ACCEPT */
            if (g_want[0] && same_code(g_want, theirs)
                && strcmp(my_code(), theirs) > 0)
                return;
            connect_to(a, b + 20, 1);
        }
        hdr(r, K_ACCEPT);
        info_put(r + 4);
        send_to(a, r, sizeof r);
        break;
    }
    case K_ACCEPT:
        if (n < 4 + INFO_BYTES || g_connected || !g_want[0])
            return;
        {
            struct mpnet_peer t;
            info_get(b + 4, &t);
            if (!same_code(t.code, g_want))
                return;
        }
        connect_to(a, b + 4, 0);
        break;
    case K_BUSY:
        if (!g_connected && g_want[0]) {
            g_want[0] = 0;
            g_status = MPNET_BUSY;
        }
        break;
    case K_DATA_U:
        if (from_peer(a) && n >= 8) {
            g_heard = now_ms();
            deliver(b + 4, n - 4);
        }
        break;
    case K_DATA_R: {
        uint32_t seq;
        unsigned char ack[8];

        if (!from_peer(a) || n < 12)
            return;
        g_heard = now_ms();
        seq = get32(b + 4);
        hdr(ack, K_ACK);
        put32(ack + 4, seq);
        send_peer(ack, 8);
        if ((int32_t)(seq - g_in_next) < 0 || seq - g_in_next >= WINDOW)
            return;                     /* a copy, or too far ahead */
        if (seq != g_in_next) {
            struct held *h = &g_in[seq % WINDOW];
            h->used = 1;
            h->seq = seq;
            h->len = n - 8;
            memcpy(h->buf, b + 8, (size_t)(n - 8));
            return;
        }
        g_in_next++;
        deliver(b + 8, n - 8);
        /* and whatever was waiting behind it */
        while (g_connected) {
            struct held *h = &g_in[g_in_next % WINDOW];
            if (!h->used || h->seq != g_in_next)
                break;
            h->used = 0;
            g_in_next++;
            deliver(h->buf, h->len);
        }
        break;
    }
    case K_ACK:
        if (from_peer(a) && n >= 8) {
            uint32_t seq = get32(b + 4);
            struct pend *p = &g_out[seq % WINDOW];
            g_heard = now_ms();
            if (p->used && p->seq == seq)
                p->used = 0;
        }
        break;
    case K_BYE:
        if (from_peer(a))
            link_lost();
        break;
    case K_KEEP:
        if (from_peer(a))
            g_heard = now_ms();
        break;
    }
}

void mpnet_flush(void)
{
    if (g_connected)
        mpeos_tick();
}

void mpnet_poll(void)
{
    unsigned char b[MAX_PACKET + 16];
    struct paddr a;
    unsigned t;
    int n, i;

    mpeos_tick();
    if (!g_session)
        return;
    memset(&a, 0, sizeof a);
    while (g_sock != INVALID_SOCKET) {
        socklen_t al = sizeof a.in;
        n = (int)recvfrom(g_sock, (char *)b, sizeof b, 0, (struct sockaddr *)&a.in, &al);
        if (n <= 0)
            break;
        on_packet(b, n, &a);
        if (!g_session)                 /* the packet ended the session */
            return;
    }
    for (;;) {
        struct paddr e;
        memset(&e, 0, sizeof e);
        e.eos = 1;
        n = mpeos_recv(e.who, sizeof e.who, b, sizeof b);
        if (n == 0)
            break;
        if (n > 0)
            on_packet(b, n, &e);
        if (!g_session)
            return;
    }
    t = now_ms();
    if (!g_connected && g_want[0] && !g_eos_target[0]
        && mpeos_lookup_result(g_eos_target, sizeof g_eos_target) == 1)
        g_join_last = 0;                /* found over the internet: ask now */
    if (!g_connected && g_want[0]) {
        if (t - g_find_start > (g_eos_target[0] ? FIND_EOS_MS : FIND_MS)) {
            g_want[0] = 0;
            g_status = MPNET_NOT_FOUND;
        } else if (!g_join_last || t - g_join_last > JOIN_MS) {
            g_join_last = t;
            send_join();
        }
    }
    if (!g_connected)
        return;
    for (i = 0; i < WINDOW; i++) {
        struct pend *p = &g_out[i];
        if (p->used && t - p->sent > RESEND_MS) {
            p->sent = t;
            send_peer(p->buf, p->len);
        }
    }
    if (t - g_keep_last > KEEP_MS) {
        unsigned char k[4];
        g_keep_last = t;
        hdr(k, K_KEEP);
        send_peer(k, 4);
    }
    if (t - g_heard > LINK_LOST_MS)
        link_lost();
}
