/*
 * limeMPGameSpecific.c -- src/gamecode/limeMPGameSpecific.m, the game side of
 * multiplayer.
 *
 * armv7 0x000af938 - 0x000b20d8, 48 functions. **Draft**: transcribed from the
 * disassembly, not yet differential-tested (most of it is Objective-C message
 * sends to the session, which the oracle cannot run). What it does was
 * checked by playing two games against each other.
 *
 * The .m file is plain C apart from its calls into `gSession`, the
 * limeMPSession object that wrapped GKSession (Bluetooth). Every such call is
 * a `mpnet_*` call here, and runtime/mp_net.c answers it -- that file is the
 * port's, and it is where the PC network is. Everything in this file is what
 * the binary does with the answers.
 *
 * ## The packets
 *
 * `sendPacket` (0xafee4) builds every payload; its jump table has fourteen
 * cases. On the wire each payload follows a 4-byte big-endian type, which
 * limeMPSession added (`receiveData:` strips it and calls processPacket).
 *
 *      1  CHARACTER  {int}                    7  INGAME EVENT {int, int}
 *      2  MENU       {int}                    8  KODE      {3 digits, index}
 *      3  FE MENU    {int}                    9  LEVEL     {int}
 *      4  SYNC       {word, int}             10  FPS       {float, float}
 *      5  SPRITELIST 0xfc bytes, unreliable  11  HEARTBEAT {0}
 *      6  JOYSTICK   {bits, fps}, unreliable 12  TIMER     {4 words}
 *                                            13  PLAY      {1}
 *                                            14  GENERIC   {int, word}
 *
 * The float arguments are how the original passed them -- `sendPacket(type,
 * float a, float b, ...)`, softfp, so a float travels in a core register --
 * and the senders convert their ints with `vcvt.f32.s32` before the call.
 * Where the decomp's callers pass a raw word (sendGenericPacket(0x3f800000,
 * 0), the 1.0f the binary loads), it is reinterpreted as the float it is.
 *
 * ## The sprite list
 *
 * The parent steps the fight and sends, every tick, the object list (up to
 * eight 16-byte objects, linked through their first word), both players'
 * positions, the game's transferable flags and the event queue. The guest
 * queues what arrives (120 entries) and UpdateArcadeCode drains the queue
 * through setNextSpritesAndEvents. The links travel as the marker 0x29a and
 * unpackSpriteList makes them point at the queue entry's own objects again.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mp_net.h"

/* ---- the file's own data, 0x0017c920 onward ---- */

static float playerFPS   = -1.0f;      /* 0x0017c920 */
static float opponentFPS = -1.0f;      /* 0x0017c924 */
static long  packetCnt;                 /* 0x0017c934 */
static long  lastPacketCnt;             /* 0x0017c938 */
static long  rightAfterPause;           /* 0x0017c93c */
static long  lbcnt;                     /* 0x0017c940 */
static long  syncCnt;                   /* 0x0017c94c */
static long  syncCntTmp;                /* 0x0017c950 */
static float syncAverage;               /* 0x0017c954 */
static long  run;                       /* 0x0017c95c */
long heartbeat[3];                      /* 0x0017ca64: on, countdown, sent */
long disconnectedFromCharacterSelect;   /* pointer slot 0x000f32b8 */

#define MP_QUEUE_LEN      120
#define MP_PACKET_BYTES   0xfc
/* The queue the guest's sprite lists wait in; 0x0017ca5c holds its address.
 * Static, so it sits in the image -- below 2 GB, where the objects' 32-bit
 * link words reach it in the x64 build. */
static unsigned char incomingQueue[MP_QUEUE_LEN * MP_PACKET_BYTES];
static unsigned char packedSpriteList[0x80];

/* ---- the rest of the game ---- */

