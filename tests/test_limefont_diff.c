/*
 * test_limefont_diff.c — clean limeFont.cpp against the oracle.
 *
 * The two string-measurement routines, which is where the font format's
 * consequences actually land: every menu layout in the game is positioned by
 * what these return.
 *
 * ## Why a hand-built FONT rather than a loaded one
 *
 * `limeCreateFONT` needs a metrics file and two textures. Driving it would test
 * the loader and the measurement together, and a divergence would not say which
 * one moved. So the test **builds the same FONT on both sides** — host pointers
 * for the clean C, guest addresses for the oracle — and compares only the
 * measurement.
 *
 * That also lets the glyph table be chosen rather than inherited, which matters:
 * the interesting cases are the ones a real font would rarely contain.
 *
 * ## What the sweeps are chosen to catch
 *
 *  - **Characters that are not in the table.** The search returns -1 and the
 *    caller falls back to `FONT+0x10`. A version that returned 0 instead would
 *    silently measure every unknown character as the first glyph.
 *
 *  - **The space.** It is deliberately *not* found — the search abandons itself
 *    on `0x20` and takes the fallback. A font atlas has no cell for it.
 *
 *  - **Kerning present and absent.** `FONT+0x28` is added only when the pointer
 *    is non-null, and it is SIGNED. Half the entries here are negative.
 *
 *  - **The simple flag**, which replaces every glyph width with one value.
 *
 *  - **UTF-16 with and without the BOM**, against both routines. That is the
 *    whole difference between them: `limeGetStringWidth` detects the encoding,
 *    `limeGetStringWidthUCNoHeader` assumes it.
 *
 * Exact bit equality on the returned float. The sum is integer until the end
 * and is scaled once, so there is nothing to round differently.
 */

#include "arm_runtime.h"
#include "limefont.h"                   /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <stdio.h>
#include <string.h>

#define RAM_SIZE   (4u << 20)
#define STACK_TOP  0x003F0000u

/* guest layout for the objects the test builds */
#define G_FONT     0x00100000u
#define G_CODES    0x00101000u
#define G_CODESW   0x00102000u
#define G_ADV      0x00103000u
#define G_KERN     0x00104000u
#define G_ATLASU   0x00105000u
#define G_ATLASV   0x00106000u
#define G_TEXT     0x00107000u

/* FONT offsets, each established in docs/FONT-FORMAT.md */
#define F_SIMPLE   0x04u
#define F_GLYPHH   0x08u
#define F_SPACING  0x0Cu
#define F_FALLBACK 0x10u
#define F_SCALE    0x14u
#define F_NGLYPHS  0x18u
#define F_ATLASU   0x1Cu
#define F_ATLASV   0x20u
#define F_ADVANCE  0x24u
#define F_KERNING  0x28u
#define F_DEFADV   0x2Cu
#define F_ATLASW   0x34u
#define F_ATLASH   0x38u
#define F_CODES    0x48u
#define F_CODESW   0x4Cu

#define NGLYPHS    24

static int  g_fail  = 0;
static long g_cases = 0;

static FONT     g_clean;
static uint8_t  g_codes[NGLYPHS];
static int16_t  g_codesW[NGLYPHS];
static int16_t  g_adv[NGLYPHS];
static int8_t   g_kern[NGLYPHS];
static int16_t  g_atlasU[NGLYPHS];
static int16_t  g_atlasV[NGLYPHS];

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static int same_bits(float a, float b)
{
    if (a != a && b != b) return 1;
    return memcmp(&a, &b, 4) == 0;
}

/* Deterministic. */
static uint32_t g_seed = 0x1234ABCDu;

static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed; }
static int nexti(int lo, int hi) { return lo + (int)(nextu() % (uint32_t)(hi - lo + 1)); }


/* Build the same font on both sides. `simple` and `kerned` select the two
 * branches the width routines take. */
