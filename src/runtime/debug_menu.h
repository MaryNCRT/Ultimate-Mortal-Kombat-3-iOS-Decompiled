/* debug_menu.h -- the in-game debug menu (F2); see debug_menu.c. */
#ifndef UMK3_DEBUG_MENU_H
#define UMK3_DEBUG_MENU_H

#define DBG_SCREENS 51                  /* FETaskFunctionList's screens */

enum { DBG_NONE, DBG_FIGHT, DBG_SCREEN, DBG_FIGHT_KEY, DBG_BOSS, DBG_FINISHER,
       DBG_SAVE_STATE, DBG_LOAD_STATE };

#define DBG_STATE_SLOTS 4               /* savestate.h's ST_SLOTS */

struct dbg_request {
    int what;                           /* DBG_* */
    int a, b, c;                        /* fight: p1, p2, stage; screen: n;
                                           fight key: 0..3 as F9..F12;
                                           boss: 24 Motaro, 25 Shao Kahn;
                                           finisher: 0..6, DoASpecial's
                                           0xd..0x13; state: the slot */
};

int  dbg_menu_is_open(void);
void dbg_menu_toggle(void);
/* After GameCodeMain: a pending open grabs the frame just drawn. */
void dbg_menu_after_frame(int vx, int vy, int vw, int vh, int task, int fe);
/* While open, instead of the game's tick: 1 and *rq filled when a pick
 * closes the menu. */
int  dbg_menu_tick(struct dbg_request *rq);
/* The pointer as fractions of the game view (0..1) and whether a button is
 * down, each frame the menu is open. */
void dbg_menu_mouse(float x, float y, int down);
/* the pad, each frame the menu is open (include platform/platform.h first) */
void dbg_menu_pad(const PLAT_PAD *p);
/* Each fighter's palette for the fights loaded next: 0 the game's rule,
 * 1 the first, 2 the alternate (LoadLevelCharacters, port only). */
extern int DbgPalette[2];
void dbg_menu_draw(void);

/* The box both in-game menus draw (debug_menu.c), shared with the settings
 * menu (settings_menu.c): over the frame ui_grab took, dimmed. `geo` gets
 * where it went, in the menu's space (320 high, ow wide), for the mouse:
 * the tabs from title0 to top, the rows from top, rowh each, between bx
 * and bx + bw. */
/* where the box went: the rows (bx, bw, top, rowh), the tabs' band
 * (title0..top), and with the side panel its column (sx, sw), its first
 * tab (stop) and the tabs' step (srow), all in the menu's space */
struct ui_geo { float bx, bw, title0, top, rowh, ow, sx, sw, stop, srow,
                 cw, gap; int split; };
/* the row under (x, y), 0 the first shown: rows 0..split-1 down the left
 * column (cw wide), the rest down the right, gap further; -1 none */
int ui_geo_row(const struct ui_geo *g, float x, float y, int rows);
/* where the right column starts: at a section's heading, so a section is
 * never cut in two; rows when it is all one column */
int ui_menu_split(const char *const *label, int rows);
void ui_grab(int vx, int vy, int vw, int vh);
/* a picture beside the box: uploaded once, drawn at (x, y), size points */
void ui_icon_set(const unsigned char *rgba, int w, int h);
void ui_icon_draw(float x, float y, float size);
/* PROFILE's gradient picker over the menu (debug_menu.c); include
 * platform/platform.h first */
struct plat_grad;
struct plat_grad_hit;
void ui_gradient_show(const struct plat_grad *g, unsigned key, struct plat_grad_hit *hit);
void ui_badge_at(int slot, const char *name, const unsigned char *icon,
                 int icon_px, unsigned key, float x, float y, int align,
                 float size);
float ui_space_width(void);
void ui_rect_outline(float x, float y, float w, float h, float t,
                     float r, float g, float b, float a);
/* the player's badge, top right; rebuilt when `key` changes */
void ui_badge(const char *name, const unsigned char *icon, int icon_px,
              unsigned key);
/* points the box is moved right by (a picture beside it), 0 centred */
extern float ui_menu_shift;
/* 1: the next box goes over the one already drawn (a list it opened):
 * that one dimmed instead of the frozen frame */
extern int ui_menu_layer;
/* the rows in this many columns side by side (filled down), 1 one list */
extern int ui_menu_cols;
/* The pad's highlight on the game's own menus (pad_nav.c): a glowing frame
 * round a box given in the touch's units, `sh` of them to the screen's
 * height. */