extern long   GameMode;
extern int    CurrentTask;
extern int    FE_CurrentTask;
extern float  FrameCount;
extern float  limeFPSScaleFactor;
extern float  averageScaleFactorOutput;
extern float  DisplayAverageFPS;
extern float  FE_FadeAdd;
extern float  Player1Pos[3];
extern float  Player2Pos[3];
extern long  *MKEventQueue;
extern long   thisSessionId;
extern long  *requestedLevel;
extern long  *syncState;
extern long  *startTime;                /* a float */
extern long  *readyToSync;
extern long  *lastTimestamp;            /* a float */
extern int   *incomingQueueStartPtr;
extern int   *incomingQueueLenPtr;
extern int    opponentCharacter;
extern long   LevelSelect;
extern long   WaitForOpponent;
extern long   otherPlayerPaused;
extern long   opponentPressedPlay;
extern int    syncCharacters;
extern int    lastTimerTimestamp;       /* a float */
extern int    KodeSelector[10];
extern float  KodeSelectorParticle[10];
extern long   SpritelistReceived;
extern long   currentMPJoystickData;
extern long   currentMPSpriteList;
extern long   mpOpponentJoystickInput[];
extern char   mpSpriteList[];
extern char   mpEventQueue[];
extern long   startDebug;
extern long   GamePaused;
extern long   PauseMenuAreYouSure;
extern long   IsInFinishing;

void PushFETask(int task);
void PopFETaskDeferred(void);
void PopFETaskDeferred2(void);
void switchToTask(int task);
void switchToFETask(int task);
void processCharacterSelectedPacket(void);
void resetCharacterSelection(void);
void checkIfKode(void);
void preprocessPreloadKode(void);
void UpdateStats(void);
int  getTransferableFlags(void);
void setTransferableFlags(long flags);
void EASDK_LogEvent(long id, long a, const char *s1, long b, const char *s2);

long isParent(void);
void endMP(void);
void startMP(void);
void disconnected(void);

#define NSLog(...)  ((void)0)           /* the shipped logging, kept quiet */

static float word_float(long w)
{
    float f;
    int32_t i = (int32_t)w;
    memcpy(&f, &i, 4);
    return f;
}

static int32_t float_word(float f)
{
    int32_t i;
    memcpy(&i, &f, 4);
    return i;
}


/* ------------------------------------------------------------------ state */

/* armv7 0x000af938 */
long isHeartbeatOn(void)
{
    return heartbeat[0] != 0;
}

/* armv7 0x000af94c. `n * 240`, written (n << 8) - (n << 4). */
void enableHeartbeat(long n)
{
    heartbeat[0] = 1;
    heartbeat[2] = 0;
    heartbeat[1] = n * 240;
}

/* armv7 0x000af968: five seconds after a pause, one otherwise (at 240 a
 * second, the countdown's rate). */
long getCurrentHeartbeat(void)
{
    return rightAfterPause ? 0x4b0 : 0xf0;
}

/* armv7 0x000af980 */
long getMultiplayerState(void)
{
    if (!mpnet_session_exists())
        return 0;
    return mpnet_connection_state();
}

/* armv7 0x000af9a4 */
long isMPConnected(void)
{
    return getMultiplayerState() == 2;
}

/* armv7 0x000af9b8 */
long isParent(void)
{
    if (!mpnet_session_exists())
        return 0;
    return mpnet_am_server();
}

/* armv7 0x000af9dc */
long isParentBasedOnSpeed(void)
{
    if (GameMode != 1)
        return 0;
    return isParent();
}

/* armv7 0x000af9fc. The two machines agree when the fight starts: the
 * parent sends 5 until the guest answers, the guest moves 5 -> 6 and sets
 * the start two seconds ahead (120 frames, scaled) when the 6 comes. */
static void processSyncCommand(long a, long b)
{
    (void)a;
    if (!*readyToSync)
        return;
    if (isParent()) {
        if (*syncState == 0 && b == 5)
            *syncState = 3;
        return;
    }
    if (*syncState == 5) {
        if (b == 5)
            *syncState = 6;
    } else if (*syncState == 6 && b == 6) {
        float *st = (float *)startTime;
        if (*st == 0.0f)
            *st = 120.0f / limeFPSScaleFactor + FrameCount;
    }
}

/* armv7 0x000afaa0 */
long isWorking(void)
{
    if (!mpnet_session_exists())
        return 0;
    return mpnet_is_working();
}

/* armv7 0x000afac4. The Bluetooth lobby's row `index`: the peer's display
 * name into `out`, cut to 32 characters with "..." when longer, and whether
 * the peer is on the invited list. There is no such list here. */
