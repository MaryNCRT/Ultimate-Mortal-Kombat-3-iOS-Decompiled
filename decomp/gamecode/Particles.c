/*
 * Particles.c — src/gamecode/Particles.cpp
 *
 * Five functions in the original. This is the one that sets the pool up.
 * Verified against the oracle by tests/test_gamecode2_diff.c.
 */

#include <stdint.h>
#include "lime_glapi.h"   /* the GL calls' calling convention */

/* The pool is 0x6000 bytes of 0x30-byte records -- 512 of them -- and this
 * function touches exactly one word in each. Only that word is named; nothing
 * here reads the other eleven. */
#define PARTICLE_STRIDE  0x30u
#define PARTICLE_POOL    0x6000u
#define PARTICLE_COUNT   (PARTICLE_POOL / PARTICLE_STRIDE)   /* 512 */

typedef struct PARTICLE {
    float    field00;            /* 0x00  a size, four steps of PSize/4 */
    float    field04;            /* 0x04  set together with field08, same value */
    float    field08;            /* 0x08  the free marker InitParticles clears */
    int32_t  field0c;            /* 0x0c  the type AddParticles was called with */
    float    field10;            /* 0x10  x, passed straight through */
    float    field14;            /* 0x14  y + blood_zoff */
    float    field18;            /* 0x18  z + blood_yoff */
    uint8_t  _pad1c[4];          /* 0x1c  not touched by AddParticles */
    float    field20;            /* 0x20  cleared on emit */
    float    vx;                 /* 0x24 */
    float    vy;                 /* 0x28  zero for every type except blood */
    float    vz;                 /* 0x2c */
} PARTICLE;

extern PARTICLE Particles[PARTICLE_COUNT];   /* 0x001f44d4 */

void InitParticles(void);


/* ------------------------------------------------------------ InitParticles
 *
 * armv7 0x0005ab54, 24 bytes.
 *
 *      add.w r1, r3, #0x6000        ; the END, computed ONCE
 *  L:  movs  r2, #0
 *      str   r2, [r3, #8]
 *      adds  r3, #0x30
 *      cmp   r3, r1
 *      bne   L
 *
 * The bound is a byte span rather than a count, and the loop is entered at the
 * `movs` rather than at the `add` -- so `r1` is the fixed end of the pool and
 * not something recomputed from a moving cursor. Reading the branch target one
 * instruction earlier turns this into an infinite loop, which is the kind of
 * thing that reads fine and never terminates.
 *
 * It clears ONE word per record and leaves the other eleven alone. A memset
 * over the pool would be shorter, agree on everything this function is asked
 * about, and destroy whatever the rest of each record was carrying.
 */
void InitParticles(void)
{
    unsigned i;

    for (i = 0; i < PARTICLE_COUNT; i++)
        Particles[i].field08 = 0;
}


extern long PIrand_seed;                /* 0x0016f740 */


/* ---------------------------------------------------------------- PI_limeRand
 *
 * armv7 0x0005ab18, 52 bytes.  **Complete.**
 *
 *      seed = seed * 0x41c64e6d + 0x3039
 *      return (seed / 0x10000) % 0x8000
 *
 * **A textbook LCG with the textbook constants**: 1103515245 and 12345 are the
 * pair from the C standard's own example `rand()`, and the shift-and-mask tail
 * is `(seed / 65536) % 32768` written out by the compiler, sign handling and
 * all -- `bics r0, r0, r0, asr #32` is the branchless "clamp negatives to zero"
 * that the division needs.
 *
 * The particle system keeps its OWN seed, separate from `limeRand`. That is
 * worth preserving in a port: sharing one generator between particles and
 * gameplay makes the fight's randomness depend on how many sparks were drawn.
 */
long PI_limeRand(void)
{
    PIrand_seed = PIrand_seed * 1103515245L + 12345L;
    return (PIrand_seed / 0x10000) % 0x8000;
}


