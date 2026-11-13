#include <stdio.h>
#include <string.h>
#include "lobby_queue.h"

/* lobby_queue_test.c -- a lobby's line (src/runtime/lobby_queue.c, port). */
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const char *order(const lq_line *l)
{
    static char s[128];
    int i;
    s[0] = 0;
    for (i = 0; i < l->n; i++)
        strcat(s, l->id[i]);
    return s;
}

int main(void)
{
    lq_opts o, p;
    lq_line l, m;
    char t[LQ_TEXT];
    char mem[LQ_MAX][LQ_ID] = { "A", "B", "C", "D" };

    lq_opts_default(&o);
    lq_opts_encode(&o, t, sizeof t);
    CHECK(!strcmp(t, "o0;a0;s0;l1;n0;e0;p0;j0;c1;t0"));
    CHECK(lq_opts_decode("o1;t30", &p) && p.readyt == 30 && !lq_opts_decode("o1;t31", &p));
    CHECK(!lq_opts_decode("o0;e6", &p) && !lq_opts_decode("o0;p14", &p));
    CHECK(lq_opts_decode("n1;l2;s3;a1;o1", &p) && p.mode == 1 && p.after == 1 && p.streak == 3
          && p.line == 2 && p.autostart == 1);
    CHECK(!lq_opts_decode("", &p) && !lq_opts_decode("o7", &p) && !lq_opts_decode("a1", &p));
    CHECK(lq_opts_decode("o1;x5", &p));    /* a later version's key skipped */

    /* new members join the back, in the lobby's order */
    lq_clear(&l);
    CHECK(lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL));
    CHECK(!strcmp(order(&l), "ABCD"));
    CHECK(!lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL));

    /* text and back */
    strcpy(l.done, "77.3");
    l.streak = 2;
    lq_set_in(&l, "C", 0);
    lq_encode(&l, t, sizeof t);
    CHECK(!strcmp(t, "2|77.3|A,B,D|C"));
    CHECK(lq_decode(t, &m) && !strcmp(order(&m), "ABD") && m.nout == 1 && !strcmp(m.out[0], "C")
          && m.streak == 2 && !strcmp(m.done, "77.3"));
    CHECK(lq_decode("", &m) && m.n == 0);
    CHECK(!lq_decode("x|y", &m));

    /* out stays out through a sync; back in at the back */
    CHECK(!lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL));
    CHECK(lq_set_in(&l, "C", 1) && !strcmp(order(&l), "ABDC") && l.nout == 0);
    CHECK(!lq_set_in(&l, "C", 1));

    /* up and down */
    CHECK(lq_move(&l, "C", -1) && !strcmp(order(&l), "ABCD"));
    CHECK(!lq_move(&l, "A", -1) && !lq_move(&l, "D", 1));
    CHECK(lq_move(&l, "B", -1) && !strcmp(order(&l), "BACD") && l.streak == 0);
    lq_move(&l, "B", 1);

    /* put up to fight: D to the second place; C, out, to the first */
    CHECK(lq_put(&l, "D", 1) && !strcmp(order(&l), "ADBC"));
    lq_set_in(&l, "C", 0);
    CHECK(lq_put(&l, "C", 0) && !strcmp(order(&l), "CADB") && l.nout == 0);
    CHECK(!lq_put(&l, "C", 0));
    lq_clear(&l);
    lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL);

    /* a place given: B gives theirs to D; A gives theirs to C, who was out */
    CHECK(lq_swap(&l, "B", "D") && !strcmp(order(&l), "ADCB"));
    lq_set_in(&l, "C", 0);
    CHECK(lq_swap(&l, "A", "C") && !strcmp(order(&l), "CDB") && l.nout == 1 && !strcmp(l.out[0], "A"));
    lq_clear(&l);
    lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL);

    /* 1 VS 1: nothing turns */
    lq_opts_default(&o);
    CHECK(!lq_result(&l, &o, "B", "A") && !strcmp(order(&l), "ABCD"));

    /* winner stays */
    o.mode = LQ_ROTATION;
    CHECK(lq_result(&l, &o, "B", "A") && !strcmp(order(&l), "BCDA") && l.streak == 1);
    CHECK(lq_result(&l, &o, "B", "C") && !strcmp(order(&l), "BDAC") && l.streak == 2);
    CHECK(lq_result(&l, &o, "D", "B") && !strcmp(order(&l), "DACB") && l.streak == 1);

    /* a streak limit of 2: the second win in a row sends the winner back too */
    o.streak = 2;
    CHECK(lq_result(&l, &o, "D", "A") && !strcmp(order(&l), "CBAD") && l.streak == 0);

    /* both rotate */
    o.streak = 0;
    o.after = LQ_BOTH_ROTATE;
    CHECK(lq_result(&l, &o, "B", "C") && !strcmp(order(&l), "ADBC"));

    /* a draw: both to the back in ROTATION, nothing in 1 VS 1 */
    o.after = LQ_WINNER_STAYS;
    CHECK(lq_draw(&l, &o, "A", "D") && !strcmp(order(&l), "BCAD"));
    o.mode = LQ_1V1;
    CHECK(!lq_draw(&l, &o, "B", "C") && !strcmp(order(&l), "BCAD"));
    o.mode = LQ_ROTATION;

    /* two players only: the winner at the front */
    {
        char two[LQ_MAX][LQ_ID] = { "A", "B" };
        lq_clear(&l);
        lq_sync(&l, (const char (*)[LQ_ID])two, 2, 0, NULL);
        o.after = LQ_WINNER_STAYS;
        o.streak = 2;
        CHECK(lq_result(&l, &o, "B", "A") && !strcmp(order(&l), "BA"));
        CHECK(lq_result(&l, &o, "B", "A") && !strcmp(order(&l), "BA"));
    }

    /* a member gone: out of the line; a new front, the streak ends */
    {
        char three[LQ_MAX][LQ_ID] = { "B", "C" };
        lq_clear(&l);
        lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL);
        l.streak = 3;
        CHECK(lq_sync(&l, (const char (*)[LQ_ID])three, 2, 0, NULL) && !strcmp(order(&l), "BC") && l.streak == 0);
    }

    /* the loser kept, through the text */
    lq_clear(&l);
    lq_sync(&l, (const char (*)[LQ_ID])mem, 4, 0, NULL);
    o.mode = LQ_ROTATION;
    o.streak = 0;
    o.after = LQ_WINNER_STAYS;
    lq_result(&l, &o, "A", "B");
    lq_encode(&l, t, sizeof t);
    CHECK(lq_decode(t, &m) && !strcmp(m.loser, "B") && !strcmp(order(&m), "ACDB"));

    /* the stage */
    {
        int votes[4] = { 3, 5, 5, -1 };
        o.stage = LQ_STAGE_FIGHT;
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 7) == -1);
        o.stage = LQ_STAGE_LEADER;
        o.pick = 9;
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 7) == 9);
        o.stage = LQ_STAGE_VOTE;
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 7) == 5);
        o.stage = LQ_STAGE_FIGHTERS;            /* A 3, C 5: one of the two */
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 0) == 3);
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 1) == 5);
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "D", 1) == 3);
        o.stage = LQ_STAGE_LOSER;               /* B lost: B's vote */
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 1) == 5);
        o.stage = LQ_STAGE_RANDOM;
        CHECK(lq_stage(&o, &l, (const char (*)[LQ_ID])mem, votes, 4, "A", "C", 20) == 6);
    }

    /* asking to come in: the leader straight in, the others wait */
    {
        char two[LQ_MAX][LQ_ID] = { "A", "B", "C" };
        lq_clear(&l);
        CHECK(lq_sync(&l, (const char (*)[LQ_ID])two, 3, 1, "A") && !strcmp(order(&l), "A") && l.npend == 2);
        CHECK(lq_is_pending(&l, "B") && lq_accept(&l, "C") && !strcmp(order(&l), "AC") && l.npend == 1);
        lq_encode(&l, t, sizeof t);
        CHECK(lq_decode(t, &m) && m.npend == 1 && !strcmp(m.pend[0], "B"));
        CHECK(!lq_sync(&l, (const char (*)[LQ_ID])two, 3, 1, "A"));
        CHECK(lq_sync(&l, (const char (*)[LQ_ID])two, 3, 0, "A") && !strcmp(order(&l), "ACB") && l.npend == 0);
    }

    /* what a player may ask */
    o.line = LQ_LINE_LEADER;
    CHECK(!lq_allowed(&o, "in") && !lq_allowed(&o, "up"));
    o.line = LQ_LINE_INOUT;
    CHECK(lq_allowed(&o, "out") && !lq_allowed(&o, "down"));
    o.line = LQ_LINE_FREE;
    CHECK(lq_allowed(&o, "up") && lq_allowed(&o, "in") && !lq_allowed(&o, "kick"));
    o.cede = 1;
    CHECK(lq_allowed(&o, "pass") && lq_allowed(&o, "cede:X"));
    o.cede = 0;
    CHECK(!lq_allowed(&o, "pass") && !lq_allowed(&o, "cede:X"));

    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("lobby_queue: all passed\n");
    return 0;
}