long getPeerNumber(long index, char *out, long size)
{
    (void)index;
    if (out && size > 0)
        out[0] = 0;
    return 0;
}

/* armv7 0x000afbcc */
long getNumberOfPeers(void)
{
    return mpnet_peer_count();
}

/* armv7 0x000afbfc */
void connectToPeer(long index)
{
    (void)index;                        /* [gSession connectTo:index] */
}

/* armv7 0x000afc20. Also turns the idle timer back on
 * ([UIApplication setIdleTimerDisabled:NO]); a PC has none. */
void heartbeatSetIncoming(long n)
{
    heartbeat[1] = n * 240;
}

/* armv7 0x000afc60 */
void disableHeartbeat(void)
{
    heartbeat[0] = 0;
}

/* armv7 0x000afc9c. The FPS each side reports; -1 is "not known". */
static void processDecideCommand(float theirs, float mine)
{
    if (opponentFPS != -1.0f && playerFPS != -1.0f)
        return;
    NSLog("JUDGING FPS:%f %f", theirs, mine);
    if (isParent()) {
        if (theirs != -1.0f && mine != -1.0f) {
            opponentFPS = theirs;
            playerFPS = mine;
        }
    } else if (theirs != -1.0f && mine == -1.0f) {
        opponentFPS = theirs;
        playerFPS = averageScaleFactorOutput;
    }
}

/* armv7 0x000afd78. The links of a queued sprite list made to point at the
 * entry's own objects; returns how many were linked. 0x14cd is a frame the
 * original logged as "LEGS IN SPRITELIST" under startDebug. */
static long unpackSpriteList(unsigned char *entry)
{
    long i = 0;

    if (*(uint32_t *)(entry + 0x78) != 0) {
        for (;;) {
            unsigned char *obj = entry + 0x78 + i * 16;
            *(uint32_t *)obj = (uint32_t)(uintptr_t)(entry + 0x88 + i * 16);
            i++;
            if (*(uint32_t *)(entry + 0x78 + i * 16) == 0)
                break;
        }
    }
    *(uint32_t *)(entry + 0x78 + i * 16) = 0;
    return i;
}

/* armv7 0x000afe20 */
void startMP(void)
{
    /* Port: dummyObjects (0x0010de84), what a guest draws before the first
     * sprite list, is two objects whose first link holds 0x0010de94 -- the
     * iPhone address of its own second object. Pointed at that object here;
     * left as it was, the guest's first fight frame read 0x0010dea1 and
     * crashed in RenderLevelPlayers. */
    {
        extern char dummyObjects[];
        *(uint32_t *)dummyObjects = (uint32_t)(uintptr_t)(dummyObjects + 16);
    }
    NSLog("staring mp");
    if (mpnet_session_exists())
        return;
    mpnet_session_create();
    NSLog("mp started");
}

/* armv7 0x000afe84 */
void endMP(void)
{
    if (!mpnet_session_exists())
        return;
    NSLog("shutting down MP");
    mpnet_session_destroy();
    NSLog("mp ended");
}

/* armv7 0x000afed4 */
void restartConnection(void)
{
    endMP();
    startMP();
}


/* ------------------------------------------------------------- sendPacket */

/* armv7 0x000afee4, 1,364 bytes. `c` is a pointer for the joystick, the
 * kode and the sprite list. Every packet clears the heartbeat's "frames
 * since sent", so the keep-alive only goes out on a quiet line. */
