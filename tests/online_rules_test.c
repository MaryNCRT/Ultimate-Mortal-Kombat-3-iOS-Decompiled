#include <stdio.h>
#include <string.h>
#include "online_rules.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* online_rules_test.c -- the online rules block (src/runtime/online_rules.c,
 * port). The clock adjustment below is other.c t_clock3's port lines, kept in
 * step with them by hand. */
static long clock_adjust(int rule, long left)
{
    if (rule > 0 && left > rule) {
        left -= 99 - rule;
        if (left < 0) left = 0;
    }
    return left;
}

int main(void)
{
    online_rules a, b;
    char t[ONLINE_RULES_TEXT], d[160];

    rules_default(&a);
    CHECK(rules_encode(&a, t, sizeof t) > 0);
    CHECK(strcmp(t, "r2;b3;m1;t99;k0;s1;h1") == 0);
    CHECK(rules_decode(t, &b) && rules_equal(&a, &b));

    CHECK(rules_decode("h0;s0;k19;t0;m5;b5;r1", &b));  /* any order; version 1, one kode */
    CHECK(b.version == 2 && b.best_of == 5 && b.series == 5 && b.time == 0 && b.kodes == (1u << 19)
          && b.kombo_slide == 0 && b.host_adv == 0);
    CHECK(!rules_equal(&a, &b));
    CHECK(rules_decode("r1;b3;m1;t99;k-1;s1;h1", &b) && b.kodes == 0 && rules_equal(&a, &b));
    CHECK(rules_decode("r2;b3;m1;t99;k24579;s1;h1", &b));   /* kodes 0, 1, 13, 14 */
    CHECK(b.kodes == 24579u);
    rules_encode(&b, t, sizeof t);
    CHECK(strcmp(t, "r2;b3;m1;t99;k24579;s1;h1") == 0);
    CHECK(!rules_decode("r2;b3;m1;t99;k-1;s1;h1", &b));     /* no -1 in a set */
    CHECK(!rules_decode("r2;b3;m1;t99;k1048576;s1;h1", &b)); /* kode 20 */

    CHECK(!rules_decode("r1;b3;t99;k-1;s1;h1", &b));     /* the series missing */
    CHECK(!rules_decode("r1;b3;m2;t99;k-1;s1;h1", &b));  /* a series of 2 */
    CHECK(!rules_decode("r1;b4;m1;t99;k-1;s1;h1", &b));  /* best of 4 */
    CHECK(!rules_decode("r1;b3;m1;t45;k-1;s1;h1", &b));  /* time 45 */
    CHECK(!rules_decode("r1;b3;m1;t99;k20;s1;h1", &b));  /* kode 20 */
    CHECK(!rules_decode("r3;b3;m1;t99;k0;s1;h1", &b));   /* another version */
    CHECK(!rules_decode("r1;b3;m1;t99x;k-1;s1;h1", &b)); /* junk */
    CHECK(!rules_decode("", &b));
    CHECK(!rules_decode(NULL, &b));

    rules_describe(&a, d, sizeof d);
    CHECK(strcmp(d, "BEST OF 3 ROUNDS, ONE FIGHT, TIME 99, NO KODE, KOMBO SLIDE ON, HOST ADVANTAGE ON") == 0);

    CHECK(rules_wins_needed(2) == 2);                    /* nothing active */
    rules_decode("r1;b5;m1;t30;k-1;s0;h1", &b);
    rules_set_active(&b);
    CHECK(rules_kode() == -1 && !rules_kode_on(0));
    b.kodes = (1u << 17) | (1u << 1); rules_set_active(&b);
    CHECK(rules_kode() == 1 && rules_kode_on(17) && rules_kode_on(1) && !rules_kode_on(2));
    b.kodes = 0; rules_set_active(&b);
    CHECK(rules_wins_needed(2) == 3);
    CHECK(rules_kombo_slide(1) == 0);
    rules_fight_begin();
    CHECK(PortRoundTime == 30);
    b.time = 0; rules_set_active(&b); rules_fight_begin();
    CHECK(PortRoundTime == -1);
    b.time = 99; rules_set_active(&b); rules_fight_begin();
    CHECK(PortRoundTime == 0);
    b.best_of = 1; rules_set_active(&b);
    CHECK(rules_wins_needed(2) == 1);
    /* a series of three fights: first to two, then a new one */
    {
        int a1, a2;
        CHECK(rules_series_needed() == 0);
        b.series = 3; rules_set_active(&b);
        CHECK(rules_series_needed() == 2);
        CHECK(rules_series_score(&a1, &a2) == -1 && a1 == 0 && a2 == 0);
        rules_series_won(0);
        rules_series_won(1);
        CHECK(rules_series_score(&a1, &a2) == -1 && a1 == 1 && a2 == 1);
        rules_series_won(1);
        CHECK(rules_series_score(&a1, &a2) == 1 && a2 == 2);
        rules_series_won(0);                             /* the next series */
        CHECK(rules_series_score(&a1, &a2) == -1 && a1 == 1 && a2 == 0);
        b.series = 5; rules_set_active(&b);
        CHECK(rules_series_needed() == 3);
    }
    rules_clear_active();
    CHECK(rules_active() == NULL && PortRoundTime == 0);
    CHECK(rules_series_score(NULL, NULL) == -1);

    /* the clock: a fresh 99 becomes the rule, a running clock is left alone */
    CHECK(clock_adjust(30, 99) == 30);
    CHECK(clock_adjust(30, 98) == 29);
    CHECK(clock_adjust(30, 29) == 29);
    CHECK(clock_adjust(30, 30) == 30);
    CHECK(clock_adjust(60, 99) == 60);
    CHECK(clock_adjust(60, 45) == 45);
    CHECK(clock_adjust(0, 99) == 99);

    /* the local versus's block and the online one, by the fight's mode */
    {
        online_rules on, loc;
        rules_decode("r1;b1;m1;t60;k-1;s1;h1", &on);
        rules_decode("r1;b5;m3;t30;k2;s0;h1", &loc);
        rules_set_active(&on);
        rules_set_local(&loc);
        rules_use_mode(1);
        CHECK(rules_wins_needed(2) == 1 && rules_kode() == -1);
        rules_use_mode(6);
        CHECK(rules_wins_needed(2) == 3 && rules_kode() == 2 && rules_kode_on(2) && rules_series_needed() == 2);
        rules_use_mode(0);                               /* arcade: none */
        CHECK(rules_active() == NULL && rules_wins_needed(2) == 2 && !rules_kode_on(2));
        rules_set_local(NULL);
        rules_use_mode(6);
        CHECK(rules_active() == NULL && rules_local() == NULL);
    }

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