extern long   GamePaused;               /* pointer slot */
extern long   DoIntro;                  /* pointer slot */
extern void **GameObjects;             /* 0x0014dfec, one per object */
extern float  Player1Pos[];               /* pointer slot -> 0x00150564 */
extern float  Player2Pos[];               /* pointer slot -> 0x00150570 */
extern char  *PlayersP;                 /* pointer slot -> _Players */
extern float  SmokeAdjust;              /* 0x0016f77c */
extern float  SmokeAdjust2;             /* 0x0016f780 */
extern float  SmokeAdjustZ;             /* 0x0016f778 */

long limeRand(void);
void AddParticles(long type, float x, float y, float z, float speed, long arg);


/* -------------------------------------------------------------- DoSmokesSmoke
 *
 * armv7 0x0005aff0, 624 bytes.  **Complete.**
 *
 * Emits Smoke's trail particles. Called with both fighters' ids and does the
 * same work twice, once per player.
 *
 * **The two characters that smoke are 22 and 14.** Both ids are hardcoded, and
 * they behave differently:
 *
 *      id == 22   the X offset is `SmokeAdjust`, or `SmokeAdjust2` when the
 *                 player's flag at +0x540 (player 1) / +0xb30 (player 2) is set
 *      id == 14   the X offset is 0.0
 *
 * So one of them is nudged sideways by a tunable and the other is not, and
 * which tunable depends on a per-player flag. `_SmokeAdjust2` is the same
 * global whose neighbourhood several format strings live in.
 *
 * Note +0xb30 is +0x540 plus 0x5f0 -- the player stride, confirmed again.
 *
 * ### The frame guard, and the intro exception
 *
 * Normally it reads a frame id out of `GameObjects` -- **+0x08 for player one
 * and +0x18 for player two**, both `int16` -- and emits nothing when that id is
 * 20000 or 6830. 20000 is the same `ARCADE_POS_RAW` sentinel
 * `ArcadePosTo3dPos` special-cases; 6830 is not otherwise known here.
 *
 * **While `DoIntro` is set the guard is skipped entirely** and the position is
 * taken straight from `Player1Pos` / `Player2Pos`. So smoke keeps coming during
 * the intro regardless of what frame the fighter is on.
 *
 * ### The puff height is randomised in double precision
 *
 *      z = pos[2] + 1.4 + (limeRand() & 15) / 15.0 * -0.7
 *
 * -- so it lands somewhere in a 0.7-unit band, from `z + 1.4` down to
 * `z + 0.7`, in sixteen steps. The whole expression is `.f64`: the add, the
 * divide by 15 and the `vmla`, narrowed to float only at the call. Sixteen
 * discrete heights, not a continuous range.
 *
 * `AddParticles(8, x, y, z, 60.0f, -1)` -- type 8, the same 60.0f speed the
 * blood functions use, and -1 where they pass a count.
 */
void DoSmokesSmoke(long id1, long id2)
{
    if (GamePaused != 0)
        return;

    /* ---- player one ---- */
    if (id1 == 0x16 || id1 == 0x0e) {
        const float *pos = 0;
        float xoff = 0.0f;

        if (id1 == 0x16) {
            xoff = SmokeAdjust;
            if (*(const long *)(PlayersP + 0x540) != 0)
                xoff = SmokeAdjust2;
        }

        if (DoIntro != 0) {
            pos = Player1Pos;
        } else {
            const short *obj = (const short *)(void *)GameObjects;
            if (obj != 0) {
                long fid = obj[4];              /* +0x08, int16 */
                if (fid != 20000 && fid != 6830)
                    pos = Player1Pos;
            }
        }

        if (pos != 0) {
            double z = (double)pos[2] + 1.4;
            long r = limeRand() & 0xf;

            AddParticles(8, pos[0] + xoff, pos[1] + SmokeAdjustZ,
                         (float)(z + ((double)r / 15.0) * -0.7),
                         60.0f, -1);
        }
    }

    /* ---- player two ---- */
    if (id2 == 0x16 || id2 == 0x0e) {
        const float *pos = 0;
        float xoff = 0.0f;

        if (id2 == 0x16) {
            xoff = SmokeAdjust;
            if (*(const long *)(PlayersP + 0xb30) != 0)   /* 0x540 + 0x5f0 */
                xoff = SmokeAdjust2;
        }

        if (DoIntro != 0) {
            pos = Player2Pos;
        } else {
            const short *obj = (const short *)(void *)GameObjects;
            if (obj != 0) {
                long fid = obj[0x18 / 2];       /* +0x18, int16 */
                if (fid != 20000 && fid != 6830)
                    pos = Player2Pos;
            }
        }

        if (pos != 0) {
            double z = (double)pos[2] + 1.4;
            long r = limeRand() & 0xf;

            AddParticles(8, pos[0] + xoff, pos[1] + SmokeAdjustZ,
                         (float)(z + ((double)r / 15.0) * -0.7),
                         60.0f, -1);
        }
    }
}