static void sendPacket(long type, float a, float b, const void *c)
{
    unsigned char d[0x100];
    int32_t w[4];
    int len = 4, unreliable = 0;

    heartbeat[2] = 0;
    if (!mpnet_session_exists())
        return;

    switch (type) {
    case 1:                             /* CHARACTER SELECTED */
    case 2:                             /* GOTO MENU */
    case 3:                             /* GOTO FE MENU */
    case 9:                             /* LEVEL SELECTED */
        w[0] = (int32_t)a;
        break;
    case 4:                             /* SYNC */
        w[0] = float_word(a);
        w[1] = (int32_t)b;
        len = 8;
        break;
    case 5: {                           /* SPRITELIST */
        int32_t v;

        v = (int32_t)packetCnt++;
        memcpy(d + 0x00, &v, 4);
        v = float_word(FrameCount);
        memcpy(d + 0x04, &v, 4);
        memcpy(d + 0x08, Player1Pos, 12);
        memcpy(d + 0x14, Player2Pos, 12);
        v = getTransferableFlags();
        memcpy(d + 0x20, &v, 4);
        memcpy(d + 0x24, MKEventQueue, 0x54);
        memcpy(d + 0x78, packedSpriteList, 0x80);
        v = (int32_t)thisSessionId;
        memcpy(d + 0xf8, &v, 4);
        mpnet_send(type, d, MP_PACKET_BYTES, 0);
        return;
    }
    case 6:                             /* JOYSTICK */
        w[0] = *(const int32_t *)c;
        w[1] = (int32_t)DisplayAverageFPS;
        len = 8;
        unreliable = 1;
        break;
    case 7:                             /* INGAME EVENT */
        w[0] = (int32_t)a;
        w[1] = (int32_t)b;
        len = 8;
        break;
    case 8: {                           /* KODE: three digits and the slot */
        const int *k = (const int *)c;
        unsigned char *p = (unsigned char *)w;
        p[0] = (unsigned char)k[0];
        p[1] = (unsigned char)k[1];
        p[2] = (unsigned char)k[2];
        p[3] = (unsigned char)(int32_t)a;
        break;
    }
    case 10:                            /* FPS */
        w[0] = float_word(a);
        w[1] = float_word(b);
        len = 8;
        break;
    case 11:                            /* HEARTBEAT */
        w[0] = 0;
        break;
    case 12:                            /* TIMER */
        w[0] = float_word(b);
        w[1] = float_word(a);
        w[2] = (int32_t)thisSessionId;
        w[3] = (int32_t)*requestedLevel;
        len = 16;
        break;
    case 13:                            /* PLAY */
        w[0] = 1;
        break;
    case 14:                            /* GENERIC */
        w[0] = (int32_t)a;
        w[1] = float_word(b);
        len = 8;
        break;
    default:
        NSLog("UNKNOWN PACKET TYPE WHILE SENDING");
        return;
    }
    mpnet_send(type, w, len, !unreliable);
}

/* armv7 0x000b0438 */
static void sendIngameEventPacket(long event, long arg)
{
    sendPacket(7, (float)event, (float)arg, NULL);
}

/* armv7 0x000b0468 */
void sendQuit(void)
{
    sendIngameEventPacket(12, 1);
}

/* armv7 0x000b0478 */
void sendPause(long state)
{
    sendIngameEventPacket(11, state);
}

/* armv7 0x000b0488. Only the first word is sent; the second argument is
 * stored next to it and never read. */
void sendJoystickInputPacket(long bits, long unused)
{
    int32_t v[2];
    v[0] = (int32_t)bits;
    v[1] = (int32_t)unused;
    sendPacket(6, 0.0f, 0.0f, v);
}

/* armv7 0x000b04ac */
void sendPlayPacket(void)
{
    sendPacket(13, 0.0f, 0.0f, NULL);
}

/* armv7 0x000b04c8. The binary takes both as floats (softfp); the decomp's
 * one caller passes them as integers, converted here. */
void sendTimerPacket(long timer, long wait)
{
    sendPacket(12, (float)timer, (float)wait, NULL);
}

/* armv7 0x000b04e8 */
static void sendSyncPacket(float frame, float cmd)
{
    sendPacket(4, frame, cmd, NULL);
}

/* armv7 0x000b0508, 556 bytes. Called every frame while the fight waits to
 * start; returns 1 on the frame both machines go. The parent sends 5 every
 * fourteenth call until the guest's 5 comes back (syncState 3), then 6 every
 * fifth call and sets its start two seconds ahead; the guest, once it has
 * seen the 5, answers 5 every fifth call and starts two seconds after the
 * first 6 (processSyncCommand). A frame of 0 resets everything.
 *
 * GameCode.c declares this with a long; the binary passes FrameCount as a
 * float. The value is the same -- FrameCount steps by whole frames. */
