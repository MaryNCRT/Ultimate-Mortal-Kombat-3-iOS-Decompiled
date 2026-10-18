/*
 * lime/common/limeFont.cpp -- text measurement.
 *
 * Recovered from the armv6 slice. Addresses below are armv6.
 *
 * ---------------------------------------------------------------------------
 * The engine's strings are UTF-16, and it detects that from a BOM
 *
 * Every function here begins by testing the first two bytes against -1 and -2
 * -- `0xFF 0xFE`, the **UTF-16 little-endian byte order mark**. When it is
 * present the string is walked two bytes at a time and terminated by a pair of
 * zeros; when it is absent the string is treated as bytes.
 *
 * So a single `const char *` may be either encoding and the engine decides at
 * runtime, per string. That is worth knowing before writing any text handling
 * for the port: passing UTF-16 to something expecting bytes stops at the first
 * character, and passing bytes to something expecting UTF-16 reads past the
 * end.
 *
 * It also fills a gap. The `.lproj` bundles contain nothing but a `dummy.txt`,
 * so the localised strings are not where iOS would normally put them -- and
 * this is the machinery that reads them wherever they are.
 *
 * ---------------------------------------------------------------------------
 * FONT, as far as these functions reveal it
 *
 *      +0x0c   glyph table pointer
 *      +0x14   float, multiplied into the final width
 *      +0x18   uint16 glyph count (sign-extended, then doubled -- so the
 *              entries are 2 bytes, or it indexes a paired table)
 *      +0x48   a second table, consulted inside the per-glyph search
 *
 * Those offsets are read off the code and are certain. What the tables contain
 * is not, and is left alone below rather than guessed at.
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lime.h"


/* --------------------------------------------------------- limeFontStrLen
 *
 * armv6 0x000aed3c, 68 bytes.  **Complete** -- every instruction accounted for.
 *
 * Length in *characters*, not bytes, for a string that may or may not carry a
 * UTF-16 BOM.
 *
 * Two details worth keeping:
 *
 *  - A NULL argument returns the argument unchanged rather than a constant.
 *    For a length that is the same value, but a caller testing for a sentinel
 *    will not find one.
 *  - Strings of one byte or less short-circuit **before** the BOM test, since
 *    a BOM needs two bytes and `strlen` of a lone `0xFF` is 1.
 */
int limeFontStrLen(const char *s)
{
    int len;
    int i;

    if (s == NULL)
        return 0;

    len = (int)strlen(s);
    if (len <= 1)
        return len;

    /* 0xFF 0xFE -- UTF-16 LE. Read as signed bytes, that is -1 then -2. */
    if (!((signed char)s[0] == -1 && (signed char)s[1] == -2))
        return len;                     /* plain bytes */

    /* past the BOM, count 16-bit units until a zero pair */
    for (i = 0; ; i += 2) {
        if (s[i + 2] == 0 && s[i + 3] == 0)
            break;
    }
    return i / 2;
}


/* ------------------------------------------------------ limeGetStringWidth
 *
 * armv6 0x000aeae4, 400 bytes.  **Not fully decompiled** -- see below.
 *
 * Measured width of a string in a font, returned as a float.
 *
 * What is established:
 *
 *  - the same BOM test as limeFontStrLen chooses between byte and UTF-16
 *    traversal;
 *  - a NULL string loads a literal and returns immediately, so it is a width
 *    of nothing rather than an error;
 *  - the only function it calls is `strlen`. **There is no per-glyph helper**
 *    -- the table search is inline, over FONT+0x0c with a count at FONT+0x18
 *    and a second table at FONT+0x48;
 *  - the accumulated value is multiplied by the float at **FONT+0x14** once,
 *    at the end (`vldr s15,[r4,#0x14]` / `vmul.f32 s15,s14,s15` at 0xaec88).
 *
 * What is not established is the layout of those tables, and therefore what
 * the loop accumulates per character. Writing a body here would mean inventing
 * it, so the body is deliberately absent. An earlier draft of this file did
 * invent a `limeFontAdvance()` helper and `tools/symcheck.py` rejected it --
 * which is the whole reason that check exists.
 */
/* The body is still not written -- see the fuller note further down this file,
 * which records what limeCreateFONT since established about the surrounding
 * format. One correction to the paragraph above: the array the loop accumulates
 * is at **FONT+0x24**, not FONT+0x0c. `ldr r1, [r4, #0x24]` at 0xaec48 is the
 * read, and it is the third of the three planar metric arrays limeCreateFONT
 * fills. FONT+0x0c is a separate field copied straight from the caller. */


/* --------------------------------------------- limeGetStringWidthUCNoHeader
 *
 * armv6 0x000ae8ec, 312 bytes.  **Not fully decompiled**, same reason.
 *
 * The same measurement for a string that is **known** to be UTF-16 and has no
 * BOM -- "UC" for Unicode, "NoHeader" for the missing mark. It walks pairs
 * from offset zero and stops on a zero pair, with no detection step at all.
 *
 * The pair exists because a BOM is a property of how a string was *stored*,
 * not of the string: text assembled at runtime has no reason to carry one.
 */
