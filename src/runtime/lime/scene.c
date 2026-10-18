/*
 * scene.c -- `.scene` parser.  See scene.h and docs/SCENE-FORMAT.md.
 *
 * Every stride and every field offset below is transcribed from LIME_LoadScene
 * in the armv7 slice, not fitted to file sizes. The two places that matter:
 *
 *   - The tail record is 40 bytes on disk and 32 in memory, and the loader
 *     reads its first field at +4 relative to the count word that precedes the
 *     array (`ldr r3, [r1, #0x28]!` -- pre-indexed, with writeback). Reading it
 *     as +0 shifts every field by one float and turns the quaternion into
 *     nonsense that still looks plausible.
 *
 *   - A track record becomes a visible node key only when its first float
 *     exceeds 0.03 (the literal at 0x0005f464).
 */
#include "scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 0x0005f464 */
#define VISIBLE_THRESHOLD 0.03f

static uint8_t *slurp(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;

    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);

    b = (uint8_t *)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *len = n;
    return b;
}

static int32_t rd_i32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static float   rd_f32(const uint8_t *p) { float   v; memcpy(&v, p, 4); return v; }
static uint16_t rd_u16(const uint8_t *p){ uint16_t v; memcpy(&v, p, 2); return v; }

bool lime_scene_load(const char *path, LimeScene *out,
                     LimeFindMeshFn find, void *user)
{
    uint8_t *buf;
    long     len;
    int32_t  num_objects, count2, count3, i, k;
    long     obj_stride, tail_off;

    memset(out, 0, sizeof(*out));

    buf = slurp(path, &len);
    if (!buf) return false;
    if (len < 8) { free(buf); return false; }

    num_objects = rd_i32(buf + 0);
    count2      = rd_i32(buf + 4);
    if (num_objects < 0 || count2 <= 0) { free(buf); return false; }

    obj_stride = 64 + (long)count2 * 12;
    tail_off   = 8 + (long)num_objects * obj_stride;
    if (tail_off + 4 > len) { free(buf); return false; }

    count3 = rd_i32(buf + tail_off);
    if (count3 < 0 || tail_off + 4 + (long)count3 * 40 != len) {
        free(buf); return false;            /* ROBO1/ROBO2: a stub export */
    }

    /* ------------------------------------------------------ the palette.
     *
     * limeMalloc("SceneMtxPalette", count3 << 5) -- 32 bytes each, which is
     * exactly QSTMATRIX. The four floats are multiplied by 32767.0 (the
     * literal at 0x0005f468) and narrowed with vcvt.s32.f32, which rounds
     * toward zero; a C float-to-int conversion does the same. The remaining
     * six words are copied verbatim, so scale and translation stay float. */
    if (count3 > 0) {
        out->palette = (LimeQST *)calloc((size_t)count3, sizeof(LimeQST));
        if (!out->palette) { free(buf); return false; }
        for (i = 0; i < count3; i++) {
            const uint8_t *r = buf + tail_off + 4 + (long)i * 40;
            LimeQST *d = &out->palette[i];
            for (k = 0; k < 4; k++)
                d->q[k] = (int16_t)(rd_f32(r + k * 4) * 32767.0f);
            for (k = 0; k < 3; k++) {
                d->scale[k]       = rd_f32(r + 0x10 + k * 4);
                d->translation[k] = rd_f32(r + 0x1c + k * 4);
            }
        }
    }
    out->palette_size = count3;
    out->num_nodes    = num_objects;
    out->num_frames   = count2;
    out->scale        = 1.0f;               /* 0x3f800000 at 0x0005f194 */

    if (num_objects == 0) { free(buf); return true; }

    out->nodes = (LimeSceneNode *)calloc((size_t)num_objects,
                                         sizeof(LimeSceneNode));
    if (!out->nodes) { lime_scene_free(out); free(buf); return false; }

    /* -------------------------------------------------- the node streams */
    for (i = 0; i < num_objects; i++) {
        const uint8_t *obj = buf + 8 + (long)i * obj_stride;
        LimeSceneNode *nd  = &out->nodes[i];
        int mesh = -1;
        int32_t nkeys = 0;

        memcpy(nd->name, obj, 64);
        nd->name[63] = '\0';

        nd->keys   = (LimeSceneKey *)calloc((size_t)count2,
                                            sizeof(LimeSceneKey));
        nd->stream = (uint16_t *)malloc((size_t)count2 * sizeof(uint16_t));
        if (!nd->keys || !nd->stream) { lime_scene_free(out); free(buf); return false; }

        /* One name, one lookup: the original calls LIME_FindMeshByName inside
         * the loop, but always with this object's name. */
        if (find) mesh = find(nd->name, user);

        for (k = 0; k < count2; k++) {
            const uint8_t *trk = obj + 64 + (long)k * 12;
            float value = rd_f32(trk + 0);

            nd->stream[k] = LIME_SCENE_HIDDEN;      /* strh -1 first */
            if (!(value > VISIBLE_THRESHOLD))       /* ble -> skip */
                continue;

            nd->keys[nkeys].alpha         = value;
            nd->keys[nkeys].mesh_index    = (uint8_t)(mesh < 0 ? 0xFF : mesh);
            nd->keys[nkeys].field5        = (uint8_t)rd_i32(trk + 4);
            nd->keys[nkeys].palette_index = rd_u16(trk + 8);
            nd->stream[k] = (uint16_t)nkeys;
            nkeys++;
        }
        nd->num_keys = nkeys;
    }

    free(buf);
    return true;
}