/* ------------------------------------------------------------- AddParticles
 *
 * armv7 0x0005ab6c, 1,156 bytes.  **Complete.**
 *
 * Takes the first free slot in the 512-entry pool and fills it in. Four
 * different sets of initial velocities depending on the type, and they are
 * different enough that a port cannot collapse them.
 *
 * ### Finding a slot
 *
 * `field08` is the free marker -- it is the one word `InitParticles` clears,
 * and this function scans for the first slot whose `field08` is **exactly
 * 0.0f**. The scan runs indices 0 to 511 and **gives up silently** if all are
 * busy: no counter, no log, the particle is simply not emitted. With
 * `DoBlood` calling this eighty times a burst that matters, and it is the
 * behaviour to reproduce rather than fix.
 *
 * `type == 1` returns before the scan even starts. Nothing in the decompiled
 * code passes 1.
 *
 * ### What every type gets
 *
 *      field0c = type
 *      field10 = x                       passed straight through
 *      field14 = y + blood_zoff
 *      field18 = z + blood_yoff
 *      field20 = 0
 *      field04 = field08 = speed * 1.8f
 *
 * so the y and z the caller passes are **offsets from two globals**, not world
 * coordinates. `blood_zoff` lands on the y field and `blood_yoff` on the z --
 * that crossing is in the original and is what the names have to live with.
 *
 * Types 2, 4, 6 and 8 then overwrite `field04`/`field08` with a life instead:
 *
 *      max(0, (int)(speed - (limeRand() & 0xf)))
 *
 * clamped at zero with `bic r0, r0, r0, asr #31`, so a slow enough particle is
 * born already dead. Those four also set a size:
 *
 *      field00 = ((limeRand() & 3) + 1) * PSize * 0.25f
 *
 * four discrete sizes, a quarter of `PSize` apart.
 *
 * ### The four velocity sets
 *
 * All four draw three times from `PI_limeRand` -- the particle system's own
 * generator, not `limeRand` -- and mask to five bits.
 *
 * **Blood (2, 4, 6)** is the only one with a real Y velocity, and the only one
 * that scales by the `PScale` triple:
 *
 *      vx = (r/31.0 - 0.5)      * PScale[0]
 *      vy = (r/-31.0 * 0.5)     * PScale[1]
 *      vz = (r/31.0  * 0.5)     * PScale[2]
 *
 * **Type 0** aims the particle sideways from the fighter, and is the only place
 * the sixth argument is used at all:
 *
 *      vx = (r/32.0 - 0.5) * blood_xvs + (float)arg * blood_lrscale
 *      vx += blood_minxvel   if vx >= 0, otherwise vx -= blood_minxvel
 *      vx *= 0.01
 *      vy *= 0.0
 *      vz *= 0.1
 *
 * so `arg` is a **left/right bias** and `blood_minxvel` a floor that pushes the
 * particle away from the centre whichever side it started.
 *
 * **Smoke (8)** and **everything else** take the same three draws and differ
 * only in the final scale: `0.1 / 0.0 / 0.01` for smoke and `0.2 / 0.0 / 0.1`
 * for the default.
 *
 * ### The Y velocity is multiplied by zero
 *
 * Types 0, 8 and the default all end with `vy = vy * 0.0f` -- a literal zero in
 * the pool, not a store of zero, so the compiler was given `vy *= <something>`
 * with something folded to 0.0. The draw that produced it is still made and
 * still advances the generator, which is why removing it would change every
 * particle after it. **Only blood rises or falls**; everything else moves in
 * the XZ plane.
 */