/* Same sum as limeGetStringWidth, with one difference that the name states:
 * **UC, no header**. It never tests for a byte-order mark and never takes the
 * narrow path -- `ldr ip, [r4, #0x4c]` is the only code table it touches, so
 * every string is UTF-16 by assumption rather than by detection.
 *
 * That is the routine for text the engine already knows the encoding of, where
 * the two-byte BOM would be an intruder rather than a marker. Callers that may
 * receive either encoding use limeGetStringWidth instead.
 *
 * Everything else matches: the space at `0x20` abandons the search and takes
 * the fallback, `+0x24` accumulates, `+0x28` adds signed kerning when present,
 * `+0x0c` adds the per-character spacing, `+0x2c` serves simple fonts, and the
 * integer total is scaled once by `+0x14` at the end.
 */
float limeGetStringWidthUCNoHeader(const FONT *font, const char *text)
{
    const char *p = text;
    int total = 0;

    if (text == NULL)
        return 0.0f;

    /* no BOM test, no narrow path: UTF-16 throughout */
    while (p[0] != '\0' || p[1] != '\0') {
        int index = -1;
        int i;

        if ((uint8_t)p[0] == 0x20 && p[1] == '\0') {
            total += font->spacing + font->fallbackAdvance + font->extraUnknown;
            p += 2;
            continue;
        }

        for (i = 0; i < font->numGlyphs; i++) {
            if ((uint8_t)((const uint8_t *)font->codesW)[i * 2]     == (uint8_t)p[0] &&
                (uint8_t)((const uint8_t *)font->codesW)[i * 2 + 1] == (uint8_t)p[1]) {
                index = i;
                break;
            }
        }

        total += font->spacing;                  /* every path, as above */

        if (index < 0)
            total += font->fallbackAdvance + font->extraUnknown;
        else if (font->simple)
            total += font->defaultAdvance;
        else {
            total += font->glyphWidth[index];
            if (font->kerning != NULL)
                total += font->kerning[index];
        }

        p += 2;
    }

    return (float)total * font->field14;   /* already a float; no cast */
}


/* ------------------------------------------------------------ limeCreateFONT
 *
 * armv6 0x000af83c, 756 bytes.  **Complete, and it decodes the font format.**
 *
 * Builds a FONT from up to two texture atlases and one metrics file. This is
 * the function that unblocks this whole file: every other routine here indexes
 * tables that only this one fills in.
 *
 * ## The metrics file
 *
 * A three-byte header, then planar arrays. No magic number, no version.
 *
 * ```
 *   byte 0   glyph count, low 8 bits
 *   byte 1   bit 0      -- the SIMPLE flag, stored INVERTED at FONT+0x04
 *            bits 1..7  -- glyph count, high bits, read SIGNED then >> 1
 *   byte 2   -> FONT+0x08
 *   byte 3.. numGlyphs character codes, one byte each
 * ```
 *
 * The count is reassembled as `byte0 + ((int8_t)byte1 >> 1) << 8`, so the low
 * bit of byte 1 is stolen for the flag and the rest is the high byte of a
 * signed 15-bit count. Reading byte 1 unsigned gives a count that is wrong
 * only for fonts past 128 glyphs, which is the kind of bug that survives
 * testing on a Latin character set and breaks on Korean.
 *
 * ## Two shapes after the header
 *
 * The character codes are always read, into `FONT+0x48` as bytes and widened
 * into `FONT+0x4c` as `{code, 0}` pairs -- that is, **the same codes as
 * 16-bit values**, which is what the UTF-16 path in limeGetStringWidth
 * searches. One table, two widths, built once at load.
 *
 * Then the flag decides:
 *
 *  - **simple** -- one more byte, a single fallback advance, into `FONT+0x2c`.
 *    Every glyph is that wide.
 *  - **not simple** -- three more planar arrays of `numGlyphs` entries each,
 *    read in order into `FONT+0x1c`, `FONT+0x20` and `FONT+0x24`, and always
 *    stored as `int16` regardless of how they were read.
 *
 * ## The `wide` argument
 *
 * The metric arrays are read as **signed bytes or as halfwords** depending on
 * an argument, with the file cursor advancing by 1 or 2 to match:
 *
 *      ldrsbeq  r3, [r5, r6]       ; narrow: signed byte
 *      ldrhne   r1, [r5, r6]       ; wide:   halfword
 *      addeq    r6, r6, #1
 *      addne    r6, r6, #2
 *
 * So **the file does not say how wide its own metrics are** -- the caller
 * does. Load a font with the wrong flag and it parses without error and
 * produces garbage. Any tool reading these files needs the flag from the call
 * site, not from the data.
 *
 * ## What the third array is
 *
 * `FONT+0x24` is the **advance width**: it is the only one of the three that
 * limeGetStringWidth accumulates. The other two are recorded by offset and not
 * named here -- for a texture-atlas font they are most likely the glyph
 * position and size, but this function does not use them and naming them on a
 * guess would be worse than leaving them numbered.
 *
 * `FONT+0x10` is set to the constant **8** before anything is read, and
 * limeGetStringWidth falls back to it when the glyph count is zero. So a font
 * that fails to load still measures text, at 8 units per character, instead of
 * returning zero and collapsing every layout to a point.
 *
 * ## The last argument is a FLOAT, and the caller is what proves it
 *
 * `str r3, [r4, #0x14]` moves the stack argument into FONT+0x14 without
 * converting it, and FONT+0x14 is read back with `vldr`. From inside this
 * function alone that is ambiguous -- a raw word store says nothing about what
 * the word means. `Task_LoadGeneralData` (decomp/gamecode/GameCode.c) settles
 * it: it builds the three fonts of the game and passes `0x3ea66666` for the
 * game font and `0x3f800000` for the other two, which are 0.325f and 1.0f.
 * They are float literals, so the parameter is a float. Typed `int`, the clean
 * C would have converted 1051931443 to a float and every glyph in the game
 * would have measured out at that scale.
 *
 * The file buffer is freed before returning.
 */
