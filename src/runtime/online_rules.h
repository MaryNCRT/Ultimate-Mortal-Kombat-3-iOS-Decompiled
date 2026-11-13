/* online_rules.h -- the rules of an online fight, one block both machines
 * agree on before frame 0. Port, not in the binary: the iPhone's versus had
 * no rules beyond the VS kode its versus screen sends.
 *
 * The block travels as a short text ("r2;b3;m1;t99;k0;s1;h1") in an EOS
 * lobby's attributes and in rollback's HELLO; a fight whose two blocks
 * differ does not start (docs/research/ONLINE-PHASE-PLAN.md).
 *
 *     best_of      ROUNDS a fight: 1, 3 or 5, the binary's WinsNeeded 1, 2,
 *                  3 (GameCode.c Task_GameInit); ROUND 5 and the third
 *                  coin are the port's
 *     series       FIGHTS: 1 a single fight, 3 or 5 a series of separate
 *                  fights, first to 2 or 3 (Mary, 2026-11-12); counted by
 *                  rules_series_* below, between fights
 *     time         99 the binary's; 30, 60 the port's; 0 infinite (the
 *                  clock frozen, as training freezes it: other.c t_clock4)
 *     kodes        the VS kodes in force, a bit each (bit n kode n, 0..19,
 *                  the binary's table at 0x00176c04: blocking off, dark
 *                  kombat, ...); 0 none. The binary plays one kode at a
 *                  time; several at once are the port's (Mary, 2026-11-12),
 *                  rules_kode_on at each place the binary reads theKode.
 *                  Version 1's text had one kode ("k-1", "k2"), still read
 *     kombo_slide  1 the original's push apart in a kombo, 0 the port's
 *                  KOMBO SLIDE off (issue #114)
 *     host_adv     1 the host is player 1 and picks the stage, as today;
 *                  0 the side and the stage are drawn (Mary, 2026-11-12)
 */
#ifndef UMK3_ONLINE_RULES_H
#define UMK3_ONLINE_RULES_H

#define ONLINE_RULES_VERSION 2
#define ONLINE_RULES_TEXT    48         /* enough for any encoded block */

typedef struct online_rules {
    int version;
    int best_of;
    int series;
    int time;
    unsigned kodes;
    int kombo_slide;
    int host_adv;
} online_rules;

void rules_default(online_rules *r);
/* Text for a lobby attribute or a hello; returns its length. */
int  rules_encode(const online_rules *r, char *out, int n);
/* 1 when the text is a valid block of this version, else 0 (r untouched). */
int  rules_decode(const char *s, online_rules *r);
int  rules_equal(const online_rules *a, const online_rules *b);
/* A line for the log: "BEST OF 3, TIME 99, NO KODE, KOMBO SLIDE ON, HOST ADVANTAGE ON". */
void rules_describe(const online_rules *r, char *out, int n);

/* The block the next fight plays by, or none (the game as it was). Set by
 * the lobby before the fight; UMK3_RULES=<text> sets it for a test. */
void rules_set_active(const online_rules *r);
void rules_clear_active(void);
/* The block in force: the online one in GameMode 1, the local versus's in 6
 * (rules_use_mode, from Task_GameInit), none elsewhere. */
const online_rules *rules_active(void);
void rules_use_mode(int game_mode);
/* The online block (the lobby's), whatever the mode: what an online
 * fight's hello carries (rollback.c), NULL none. */
const online_rules *rules_online(void);
/* The local versus's block (VS RULES, SELECT on its select screen;
 * umk3.ini vs_rules=), NULL for the game's own rules. */
void rules_set_local(const online_rules *r);
const online_rules *rules_local(void);

/* Applied by the engine (port lines, marked where they are):
 *   rules_wins_needed   Task_GameInit, after the binary sets WinsNeeded
 *   PortRoundTime       other.c t_clock3 / t_clock4
 *   rules_kombo_slide   game_main.c, PortNoKomboSlide */
int  rules_wins_needed(int binary_value);
int  rules_kombo_slide(int otherwise);
int  rules_kode(void);                  /* GameCode.c, before preprocessPostloadKode: the
                                         * lowest kode in force, -1 none */
int  rules_kode_on(int kode);           /* 1 when that kode is one of the rules' */
extern int PortRoundTime;               /* 0 the binary's 99, 30, 60, -1 infinite */
void rules_fight_begin(void);           /* the fight loads: PortRoundTime from the block */

/* The series of fights (rules->series 3 or 5): a fight won by side 0 or 1
 * (player one, player two) is counted when it ends; rules_series_score
 * gives both counts and returns the side that has the series, -1 none yet.
 * A series that is over starts again at 0-0 with the next fight. */
void rules_series_won(int side);
int  rules_series_score(int *p1, int *p2);
int  rules_series_needed(void);         /* fights to win it, 0 no series */
void rules_series_reset(void);
/* Online, HOST ADVANTAGE OFF: 1 when this fight's draw put the host on the
 * right (rollback.c's gate). The series is counted by player -- host and
 * guest -- not by side, so a series whose fights draw the sides anew
 * still adds each win to whoever took it; the calls above stay in sides. */
void rules_series_swap(int swapped);
/* A lobby room's stage for the next online fight (lobby_queue.h's stage
 * option): a level number, -1 none -- the host's STAGE SELECT, or drawn.
 * Set by FrontEnd.c when this game is called to fight; rollback.c's gate
 * takes it, debug_menu.c's STAGE SELECT stays shut. */
void rules_set_room_level(int level);
int  rules_room_level(void);

#endif