extern float blood_zoff;                /* 0x0016f754 */
extern float blood_yoff;                /* 0x0016f758 */
extern float blood_lrscale;             /* 0x0016f75c */
extern float blood_xvs;                 /* 0x0016f760 */
extern float blood_minxvel;             /* 0x0016f750 */
extern float PScale[3];                 /* 0x0016f768 */
extern float PSize;                     /* 0x0016f774 */

/* The three draws every velocity set starts from. `d` is the divisor the
 * original uses -- 32.0 for everything except blood, which uses 31.0. */
static void PI_RandomVelocities(PARTICLE *p, double d)
{
    p->vx = (float)((double)(PI_limeRand() & 0x1f) / d - 0.5);
    p->vy = (float)((double)(PI_limeRand() & 0x1f) / d - 0.5);
    p->vz = (float)((double)(PI_limeRand() & 0x1f) / d);
}

/* The life and size types 2, 4, 6 and 8 share. Born dead if the draw exceeds
 * the speed. */
static void SetLifeAndSize(PARTICLE *p, float speed)
{
    long life = (long)(speed - (float)(limeRand() & 0xf));

    if (life < 0)
        life = 0;                       /* bic r0, r0, r0, asr #31 */

    p->field08 = (float)life;
    p->field04 = (float)life;

    p->field00 = (float)((limeRand() & 3) + 1) * PSize * 0.25f;
}

void AddParticles(long type, float x, float y, float z, float speed, long arg)
{
    PARTICLE *p;
    long      i;

    if (type == 1)
        return;

    /* the first slot whose free marker is exactly zero, or nothing at all */
    for (i = 0; ; i++) {
        if (Particles[i].field08 == 0.0f)
            break;
        if (i == PARTICLE_COUNT - 1)
            return;
    }
    p = &Particles[i];

    p->field0c = type;
    p->field10 = x;
    p->field08 = speed * 1.8f;
    p->field04 = speed * 1.8f;
    p->field14 = y + blood_zoff;
    p->field20 = 0.0f;
    p->field18 = z + blood_yoff;

    if (type == 8) {
        /* smoke */
        SetLifeAndSize(p, speed);
        PI_RandomVelocities(p, 32.0);
        p->vx = (float)((double)p->vx * 0.1);
        p->vy = p->vy * 0.0f;
        p->vz = (float)((double)p->vz * 0.01);
        return;
    }

    if (type == 4 || type == 2 || type == 6) {
        /* blood -- the only type with a Y velocity */
        SetLifeAndSize(p, speed);

        p->vx = (float)((double)(PI_limeRand() & 0x1f) / 31.0 - 0.5);
        p->vx = p->vx * PScale[0];
        p->vy = (float)((double)(PI_limeRand() & 0x1f) / -31.0 * 0.5);
        p->vy = p->vy * PScale[1];
        p->vz = (float)((double)(PI_limeRand() & 0x1f) / 31.0 * 0.5);
        p->vz = p->vz * PScale[2];
        return;
    }

    if (type == 0) {
        /* the sideways spray, and the only user of `arg` */
        PI_RandomVelocities(p, 32.0);

        p->vx = p->vx * blood_xvs + (float)arg * blood_lrscale;
        if (p->vx < 0.0f)
            p->vx = p->vx - blood_minxvel;
        else
            p->vx = p->vx + blood_minxvel;

        p->vx = p->vx * 0.01f;
        p->vy = p->vy * 0.0f;
        p->vz = p->vz * 0.1f;
        return;
    }

    /* everything else */
    PI_RandomVelocities(p, 32.0);
    p->vx = (float)((double)p->vx * 0.2);
    p->vy = p->vy * 0.0f;
    p->vz = (float)((double)p->vz * 0.1);
}


