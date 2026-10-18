/*
 * skin.c — see skin.h.
 *
 * Every layout here is from docs/SKIN-FORMAT.md and every constant is one the
 * disassembly gave up. The two that are easy to get wrong and are called out
 * in that document:
 *
 *   - `.skin` weights are uint16 scaled by 1/65536, the literal at 0x00060634.
 *   - `frameSize` comes from the .skinanim HEADER, never computed from the
 *     matching .bones. ROBO1 ships 20 animated bones against a 25-bone
 *     skeleton, so computing it looks right on most characters and then is not.
 */
#include "skin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "light.h"

/* ---------------------------------------------------------------- helpers */

static uint8_t *slurp(const char *path, long *size_out)
{
    FILE *fh = fopen(path, "rb");
    uint8_t *buf;
    long n;

    if (!fh) return NULL;
    fseek(fh, 0, SEEK_END);
    n = ftell(fh);
    fseek(fh, 0, SEEK_SET);
    if (n <= 0) { fclose(fh); return NULL; }

    buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(fh); return NULL; }
    if (fread(buf, 1, (size_t)n, fh) != (size_t)n) {
        free(buf); fclose(fh); return NULL;
    }
    fclose(fh);
    if (size_out) *size_out = n;
    return buf;
}

static int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static uint32_t rdu32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t rdu16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static float rdf(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }

/* ---------------------------------------------------------------- .bones */

bool lime_bones_load(const char *path, LimeSkeleton *out)
{
    long size = 0;
    uint8_t *d = slurp(path, &size);
    int32_t n, i, j;
    int8_t *claimed;

    memset(out, 0, sizeof(*out));
    if (!d) return false;
    if (size < 4) { free(d); return false; }

    n = rd32(d);
    /* 25 bytes per bone. ROBO1 and ROBO2 use 24 and are not handled: they are
     * the same two files that are the odd ones out in .meshset and .skin, and
     * treating them as a general case would mean guessing at which field the
     * missing byte came from. */
    if (n <= 0 || n > LIME_MAX_BONES || 4 + (long)n * 25 != size) {
        free(d);
        return false;
    }

    out->bones = (LimeBone *)calloc((size_t)n, sizeof(LimeBone));
    if (!out->bones) { free(d); return false; }
    out->count = n;

    for (i = 0; i < n; i++) {
        const uint8_t *rec = d + 4 + (long)i * 25;
        out->bones[i].num_children = rd32(rec);
        out->bones[i].offset[0] = rdf(rec + 4);
        out->bones[i].offset[1] = rdf(rec + 8);
        out->bones[i].offset[2] = rdf(rec + 12);
        for (j = 0; j < 9; j++)
            out->bones[i].child[j] = (int8_t)rec[16 + j];
    }
    free(d);

    /* The root is the bone nobody claims. All nine slots are children --
     * reading slot 0 as a parent link double-counts immediately, which is the
     * misreading SKIN-FORMAT.md §8 disproves. */
    claimed = (int8_t *)calloc((size_t)n, 1);
    if (!claimed) { lime_bones_free(out); return false; }
    for (i = 0; i < n; i++)
        for (j = 0; j < 9; j++) {
            int c = out->bones[i].child[j];
            if (c >= 0 && c < n) claimed[c] = 1;
        }
    out->root = -1;
    for (i = 0; i < n; i++)
        if (!claimed[i]) { out->root = i; break; }
    free(claimed);

    if (out->root < 0) { lime_bones_free(out); return false; }
    return true;
}

void lime_bones_free(LimeSkeleton *s)
{
    free(s->bones);
    memset(s, 0, sizeof(*s));
}

/* ----------------------------------------------------------------- .skin */