void limeCreateFONT(const char *tex0, const char *tex1, const char *metrics,
                    FONT *font, int spacing, int height, int width,
                    int wide, float scale)
{
    const uint8_t *data;
    int i, n, cursor;

    font->field14 = scale;              /* +0x14, the scale applied at the end */
    font->spacing = spacing;            /* +0x0c */
    font->fallbackAdvance = 8;          /* +0x10, used when nothing loaded */
    font->simple = 0;                   /* +0x04 */
    /* Named by what limeDrawFONT divides by, not by argument order: +0x34
     * normalises the horizontal metrics and +0x38 the vertical. The parameter
     * names are kept as the caller's to show the two are not the same thing. */
    font->atlasWidth  = (float)height;  /* +0x34 */
    font->atlasHeight = (float)width;   /* +0x38 */

    font->texture0 = limeLoadTexture(tex0, 0, 2);       /* +0x50 */
    if (tex1 != NULL)
        font->texture1 = limeLoadTexture(tex1, 0, 2);   /* +0x54 */

    data = limeLoadFile(metrics);
    if (data == NULL)
        return;

    n = data[0] + (((int8_t)data[1] >> 1) << 8);        /* 15-bit, packed */
    font->simple = ((data[1] & 1) != 0) ? 0 : 1;        /* stored inverted */
    font->numGlyphs = (int16_t)n;                       /* +0x18 */
    font->glyphHeight = data[2];                            /* +0x08 */

    font->codes  = limeMalloc("font", n);               /* +0x48, bytes */
    font->codesW = limeMalloc("font", n * 2);           /* +0x4c, int16 */

    cursor = 3;
    for (i = 0; i < n; i++) {
        font->codes[i] = data[cursor++];
        font->codesW[i] = font->codes[i];               /* {code, 0} */
    }

    if (font->simple) {
        font->defaultAdvance = (int8_t)data[cursor];    /* +0x2c */
        limeFree((void *)data);
        return;
    }

    font->atlasU = limeMalloc("font", n * 2);          /* +0x1c */
    font->atlasV = limeMalloc("font", n * 2);          /* +0x20 */
    font->glyphWidth = limeMalloc("font", n * 2);          /* +0x24 */

    /* Three planar passes, filling +0x1c, +0x20 and +0x24 in that order.
     *
     * The THIRD one is the advance width. limeGetStringWidth reads
     * `ldr r1, [r4, #0x24]` at 0xaec48 and that is the array it accumulates --
     * which is what names it, and what an earlier version of this file got
     * backwards by assuming the middle array was the interesting one. */
    for (i = 0; i < n; i++) {
        font->atlasU[i] = wide ? (int16_t)*(const uint16_t *)(data + cursor)
                                : (int16_t)(int8_t)data[cursor];
        cursor += wide ? 2 : 1;
    }
    for (i = 0; i < n; i++) {
        font->atlasV[i] = wide ? (int16_t)*(const uint16_t *)(data + cursor)
                                : (int16_t)(int8_t)data[cursor];
        cursor += wide ? 2 : 1;
    }
    for (i = 0; i < n; i++) {
        font->glyphWidth[i] = wide ? (int16_t)*(const uint16_t *)(data + cursor)
                                : (int16_t)(int8_t)data[cursor];
        cursor += wide ? 2 : 1;
    }

    limeFree((void *)data);
}