/* -------------------------------------------------------- MaintainParticles
 *
 * armv7 0x0005b260, 1,660 bytes.  **Complete.**
 *
 * Walks all 512 slots once a frame: integrates whatever is alive, draws it, and
 * frees the slot when it expires. Four behaviours, and the interesting thing is
 * that two of them feed each other.
 *
 * ### The blood cycle closes here
 *
 * A droplet that reaches the floor **spawns a splat and dies**, and the splat is
 * the droplet's type plus one:
 *
 *      2  red droplet    ->  3  red splat     NewBloodTexture
 *      4  green droplet  ->  5  green splat   NewGreenBloodTexture
 *      6  black droplet  ->  7  black splat   NewBlackBloodTexture
 *
 * via `AddParticles(type + 1, x, y, ShadowOffset + 0.05, 30.0f, 1)`. The floor
 * is `ShadowOffset` and the splat is placed a twentieth of a unit above it.
 *
 * **Only one droplet in four leaves a splat** -- `limeRand() & 3` has to come
 * out zero. The other three vanish. So `DoBlood`'s eighty bursts do not produce
 * eighty marks on the floor, and a port that drops the test will carpet the
 * stage.
 *
 * ### The four behaviours
 *
 * **Types 3, 5, 7 -- the splats.** No physics at all. They grow in place and
 * fade as they grow:
 *
 *      size  += ((i & 3) + 1) * 0.005 / limeFPSScaleFactor
 *      alpha  = size * -2 + 1
 *      dies when size passes 0.5
 *
 * The growth rate comes off the **slot index**, `(i & 3) + 1`, so four splats
 * landing together grow at four different rates without a random draw. Drawn
 * with `limeDrawFaceUpSprite` -- face *up*, because a splat lies on the floor;
 * everything else in this function is `limeDrawFaceMeSprite`.
 *
 * **Types 2, 4, 6 -- the droplets.** `PGrav` is *added* to the vertical velocity
 * each frame, so `PGrav` is negative in the data. Position integrates by
 * `v / limeFPSScaleFactor`. Alpha is `ttl / life`, which is what settles those
 * two fields: `field04` is the life it was born with and `field08` the life it
 * has left. Death is by hitting `ShadowOffset`, not by the timer.
 *
 * **Type 8 -- smoke.** `GamePaused` freezes the physics but not the draw. The
 * horizontal velocity is **halved every frame** and the size multiplied by
 * **1.05**, so smoke slows to a stop while it swells. Alpha is
 * `ttl / life * 0.55` -- smoke never reaches more than 55% opaque.
 *
 * **Type 0 and everything else.** `BloodGravity` is *subtracted* from the
 * vertical velocity -- the opposite sign convention to the droplets' `PGrav`,
 * two gravity constants pulling the same way through different arithmetic.
 * Death is by the timer. The sprite is an **animation frame chosen by fall
 * speed**:
 *
 *      frame = vz * BloodVelMul + 5.5,  clamped to 0 .. 11
 *      tex   = BloodTextures[(int)frame]
 *
 * twelve frames of droplet, picked by how fast it is falling, and the sprite is
 * **mirrored by the sign of the horizontal velocity** -- `u0 = 1, u1 = -1` going
 * one way and `u0 = 0, u1 = 1` going the other.
 *
 * ### A splat's size starts as whatever the slot last held
 *
 * `AddParticles` sets `field00` for types 2, 4, 6 and 8 only. A splat -- 3, 5 or
 * 7 -- takes the default arm, which sets three velocities and nothing else, so
 * its size is inherited from the previous occupant of the slot, and from
 * uninitialised memory the first time round. `InitParticles` clears `field08`
 * and nothing else, so there is no pass that would have zeroed it.
 *
 * The growth loop clamps at 0.5 immediately afterwards, which bounds the damage
 * but does not remove it: the first frame of a splat is drawn at the inherited
 * size. Recorded rather than fixed.
 *
 * ### State around the walk
 *
 *      limeDisableDepthWrites / limeDisableDepthTest
 *      limeEnableAlphaBlending_Basic
 *      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE)
 *      ... the walk ...
 *      limeEnableDepthWrites / limeEnableDepthTest / limeDisableAlphaBlending
 *
 * The `glTexEnvf` is raw GL ES, named through the indirect symbol table like the
 * calls in [RENDER-PLAYERS.md](../../docs/RENDER-PLAYERS.md).
 */