long syncGame(long frame_l)
{
    float frame = (float)frame_l;
    float *st = (float *)startTime;

    syncCntTmp++;
    if (isParent()) {
        if (frame == 0.0f) {
            syncCntTmp = 0;
            *syncState = 0;
            syncCnt = 0;
            syncAverage = 0.0f;
            *st = 0.0f;
            run = 0;
            return 0;
        }
        if (*syncState == 0) {
            if (syncCntTmp % 14 == 13)
                sendSyncPacket(frame, 5.0f);
            return 0;
        }
        if (*syncState != 3)
            return 0;
        if (syncCntTmp % 5 == 4)
            sendSyncPacket(frame, 6.0f);
        if (*st == 0.0f) {
            *st = 120.0f / limeFPSScaleFactor + FrameCount;
            return 0;
        }
        return *st <= FrameCount;
    }
    if (frame == 0.0f) {
        *syncState = 5;
        syncCnt = 0;
        syncAverage = 0.0f;
        *st = 0.0f;
        syncCntTmp = 0;
        run = 0;
        return 0;
    }
    if (*syncState == 8)
        return 1;
    if (*syncState != 6)
        return 0;
    if (syncCntTmp % 5 == 4)
        sendSyncPacket(frame, 5.0f);
    if (*st == 0.0f || *st > FrameCount)
        return 0;
    *syncState = 8;
    return 1;
}

/* armv7 0x000b0734 */
void sendFEMenuPacket(long item)
{
    sendPacket(3, (float)item, 0.0f, NULL);
}

/* armv7 0x000b075c */
void sendMenuPacket(long which)
{
    sendPacket(2, (float)which, 0.0f, NULL);
}

/* armv7 0x000b0784 */
void sendCharacterPacket(long who)
{
    sendPacket(1, (float)who, 0.0f, NULL);
}

/* armv7 0x000b07ac */
void sendKodePacket(const int *digits, long index)
{
    sendPacket(8, (float)index, 0.0f, digits);
}

/* armv7 0x000b07d4. The decomp's callers (achievements.c) declare it
 * without an argument; the binary passes the level in r0. Read from where
 * every caller has just stored it. */
void sendLevelPacket(long level)
{
    (void)level;
    sendPacket(9, (float)*requestedLevel, 0.0f, NULL);
}

/* armv7 0x000b07fc */
static void sendFPSPacket(float a, float b)
{
    sendPacket(10, a, b, NULL);
}

/* armv7 0x000b081c */
void doFPSExchange(void)
{
    if (isParent()) {
        if (playerFPS == -1.0f || opponentFPS == -1.0f) {
            NSLog("sending fps pkt (average)");
            sendFPSPacket(averageScaleFactorOutput, -1.0f);
        }
        return;
    }
    if (playerFPS != -1.0f && opponentFPS != -1.0f) {
        NSLog("sending fps pkt (initial)");
        sendFPSPacket(playerFPS, opponentFPS);
    }
}

/* armv7 0x000b08d0. `a` arrives as a float's bits (FrontEnd.c passes
 * 0x3f800000, 1.0f). */
void sendGenericPacket(long a, long b)
{
    sendPacket(14, word_float(a), word_float(b), NULL);
}

/* armv7 0x000b08f0 */
static void sendHBPacket(void)
{
    sendPacket(11, 0.0f, 0.0f, NULL);
}

/* armv7 0x000b090c */
void heartbeatSend(void)
{
    if (GameMode == 1)
        sendHBPacket();
}


/* ------------------------------------------------------------- disconnect */

/* armv7 0x000b0928. Which screen to fall back to depends on CurrentTask
 * (a tbb over 2..9): the menus push the disconnected screen (0x2c) unless
 * they are the lobby already; a fight (6) or its end (7) pushes it and
 * leaves the fight, saving the stats first when a finisher was on. */
