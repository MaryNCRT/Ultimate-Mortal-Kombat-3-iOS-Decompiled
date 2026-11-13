/* src/runtime/discord_presence.c: the JSON it sends and the join it reads.
 *
 *   cc tests/discord_presence_test.c src/runtime/discord_presence.c -Isrc/runtime
 *   ./a.out          the checks, no Discord needed
 *   ./a.out --live   also connects to the Discord running on this PC and
 *                    shows a test activity for 20 s (look at your profile),
 *                    printing a join secret if a friend presses Join
 * (Windows: add -lkernel32; nothing else is needed.) */
#include "discord_presence.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
static double now_s(void) { return GetTickCount64() / 1000.0; }
static void nap(void) { Sleep(50); }
#else
#include <unistd.h>
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static void nap(void) { usleep(50000); }
#endif

void presence_game_tick(double now) { (void)now; }   /* not under test */

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(int argc, char **argv)
{
    struct discord_activity a;
    char json[4096], s[160];
    int n;

    memset(&a, 0, sizeof a);
    snprintf(a.details, sizeof a.details, "Online match");
    snprintf(a.state, sizeof a.state, "Sub-Zero vs \"Kano\"\\");
    a.start = 1794528131;
    snprintf(a.large_text, sizeof a.large_text, "Ultimate Mortal Kombat 3 IOS Decomp");
    snprintf(a.party_id, sizeof a.party_id, "umk3-abc");
    a.party_size = 1; a.party_max = 2;
    snprintf(a.join, sizeof a.join, "umk3lobby:abc");
    n = discord_activity_json(&a, 1234, "a1", json, sizeof json);
    CHECK(n > 0);
    CHECK(strstr(json, "\"cmd\":\"SET_ACTIVITY\"") != NULL);
    CHECK(strstr(json, "\"pid\":1234") != NULL);
    CHECK(strstr(json, "\"state\":\"Sub-Zero vs \\\"Kano\\\"\\\\\"") != NULL);
    CHECK(strstr(json, "\"timestamps\":{\"start\":1794528131}") != NULL);
    CHECK(strstr(json, "\"large_image\":\"" DISCORD_LARGE_IMAGE "\"") != NULL);
    CHECK(strstr(json, "\"party\":{\"id\":\"umk3-abc\",\"size\":[1,2]}") != NULL);
    CHECK(strstr(json, "\"secrets\":{\"join\":\"umk3lobby:abc\"}") != NULL);
    n = discord_activity_json(NULL, 7, "a2", json, sizeof json);
    CHECK(n > 0 && strcmp(json, "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"a2\",\"args\":{\"pid\":7}}") == 0);
    CHECK(discord_activity_json(&a, 1, "x", json, 40) < 0);     /* too small: refused, not cut */

    CHECK(discord_find_join_secret("{\"cmd\":\"DISPATCH\",\"data\":{\"secret\":\"umk3lobby:xyz\"},\"evt\":\"ACTIVITY_JOIN\"}", s, sizeof s)
          && strcmp(s, "umk3lobby:xyz") == 0);
    CHECK(!discord_find_join_secret("{\"cmd\":\"DISPATCH\",\"data\":{\"user\":{}},\"evt\":\"ACTIVITY_JOIN_REQUEST\"}", s, sizeof s));
    CHECK(!discord_find_join_secret("{\"cmd\":\"SUBSCRIBE\",\"data\":{\"evt\":\"ACTIVITY_JOIN\"}}", s, sizeof s));

    printf("%s (%d failed)\n", fails ? "FAILED" : "ok", fails);

    if (argc > 1 && !strcmp(argv[1], "--live")) {
        double t0 = now_s(), shown = -1;
        a.start = (long long)time(NULL);
        snprintf(a.details, sizeof a.details, "Main menu");
        snprintf(a.state, sizeof a.state, "Live test of the port");
        discord_presence_set(&a);
        while (now_s() - t0 < 25.0) {
            discord_presence_tick(now_s() - t0 + 100.0);
            if (discord_presence_connected() && shown < 0) {
                shown = now_s();
                printf("live: connected to Discord\n");
            }
            if (discord_presence_take_join(s, sizeof s))
                printf("live: join secret %s\n", s);
            nap();
        }
        printf("live: %s\n", shown >= 0 ? "shown for ~20 s" : "Discord not reached");
        discord_presence_shutdown();
    }
    return fails != 0;
}
