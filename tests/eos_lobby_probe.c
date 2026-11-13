/* eos_lobby_probe.c -- the EOS lobby core (src/runtime/mp_eos.c, port) against
 * the real service, with two of these on one computer. Not a ctest: it needs
 * the EOS SDK, the product's eos.ini and the internet.
 *
 * Built with mp_eos.c alone (-DUMK3_EOS and the SDK's Include), run from a
 * folder holding the EOS DLL and eos.ini:
 *
 *   probe host [priv]        creates "PROBE <n>", waits for a ready member,
 *                            changes the rules, calls the member, answers its
 *                            ping over P2P, waits for it to leave
 *   probe join [KEY]         (UMK3_EOS_NEW_DEVICE=1 and another
 *                            UMK3_SAVE_DIR: another EOS user) searches,
 *                            joins, readies, sees the rules change, takes
 *                            the call, pings the owner, leaves
 *
 * Each prints PROBE OK or PROBE FAIL <step> and exits 0 / 1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void nap(void) { Sleep(20); }
static unsigned ms(void) { return (unsigned)GetTickCount(); }
#else
#include <time.h>
#include <unistd.h>
static void nap(void) { usleep(20000); }
static unsigned ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}
#endif
#include "mp_eos.h"

#define RULES1 "r2;b3;m1;t99;k0;s1;h1"
#define RULES2 "r2;b3;m1;t60;k0;s1;h1"

static unsigned g_end;

static int fail(const char *step)
{
    printf("PROBE FAIL %s\n", step);
    fflush(stdout);
    return 1;
}

/* ticks until cond() or the deadline */
#define WAIT(cond, step) do { \
    while (!(cond)) { \
        mpeos_tick(); nap(); \
        if ((int)(ms() - g_end) > 0) return fail(step); \
    } } while (0)

static int ready_member(char *who)
{
    struct mplobby_member m[MPLOBBY_MAX_MEMBERS];
    int i, n = mplobby_members(m, MPLOBBY_MAX_MEMBERS);
    for (i = 0; i < n; i++)
        if (!m[i].owner && m[i].ready && !strcmp(m[i].name, "GUEST")) {
            strcpy(who, m[i].who);
            return 1;
        }
    return 0;
}

static void show(void)
{
    struct mplobby_member m[MPLOBBY_MAX_MEMBERS];
    const struct mplobby_info *in = mplobby_info();
    int i, n = mplobby_members(m, MPLOBBY_MAX_MEMBERS);
    if (!in)
        return;
    printf("  lobby '%s' host '%s' rules %s priv %d key %s %d/%d\n", in->name, in->host,
           in->rules, in->priv, in->key, in->members, in->max_members);
    for (i = 0; i < n; i++)
        printf("  member %s code %s look %s ready %d%s\n", m[i].name, m[i].code, m[i].look,
               m[i].ready, m[i].owner ? " (owner)" : "");
}

static int got_packet(const char *want, char *from)
{
    char buf[64];
    int n = mpeos_recv(from, 40, buf, sizeof buf - 1);
    if (n <= 0)
        return 0;
    buf[n] = 0;
    return !strcmp(buf, want);
}

static int host(int priv)
{
    char who[40], from[40], name[32];
    unsigned seen = 0;

    snprintf(name, sizeof name, "PROBE %u", ms() % 10000);
    mplobby_set_me("AAAA-AAAA-AAAA", "HOST", "1,ff0000,00ff00,0,0,0", 1);
    if (!mplobby_create(name, RULES1, priv, 2))
        return fail("create call");
    WAIT(mplobby_state() == MPLOBBY_IN, "create");
    WAIT(mplobby_info()->rules[0] && mplobby_info()->host[0], "own attributes");
    printf("PROBE created %s key %s\n", name, mplobby_info()->key);
    show();
    fflush(stdout);
    g_end = ms() + 120000;
    WAIT(ready_member(who), "a ready member");
    printf("PROBE member ready\n");
    show();
    mplobby_set_rules(RULES2);
    WAIT(!strcmp(mplobby_info()->rules, RULES2), "rules change");
    mplobby_call(mplobby_info()->owner, who);   /* the owner hosts, the member connects */
    printf("PROBE called\n");
    fflush(stdout);
    WAIT(got_packet("ping", from), "ping");
    mpeos_send(from, "pong", 4);
    printf("PROBE ping from the member, pong sent\n");
    fflush(stdout);
    WAIT((mplobby_info() && mplobby_info()->members == 1), "the member leaving");
    printf("PROBE member left\n");
    (void)seen;
    mplobby_leave();
    WAIT(mplobby_state() == MPLOBBY_NONE, "destroy");
    printf("PROBE OK\n");
    return 0;
}

static int join(const char *key)
{
    struct mplobby_info r[MPLOBBY_MAX_RESULTS];
    char code[16], who[40], from[40];
    int i, n = 0, pick = -1;
    unsigned next = 0;

    mplobby_set_me("BBBB-BBBB-BBBB", "GUEST", "2,0000ff,ffffff,1,0,0", 0);
    /* search until a PROBE lobby shows (the host may still be making it) */
    while (pick < 0) {
        if ((int)(ms() - next) >= 0) {
            mplobby_search(NULL, key);
            next = ms() + 3000;
        }
        mpeos_tick();
        nap();
        n = mplobby_results(r, MPLOBBY_MAX_RESULTS);
        for (i = 0; i < n; i++)
            if (!strncmp(r[i].name, "PROBE", 5))
                pick = i;
        if ((int)(ms() - g_end) > 0)
            return fail("search");
    }
    printf("PROBE found '%s' host '%s' rules %s %d/%d (of %d)\n", r[pick].name, r[pick].host,
           r[pick].rules, r[pick].members, r[pick].max_members, n);
    if (!mplobby_join(r[pick].id))
        return fail("join call");
    WAIT(mplobby_state() == MPLOBBY_IN, "join");
    mplobby_set_ready(1);
    printf("PROBE joined\n");
    show();
    fflush(stdout);
    WAIT(!strcmp(mplobby_info()->rules, RULES2), "seeing the rules change");
    printf("PROBE rules changed to %s\n", mplobby_info()->rules);
    WAIT(mplobby_take_call(code, sizeof code, who, sizeof who), "the call");
    printf("PROBE called by %s\n", code);
    show();
    fflush(stdout);
    {
        unsigned again = 0;
        for (;;) {
            if ((int)(ms() - again) >= 0) {
                mpeos_send(who, "ping", 4);
                again = ms() + 500;
            }
            mpeos_tick();
            nap();
            if (got_packet("pong", from))
                break;
            if ((int)(ms() - g_end) > 0)
                return fail("pong");
        }
    }
    printf("PROBE pong from the owner\n");
    mplobby_leave();
    WAIT(mplobby_state() == MPLOBBY_NONE, "leave");
    printf("PROBE OK\n");
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* Windows has no line buffering */
    g_end = ms() + 120000;
    mpeos_start();
    WAIT(mpeos_state() == MPEOS_READY, "login");
    if (argc > 1 && !strcmp(argv[1], "host"))
        return host(argc > 2 && !strcmp(argv[2], "priv"));
    if (argc > 1 && !strcmp(argv[1], "join"))
        return join(argc > 2 ? argv[2] : NULL);
    printf("probe host [priv] | probe join [KEY]\n");
    return 2;
}