void disconnected(void)
{
    NSLog("################## at disconnected()");
    EASDK_LogEvent(0x7558, 0, 0, 0, 0);
    disconnectedFromCharacterSelect = 0;
    disableHeartbeat();

    switch (CurrentTask) {
    case 2: case 7:
        PushFETask(0x2c);
        break;
    case 3:
        if (FE_CurrentTask == 3) {
            NSLog("multiplayer@disconnected");
        } else if (FE_CurrentTask == 0x2a) {
            NSLog(" DISCONNECTED AT CHARACTER SELECT!");
            PushFETask(0x2c);
        } else {
            PushFETask(0x2c);
        }
        break;
    case 4: case 5: case 9:
        NSLog("PUSHING DISCONNECTED TASK!");
        PushFETask(0x2c);
        CurrentTask = 7;
        break;
    case 6:
        if (IsInFinishing)
            UpdateStats();
        NSLog("PUSHING DISCONNECTED TASK!");
        PushFETask(0x2c);
        CurrentTask = 7;
        break;
    default:
        NSLog("DEFAULT (%D)@disconnected");
        break;
    }
}

/* armv7 0x000b0a24. Generic 1: on the summary, the other side is there; in
 * a fight, they left. */
static void processGenericPacket(long sub, long arg)
{
    (void)arg;
    if (sub != 1)
        return;
    if (FE_CurrentTask == 0x28) {
        WaitForOpponent = 0;
    } else if (CurrentTask == 6) {
        disconnected();
        heartbeat[1] = -1;
        heartbeat[0] = 0;
        endMP();
    }
}

/* armv7 0x000b0a7c */
static void checkHeartbeat(void)
{
    if (FE_CurrentTask != 0x2a && GameMode != 1)
        return;
    if (!heartbeat[0])
        return;
    if (heartbeat[1] == 0) {
        if (otherPlayerPaused) {
            NSLog("OPPONENT BUSY, DONT DISCONNECT");
            heartbeat[1] = 100;
            return;
        }
        NSLog("NO HEARTBEAT, DISCONNECTING!");
        disconnected();
        heartbeat[0] = 0;
        heartbeat[1] = -1;
        endMP();
        NSLog("WE JUST TERMINATED CONNECTION!!!");
    }
    if (heartbeat[2] > 29) {
        sendHBPacket();
        heartbeat[2] = 0;
    }
}

/* armv7 0x000b0b18 */
void heartbeatUpdate(void)
{
    if (rightAfterPause > 0)
        rightAfterPause--;
    if (FE_CurrentTask != 0x2a && GameMode != 1)
        return;
    if (!heartbeat[0])
        return;
    if (otherPlayerPaused)
        heartbeatSetIncoming(5);
    if (heartbeat[1] > 0)
        heartbeat[1]--;
    if (heartbeat[2] <= 29)
        heartbeat[2]++;
    checkHeartbeat();
}

/* armv7 0x000b0b90. 11 pause / resume, 12 the other side quit the fight,
 * 13 and 14 the fight ended over there. */
static void processIngameEventCommand(long event, long arg)
{
    switch (event) {
    case 11:
        if (arg == 0) {
            if (GamePaused == 1 || GamePaused == 2) {
                GamePaused = 0;
                otherPlayerPaused = 0;
                rightAfterPause = 400;
            }
        } else if (arg > 0 && arg <= 2) {
            if (GamePaused == 0) {
                GamePaused = arg;
                otherPlayerPaused = 1;
                PauseMenuAreYouSure = 0;
            }
        }
        break;
    case 12:
        if (CurrentTask != 6 || FE_CurrentTask == 0x28)
            break;
        PushFETask(0x28);
        WaitForOpponent = 1;
        if (FE_FadeAdd == 0.0f)
            UpdateStats();
        FE_FadeAdd = -0.0333333351f;
        break;
    case 13:
    case 14:
        if (CurrentTask != 6 || FE_CurrentTask == 0x28)
            break;
        PushFETask(0x28);
        CurrentTask = 7;
        WaitForOpponent = 1;
        UpdateStats();
        break;
    }
}

/* armv7 0x000b0ce4. The Bluetooth lobby's keep-alive: each listed peer's
 * count goes down (gone at 0), and every 120th call a generic 7 goes to all
 * peers. No peers are listed here, so only the count runs. */
void updateLobbyHeartBeat(void)
{
    lbcnt++;
}


/* ------------------------------------------------------------ sprite list */

/* armv7 0x000b0ec0. Up to eight objects, following the links; each copy's
 * link becomes 0x29a and the last one's 0. */