static long skin_block(const uint8_t *d, long size, long at, LimeSkinBlock *b)
{
    int32_t N, M, i;
    long need;

    if (at + 8 > size) return -1;
    N = rd32(d + at);
    M = rd32(d + at + 4);
    if (N <= 0 || M <= 0) return -1;

    need = 8 + (long)N * 108 + (long)M * 30;
    if (at + need > size) return -1;

    memset(b, 0, sizeof(*b));
    b->num_matrices = N;
    b->num_verts    = M;
    b->indexes = (uint32_t *)malloc((size_t)N * 4);
    b->weights = (float *)malloc((size_t)N * 4 * sizeof(float));
    b->a       = (float *)malloc((size_t)N * 12 * sizeof(float));
    b->b       = (float *)malloc((size_t)N * 12 * sizeof(float));
    b->tri     = (uint16_t *)malloc((size_t)M * 3 * sizeof(uint16_t));
    b->uv      = (float *)malloc((size_t)M * 6 * sizeof(float));
    if (!b->indexes || !b->weights || !b->a || !b->b || !b->tri || !b->uv)
        return -1;

    {
        const uint8_t *p = d + at + 8;

        for (i = 0; i < N; i++) b->indexes[i] = rdu32(p + (long)i * 4);
        p += (long)N * 4;

        /* uint16 fixed point, 1/65536 -- the literal at 0x00060634 */
        for (i = 0; i < N * 4; i++)
            b->weights[i] = (float)rdu16(p + (long)i * 2) / 65536.0f;
        p += (long)N * 8;

        /* entries are { MATRIX43 a; MATRIX43 b; } -- 96 bytes, A then B */
        for (i = 0; i < N; i++) {
            int k;
            const uint8_t *e = p + (long)i * 96;
            for (k = 0; k < 12; k++) b->a[i * 12 + k] = rdf(e + k * 4);
            for (k = 0; k < 12; k++) b->b[i * 12 + k] = rdf(e + 48 + k * 4);
        }
        p += (long)N * 96;

        /* vertData: M * 24 -- three UV pairs, one per triangle corner */
        for (i = 0; i < M * 6; i++) b->uv[i] = rdf(p + (long)i * 4);
        p += (long)M * 24;

        /* vertExtra: M * 6 -- three uint16 indices into the skinned positions */
        for (i = 0; i < M * 3; i++) b->tri[i] = rdu16(p + (long)i * 2);
    }
    return at + need;
}

bool lime_skin_load(const char *path, LimeSkin *out)
{
    long size = 0, at;
    uint8_t *d = slurp(path, &size);
    int32_t count, i;

    memset(out, 0, sizeof(*out));
    if (!d) return false;
    if (size < 4) { free(d); return false; }

    count = rd32(d);
    if (count == 1 || count == 2) {
        at = 4;
        for (i = 0; i < count; i++) {
            at = skin_block(d, size, at, &out->block[i]);
            if (at < 0) { free(d); lime_skin_free(out); return false; }
        }
        out->count = count;
        /* the walk must land exactly on the last byte -- there is no length
         * field, so an exact landing is the whole of the evidence */
        if (at != size) { free(d); lime_skin_free(out); return false; }
        free(d);
        return true;
    }

    /* ROBO1 and ROBO2 have no leading count: the first int32 is already the
     * matrix count and the file is one bare block. */
    at = skin_block(d, size, 0, &out->block[0]);
    if (at == size) { out->count = 1; free(d); return true; }
    free(d);
    lime_skin_free(out);
    return false;
}

void lime_skin_free(LimeSkin *s)
{
    int i;
    for (i = 0; i < 2; i++) {
        free(s->block[i].indexes); free(s->block[i].weights);
        free(s->block[i].a); free(s->block[i].b);
        free(s->block[i].tri); free(s->block[i].uv);
    }
    memset(s, 0, sizeof(*s));
}

/* ------------------------------------------------------------- .skinanim */

bool lime_anim_load(const char *path, LimeAnim *out)
{
    long size = 0;
    uint8_t *d = slurp(path, &size);

    memset(out, 0, sizeof(*out));
    if (!d) return false;
    if (size < 12) { free(d); return false; }

    out->data       = d;
    out->header     = 12;
    out->num_frames = rd32(d + 4);
    out->frame_size = rd32(d + 8);

    if (out->frame_size <= 16 ||
        12 + (long)out->num_frames * out->frame_size != size) {
        /* SINDEL_STANDARD reads a second float where the count should be. A
         * 16-byte header makes its arithmetic land exactly, but its own count
         * is then half the frames -- unresolved in SKIN-FORMAT.md §5, and this
         * accepts the 16-byte reading without claiming to know which. */
        out->header     = 16;
        out->num_frames = rd32(d + 8);
        out->frame_size = rd32(d + 12);
        if (out->frame_size <= 16 ||
            16 + (long)out->num_frames * out->frame_size != size) {
            free(d);
            memset(out, 0, sizeof(*out));
            return false;
        }
    }

    /* The bone count follows from the frame size and is NOT the skeleton's.
     * ROBO1 animates 20 of its 25 bones. */
    out->num_bones = (out->frame_size - 16) / 20;
    return true;
}

