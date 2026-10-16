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
struct ui_geo { float bx, bw, title0, top, rowh, ow; };
void ui_grab(int vx, int vy, int vw, int vh);
/* a picture beside the box: uploaded once, drawn at (x, y), size points */
void ui_icon_set(const unsigned char *rgba, int w, int h);
void ui_icon_draw(float x, float y, float size);
/* the player's badge, bottom right; rebuilt when `key` changes */
void ui_badge(const char *name, const unsigned char *icon, int icon_px,
              unsigned key);
/* points the box is moved right by (a picture beside it), 0 centred */
extern float ui_menu_shift;
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

#endif
