/*
 * lime_app.c -- what UMK3AppDelegate does when the app loses and regains the
 * foreground, driven on PC by the window's keyboard focus (plat_focused).
 *
 * Read off the binary (-[UMK3AppDelegate applicationWillResignActive:] at
 * 0x00064cc4 and -[UMK3AppDelegate applicationDidBecomeActive:] at 0x00064f48;
 * the pointer slots name Settings, GameMode, CurrentTask, GamePaused,
 * otherPlayerPaused and interruptionFrequency):
 *
 *   resign active
 *     if (Settings[2]) limeStopTune()           -- music on: stop it
 *     EASDK_Pause(); interruptionFrequency++    -- analytics, nothing here
 *     [glView stopAnimation]                    -- the caller stops ticking
 *     if (GameMode == 1) {                      -- online match
 *         if (!GamePaused) { GamePaused = 1; sendPause(1) x3 }
 *     } else if (CurrentTask == 6) {
 *         if (!GamePaused) GamePaused = 1
 *     }
 *     (EASDK event logging, dismissing UIKit alerts -- nothing to do here)
 *
 *   become active
 *     [glView startAnimation]; EASDK_Resume()
 *     if (GameMode == 1 && !otherPlayerPaused && CurrentTask != 6) {
 *         sendPause(0) x3; GamePaused = 0
 *     }
 *     if (limeCheckForUserMusic()) Settings[2] = 0
 *     else if (Settings[2] && CurrentTask != 2 && CurrentTask != 5)
 *         limeRestartPlayTune()
 *
 * applicationDidEnterBackground / WillEnterForeground only tell the EA SDK,
 * which this port stubs; there is no separate background state on PC.
 */

extern int  Settings[10];               /* 0x00100e34 */
extern long GameMode;                   /* 0x0014faa4 */
extern int  CurrentTask;                /* 0x00150590 */
extern long GamePaused;                 /* 0x0014e1fc */
extern long otherPlayerPaused;          /* 0x0014e200 */

void limeStopTune(void);
void limeRestartPlayTune(void);
int  limeCheckForUserMusic(void);
void sendPause(long state);

/* interruptionFrequency (0x0014e264) only feeds the EA analytics log. */
static long g_interruptions;

long lime_app_interruptions(void) { return g_interruptions; }

void lime_app_resign_active(void)
{
    if (Settings[2] != 0)
        limeStopTune();
    g_interruptions++;

    if (GameMode == 1) {
        if (GamePaused == 0) {
            GamePaused = 1;             /* r4, which held GameMode == 1 */
            sendPause(1);
            sendPause(1);
            sendPause(1);
        }
    } else if (CurrentTask == 6) {
        if (GamePaused == 0)
            GamePaused = 1;
    }
}

void lime_app_become_active(void)
{
    if (GameMode == 1 && otherPlayerPaused == 0 && CurrentTask != 6) {
        sendPause(0);
        sendPause(0);
        sendPause(0);
        GamePaused = 0;
    }

    if (limeCheckForUserMusic())
        Settings[2] = 0;
    else if (Settings[2] != 0 && CurrentTask != 2 && CurrentTask != 5)
        limeRestartPlayTune();
}