/* -------------------------------------------------------- limeGetStringWidth
 *
 * armv6 0x000aeae4, 600 bytes.  **Partially decompiled -- see below.**
 *
 * Measures a string in the font.
 *
 * Three things are established and worth having even without the body.
 *
 * **It detects UTF-16 from a byte-order mark.** The first two bytes are read
 * signed and compared against -1 and -2, which is `0xFF 0xFE` -- a little-
 * endian BOM. When it matches, the cursor skips two bytes and a flag switches
 * the whole loop to 16-bit characters:
 *
 *      ldrsb    r3, [sl]
 *      cmn      r3, #1              ; == 0xFF ?
 *      ldrsbeq  r3, [sl, #1]
 *      cmneq    r3, #2              ; == 0xFE ?
 *
 * This is the same runtime detection the project already documented for game
 * text, now confirmed inside the font code itself. Both encodings travel as
 * `const char *`, so a port cannot decide the encoding from the type.
 *
 * **`FONT+0x24` is the advance width.** Of the three metric arrays
 * limeCreateFONT fills, this is the only one this function reads, and it is
 * what accumulates. That is what names the field.
 *
 * **The total is scaled, not summed.** The accumulated integer is converted to
 * float and multiplied by `FONT+0x14`, the value passed to limeCreateFONT:
 *
 *      vcvt.f32.s32 s14, s15
 *      vldr         s15, [r4, #0x14]
 *      vmul.f32     s15, s14, s15
 *
 * So metrics are stored in whatever units the font was authored in and scaled
 * at measure time. A port must apply the same scale or every layout drifts.
 *
 * ## The glyph search, settled
 *
 * The two code tables limeCreateFONT builds are not redundant -- the search
 * picks one by encoding:
 *
 *      cmp     r5, #0              ; the UTF-16 flag
 *      beq     <narrow>            ; -> FONT+0x48, one signed byte per glyph
 *      ldr     r1, [r4, #0x4c]     ; wide  -> FONT+0x4c, two bytes per glyph
 *      ldrb    fp, [r1, r2, lsl #1]
 *      ...
 *      ldrbeq  r3, [lr, r2, lsl #1]  ; and the HIGH byte must match too
 *
 * A narrow string compares one byte; a wide string compares both halves and
 * only counts a hit when both agree. That is the whole reason the codes are
 * stored twice at load: one table per encoding, chosen per string.
 *
 * ## A space is deliberately not found
 *
 *      cmp      r3, #0x20
 *      moveq    r2, sb              ; end the loop
 *      mvneq    r0, #-1             ; report "no glyph"
 *
 * The search abandons itself on a space and returns the not-found index on
 * purpose, so a space takes the fallback advance rather than a glyph width.
 * A font atlas has no cell for it, and drawing one would emit a stray quad --
 * limeDrawFONT has the matching special case.
 *
 * ## What accumulates
 *
 *      total += spacing + glyphWidth[i]
 *      if (kerning) total += kerning[i]
 *
 * `FONT+0x28` is a per-glyph SIGNED byte added to the width when the pointer is
 * non-null -- kerning, and the field this file previously recorded as padding.
 * Nothing in limeCreateFONT was seen to allocate it, so a font without kerning
 * leaves it null and every glyph keeps its plain width.
 *
 * The whole sum is integer until the end, then converted once and multiplied by
 * `FONT+0x14`. Rounding therefore happens **after** the sum, not per glyph, and
 * a port that scales each advance individually accumulates a different error.
 */
float limeGetStringWidth(const FONT *font, const char *text)
{
    int wide = (text != NULL &&
                (uint8_t)text[0] == 0xFF && (uint8_t)text[1] == 0xFE);
    const char *p = text;
    int total = 0;

    if (text == NULL)
        return 0.0f;                    /* a width of nothing, not an error */

    if (wide)
        p += 2;                         /* step over the byte-order mark */

    while (*p != '\0') {
        int code = (int)(int8_t)*p;
        int index = -1;
        int i;

        if (code == 0x20) {             /* space: not searched for */
            total += font->spacing + font->fallbackAdvance + font->extraUnknown;
            p += wide ? 2 : 1;
            continue;
        }

        for (i = 0; i < font->numGlyphs; i++) {
            if (wide) {
                if ((int8_t)font->codesW[i] == code &&
                    ((const uint8_t *)font->codesW)[i * 2 + 1] == (uint8_t)p[1]) {
                    index = i;
                    break;
                }
            } else if ((int8_t)font->codes[i] == code) {
                index = i;
                break;
            }
        }

        /* **The spacing is added on ALL THREE paths**, not only when a glyph is
         * found. An earlier version added it once, inside the found branch,
         * which measured every unknown character and every space short by
         * exactly the spacing -- invisible in English and cumulative in
         * anything that leans on the fallback.
         *
         * The not-found path also adds FONT+0x3c, a second allowance the other
         * two do not touch:
         *     ldr r3, [r4, #0x10]   ; the fallback
         *     ldr r2, [r4, #0x3c]   ; and this
         *     add r3, r3, r2
         */
        total += font->spacing;                  /* +0x0c, every character */

        if (index < 0)
            total += font->fallbackAdvance       /* +0x10 */
                   + font->extraUnknown;         /* +0x3c, not-found only */
        else if (font->simple)
            total += font->defaultAdvance;       /* +0x2c */
        else {
            total += font->glyphWidth[index];    /* +0x24 */
            if (font->kerning != NULL)
                total += font->kerning[index];   /* +0x28, signed */
        }

        p += wide ? 2 : 1;
    }

    /* converted once, at the end -- not per glyph */
    return (float)total * font->field14;   /* already a float; no cast */
}