void lime_anim_free(LimeAnim *a)
{
    free(a->data);
    memset(a, 0, sizeof(*a));
}

/* --------------------------------------------------------------- posing */

/* GetMFromQuat2: quaternion to a 3x3 at stride 3. The engine does not
 * renormalise, so this does not either. */
static void quat_to_m3(const float *q, float *m)
{
    float x = q[0], y = q[1], z = q[2], w = q[3];

    m[0] = 1.0f - 2.0f * (y * y + z * z);
    m[1] =        2.0f * (x * y + z * w);
    m[2] =        2.0f * (x * z - y * w);

    m[3] =        2.0f * (x * y - z * w);
    m[4] = 1.0f - 2.0f * (x * x + z * z);
    m[5] =        2.0f * (y * z + x * w);

    m[6] =        2.0f * (x * z + y * w);
    m[7] =        2.0f * (y * z - x * w);
    m[8] = 1.0f - 2.0f * (x * x + y * y);
}

/* GetSlerpedQ, and the name is wrong on purpose: there is no acos, no sin and
 * no renormalisation in the original. It is a lerp with a shortest-arc sign
 * flip. Substituting a real slerp changes every in-between pose. */
static void blend_q(const float *a, const float *b, float t, float *out)
{
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    float s = (dot < 0.0f) ? -1.0f : 1.0f;
    int i;
    for (i = 0; i < 4; i++)
        out[i] = a[i] * (1.0f - t) + b[i] * s * t;
}

bool lime_pose(const LimeSkeleton *sk, const LimeAnim *anim,
               int frame_a, int frame_b, float t, LimePalette *palette)
{
    const uint8_t *fa, *fb;
    float root_pos[3];
    int order = 0;
    int stack_bone[LIME_MAX_BONES];
    int stack_parent[LIME_MAX_BONES];
    int top = 0, i;

    if (frame_a < 0 || frame_a >= anim->num_frames ||
        frame_b < 0 || frame_b >= anim->num_frames)
        return false;

    fa = anim->data + anim->header + (long)frame_a * anim->frame_size;
    fb = anim->data + anim->header + (long)frame_b * anim->frame_size;

    /* the root position is interpolated the same way, through LerpVector3 */
    for (i = 0; i < 3; i++)
        root_pos[i] = rdf(fa + 4 + i * 4) * (1.0f - t)
                    + rdf(fb + 4 + i * 4) * t;

    /* Depth-first, ONE ANIMATION FRAME CONSUMED PER BONE VISITED. The frames
     * are indexed by visit order and not by bone index -- that is what makes
     * CreateMatrixPaletteRecurse2's four globals globals. Written iteratively
     * here because a 128-deep recursion in a frame loop is not worth it. */
    stack_bone[top] = sk->root;
    stack_parent[top] = -1;
    top++;

    while (top > 0) {
        int bi, parent, n, k, j;
        float q[4], r[9];
        const float *pm;
        const float *pt;
        float local_t[3];

        top--;
        bi = stack_bone[top];
        parent = stack_parent[top];
        n = order++;

        if (n < anim->num_bones) {
            float qa[4], qb[4];
            for (k = 0; k < 4; k++) {
                qa[k] = rdf(fa + 16 + (long)n * 20 + k * 4);
                qb[k] = rdf(fb + 16 + (long)n * 20 + k * 4);
            }
            blend_q(qa, qb, t, q);
            quat_to_m3(q, r);
        } else {
            /* ROBO1 and ROBO2 ship fewer animated bones than skeleton bones.
             * Those tail bones genuinely have no rotation track, so identity
             * is the right fallback -- worth knowing rather than discovering
             * as a bent limb. */
            memset(r, 0, sizeof(r));
            r[0] = r[4] = r[8] = 1.0f;
        }

        /* the root takes its translation from the animation, everyone else
         * from the skeleton */
        if (n == 0) {
            local_t[0] = root_pos[0];
            local_t[1] = root_pos[1];
            local_t[2] = root_pos[2];
        } else {
            local_t[0] = sk->bones[bi].offset[0];
            local_t[1] = sk->bones[bi].offset[1];
            local_t[2] = sk->bones[bi].offset[2];
        }

        if (parent < 0) {
            memcpy(palette[bi].m, r, sizeof(r));
            memcpy(palette[bi].t, local_t, sizeof(local_t));
        } else {
            pm = palette[parent].m;
            pt = palette[parent].t;
            /* MatrixMul2, row-vector convention -- the one Xform2 uses */
            for (j = 0; j < 3; j++)
                for (k = 0; k < 3; k++)
                    palette[bi].m[j * 3 + k] =
                        r[j * 3 + 0] * pm[0 * 3 + k] +
                        r[j * 3 + 1] * pm[1 * 3 + k] +
                        r[j * 3 + 2] * pm[2 * 3 + k];
            for (k = 0; k < 3; k++)
                palette[bi].t[k] = local_t[0] * pm[0 * 3 + k]
                                 + local_t[1] * pm[1 * 3 + k]
                                 + local_t[2] * pm[2 * 3 + k]
                                 + pt[k];
        }

        /* push children in reverse so the pop order is slot order, which is
         * what makes the frame cursor line up with the original's walk */
        for (j = 8; j >= 0; j--) {
            int c = sk->bones[bi].child[j];
            if (c >= 0 && c < sk->count && top < LIME_MAX_BONES) {
                stack_bone[top] = c;
                stack_parent[top] = bi;
                top++;
            }
        }
    }
    return true;
}