static long packSpriteList(const unsigned char *obj)
{
    long n = 0, legs = 0;
    unsigned char *o = packedSpriteList;

    memset(packedSpriteList, 0, sizeof packedSpriteList);
    for (;;) {
        uint32_t next;

        memcpy(o, obj, 16);
        *(uint32_t *)o = 0x29a;
        if (*(const int16_t *)(o + 8) == 0x14cd)
            legs = 1;
        next = *(const uint32_t *)obj;
        n++;
        if (n > 7 || next == 0)
            break;
        obj = (const unsigned char *)(uintptr_t)next;
        o += 16;
    }
    *(uint32_t *)o = 0;
    if (legs && n == 3 && startDebug)
        NSLog(" THREE ENTRIES WITH LEGS IN SPRITELIST!!!");
    return n;
}

/* armv7 0x000b0f80 */
void sendSpriteListPacket(void *objects, long a, long b)
{
    (void)a;
    if (!objects)
        return;
    packSpriteList((const unsigned char *)objects);
    sendPacket(5, (float)b, 0.0f, objects);
}

/* armv7 0x000b1fd8. The oldest queued list into the other half of the double
 * buffer: the objects into mpSpriteList (160 apart), the positions and the
 * events into mpEventQueue (216 apart). */
long setNextSpritesAndEvents(void)
{
    unsigned char *e;
    long k;

    if (*incomingQueueLenPtr <= 0)
        return 0;
    e = incomingQueue + *incomingQueueStartPtr * MP_PACKET_BYTES;
    k = (currentMPSpriteList + 1) & 1;
    memcpy(mpSpriteList + k * 160, e + 0x78, 0x80);
    memcpy(mpEventQueue + k * 216, e + 0x08, 12);
    memcpy(mpEventQueue + k * 216 + 0x0c, e + 0x14, 12);
    memcpy(mpEventQueue + k * 216 + 0x18, e + 0x24, 0x54);
    currentMPSpriteList = k;
    *incomingQueueStartPtr = (*incomingQueueStartPtr + 1) % MP_QUEUE_LEN;
    (*incomingQueueLenPtr)--;
    return 1;
}


/* ---------------------------------------------------------- processPacket */

/* armv7 0x000b0fb4, 4,132 bytes. Every packet from the other side, its type
 * already taken off by the session. Each case checks the payload's exact
 * length and drops anything else. Any packet at all refills the heartbeat.
 *
 * The original's generic subtypes 2, 3, 4 and 5 are GKSession's invitation
 * handshake and alerts (mpDidReceiveInvitation, mpInvitationDidFail,
 * mpIncorrectVersion, "HIDEME"); the port's session does its own handshake
 * (mp_net.c), so those arrive here never. 6 is the peer leaving and 7 the
 * lobby keep-alive. */