#define PARTICLE_FRAMES   12            /* BloodTextures, picked by fall speed */
#define SPLAT_MAX_SIZE    0.5f
#define SMOKE_MAX_ALPHA   0.55f
#define GL_TEXTURE_ENV        0x2300u
#define GL_TEXTURE_ENV_MODE   0x2200u
#define GL_MODULATE           8448.0f

extern float  BloodGravity;             /* 0x0016f74c */
extern float  BloodSize;                /* 0x0016f744 */
extern float  BloodVelMul;              /* 0x0016f748 */
extern float  PGrav;                    /* 0x0016f764 -- negative in the data */
extern float  ShadowOffset;             /* pointer slot -> 0x0014dfc8, the floor */
extern float  limeFPSScaleFactor;       /* pointer slot -> 0x00171acc */
extern float  FaceMeMatrix[];           /* 0x0016f700 */
extern void *P32 *BloodTextures;            /* pointer slot -> 0x001f4460 */
extern void *NewBloodTexture;          /* pointer slot -> 0x001f4490 */
extern void *NewGreenBloodTexture;     /* pointer slot -> 0x001f4494 */
extern void *NewBlackBloodTexture;     /* pointer slot -> 0x001f4498 */
extern void **SmokeTexture;             /* pointer slot -> 0x001f449c */

void limeDisableDepthWrites(void);
void limeDisableDepthTest(void);
void limeEnableDepthWrites(void);
void limeEnableDepthTest(void);
void limeEnableAlphaBlending_Basic(void);
void limeDisableAlphaBlending(void);
void LIME_GLAPI glTexEnvf(unsigned target, unsigned pname, float param);
void limeDrawFaceMeSprite(void *tex, const float *m, float x, float y, float z,
                          float u0, float v0, float u1, float v1, float size,
                          float r, float g, float b, float a);
void limeDrawFaceUpSprite(void *tex, const float *m, float x, float y, float z,
                          float u0, float v0, float u1, float v1, float size,
                          float r, float g, float b, float a);

