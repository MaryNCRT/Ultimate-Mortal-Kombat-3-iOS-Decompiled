/* lobby_queue.h -- a lobby's line of players and how it turns. Port, not in
 * the binary (docs/research/ONLINE-PHASE-PLAN.md, "The room: 1 VS 1,
 * ROTATION, the line").
 *
 * The lobby keeps one line: the first two fight, the next ones wait, and a
 * member may stand out of it. The lobby's owner (the leader) writes it as
 * the lobby attribute "q"; the room's options as "room". Pure logic here,
 * no EOS: mp_eos.c carries the texts, FrontEnd.c drives them.
 *
 *     mode       LQ_1V1 the same two fight again; LQ_ROTATION the line
 *                turns when a fight (or a series) is over
 *     after      ROTATION: LQ_WINNER_STAYS (the loser to the back) or
 *                LQ_BOTH_ROTATE (both to the back, the next two fight)
 *     streak     ROTATION, winner stays: wins in a row before the winner
 *                goes to the back too; 0 no limit
 *     line       who moves the line: LQ_LINE_LEADER the leader alone;
 *                LQ_LINE_INOUT each player also steps in and out of it;
 *                LQ_LINE_FREE each also moves one place up or down
 *     autostart  0 the leader calls each fight; 1 the next fight is called
 *                by itself once the first two are ready
 *     stage      who picks the stage: LQ_STAGE_FIGHT the fight's own way
 *                (the host's STAGE SELECT, or drawn with HOST ADVANTAGE
 *                OFF); LQ_STAGE_LEADER the leader (`pick`); LQ_STAGE_VOTE
 *                the most votes; LQ_STAGE_FIGHTERS the two who fight (both
 *                the same, or one of their two drawn); LQ_STAGE_LOSER the
 *                last fight's loser; LQ_STAGE_RANDOM drawn
 *     pick       the leader's stage, mpLevelList's place (0..13)
 *     ask        who comes in: 0 freely (into the line's back), 1 on asking
 *                (waiting for the leader's ACCEPT; REJECT kicks them)
 *     cede       1 one waiting may ask to go up ("pass") and one of the two
 *                about to fight may give them their place ("cede:<id>");
 *                0 not allowed
 *     readyt     seconds the two about to fight have to be READY (0 no
 *                limit, else up to 30); one who is not goes to the line's
 *                back and the next comes up
 */
#ifndef UMK3_LOBBY_QUEUE_H
#define UMK3_LOBBY_QUEUE_H

#define LQ_MAX   8                      /* MPLOBBY_MAX_MEMBERS */
#define LQ_ID    40                     /* an EOS user as text */
#define LQ_TEXT  ((LQ_ID + 1) * LQ_MAX * 2 + 64)
#define LQ_OPTS_TEXT 48

enum { LQ_1V1, LQ_ROTATION };
enum { LQ_WINNER_STAYS, LQ_BOTH_ROTATE };
enum { LQ_LINE_LEADER, LQ_LINE_INOUT, LQ_LINE_FREE };
enum { LQ_STAGE_FIGHT, LQ_STAGE_LEADER, LQ_STAGE_VOTE, LQ_STAGE_FIGHTERS, LQ_STAGE_LOSER,
       LQ_STAGE_RANDOM, LQ_STAGE_N };
#define LQ_STAGES 14                    /* mpLevelList */

typedef struct lq_opts {
    int mode, after, streak, line, autostart, stage, pick, ask, cede, readyt;
} lq_opts;

void lq_opts_default(lq_opts *o);
/* "o1;a0;s3;l1;n0;e0;p0"; lq_opts_decode: 1 when valid (o untouched otherwise) */
int  lq_opts_encode(const lq_opts *o, char *out, int n);
int  lq_opts_decode(const char *s, lq_opts *o);

/* The line: `id` in order (the first two fight), `out` those standing out
 * of it, `streak` the first one's wins in a row, `done` the last fight call
 * whose end was counted (mp_eos.c's "go" number), so a new leader does not
 * count it twice. */
typedef struct lq_line {
    char id[LQ_MAX][LQ_ID];
    int  n;
    char out[LQ_MAX][LQ_ID];
    int  nout;
    int  streak;
    char done[24];
    char loser[LQ_ID];                  /* the last fight's loser, "" none */
    char pend[LQ_MAX][LQ_ID];           /* asking to come in (ask 1) */
    int  npend;
} lq_line;

void lq_clear(lq_line *l);
/* "<streak>|<done>|id,id,...|id,...|<loser>|id,..."; decode of "" is an
 * empty line */
int  lq_encode(const lq_line *l, char *out, int n);
int  lq_decode(const char *s, lq_line *l);
int  lq_find(const lq_line *l, const char *id);    /* place in the line, -1 none */
int  lq_is_out(const lq_line *l, const char *id);

/* The members now in the lobby: those gone leave the line, those new join
 * its back -- or, `ask` set, wait to be let in (`pend`), all but `leader`.
 * 1 when it changed. */
int  lq_sync(lq_line *l, const char (*members)[LQ_ID], int n, int ask, const char *leader);
int  lq_is_pending(const lq_line *l, const char *id);
/* The leader lets one who asked in: to the line's back. 1 when it did. */
int  lq_accept(lq_line *l, const char *id);

/* One place up (dir -1) or down (+1); 1 when it moved. */
int  lq_move(lq_line *l, const char *id, int dir);
/* To place `at` in the line (0 the first), in it or out of it before: the
 * leader putting a player up to fight. 1 when it moved. */
int  lq_put(lq_line *l, const char *id, int at);
/* Two players' places swapped (one ceding a place to the other); the one
 * coming in may have been out of the line. 1 when it did. */
int  lq_swap(lq_line *l, const char *a, const char *b);
/* Into the line's back (in 1) or out of it (in 0); 1 when it changed. */
int  lq_set_in(lq_line *l, const char *id, int in);

/* A fight over between `winner` and `loser` (series over, when there is
 * one): the line turns by the options. 1 when it changed. */
int  lq_result(lq_line *l, const lq_opts *o, const char *winner, const char *loser);
/* A fight drawn (on time, no round left): ROTATION sends both to the back,
 * nobody having earned to stay. 1 when it changed. */
int  lq_draw(lq_line *l, const lq_opts *o, const char *a, const char *b);

/* A request a member makes of the leader (member attribute "want"):
 * "in", "out", "up", "down", "pass", "cede:<id>"; 1 when the options let
 * that player do it. */
int  lq_allowed(const lq_opts *o, const char *act);

/* The stage for the next fight, mpLevelList's place, -1 the fight's own
 * way. `votes` each member's (-1 none), `who` their ids; `a`, `b` the two
 * who fight; `roll` a random number. */
int  lq_stage(const lq_opts *o, const lq_line *l, const char (*who)[LQ_ID], const int *votes,
              int n, const char *a, const char *b, unsigned roll);

#endif