static void build_font(int simple, int kerned)
{
    int i;

    for (i = 0; i < NGLYPHS; i++) {
        g_codes[i]  = (uint8_t)('A' + i);        /* 'A'..'X' -- no space */
        g_codesW[i] = (int16_t)g_codes[i];
        g_adv[i]    = (int16_t)nexti(1, 40);
        g_kern[i]   = (int8_t)nexti(-8, 8);      /* signed, both ways */
        g_atlasU[i] = (int16_t)nexti(0, 500);
        g_atlasV[i] = (int16_t)nexti(0, 500);
    }

    memset(&g_clean, 0, sizeof(g_clean));
    g_clean.simple          = simple;
    g_clean.glyphHeight     = nexti(8, 32);
    g_clean.spacing         = nexti(0, 4);
    g_clean.fallbackAdvance = 8;
    g_clean.field14         = (float)nexti(1, 3) * 0.5f;   /* a real float */
    g_clean.numGlyphs       = NGLYPHS;
    g_clean.atlasU          = g_atlasU;
    g_clean.atlasV          = g_atlasV;
    g_clean.glyphWidth      = g_adv;
    g_clean.kerning         = kerned ? g_kern : NULL;
    g_clean.defaultAdvance  = nexti(5, 20);
    g_clean.extraUnknown    = nexti(0, 6);   /* +0x3c, not-found only */
    g_clean.atlasWidth      = 512.0f;
    g_clean.atlasHeight     = 512.0f;
    g_clean.codes           = g_codes;
    g_clean.codesW          = g_codesW;

    /* the same, in guest memory */
    for (uint32_t a = 0; a < 0x80u; a += 4u) MEM_ST32(G_FONT + a, 0u);
    MEM_ST32(G_FONT + F_SIMPLE,   (uint32_t)g_clean.simple);
    MEM_ST32(G_FONT + F_GLYPHH,   (uint32_t)g_clean.glyphHeight);
    MEM_ST32(G_FONT + F_SPACING,  (uint32_t)g_clean.spacing);
    MEM_ST32(G_FONT + F_FALLBACK, (uint32_t)g_clean.fallbackAdvance);
    MEM_ST32(G_FONT + F_SCALE,    F32_U32(g_clean.field14));
    MEM_ST16(G_FONT + F_NGLYPHS,  (uint16_t)NGLYPHS);
    MEM_ST32(G_FONT + F_ATLASU,   G_ATLASU);
    MEM_ST32(G_FONT + F_ATLASV,   G_ATLASV);
    MEM_ST32(G_FONT + F_ADVANCE,  G_ADV);
    MEM_ST32(G_FONT + F_KERNING,  kerned ? G_KERN : 0u);
    MEM_ST32(G_FONT + F_DEFADV,   (uint32_t)g_clean.defaultAdvance);
    MEM_ST32(G_FONT + 0x3Cu,      (uint32_t)g_clean.extraUnknown);
    MEM_ST32(G_FONT + F_ATLASW,   F32_U32(g_clean.atlasWidth));
    MEM_ST32(G_FONT + F_ATLASH,   F32_U32(g_clean.atlasHeight));
    MEM_ST32(G_FONT + F_CODES,    G_CODES);
    MEM_ST32(G_FONT + F_CODESW,   G_CODESW);

    for (i = 0; i < NGLYPHS; i++) {
        MEM_ST8 (G_CODES  + (uint32_t)i,        g_codes[i]);
        MEM_ST16(G_CODESW + 2u * (uint32_t)i,   (uint16_t)g_codesW[i]);
        MEM_ST16(G_ADV    + 2u * (uint32_t)i,   (uint16_t)g_adv[i]);
        MEM_ST8 (G_KERN   + (uint32_t)i,        (uint8_t)g_kern[i]);
        MEM_ST16(G_ATLASU + 2u * (uint32_t)i,   (uint16_t)g_atlasU[i]);
        MEM_ST16(G_ATLASV + 2u * (uint32_t)i,   (uint16_t)g_atlasV[i]);
    }
}

static void put_text(const char *s, int nbytes)
{
    int i;
    for (i = 0; i < nbytes; i++) MEM_ST8(G_TEXT + (uint32_t)i, (uint8_t)s[i]);
    MEM_ST8(G_TEXT + (uint32_t)nbytes, 0u);
}

