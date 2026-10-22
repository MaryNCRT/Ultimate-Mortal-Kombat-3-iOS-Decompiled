/* rollback.h -- rollback netcode for ONLINE fights. Port, not in the binary;
 * see rollback.c. */
#ifndef UMK3_ROLLBACK_H
#define UMK3_ROLLBACK_H

#include <stdint.h>

/* the packet types, after the binary's 1..14 (limeMPGameSpecific.c) */
#define RB_PKT_INPUT  20                /* unreliable, every frame */
#define RB_PKT_HELLO  21                /* reliable, once a fight */
#define RB_PKT_HASH   22                /* reliable, once a second */

/* 1 from an online fight's Task_GameInit to its Task_GameDestroy: both
 * machines run the fight engine, from both players' inputs. */
int  rb_active(void);
/* 1 while a frame is run again after a rollback (or a stall): no sound. */
int  rb_quiet(void);
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

#endif