void MaintainParticles(void)
{
    long i;

    limeDisableDepthWrites();
    limeDisableDepthTest();
    limeEnableAlphaBlending_Basic();
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    for (i = 0; i != PARTICLE_COUNT; i++) {
        PARTICLE *p = &Particles[i];
        long      type;

        if (p->field08 == 0.0f)
            continue;                   /* a free slot */

        type = p->field0c;

        /* ---- 3, 5, 7: the splats. No physics; they grow and fade. ---- */
        if (type == 5 || type == 3 || type == 7) {
            float alpha;
            void *tex;

            p->field00 = (float)((double)p->field00
                                 + (double)((i & 3) + 1) * 0.005
                                   / (double)limeFPSScaleFactor);
            if (p->field00 > SPLAT_MAX_SIZE) {
                p->field00 = SPLAT_MAX_SIZE;
                p->field08 = 0.0f;      /* the slot frees on the last frame */
            }
            alpha = p->field00 * -2.0f + 1.0f;

            tex = (type == 3) ? NewBloodTexture
                : (type == 7) ? NewBlackBloodTexture
                              : NewGreenBloodTexture;

            limeDrawFaceUpSprite(tex, FaceMeMatrix,
                                 p->field10, p->field14, p->field18,
                                 0.0f, 0.0f, 1.0f, 1.0f,
                                 p->field00, 1.0f, 1.0f, 1.0f, alpha);
            continue;
        }

        /* ---- 2, 4, 6: the droplets. Die on the floor, not on the clock. ---- */
        if (type == 4 || type == 2 || type == 6) {
            void *tex;

            p->field08 -= 1.0f / limeFPSScaleFactor;
            if (p->field08 < 0.0f)
                p->field08 = 0.0f;

            p->vz      += PGrav / limeFPSScaleFactor;   /* PGrav is negative */
            p->field10 += p->vx / limeFPSScaleFactor;
            p->field14 += p->vy / limeFPSScaleFactor;
            p->field18 += p->vz / limeFPSScaleFactor;

            if (p->field18 <= ShadowOffset) {
                /* it landed: free the slot, and one in four leaves a mark */
                p->field08 = 0.0f;
                if ((limeRand() & 3) == 0)
                    AddParticles(type + 1, p->field10, p->field14,
                                 (float)((double)ShadowOffset + 0.05),
                                 30.0f, 1);
                continue;
            }

            tex = (type == 2) ? NewBloodTexture
                : (type == 6) ? NewBlackBloodTexture
                              : NewGreenBloodTexture;

            limeDrawFaceMeSprite(tex, FaceMeMatrix,
                                 p->field10, p->field14, p->field18,
                                 0.0f, 0.0f, 1.0f, 1.0f,
                                 p->field00, 1.0f, 1.0f, 1.0f,
                                 p->field08 / p->field04);
            continue;
        }

        /* ---- 8: smoke. Paused freezes the physics, not the draw. ---- */
        if (type == 8) {
            if (!GamePaused) {
                p->field08 -= 1.0f / limeFPSScaleFactor;
                if (p->field08 < 0.0f)
                    p->field08 = 0.0f;

                p->field10 += p->vx / limeFPSScaleFactor;
                p->field14 += p->vy / limeFPSScaleFactor;
                p->field18 += p->vz / limeFPSScaleFactor;

                p->vx      = p->vx * 0.5f;      /* slows to a stop */
                p->field00 = p->field00 * 1.05f;/* and swells */
            }

            limeDrawFaceMeSprite(*SmokeTexture, FaceMeMatrix,
                                 p->field10, p->field14, p->field18,
                                 0.0f, 0.0f, 1.0f, 1.0f,
                                 p->field00, 1.0f, 1.0f, 1.0f,
                                 p->field08 / p->field04 * SMOKE_MAX_ALPHA);
            continue;
        }

        /* ---- 0 and anything else: the frame-animated droplet ---- */
        {
            float frame, size;
            long  mirrored;

            p->field08 -= 1.0f / limeFPSScaleFactor;
            if (p->field08 < 0.0f)
                p->field08 = 0.0f;

            /* note the sign: this one SUBTRACTS its gravity */
            p->vz      -= BloodGravity / limeFPSScaleFactor;
            p->field10 += p->vx / limeFPSScaleFactor;
            p->field14 += p->vy / limeFPSScaleFactor;
            p->field18 += p->vz / limeFPSScaleFactor;

            /* the size is computed fresh and passed straight to the draw --
             * field00 is not written on this path */
            size = BloodSize * ((type == 0) ? 0.25f : 0.15f);

            frame = p->vz * BloodVelMul + 5.5f;
            if (frame < 0.0f)
                frame = 0.0f;
            if (frame > 11.0f)
                frame = 11.0f;
            p->field20 = frame;

            mirrored = (p->vx >= 0.0f);

            limeDrawFaceMeSprite(BloodTextures[(long)p->field20], FaceMeMatrix,
                                 p->field10, p->field14, p->field18,
                                 mirrored ? 1.0f : 0.0f, 0.0f,
                                 mirrored ? -1.0f : 1.0f, 1.0f,
                                 size, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    limeEnableDepthWrites();
    limeEnableDepthTest();
    limeDisableAlphaBlending();
}