static void check(const char *what, float clean, float orc)
{
    g_cases++;
    if (!same_bits(clean, orc)) {
        printf("  DIVERGE %s: clean=%.9g  oracle=%.9g\n", what, clean, orc);
        g_fail++;
    }
}


/* ------------------------------------------------------ limeGetStringWidth */

static void test_width(const char *text, int nbytes, const char *label)
{
    arm_ctx ctx;
    float clean, orc;

    put_text(text, nbytes);

    clean = limeGetStringWidth(&g_clean, text);

    ctx_reset(&ctx);
    ctx.r[0] = G_FONT;
    ctx.r[1] = G_TEXT;
    func_0007defc_limeGetStringWidth(&ctx);
    orc = U32_F32(ctx.r[0]);   /* soft-float: the return comes back in r0 */

    check(label, clean, orc);
}


/* ------------------------------------------- limeGetStringWidthUCNoHeader */

static void test_width_uc(const char *text, int nbytes, const char *label)
{
    arm_ctx ctx;
    float clean, orc;

    put_text(text, nbytes);

    clean = limeGetStringWidthUCNoHeader(&g_clean, text);

    ctx_reset(&ctx);
    ctx.r[0] = G_FONT;
    ctx.r[1] = G_TEXT;
    func_0007ddc4_limeGetStringWidthUCNoHeader(&ctx);
    orc = U32_F32(ctx.r[0]);   /* soft-float: the return comes back in r0 */

    check(label, clean, orc);
}


/* Build a UTF-16LE string from ASCII, optionally with the byte-order mark. */
static int to_utf16(const char *ascii, char *out, int with_bom)
{
    int n = 0, i;
    if (with_bom) { out[n++] = (char)0xFF; out[n++] = (char)0xFE; }
    for (i = 0; ascii[i]; i++) { out[n++] = ascii[i]; out[n++] = 0; }
    out[n] = 0; out[n + 1] = 0;
    return n + 2;
}


int main(void)
{
    static const char *samples[] = {
        "A", "AB", "ABCDEFGH", "AXAXAX",
        "A B C",                    /* spaces: not found, fallback advance */
        "Az9",                      /* 'z' and '9' are outside the table */
        "   ",                      /* nothing but spaces */
        "XWVUTSRQ",
    };
    char wide[128];
    int n, s, k;

    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    printf("=== clean limeFont.cpp vs the recompiled original ===\n\n");

    for (s = 0; s < 2; s++) {           /* simple off, then on */
        for (k = 0; k < 2; k++) {       /* kerning absent, then present */
            char label[96];
            build_font(s, k);

            for (n = 0; n < (int)(sizeof(samples) / sizeof(samples[0])); n++) {
                snprintf(label, sizeof(label), "width(%s) simple=%d kern=%d",
                         samples[n], s, k);
                test_width(samples[n], (int)strlen(samples[n]), label);

                /* the same text as UTF-16, with the BOM: the detecting routine
                 * must take its wide path and agree */
                {
                    int len = to_utf16(samples[n], wide, 1);
                    snprintf(label, sizeof(label), "width(utf16+bom %s) simple=%d kern=%d",
                             samples[n], s, k);
                    test_width(wide, len, label);
                }

                /* and without it, against the routine that never looks */
                {
                    int len = to_utf16(samples[n], wide, 0);
                    snprintf(label, sizeof(label), "widthUC(%s) simple=%d kern=%d",
                             samples[n], s, k);
                    test_width_uc(wide, len, label);
                }
            }

            /* a long random run, to catch an accumulation that only drifts */
            for (n = 0; n < 200; n++) {
                char buf[64];
                int i, len = nexti(1, 40);
                for (i = 0; i < len; i++) buf[i] = (char)('A' + nexti(0, 25));
                buf[len] = '\0';
                snprintf(label, sizeof(label), "width(random %d) simple=%d kern=%d",
                         len, s, k);
                test_width(buf, len, label);
            }
        }
    }

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean font measurement matches the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