void lime_skin_apply(const LimeSkinBlock *blk, const LimePalette *palette,
                     int num_bones, float *pos, unsigned char *light)
{
    int i, k;

    for (i = 0; i < blk->num_matrices; i++) {
        uint32_t idx = blk->indexes[i];
        const float *a = blk->a + (long)i * 12;
        const float *b = blk->b + (long)i * 12;
        const float *w = blk->weights + (long)i * 4;
        float p[3] = { 0, 0, 0 };
        float n[3] = { 0, 0, 0 };
        float len;

        for (k = 0; k < 4; k++) {
            unsigned bone = (idx >> (8 * k)) & 0xFFu;
            const float *m;
            const float *t;
            int c;

            /* 0xFF marks an unused influence. Reading `indexes` as a signed
             * int32 is what made these look like negative numbers. */
            if (bone == 0xFFu || (int)bone >= num_bones) continue;

            m = palette[bone].m;
            t = palette[bone].t;

            /* pos = SUM ( A[i]*M3x3 + w[i]*T ), nrm = SUM ( B[i]*M3x3 ).
             * The A and B vectors are ALREADY pre-multiplied by their weight,
             * which is why only the translation carries an explicit w -- and
             * why Xform2 ignores its weight beyond the zero test. */
            for (c = 0; c < 3; c++) {
                p[c] += a[k*3+0] * m[0*3+c] + a[k*3+1] * m[1*3+c]
                      + a[k*3+2] * m[2*3+c] + w[k] * t[c];
                n[c] += b[k*3+0] * m[0*3+c] + b[k*3+1] * m[1*3+c]
                      + b[k*3+2] * m[2*3+c];
            }
        }

        pos[i * 3 + 0] = p[0];
        pos[i * 3 + 1] = p[1];
        pos[i * 3 + 2] = p[2];

        if (light) {
            float v;
            len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
            if (len > 1e-8f) { n[0] /= len; n[1] /= len; n[2] /= len; }
            /* Vertex colour IS the lit skinned normal -- one grey byte, R=G=B.
             * There is no per-pixel lighting anywhere in this engine. */
            v = lime_light_vert(n[0], n[1], n[2]);
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            light[i] = (unsigned char)(v * 255.0f + 0.5f);
        }
    }
}
