/*
 * VarEdit.c — src/gamecode/VarEdit.cpp
 *
 * The in-game variable editor: a table of tuning globals with a label, a
 * pointer and an editing range each. Hand-written from the disassembly of the
 * armv7 slice.
 */

#include <stdint.h>

/* `_EditVars` -- 0x00297970, **32 bytes an entry**. Five words are written:
 *
 *      +0x00   (untouched by this function)
 *      +0x04   const char *label
 *      +0x08   void *target        the global being edited
 *      +0x0c   float min / lower bound
 *      +0x10   float step
 *      +0x14   long  type          1 for the editable rows, 0 for headings
 *
 * A heading row -- `"** PLAYER VARS **"` and friends -- has only the label and
 * a type of 0, which is what separates the four groups. */
#define EDITVAR_STRIDE  0x20

typedef struct EDITVAR {
    long        pad;                    /* 0x00 */
    const char *P32 label;                  /* 0x04 */
    void       *P32 target;                 /* 0x08 */
    float       lower;                  /* 0x0c */
    float       step;                   /* 0x10 */
    long        type;                   /* 0x14, 0 = heading */
} EDITVAR;

extern EDITVAR EditVars[];              /* 0x00297970 */

extern float PlayerBright1, SwapLayers, BlendMode;
extern float JSIZE, JINNERDIAL, JOUTERDIAL;
extern float camzoomedin, camzoomedout, distzoomedin, distzoomedout;
extern float camheight, camlookatheight;


/* ------------------------------------------------------------------ InitVarEdit
 *
 * armv7 0x0005d118, 628 bytes.  **Complete.**
 *
 * Builds the variable-editor table: four headings and twelve editable rows,
 * written field by field with no loop.
 *
 *      ** PLAYER VARS **
 *          PlayerBright1, SwapLayers, BlendMode
 *      ** JOYSTICK VARS **
 *          JSIZE, JINNERDIAL, JOUTERDIAL
 *      ** CAMERA ZOOM VARS **
 *          camzoomedin, camzoomedout, distzoomedin, distzoomedout
 *      ** CAMERA VIEW VARS **
 *          camheight, camlookatheight
 *
 * ### This names the camera tuning the port had been fitting by eye
 *
 * The six camera globals are not just names -- their shipped values are in
 * `__DATA` and they are what the retail game actually uses:
 *
 *      camzoomedin     3.5999999   distzoomedin    0.69999999
 *      camzoomedout    5.5500002   distzoomedout   2.1500001
 *      camheight       1.1599999   camlookatheight 0.079999998
 *
 * **The first two match the values `runtime/demo.c` arrived at independently**
 * by matching a video capture -- 3.60 and 5.55. That is a genuine
 * confirmation: two different methods, the same numbers, and the binary's own
 * names for them.
 *
 * The other four the demo does not have at all. `distzoomedin` and
 * `distzoomedout` are the fighter separations the zoom interpolates between,
 * and `camheight` / `camlookatheight` are the eye and target heights the demo
 * currently approximates with `g_cam_eye` and a zero pitch.
 *
 * The joystick trio is equally useful: `JSIZE` 64, `JINNERDIAL` 22.85,
 * `JOUTERDIAL` 112 -- and note that `CheckLeftDial` **overwrites** JOUTERDIAL
 * on every call with `FE_W(80)`, so 112 is only the value it starts at.
 *
 * The editing ranges are 1.0 for the three player flags, 32 and 16 for two of
 * the joystick rows, and 0.5, 2.0, 8.0, 128.0 elsewhere; they bound the editor,
 * not the variables.
 */
void InitVarEdit(void)
{
    EditVars[0].label = "** PLAYER VARS **";
    EditVars[0].type  = 0;

    EditVars[1].label  = "PlayerBright1:";
    EditVars[1].target = &PlayerBright1;
    EditVars[1].lower  = 0.0f;
    EditVars[1].step   = 1.0f;
    EditVars[1].type   = 1;

    EditVars[2].label  = "SwapLayers:";
    EditVars[2].target = &SwapLayers;
    EditVars[2].lower  = 0.0f;
    EditVars[2].step   = 1.0f;
    EditVars[2].type   = 1;

    EditVars[3].label  = "BlendMode:";
    EditVars[3].target = &BlendMode;
    EditVars[3].lower  = 0.0f;
    EditVars[3].step   = 1.0f;
    EditVars[3].type   = 1;

    EditVars[4].label = "** JOYSTICK VARS **";
    EditVars[4].type  = 0;

    EditVars[5].label  = "JSIZE:";
    EditVars[5].target = &JSIZE;
    EditVars[5].lower  = 32.0f;
    EditVars[5].type   = 1;

    EditVars[6].label  = "JINNERDIAL:";
    EditVars[6].target = &JINNERDIAL;
    EditVars[6].lower  = 16.0f;
    EditVars[6].type   = 1;

    EditVars[7].label  = "JOUTERDIAL:";
    EditVars[7].target = &JOUTERDIAL;
    EditVars[7].type   = 1;

    EditVars[8].label = "** CAMERA ZOOM VARS **";
    EditVars[8].type  = 0;

    EditVars[9].label   = "camzoomedin:";
    EditVars[9].target  = &camzoomedin;
    EditVars[9].type    = 1;

    EditVars[10].label  = "camzoomedout:";
    EditVars[10].target = &camzoomedout;
    EditVars[10].type   = 1;

    EditVars[11].label  = "distzoomedin:";
    EditVars[11].target = &distzoomedin;
    EditVars[11].type   = 1;

    EditVars[12].label  = "distzoomedout:";
    EditVars[12].target = &distzoomedout;
    EditVars[12].type   = 1;

    EditVars[13].label = "** CAMERA VIEW VARS **";
    EditVars[13].type  = 0;

    EditVars[14].label  = "camheight:";
    EditVars[14].target = &camheight;
    EditVars[14].type   = 1;

    EditVars[15].label  = "camlookatheight:";
    EditVars[15].target = &camlookatheight;
    EditVars[15].type   = 1;
}