/* -------------------------------------------------------------- limeDrawFONT
 *
 * armv6 0x000af358, 1252 bytes.  **Structurally complete -- and it names the
 * two arrays FONT-FORMAT.md could not.**
 *
 * Draws a string. The largest function in this file and the one every other
 * font routine exists to serve.
 *
 * ## The three metric arrays are read together, per glyph
 *
 *      ldr r2, [r4, #0x24]     ; advance
 *      ldr r2, [r4, #0x1c]     ; metricA
 *      ldr r2, [r4, #0x20]     ; metricB
 *
 * Three loads in eleven instructions, inside the per-character loop, and then:
 *
 *      vldr s9,  [r4, #0x34]   ; the font's cell height
 *      vldr s13, [r4, #0x38]   ; the font's cell width
 *      ...
 *      bl   _limeDrawSprite
 *
 * So all three per-glyph values plus the font's cell dimensions feed one
 * sprite call. That is an **atlas lookup**: the glyph is a rectangle in the
 * texture, and three int16 per glyph plus a fixed cell size is exactly what
 * locating it takes.
 *
 * `+0x24` is already known to be the advance, because limeGetStringWidth
 * accumulates it and nothing else. **Which of `+0x1c` and `+0x20` is the atlas
 * offset and which is the drawn width is not settled here** -- both are loaded
 * into the same register two instructions apart and their order into
 * limeDrawSprite runs through float conversions this pass did not follow. They
 * stay numbered. What *is* now established is that they are atlas geometry
 * rather than kerning or line metrics, which is more than FONT-FORMAT.md could
 * say before.
 *
 * ## Four alignment modes
 *
 *      cmp r5, #0 / #1 / #2 / #3
 *
 * A four-way branch on an argument, taken before the loop and after a call to
 * limeGetStringWidth whose result is scaled by `FONT+0x14`. Measuring the whole
 * string before drawing it is what centring and right-alignment require, so
 * the modes are almost certainly left, centre, right and one more. The fourth
 * is not identified and is not guessed.
 *
 * ## Space is special-cased, twice
 *
 *      cmp r3, #0x20
 *      cmp ip, #0x20
 *
 * Two separate comparisons against `' '` on two paths. A space advances the
 * cursor without a sprite call -- which matters for a port, because a font
 * atlas usually has no glyph cell for it and drawing one would emit a stray
 * quad.
 *
 * ## The fallback chain
 *
 * When the glyph is not found the code reaches for `FONT+0x2c` (the SIMPLE
 * font's single advance) and then `FONT+0x10` (the constant 8 written before
 * the metrics file is even opened). So an unknown character still moves the
 * cursor, and a font that failed to load still lays text out at 8 units each.
 * Both fallbacks documented in FONT-FORMAT.md are reached from here.
 *
 * ## The three arrays, settled
 *
 * An earlier pass could say they were atlas geometry but not which was which.
 * What settles it is not what they are loaded from but **what they are divided
 * by** on the way to limeDrawSprite:
 *
 *      vdiv.f32 s15, s15, s9    ; [0x1c] / [0x34]   -> stack +0x04
 *      vdiv.f32 s15, s11, s13   ; [0x20] / [0x38]   -> stack +0x08
 *      vdiv.f32 s15, s10, s9    ; [0x24] / [0x34]   -> stack +0x0c
 *      vdiv.f32 s15, s12, s13   ; [0x08] / [0x38]   -> stack +0x10
 *
 * Four normalised texture coordinates. Two are divided by `+0x34` and two by
 * `+0x38`, and **a shared divisor means a shared axis**. `+0x24` is already
 * known to be the advance, because limeGetStringWidth sums it and nothing else,
 * so its axis is the horizontal one -- and that fixes every remaining field at
 * once:
 *
 * | field | is |
 * |---|---|
 * | `+0x1c` | the glyph's **x** position in the atlas |
 * | `+0x20` | the glyph's **y** position |
 * | `+0x24` | the glyph's **width**, which is also its advance |
 * | `+0x08` | the glyph **height** -- one value for all glyphs, from the header |
 * | `+0x34` | the atlas **width** |
 * | `+0x38` | the atlas **height** |
 *
 * That `+0x08` is a single header byte rather than an array is the confirmation
 * rather than a loose end: a font whose glyphs all share a height needs to store
 * it once, and this format does.
 *
 * The two constants at `+0x34` and `+0x38` were named the other way round by an
 * earlier pass, purely from the order of limeCreateFONT's arguments. The divisor
 * is the evidence; argument order is not.
 *
 * ## Which atlas a glyph lives in is decided by its y position
 *
 *      vcmp.f32 s13, s11        ; atlasHeight vs the glyph's y
 *      movhi    r3, #0x50       ; still inside  -> texture0
 *      movls    r3, #0x54       ; past the end  -> texture1
 *      ldr      r0, [r4, r3]
 *
 * So the second texture is not a separate style or a fallback: **it is the
 * overflow of the first**. Glyphs are laid out downward, and one that would run
 * past the bottom of atlas 0 is in atlas 1 instead, at a y that keeps counting.
 * That is why limeCreateFONT takes two texture names and loads the second only
 * if it is given one -- a font small enough to fit needs no second page.
 *
 * A port must reproduce the comparison, not just load both textures. Binding
 * atlas 0 for everything renders the tail of a large character set as garbage
 * from the wrong page.
 *
 * ## What is still not written out
 *
 * The glyph search that turns a character code into `i`, and the alignment
 * arithmetic that positions the run, are not transcribed -- they interleave
 * across four float registers with several early exits. The body below is the
 * per-glyph draw, which is the part the evidence covers.
 */
