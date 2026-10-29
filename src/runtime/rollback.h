/* rollback.h -- rollback netcode for ONLINE fights. Port, not in the binary;
 * see rollback.c. */
#ifndef UMK3_ROLLBACK_H
#define UMK3_ROLLBACK_H

#include <stdint.h>

/* the packet types, after the binary's 1..14 (limeMPGameSpecific.c) */
#define RB_PKT_INPUT  20                /* unreliable, every frame */
#define RB_PKT_HELLO  21                /* reliable, once a fight */
#define RB_PKT_HASH   22                /* reliable, once a second */
#define PORT_PKT_PALETTE 23             /* reliable: a side's colour pick (FrontEnd.c) */

/* 1 from an online fight's Task_GameInit to its Task_GameDestroy: both
 * machines run the fight engine, from both players' inputs. */
int  rb_active(void);
/* 1 while a frame is run again after a rollback (or a stall): no sound. */
int  rb_quiet(void);
/* the rollback's numbers on one line, 0 when no rollback fight is on */
int  rb_stats(char *out, int n);
/* 1 while a frame is run again after a rollback: nothing drawn. */
int  rb_nodraw(void);

/* Task_GameInit, state 0, of an online fight: the two fighters and the
 * level, which also seed the two generators. */
int  rb_host_level(long *level);
void rb_online_begin(int c1, int c2, int level);
/* Task_GameDestroy */
void rb_online_end(void);
/* Task_MultiplayerSync: the other machine said hello, the fight may start. */
int  rb_ready(void);
/* Task_GameMain: runs `frame` -- the game's own frame -- once, or again from
 * a snapshot after a rollback, or holds it while the other side catches up. */
void rb_frame(void (*frame)(void));
/* Inside the frame: the two players' inputs for it (rb_game_* packing). */
void rb_inputs(uint16_t *p1, uint16_t *p2);
/* processPacket, types RB_PKT_* */
void rb_net(long type, const void *data, long len);
/* game_main.c, every frame: the last inputs go on being sent for a while
 * after a fight, so the other side can finish it. */
void rb_poll(void);
/* The replay player's frames: no sound, nothing drawn. */
void rb_silent(int quiet, int nodraw);

/* REPLAYS (rollback_probe.c): umk3.ini replays= records every fight but an
 * online one to replays/recent; --replay <file> (inside the exe's folder)
 * plays one, whose fighters and level rb_replay_peek reads first. */
void rb_replay_auto(int on);
void rb_replay_open(const char *rel);
int  rb_replay_peek(int32_t *c1, int32_t *c2, int32_t *level);
/* Task_GameDestroy */
void rb_replay_fight_over(void);
/* The player's buttons, left to right on its panel (debug_menu.c draws
 * them), and the bar above them. */
enum { RPB_START, RPB_BACK5, RPB_FRAME_BACK, RPB_PLAY, RPB_FRAME_FWD,
       RPB_FWD5, RPB_SLOWER, RPB_FASTER, RPB_SETTINGS, RPB_COUNT,
       RPB_BAR = 100 };
/* rp_info's state: the speed (0..4: x1/4 x1/2 x1 x2 x4) and these bits */
#define RPS_SPEED   0x07
#define RPS_PAUSED  0x08
#define RPS_SEEKING 0x10
#define RPS_END     0x20
#define RPS_DIFFERS 0x40

/* The player: 1 while --replay's fight is on; the frame in GameCodeMain's
 * place; a button pressed; the keys as held; the mouse on the panel (the
 * button under it, where along the bar, down), returning a button the
 * caller acts on itself (RPB_SETTINGS) or -1; the time and the state,
 * returning how far the fight is, 0..1. */
int   rp_playing(void);
void  rp_frame(void (*frame)(void));
void  rp_press(int id);
void  rp_keys(int play, int back, int fwd, int faster, int slower, int start);
int   rp_ui(int id, float at, int down);
float rp_info(char *time, int n, int *state);

#endif