const LimeSceneKey *lime_scene_key(const LimeScene *s, int node, int frame)
{
    const LimeSceneNode *nd;
    int f;

    if (!s || node < 0 || node >= s->num_nodes || s->num_frames <= 0)
        return NULL;

    nd = &s->nodes[node];
    if (!nd->keys || !nd->stream) return NULL;

    f = frame % s->num_frames;               /* __modsi3, then clamped */
    if (f < 0) f = 0;

    if (nd->stream[f] == LIME_SCENE_HIDDEN) return NULL;
    if (nd->stream[f] >= nd->num_keys)      return NULL;
    return &nd->keys[nd->stream[f]];
}

void lime_scene_free(LimeScene *s)
{
    int32_t i;
    if (!s) return;
    if (s->nodes) {
        for (i = 0; i < s->num_nodes; i++) {
            free(s->nodes[i].keys);
            free(s->nodes[i].stream);
        }
        free(s->nodes);
    }
    free(s->palette);
    memset(s, 0, sizeof(*s));
}

/* ------------------------------------------------------------------------
 * The QST -> GL matrix conversion.
 *
 * Transcribed from ConvertQSTMatrixtoPCMatrix. Two details are load-bearing
 * and both were found by a bit-precision differential test, not by reading:
 *
 *   - the constant is the double 3.0518509447574615e-05, NOT 1/32767. Its
 *     reciprocal is 32767.000030516647, and the difference shows up as about
 *     four ULP in every quaternion-derived element.
 *
 *   - each product is doubled ONCE into its own value, and the diagonal
 *     subtracts the doubled pair as a single term. Written as
 *     `1.0 - (yy+yy) - (zz+zz)` -- two subtractions -- the diagonal differs in
 *     the last bits. Floating point is not associative.
 */
void lime_qst_matrix(const LimeQST *src, float *dst)
{
    const double k = 3.0518509447574615e-05;
    double x = (double)src->q[0] * k;
    double y = (double)src->q[1] * k;
    double z = (double)src->q[2] * k;
    double w = (double)src->q[3] * k;

    double xx = x * x, yy = y * y, zz = z * z;
    double xy = x * y, xz = x * z, yz = y * z;
    double wx = w * x, wy = w * y, wz = w * z;

    double xx2 = xx + xx, yy2 = yy + yy, zz2 = zz + zz;
    double xy2 = xy + xy, xz2 = xz + xz, yz2 = yz + yz;
    double wx2 = wx + wx, wy2 = wy + wy, wz2 = wz + wz;

    dst[3] = 0.0f;  dst[7] = 0.0f;  dst[11] = 0.0f;

    dst[0]  = (float)(1.0 - (yy2 + zz2));
    dst[1]  = (float)(xy2 + wz2);
    dst[2]  = (float)(xz2 - wy2);

    dst[4]  = (float)(xy2 - wz2);
    dst[5]  = (float)(1.0 - (xx2 + zz2));
    dst[6]  = (float)(yz2 + wx2);

    dst[8]  = (float)(xz2 + wy2);
    dst[9]  = (float)(yz2 - wx2);
    dst[10] = (float)(1.0 - (xx2 + yy2));

    dst[0] *= src->scale[0];  dst[1] *= src->scale[0];  dst[2]  *= src->scale[0];
    dst[4] *= src->scale[1];  dst[5] *= src->scale[1];  dst[6]  *= src->scale[1];
    dst[8] *= src->scale[2];  dst[9] *= src->scale[2];  dst[10] *= src->scale[2];

    dst[12] = src->translation[0];
    dst[13] = src->translation[1];
    dst[14] = src->translation[2];
    dst[15] = 1.0f;
}