void limeDrawFONT(FONT *font, const char *text, float x, float y,
                  long alignment, float scale, const float *colour)
{
    /* armv7 0x0007e5b8, transcribed whole. The earlier body (from armv6)
     * read every string as UTF-16; the armv7 function reads ASCII unless the
     * string starts with the FF FE byte-order mark, and the HUD hands it
     * ASCII -- CharacterNames[model] straight (0x28658) -- so every fighter's
     * name came out as stray letters. Also from the binary: no half-texel
     * on the UVs (the literal added is 0.0f, 0x7e9cc).
     *
     * With `colour` NULL the original does not draw: it appends each glyph's
     * quad to a batch inside the font (+0x58 onward, counted at +0x18058).
     * That batch is not part of FONT here, and no caller in this tree passes
     * NULL, so that path is left out and draws nothing. */
    const unsigned char *t = (const unsigned char *)text;
    float k, w;
    size_t len, end, step, at;
    int unicode = 0;
    int unknown = 0;

    if (text == NULL)
        return;

    w = limeGetStringWidth(font, text);
    k = scale * font->field14;                  /* d9 */

    len = strlen(text);
    if (len > 1 && t[0] == 0xff && t[1] == 0xfe) {
        t += 2;
        unicode = 1;
    }

    if (alignment == 1)
        x += -0.5f * (w * scale);
    else if (alignment == 2)
        x -= w * scale;
    if (alignment != 3) {
        font->extraUnknown = 0;                 /* +0x3c */
        font->extraAtCount = 0;                 /* +0x40 */
    }

    if (!unicode) {
        end = strlen((const char *)t);
        step = 1;
    } else {
        for (end = 0; t[end] != 0 || t[end + 1] != 0; end += 2)
            ;
        step = 2;
    }

    for (at = 0; at < end; at += step) {
        int8_t lo = (int8_t)t[at];
        uint8_t hi = unicode ? t[at + 1] : 0;
        int n = font->numGlyphs, i, idx = -1;

        for (i = 0; i < n; i++) {
            if (!unicode) {
                if ((int8_t)font->codes[i] == lo) {
                    idx = (lo == 0x20) ? -1 : i;
                    break;
                }
            } else {
                const uint8_t *cw = (const uint8_t *)&font->codesW[i];
                if ((int8_t)cw[0] == lo && cw[1] == hi) {
                    idx = i;
                    if ((lo == (int8_t)0xa0 || lo == 0x20) && hi == 0)
                        idx = -1;               /* a space, plain or hard */
                    break;
                }
            }
        }

        if (idx < 0) {
            /* not drawn: the fallback advance, then the extras */
            x += (float)(font->spacing + font->fallbackAdvance) * k;
            x += (float)font->extraUnknown * k;
            unknown++;
            if (font->countForExtra == unknown)
                x += (float)font->extraAtCount * k;
            continue;
        }

        {
            int   gw;
            float pu, pv;

            if (font->simple != 0) {            /* +0x04: a fixed-width strip */
                gw = font->defaultAdvance;
                pu = (float)(gw * idx);
                pv = 0.0f;
            } else {
                gw = font->glyphWidth[idx];
                pu = (float)font->atlasU[idx];
                pv = (float)font->atlasV[idx];
            }

            if (colour != NULL) {
                float gh = (float)font->glyphHeight;
                TEXTURE *page = (font->atlasHeight > pv) ? font->texture0
                                                          : font->texture1;
                limeDrawSprite(page, x, y, (float)gw * k, gh * k,
                               pu / font->atlasWidth + 0.0f,
                               pv / font->atlasHeight + 0.0f,
                               (float)gw / font->atlasWidth,
                               gh / font->atlasHeight, colour);
            }

            if (font->simple != 0) {
                x += (float)(font->spacing + font->defaultAdvance) * k;
            } else {
                x += (float)(font->glyphWidth[idx] + font->spacing) * k;
                if (font->kerning != NULL)
                    x += (float)font->kerning[idx] * k;
            }
        }
    }
}