void ui_nav_frame(float x, float y, float w, float h, float sh, int p2);

/* A grid of pictures over the menu (plat_ui_grid), the menu dimmed; made
 * again when `key` or `sel` change. geo: the first cell's corner, the step
 * and the cell's size, in the menu's space. */
struct ui_grid_geo { float x0, y0, step, cell; int cols, n; };
void ui_grid_show(const char *title, const unsigned char *const *cells,
                  int n, int cols, int cpx, float cell, int sel,
                  const char *footer, unsigned key, struct ui_grid_geo *geo);
void ui_menu_show(const char *title, const char *const *tabs, int ntabs,
                  int page, const char *const *label,
                  const char *const *value, const int *enabled, int rows,
                  int sel, const char *footer, struct ui_geo *geo);

/* Whether the fight keys can act now (a round in play) and whether the
 * Arcade boss jumps apply; game_main.c answers both. */
int  dbg_round_live(void);
int  dbg_in_arcade(void);
/* Whether FINISH HIM/HER is up (IsInFinishing), so a finisher can be forced. */
int  dbg_finishing(void);
/* The picture behind the bars in fullscreen (port only). */
int  dbg_frame_load(const char *path);
void dbg_frame_draw(int ww, int wh);
int  dbg_keys_on(void);                /* the menu's DIRECT KEYS row */
int  dbg_hitboxes_on(void);            /* TOOLS: HITBOXES */
int  dbg_speed_percent(void);          /* TOOLS: FIGHT SPEED, 10..400 */
/* game_main.c answers: a state can be saved now / slot n holds one */
int  dbg_state_can_save(void);
int  dbg_state_has(int slot);
int  dbg_info_on(void);
void dbg_info_toggle(void);
void dbg_info_draw(const char *line);
/* a line of the corner's overlay: row 0 the top one */
void dbg_info_draw_at(const char *line, int row);
/* the replay player's panel along the bottom and strip along the top
 * (rollback_probe.c): how far the fight is, its clock, rp_info's state, the
 * button under the mouse, the replay's name and the REPLAYS list (rp_list);
 * and which button (RPB_*, rollback.h) is at a place of the view */
void dbg_replay_draw(float at, const char *time, int state, int hover,
                     const char *title, const char *const *labels, int list_n,
                     int n_recent, int current);
int  dbg_replay_hit(float fx, float fy, float *at, int list_n, int n_recent);
/* Between one replay and the next, over the front end: the next one's name. */
void dbg_replay_loading(const char *title);
/* HIDE: the line that says how to have the player back, `alpha` 0..1 */
void dbg_replay_hidden(float alpha);
/* SIZE: the player's scale in percent (umk3.ini replay_scale), and the
 * button's step to the next one (75, 100, 125, 150) */
extern int dbg_replay_pct;
void dbg_replay_size(void);
int  dbg_fps_on(void);                 /* TOOLS: FRAMES PER SECOND */
int  dbg_net_on(void);                 /* TOOLS: ONLINE NUMBERS */
/* FIGHT: INFINITE HEALTH (bit 0 player one, bit 1 two) and FREEZE CLOCK */
int  dbg_health_mask(void);
int  dbg_clock_frozen(void);

/* STAGE SELECT (port): after the fighters, in VS, training and online (the
 * host), a window to pick the stage. Task_GameInit asks stage_menu_gate
 * before anything loads: 1 while the window is up (it waits), 0 once the
 * stage is in *level (or left as the game chose; CANCEL goes back to the
 * select screen, online the game's own). game_main gives
 * it the frame while it is open, as the settings menu. */
int  stage_menu_gate(long *level);
int  stage_menu_is_open(void);
/* Online, the guest sees the host's STAGE SELECT: the host's stage as it
 * moves (the packet), each frame the guest waits (1 while the host's window
 * is up), the end of the wait, and the window drawn over the guest's frame. */
void stage_menu_remote(int level, int list);
int  stage_menu_watch_frame(void);
void stage_menu_watch_end(void);
void stage_menu_watch_draw(void);
void stage_menu_draw(void);
void stage_menu_mouse(float fx, float fy, int down);
void stage_menu_keys(int left, int right, int up, int down, int ok, int back,
                     int tab);

#endif