void processPacket(const void *payload, long len, long type)
{
    int32_t w[0x40];

    heartbeat[1] = getCurrentHeartbeat();
    if (len > (long)sizeof w)
        return;
    memcpy(w, payload, (size_t)len);

    switch (type) {
    case 1:
        if (len != 4)
            return;
        NSLog("GOT CHARACTER SELECTED %d", w[0]);
        opponentCharacter = w[0];
        processCharacterSelectedPacket();
        break;
    case 2:
        if (len != 4)
            return;
        if (w[0] != CurrentTask)
            switchToTask(w[0]);
        if (w[0] == 4)
            disableHeartbeat();
        break;
    case 3:
        if (len != 4)
            return;
        if (w[0] == -1 || w[0] == -2) {
            /* the other side picked REMATCH (-1) or another fighter (-2)
             * on the summary */
            if (FE_CurrentTask != 0x28 || FE_FadeAdd != 0.0f)
                return;
            if (w[0] == -1)
                PopFETaskDeferred();
            else
                PopFETaskDeferred2();
            resetCharacterSelection();
            enableHeartbeat(3);
            if (isParentBasedOnSpeed())
                thisSessionId++;
            startDebug = 1;
            return;
        }
        if (w[0] == FE_CurrentTask || FE_FadeAdd != 0.0f)
            return;
        if (w[0] == 0x2b)
            NSLog("SWITCHING MENU TO INCOMING FE_TASK_MULTIPLAYER_VERSUS_SCREEN!");
        switchToFETask(w[0]);
        break;
    case 4:
        if (len != 8)
            return;
        processSyncCommand(w[0], w[1]);
        break;
    case 5: {
        float ts;

        if (len != MP_PACKET_BYTES)
            return;
        if (w[0xf8 / 4] != (int32_t)thisSessionId) {
            NSLog("SPRITEPACKET with wrong session id (%d != %d)!!!");
            return;
        }
        if (CurrentTask != 6)
            return;
        memcpy(&ts, &w[1], 4);
        {
            float last;
            memcpy(&last, lastTimestamp, 4);
            if (!(ts > last) && ts != 0.0f) {
                NSLog("PACKET with incorrect timestamp!");
                return;
            }
        }
        if (w[0] != lastPacketCnt + 1)
            NSLog("DROPPED PACKET (old:%d - new:%d)");
        lastPacketCnt = w[0];
        memcpy(lastTimestamp, &ts, 4);
        setTransferableFlags(w[0x20 / 4]);
        SpritelistReceived = 1;
        if (*incomingQueueLenPtr > 0x77) {
            NSLog("########## INCOMING QUEUE EXCEEDED !!!");
            return;
        }
        {
            unsigned char *e = incomingQueue
                + ((*incomingQueueLenPtr + *incomingQueueStartPtr) % MP_QUEUE_LEN)
                  * MP_PACKET_BYTES;
            memcpy(e, w, MP_PACKET_BYTES);
            unpackSpriteList(e);
        }
        (*incomingQueueLenPtr)++;
        break;
    }
    case 6: {
        long k;

        if (len != 8)
            return;
        NSLog("GOT JOYSTICK_INPUT: %d,%d", w[0], w[0]);
        k = (currentMPJoystickData + 1) & 1;
        currentMPJoystickData = k;
        mpOpponentJoystickInput[k * 2] = w[0];
        mpOpponentJoystickInput[k * 2 + 1] = w[1];
        break;
    }
    case 7:
        if (len != 8)
            return;
        processIngameEventCommand(w[0], w[1]);
        break;
    case 8: {
        const signed char *d = (const signed char *)w;
        if (len != 4 || FE_CurrentTask != 0x2b)
            return;
        /* the parent keeps the guest's digits in the second row (+0x1c),
         * the guest the parent's in the first */
        if (isParent()) {
            KodeSelector[7] = d[0];
            KodeSelector[8] = d[1];
            KodeSelector[9] = d[2];
        } else {
            KodeSelector[0] = d[0];
            KodeSelector[1] = d[1];
            KodeSelector[2] = d[2];
        }
        KodeSelectorParticle[d[3] + (isParent() ? 7 : 0)] = 0.001f;
        checkIfKode();
        preprocessPreloadKode();
        break;
    }
    case 9:
        if (len != 4)
            return;
        LevelSelect = w[0];
        break;
    case 10: {
        float a, b;
        if (len != 8)
            return;
        memcpy(&a, &w[0], 4);
        memcpy(&b, &w[1], 4);
        processDecideCommand(a, b);
        break;
    }
    case 12: {
        float ts, last;
        if (len != 16 || GameMode != 1)
            return;
        memcpy(&ts, &w[0], 4);
        memcpy(&last, &lastTimerTimestamp, 4);
        if (!(ts > last))
            return;
        memcpy(&lastTimerTimestamp, &ts, 4);
        thisSessionId = w[2];
        if (!isParentBasedOnSpeed())
            *requestedLevel = w[3];
        break;
    }
    case 13:
        NSLog("GOT PLAY PACKET");
        if (syncCharacters == 0)
            syncCharacters = 1;
        opponentPressedPlay = 1;
        break;
    case 14:
        if (len != 8)
            return;
        switch (w[0]) {
        case 6:                         /* the peer we were connected to left */
            if (GameMode == 1 && mpnet_session_state() == 2) {
                NSLog("DESTROY ALL!");
                disconnected();
                endMP();
            }
            break;
        case 2: case 3: case 4: case 5: case 7:
            break;                      /* GKSession's own, see above */
        default:
            if (GameMode == 1)
                processGenericPacket(w[0], w[1]);
            break;
        }
        break;
    }
}