/* ------------------------------------------------------- limeDrawFONTAtAngle
 *
 * armv6 0x000aeda4, 1460 bytes.  **Structurally complete.**
 *
 * limeDrawFONT with the run rotated about Z. It calls exactly six functions,
 * and the list is the whole design:
 *
 *      _RotMatrixZ                     ; build the rotation from the angle
 *      _RotVector                      ; rotate the starting position
 *      _limeGetStringWidth             ; measure, for alignment
 *      _strlen  x2
 *      _limeDrawRotSpriteFromTopLeft   ; and draw each glyph through it
 *
 * Everything between is the same machinery as the unrotated version: the same
 * four alignment cases, the same glyph search over `+0x48` and `+0x4c`, the same
 * `0x20` space case, and the same three metric arrays at `+0x24`, `+0x1c` and
 * `+0x20`. The atlas lookup does not change -- rotation happens in the
 * destination, not in the texture.
 *
 * **`RotMatrixZ` and `RotVector` are both in Matrix.cpp and both verified**
 * against the original, so the rotation half of this function rests on code
 * that has already been run against the binary rather than on a reading.
 *
 * It draws through `limeDrawRotSpriteFromTopLeft` rather than `limeDrawSprite`.
 * The name is doing work: the unrotated path anchors wherever its own sprite
 * call anchors, and this one is explicitly **top-left**, which is the corner a
 * rotation has to be applied about for a run of glyphs to stay a straight line.
 * A port that rotates about the centre of each glyph produces text that fans
 * out instead of tilting.
 *
 * The alignment arithmetic and the glyph search are not transcribed, for the
 * same reason as in limeDrawFONT: they interleave across the float registers
 * with several early exits.
 */
void limeDrawFONTAtAngle(FONT *font, const char *text, float x, float y,
                         long alignment, float scale,
                         const float *colour, float angle)
{
    const float half = 0.5f;
    limeMATRIX44 rot;
    limeVECTOR3 pos;
    const char *p;
    float pen = 0.0f;                   /* distance along the rotated line */
    float rad, ca, sa;

    if (font == NULL || text == NULL || font->codesW == NULL)
        return;

    /* **The angle is in TURNS.** Not degrees, not radians. The binary's first
     * act is to multiply it by a literal pi and then double the result:
     *
     *      vldr     s14, [pc, #0x298]      ; 3.14159274
     *      vmul.f32 s14, s28, s14          ; angle * pi
     *      vadd.f32 s14, s14, s14          ; and doubled -- angle * 2pi
     *      bl       RotMatrixZ
     *
     * so RotMatrixZ receives radians, which is what its cosf/sinf want.
     *
     * This was first written as degrees and the menu paid for it. The main
     * menu's five entries pass -0.021, -0.0021, 0.008, 0.023 and 0.015. As
     * turns those are -7.6, -0.8, 2.9, 8.3 and 5.4 degrees, which is the tilt
     * the button art is drawn with. Read as degrees they are a 360th of that:
     * text that looks perfectly straight and gives no hint anything is wrong.
     */
    rad = angle * 6.28318530717958647692f;      /* turns -> radians */

    RotMatrixZ(rot, rad);               /* Matrix.cpp, verified */

    /* **The alignment shift runs along the TILTED line, not along x.**
     * Transcribed from the dispatch at 0x0007e154 -- the thing issue #27 said
     * was missing. `limeGetStringWidth` answers in r0 and is moved into a float
     * register whole (`vmov s22, r0`, so the return is raw bits, soft-float),
     * then multiplied by the caller's scale:
     *
     *      0x7e12e  bl       limeGetStringWidth
     *      0x7e13a  vmov     s22, r0          ; s22 = the width
     *      0x7e15a  vmul.f32 d7, d11, d13     ; s14 = s22 * s26 = width * scale
     *
     * and the two non-zero cases then shift the pen along the ROTATED
     * direction `RotVector` left at sp+0x68 and sp+0x6c -- the same unit vector
     * (1,0,0) the anchor note above describes:
     *
     *      alignment 2, at 0x7e59c:
     *          vldr     s12, [sp, #0x68]      ; dir.x
     *          vmls.f32 s18, s14, s12        ; penX -= width*scale * dir.x
     *          vldr     s12, [sp, #0x6c]      ; dir.y
     *          vmls.f32 s20, s14, s12        ; penY -= width*scale * dir.y
     *
     *      alignment 1, at 0x7e57a:
     *          vmov.f32 s12, #-0.5
     *          vmul.f32 d6, d7, d6           ; s12 = width*scale * -0.5
     *          ...                            ; then the same two axes
     *
     * So centring and right-alignment move the pen BACK ALONG THE TEXT, which
     * on a line tilted by a few degrees is not the same as moving it left. The
     * old code moved it left, and that is where the residual error in issue #27
     * came from: the anchors were already exact, and so was the width.
     *
     * `(ca, sa)` below is that same direction -- the glyph loop already
     * advances along it -- so it is computed here rather than twice. */
    ca = (float)cos((double)rad);
    sa = (float)sin((double)rad);

    if (alignment == 1 || alignment == 2) {
        float w = limeGetStringWidth(font, text) * scale;

        if (alignment == 1)
            w *= 0.5f;

        x -= w * ca;
        y -= w * sa;
    }

    /* **The ANCHOR is not rotated.** What goes through the matrix is the unit
     * vector (1,0,0) -- the direction the glyphs advance along:
     *
     *      add   r0, sp, #0x28          ; the matrix RotMatrixZ just built
     *      add   r1, sp, #0x74          ; in
     *      add   r2, sp, #0x68          ; out
     *      mov.w r3, #0x3f800000
     *      str   r3, [sp, #0x74]        ; in = (1, 0, 0)
     *      movs  r3, #0
     *      str   r3, [sp, #0x78]
     *      str   r3, [sp, #0x7c]
     *      bl    RotVector
     *
     * so the caller's x and y are where the text starts, full stop, and the
     * angle only decides which way it runs from there.
     *
     * This was first written rotating `pos` about the screen origin, which
     * drags every line left and up in proportion to how far down the screen it
     * is. On the main menu that put OPTIONS within a pixel of right and
     * MORE GAMES twenty-eight pixels short: 167 became 139, and 190 became
     * 152 for EXTRAS. The reference build puts them at 167 and 189.
     */
    pos.x = x; pos.y = y; pos.z = 0.0f;

    /* The advance runs along the tilted line, so it is added as a rotated
     * offset rather than to pos.x -- through the same radians the matrix was
     * built from, not a second conversion. `ca` and `sa` were computed above,
     * because the alignment shift needs the same direction. */

    p = text;
    if ((uint8_t)p[0] == 0xff && (uint8_t)p[1] == 0xfe)
        p += 2;                         /* the byte-order mark limeUC writes */

    while (p[0] != '\0' || p[1] != '\0') {
        int index = -1;
        int i;
        float gx, gy;

        if ((uint8_t)p[0] == 0x20 && p[1] == '\0') {
            pen += (float)(font->spacing + font->fallbackAdvance
                           + font->extraUnknown) * font->field14 * scale;
            p += 2;
            continue;
        }

        for (i = 0; i < font->numGlyphs; i++) {
            if ((uint8_t)((const uint8_t *)font->codesW)[i * 2]     == (uint8_t)p[0] &&
                (uint8_t)((const uint8_t *)font->codesW)[i * 2 + 1] == (uint8_t)p[1]) {
                index = i;
                break;
            }
        }

        if (index < 0) {
            pen += (float)(font->spacing + font->fallbackAdvance
                           + font->extraUnknown) * font->field14 * scale;
            p += 2;
            continue;
        }

        gx = pos.x + pen * ca;
        gy = pos.y + pen * sa;

        i = index;
        {
        /* The half is a half-TEXEL and belongs inside the division: adding it
         * after divides puts the sample half an atlas away, which on a 1024
         * sheet is 512 texels and drew fragments of the wrong glyphs. It is
         * the standard offset to the centre of a texel, so that a linear
         * filter reads the texel meant rather than the seam between two. */
        float u  = ((float)font->atlasU[i] + half) / font->atlasWidth;
        float v  = ((float)font->atlasV[i] + half) / font->atlasHeight;
        float du = (float)font->glyphWidth[i] / font->atlasWidth;
        float dv = (float)font->glyphHeight   / font->atlasHeight;

        TEXTURE *page = ((float)font->atlasV[i] < font->atlasHeight)
                        ? font->texture0
                        : font->texture1;

        /* The font's own scale, as in the unrotated path above. */
        limeDrawRotSpriteFromTopLeft(page, gx, gy,
                                     (float)font->glyphWidth[i]
                                         * font->field14 * scale,
                                     (float)font->glyphHeight
                                         * font->field14 * scale,
                                     u, v, du, dv, angle, colour);

        pen += (float)(font->spacing
                       + (font->simple
                          ? font->defaultAdvance
                          : font->glyphWidth[i]
                            + (font->kerning ? font->kerning[i] : 0)))
               * font->field14 * scale;
        }
        p += 2;
    }
}
