/*
 * text.c — src/gamecode/text.cpp (string handling)
 *
 * Eighteen functions in the original. These three are the primitives the rest
 * sit on, and they are worth having early because this engine's strings are
 * UTF-16 more often than they look -- limeFont detects the encoding at runtime
 * from a BOM and both encodings travel as `const char *`.
 *
 * Verified against the oracle by tests/test_text_diff.c.
 */

#include <stdint.h>
#include <string.h>   /* strlen, for asciiToUnicode */

uint32_t decodeLHWord(const char *p);
int      putUnicodeChar(char *dst, uint32_t ch);
long     strLenUnicode(const char *s);


/* ------------------------------------------------------------ decodeLHWord
 *
 * armv7 0x000a72f8, 12 bytes.  `__Z12decodeLHWordPKc`
 *
 *      ldrb r3, [r0, #1] ; ldrb r0, [r0]
 *      orr.w r0, r0, r3, lsl #8
 *
 * A little-endian sixteen-bit read, byte by byte -- "LH" is low-high. Both
 * loads are `ldrb` and not `ldrsb`, so neither half is sign-extended and the
 * result is a clean 0..0xFFFF.
 *
 * It reads the two bytes SEPARATELY rather than loading a halfword, which is
 * what makes it safe on an unaligned pointer. A port that "simplifies" this
 * into `*(uint16_t *)p` gets the same answer on x86 and faults on any target
 * that cares about alignment.
 */
uint32_t decodeLHWord(const char *p)
{
    return (uint32_t)(uint8_t)p[0] | ((uint32_t)(uint8_t)p[1] << 8);
}


/* ---------------------------------------------------------- putUnicodeChar
 *
 * armv7 0x000a7304, 12 bytes.
 *
 *      strb r1, [r0] ; lsrs r1, r1, #8 ; strb r1, [r0, #1] ; movs r0, #2
 *
 * The inverse of decodeLHWord, and it returns **2** -- the number of bytes
 * written, so callers can advance a cursor without knowing the encoding. Two
 * separate byte stores again, for the same alignment reason.
 */
int putUnicodeChar(char *dst, uint32_t ch)
{
    dst[0] = (char)(ch & 0xFFu);
    dst[1] = (char)((ch >> 8) & 0xFFu);
    return 2;
}


/* ----------------------------------------------------------- strLenUnicode
 *
 * armv7 0x000a7440, 28 bytes.
 *
 *  L:  ldrsb r3, [r2, r0]      ; s[i]
 *      cbnz  r3, next
 *      ldrsb r3, [r0 + r2, #1] ; s[i + 1]
 *      cbz   r3, done
 *  next: adds r2, #2 ; b L
 *  done: add.w r0, r2, r2, lsr #31 ; asrs r0, r0, #1
 *
 * **The terminator is a pair of zero bytes, not one.** The loop only stops
 * when BOTH halves of a code unit are zero, so a character like U+0041 --
 * `41 00` -- does not end the string even though its second byte is zero. A
 * length written as `strlen(s) / 2` would return 1 for a ten-character ASCII
 * string stored as UTF-16.
 *
 * The tail is the compiler's signed divide-by-two: add the sign bit, then
 * arithmetic-shift. `r2` is even and never negative here, so it is exactly
 * `r2 / 2`, and it is written that way rather than transcribing an idiom that
 * only exists because the compiler did not know the sign.
 *
 * The byte loads are `ldrsb` rather than `ldrb`. For a test against zero the
 * sign makes no difference, which is why it is safe to write this as a plain
 * comparison -- but it is the reason the disassembly looks like it cares.
 */
long strLenUnicode(const char *s)
{
    long i = 0;

    for (;;) {
        if (s[i] == 0 && s[i + 1] == 0)
            break;
        i += 2;
    }
    return i / 2;
}


/* --------------------------------------------------- CheckAllUnicodeCharsUsed
 *
 * armv7 0x000a72f4, 4 bytes: `bx lr`.
 *
 * Empty in the shipped build. The name says it was a coverage check over the
 * font's glyph table -- the sort of thing compiled out of a release -- and the
 * symbol survives because the linker kept the entry, not because anything runs.
 * It is written out rather than skipped so the file's function count matches
 * the binary's.
 */
void CheckAllUnicodeCharsUsed(void)
{
}


extern char **LanguageTextPtrs;         /* 0x003857d8, one pointer per string */
int  sprintf(char *dst, const char *fmt, ...);
long convertAsciiStringToUnicode(char *dst, const char *src);


/* ------------------------------------------------------------------ GameText
 *
 * armv7 0x000a72a8, 36 bytes.  **Complete.**
 *
 * The string table lookup. -1 is the miss and returns a pointer to an empty
 * string in __TEXT rather than NULL, which is why callers pass the result
 * straight to the font code without checking it.
 */
const char *GameText(long index)
{
    if (index == -1)
        return "";                      /* 0x00177f8c, an empty string */
    return LanguageTextPtrs[index];
}


/* ---------------------------------------------------------- GameTextNoHeader
 *
 * armv7 0x000a72cc, 40 bytes.  **Complete.**
 *
 * The same lookup with **two bytes skipped**, so each stored string carries a
 * two-byte header the drawing code sometimes wants and sometimes does not.
 *
 * The -1 path is not just GameText's: it loads a DIFFERENT empty string, four
 * bytes further on, and branches to the `bx lr` PAST the `adds r0, #2`. So the
 * miss case returns an unadjusted pointer in both functions, and adding the two
 * to it -- which a tidier-looking rewrite would -- walks off the end of a
 * one-byte string.
 */
const char *GameTextNoHeader(long index)
{
    if (index == -1)
        return "";                      /* 0x00177f90, and NOT advanced by 2 */
    return LanguageTextPtrs[index] + 2;
}


/* ------------------------------------------------------------------- copyInt
 *
 * armv7 0x000a75ac, 40 bytes.  **Complete.**
 *
 * Formats an integer and writes it out as UNICODE, returning the byte length:
 * `convertAsciiStringToUnicode` returns a character count and the result is
 * shifted left by one (`lsls r0, r0, #1`), so two bytes per character.
 *
 * The scratch buffer is 0x40 bytes on the stack and `sprintf` is unbounded.
 * Sixty-four bytes is far beyond any %d, so it cannot overflow here -- but the
 * shape is worth noting rather than silently "fixing" to snprintf, because the
 * return value is a length the caller uses and a truncating rewrite changes it.
 */
long copyInt(char *dst, long value)
{
    char buf[0x40];

    sprintf(buf, "%d", (int)value);
    return convertAsciiStringToUnicode(dst, buf) * 2;
}


/* ----------------------------------------------------------------- copyFloat
 *
 * armv7 0x000a75d4, 44 bytes.  **Complete.**
 *
 * copyInt with "%f". **The value arrives in a REGISTER PAIR**: the prologue
 * moves r1 into r2 and r2 into r3 before the call, which is a double being
 * shuffled into the varargs slots under the soft-float ABI. So the parameter is
 * a double, not a float -- C promotes it at the call site and this function
 * never sees the original.
 */
long copyFloat(char *dst, double value)
{
    char buf[0x40];

    sprintf(buf, "%f", value);
    return convertAsciiStringToUnicode(dst, buf) * 2;
}


/* ------------------------------------------- convertAsciiStringToUnicode
 *
 * armv7 0x000a7310, 56 bytes.  **Complete.**
 *
 * Widens ASCII to UTF-16LE by interleaving zero bytes, and returns the
 * CHARACTER count -- which is why every caller shifts it left by one to get
 * bytes.
 *
 * **The terminator is two bytes and it is written on both paths.** The empty
 * -source branch at 0xa733e skips the loop and still stores the pair, so an
 * empty input produces a well-formed empty UTF-16 string rather than nothing.
 * A rewrite that returns early on an empty source leaves the destination
 * uninitialised.
 */
long convertAsciiStringToUnicode(char *dst, const char *src)
{
    long n = 0;

    while (src[n] != 0) {
        dst[n * 2]     = src[n];
        dst[n * 2 + 1] = 0;
        n++;
    }
    dst[n * 2]     = 0;             /* strb [ip, r2] */
    dst[n * 2 + 1] = 0;             /* strb [r2, lr], lr = dst + 1 */
    return n;
}


extern long   UCPtr;                    /* 0x00178034 */
extern char  *usprintfBuffers[16];      /* 0x00178038 */


/* ---------------------------------------------------------------------- UC
 *
 * armv7 0x000a7400, 52 bytes.  **Complete.**
 *
 *      UCPtr = (UCPtr + 1 > 15) ? 0 : UCPtr + 1
 *      convertAsciiStringToUnicode(usprintfBuffers[UCPtr], s)
 *      return usprintfBuffers[UCPtr]
 *
 * **Sixteen rotating buffers, and the caller owns none of them.** This is the
 * idiom that lets `UC("...")` be used inline in a drawing call: the returned
 * pointer stays valid until sixteen more conversions have happened, and then it
 * is silently reused.
 *
 * A port must keep the count at sixteen. Any smaller and a caller that holds
 * several converted strings across one frame starts seeing them change under
 * it -- and the failure looks like a text bug, not a lifetime bug.
 *
 * The pointer is re-read from `UCPtr` after the conversion rather than kept in
 * a register, which is transcribed as written; nothing in this function can
 * change it, but the original does not assume that.
 */
char *UC(const char *s)
{
    UCPtr = (UCPtr + 1 > 15) ? 0 : UCPtr + 1;
    convertAsciiStringToUnicode(usprintfBuffers[UCPtr], s);
    return usprintfBuffers[UCPtr];
}




/* ----------------------------------------------------------- copyUnicodeString
 *
 * armv7 0x000a745c, 80 bytes.  **Complete.**
 *
 * Copies a UTF-16LE string, terminator NOT included, and returns the byte
 * count. The loop condition goes through `decodeLHWord`, so the terminator is
 * a two-byte zero read as a little-endian halfword rather than a byte test --
 * which is why a character whose low byte is zero does not end the string.
 *
 * **Both pointers are null-checked and the result is 0 for either.** The guard
 * is built branchlessly out of `rsbs`/`ite`, which is why it does not look like
 * a guard; what it computes is `if (!dst || !src) return 0`.
 *
 * The returned count excludes the terminator and the destination never receives
 * one. Callers that want a terminated string write it themselves.
 */
long copyUnicodeString(char *dst, const char *src)
{
    long n = 0;

    if (dst == 0 || src == 0)
        return 0;

    while (decodeLHWord(src + n) != 0) {
        dst[n]     = src[n];
        dst[n + 1] = src[n + 1];
        n += 2;
    }
    return n;                           /* no terminator is written */
}




/* ------------------------------------------------------------- asciiToUnicode
 *
 * armv7 0x000a78dc, 84 bytes.  **Complete.**
 *
 * Widens ASCII into a caller-supplied buffer, refusing when the buffer cannot
 * hold the result plus its terminator, and returning 1 on success and 0 on
 * refusal. Unlike convertAsciiStringToUnicode it DOES write the two-byte
 * terminator, and the size check is what makes the difference: this one knows
 * how much room it has.
 *
 * **strlen is called inside the loop condition.** Not hoisted, not cached: once
 * per character, so the function is quadratic in the length of the string. It
 * is called twice more afterwards to place the terminator.
 *
 * That is transcribed rather than fixed. The strings this handles are menu
 * labels of a few dozen characters, so the cost never mattered; hoisting it is
 * a one-line change anybody porting this can make deliberately, and doing it
 * silently would hide that the original did not.
 *
 * The bound is strictly greater -- dst_bytes > strlen*2 -- which is what leaves
 * room for the terminator the check does not otherwise account for.
 */
int asciiToUnicode(const char *src, char *dst, long dst_bytes)
{
    long i;

    if (dst_bytes <= (long)strlen(src) * 2)
        return 0;

    for (i = 0; (unsigned long)i < strlen(src); i++) {   /* not hoisted */
        dst[i * 2]     = src[i];
        dst[i * 2 + 1] = 0;
    }
    dst[strlen(src) * 2]     = 0;
    dst[strlen(src) * 2 + 1] = 0;
    return 1;
}


/* `_listOfTokens` -- 0x003877f0. Twelve bytes an entry: a type tag, then room
 * for eight bytes of value. The stride and the four tags come out of
 * initArguments' own jump table; nothing else in this tree describes them. */
typedef struct TOKEN {
    long type;                          /* 0x00  1..4, see below */
    union {
        long  word;                     /* 0x04  str  -- cases 1, 2 and 4 */
        short half;                     /* 0x04  strh -- case 3, low half only */
    } value;
    long value2;                        /* 0x08, only the 8-byte case uses it */
} TOKEN;

extern TOKEN listOfTokens[];            /* 0x003877f0 */

int printf(const char *fmt, ...);


/* ------------------------------------------------------------- initArguments
 *
 * armv7 0x000a7548, 100 bytes.  **Complete.**
 *
 * usprintf's varargs marshaller: walks `count` already-parsed tokens and pulls
 * each one's value off the caller's argument block according to its type. A
 * `tbb` dispatches on type - 1, and the four cases are:
 *
 *      1, 2   one word          value  = *cursor++          (4 bytes consumed)
 *      3      a halfword        low 16 bits only            (4 bytes consumed)
 *      4      TWO words         value  = cursor[0]
 *                               value2 = cursor[1]          (8 bytes consumed)
 *
 * The `tbb` table is `{3, 3, 20, 14}` and its base is 0x000a756a, so the targets
 * are base + 2*byte: 0xa7570, 0xa7570, 0xa7592 (`ldrh`) and 0xa7586 (`ldm`).
 * **Type 4 is the eight-byte one, not type 3** -- the table is not in address
 * order and reading it as though it were swaps the two.
 *
 * **Case 4 is the double, and this is the marshaller that proves it.** A double
 * arrives as a REGISTER PAIR under this binary soft-float ABI -- the fact the
 * runtime already depends on -- and here it is again from the other side: eight
 * bytes read, eight bytes stepped, two words stored. `getToken` closes the
 * loop: the token character that produces type 4 is `f`.
 *
 * **Case 3 stores a halfword into a word slot and leaves the top half alone.**
 * `strh r3, [r5, #4]`, not `str`. So a type-3 token reuses whatever the upper
 * 16 bits of that slot held from a previous call. It still steps the cursor by
 * a full 4, because the value was promoted to int on the way in.
 *
 * An unknown tag is not fatal: it prints
 *
 *      USPRINTF ERROR - WRONG ORDER OF ARGUMENTS! TOKEN = %d
 *
 * and carries on to the next token with the cursor NOT advanced -- which
 * desynchronises everything after it. The message says "wrong order" because
 * that is how it happens in practice: the tokens are parsed from the format
 * string and the values from the call, and they only line up if they match.
 *
 * Returns 0 always.
 */
long initArguments(long count, const long *args)
{
    long i;

    for (i = 0; i < count; i++) {
        TOKEN *t = &listOfTokens[i];

        switch (t->type) {
        case 1:
        case 2:
            t->value.word = *args++;
            break;

        case 3:                         /* halfword into the low half only */
            t->value.half = (short)*args;   /* strh: top half untouched */
            args++;
            break;

        case 4:                         /* a double: two words */
            t->value.word = args[0];
            t->value2     = args[1];
            args += 2;
            break;

        default:
            printf("USPRINTF ERROR - WRONG ORDER OF ARGUMENTS! TOKEN = %d\n",
                   (int)t->type);
            break;                      /* cursor deliberately not advanced */
        }
    }
    return 0;
}


/* `_foundToken` -- 0x003877e0. Three fields are written here; +8 is never
 * touched by this function and stays unnamed. */
typedef struct FOUNDTOKEN {
    long        type;                   /* 0x00  0 none, 1 's', 2 'd', 4 'f' */
    long        index;                  /* 0x04  0..7 from the digit, -1 if none */
    long        unknown;                /* 0x08 */
    const char *P32 at;                     /* 0x0c  the unit the type char sits on */
} FOUNDTOKEN;

extern FOUNDTOKEN foundToken;           /* 0x003877e0 */


/* ------------------------------------------------------------------ getToken
 *
 * armv7 0x000a74ac, 156 bytes.  **Complete.**
 *
 * Scans a UTF-16 string for the next `#<digit><type>` escape and leaves what it
 * found in `_foundToken`, returning the type:
 *
 *      #3d   ->  type 2, index 2      an int
 *      #1s   ->  type 1, index 0      a string
 *      #2f   ->  type 4, index 1      a double
 *
 * **These are the same type numbers `initArguments` dispatches on**, and `f`
 * mapping to 4 is what pins that function jump table down: type 4 is the
 * eight-byte case, so a float promoted to double in varargs. The two functions
 * are the two halves of one format, read from opposite ends.
 *
 * The digit is `c - '1'` kept only when it lands in 0..7, so the escapes are
 * numbered from ONE in the string and from zero in the table -- eight arguments
 * maximum, and `#9d` silently leaves the index at whatever it was.
 *
 * **It starts at `s + 2`, skipping the first 16-bit unit**, and steps two bytes
 * at a time through `decodeLHWord`. Nothing here says what that first unit
 * holds, only that no caller of this function ever sees it.
 *
 * ### Two vestigial branches
 *
 * The escape handler is reached only when the in-escape flag is set, and it
 * immediately re-tests the same flag; the arm for it being clear is
 * unreachable. Likewise `#` copies the in-escape flag into the done flag before
 * setting it, and at that point it is always zero. Neither is transcribed as an
 * `if`, because writing a branch that cannot be taken would be describing the
 * compiler output rather than the program.
 */
long getToken(const char *s)
{
    const char *p = s + 2;
    int inEscape = 0;
    uint32_t c;

    foundToken.type  = 0;
    foundToken.index = -1;

    while ((c = decodeLHWord(p)) != 0) {
        if (inEscape) {
            long d = (long)c - '1';
            if ((unsigned short)d <= 7)
                foundToken.index = d;
            inEscape = 0;
            p += 2;
            continue;
        }

        switch (c) {
        case '#':
            inEscape = 1;
            p += 2;
            continue;

        case 's':
            foundToken.type = 1;
            foundToken.at   = p;
            return 1;

        case 'd':
            foundToken.type = 2;
            foundToken.at   = p;
            return 2;

        case 'f':
            foundToken.type = 4;
            foundToken.at   = p;
            return 4;

        default:
            p += 2;
            continue;
        }
    }
    return foundToken.type;
}


/* ------------------------------------------------------------------- limeUC
 *
 * armv7 0x000a7348, 184 bytes.  **Complete.**
 *
 * Takes the next of the sixteen `_usprintfBuffers` and fills it with a
 * **byte-for-byte copy of the input, prefixed with a UTF-16LE BOM**:
 *
 *      buf[0] = 0xFF
 *      buf[1] = 0xFE
 *      buf[2..] = s[0..]           copied as 16-bit units until both bytes zero
 *
 * **It does not convert anything.** The name suggests ASCII to Unicode and the
 * file already has `convertAsciiStringToUnicode` for that; this one takes a
 * string that is ALREADY UTF-16LE and only puts the byte-order mark in front of
 * it. The loop reads s[i] and s[i+1] and writes them straight through.
 *
 * The terminator is the 16-bit zero: it stops when both bytes of a unit are
 * zero, and writes that pair out as well, so the result is terminated.
 *
 * **This is where getToken second byte comes from.** That function starts
 * scanning at `s + 2`, which is exactly the BOM this writes -- the two are the
 * producer and consumer of the same convention, and neither says so on its own.
 *
 * The buffer index advances first and wraps at 16, so sixteen live results can
 * be in flight before one is overwritten. The 0xFF/0xFE pair is produced from a
 * single -1 doubled, not from two constants.
 */
char *limeUC(const char *s)
{
    char *buf;
    long i;

    UCPtr++;
    if (UCPtr > 15)
        UCPtr = 0;

    buf = usprintfBuffers[UCPtr];
    buf[0] = (char)0xFF;                /* mov r2, #-1     */
    buf[1] = (char)0xFE;                /* adds r2, r2, r2 */

    for (i = 0; s[i] != 0 || s[i + 1] != 0; i += 2) {
        buf[i + 2] = s[i];
        buf[i + 3] = s[i + 1];
    }
    buf[i + 2] = 0;                     /* the 16-bit terminator */
    buf[i + 3] = 0;

    return usprintfBuffers[UCPtr];      /* re-read, not the cached pointer */
}


/* `_listOfTokens` is sixteen entries: usprintf clears 0xc0 bytes at a stride of
 * 0xc. `_g_usprintfSemaphore` -- 0x00178078. */
#define TOKEN_SLOTS  16

extern long g_usprintfSemaphore;        /* 0x00178078 */

int  puts(const char *s);

long strLenUnicode(const char *s);
/* r0 dst, r1 fmt, r2 len, r3 args, and the pass flag is the first STACK
 * argument -- read as a byte at sp+0x24. The pass flag comes last, not before
 * the argument block. */
long processString(char *dst, const char *fmt, long len, const long *args,
                   int pass);


/* ----------------------------------------------------------------- usprintf
 *
 * armv7 0x000a7754, 176 bytes.  **Complete.**
 *
 * The UTF-16 printf. Two passes over the format string with the marshaller
 * between them:
 *
 *      n = processString(dst, fmt, len, 1, &varargs)    <- pass 1, collects
 *      initArguments(n, &varargs)                       <- pulls the values
 *      processString(dst, fmt, len, 0, &varargs)        <- pass 2, emits
 *
 * **The token count comes out of pass one and goes straight into
 * initArguments** -- that is the `count` argument whose loop bound was
 * otherwise unexplained. The three functions getToken, initArguments and
 * processString are all halves of this one.
 *
 * Sixteen token slots, cleared two words at a time before pass one: the type
 * and the value, not `value2`. So a slot whose previous use was a double keeps
 * the high half of it.
 *
 * ### The semaphore that cannot clear
 *
 * On entry, while `g_usprintfSemaphore` is non-zero it prints
 *
 *      \n<<< Waiting for g_usprintfSemaphore! >>>
 *
 * and re-reads it, forever. Nothing in this function releases it and there is
 * no yield in the loop, so on a single thread this is a hang that spams the
 * console rather than a wait. It only ever means something if usprintf can be
 * re-entered -- from an interrupt, or from a second thread -- and it is
 * transcribed as the spin it is.
 *
 * Returns 0 always.
 */
long usprintf(char *dst, const char *fmt, ...)
{
    /* The varargs are pushed as a block by the prologue -- `push {r1, r2, r3}`
     * before the frame -- so `args` is that block, read as words. */
    const long *args = (const long *)((const char *)&fmt + sizeof fmt);
    long len, n;
    int i;

    while (g_usprintfSemaphore != 0)
        puts("\n<<< Waiting for g_usprintfSemaphore! >>>");

    g_usprintfSemaphore = 1;

    for (i = 0; i < TOKEN_SLOTS; i++) {
        listOfTokens[i].type       = 0;
        listOfTokens[i].value.word = 0;     /* value2 deliberately untouched */
    }

    len = strLenUnicode(fmt);
    n   = processString(dst, fmt, len, args, 1);
#ifdef UMK3_LOW32
    /* Port, not in the binary: the 64-bit build passes each argument in an
     * eight-byte slot, not the device's words. The first pass has typed the
     * tokens, so the slots are repacked into the device's layout: one word
     * for a string or a number, two for a double (its two halves). Every
     * address of the game fits the low word. */
    {
        static long words[2 * TOKEN_SLOTS];
        va_list ap;
        long w = 0;

        va_start(ap, fmt);
        for (i = 0; i < n && i < TOKEN_SLOTS; i++) {
            unsigned long long v = va_arg(ap, unsigned long long);

            words[w++] = (long)(uint32_t)v;
            if (listOfTokens[i].type == 4)
                words[w++] = (long)(uint32_t)(v >> 32);
        }
        va_end(ap);
        args = words;
    }
#endif
    initArguments(n, args);
    processString(dst, fmt, len, args, 0);

    g_usprintfSemaphore = 0;
    return 0;
}


extern char  *LanguageTextData;         /* 0x003857dc */
extern char **LanguageTextPtrs;         /* 0x003857d8 */

long  limeFileSize(const char *name);
void *limeLoadFile(const char *name);
void *limeMalloc(const char *tag, long size);
void  limeFree(void *p);


/* --------------------------------------------------------------- LoadTextData
 *
 * armv7 0x000a7804, 216 bytes.  **Complete.**
 *
 * Loads the language string table. The file is a small header followed by
 * length-prefixed UTF-16 strings:
 *
 *      +0  int16   header size, SUBTRACTED from the file size to get the payload
 *      +6  int16   how many strings
 *      +8          each string: uint16 byte-length, then that many bytes
 *
 * Two allocations, both tagged: `"languagedata"` for the payload and
 * `"languagetextptrs"` for `count` pointers. Then
 *
 *      printf("Num Text strings Loading: %d\n", count)
 *
 * **Every string is rewritten with a UTF-16LE BOM in front of it.** The
 * destination cursor advances by `2 + len` while the source advances by
 * `2 + len` as well -- two bytes of length prefix in, two bytes of BOM out --
 * so the two stay in step by coincidence of the same number, not by design.
 * A zero-length string still gets its BOM and still costs two bytes.
 *
 * That is the third place the FF FE prefix appears: limeUC writes it, getToken
 * skips it, and this loader bakes it into every string the game ships. Anything
 * reading `LanguageTextPtrs[i]` gets a pointer AT the BOM, not past it.
 *
 * The size of the payload allocation is `fileSize - header[0]`, which is the
 * file minus its own header field -- not minus the eight bytes actually
 * consumed before the first string. The two differ unless header[0] is 8, and
 * nothing here says it is.
 *
 * The raw file is freed at the end; the payload it was copied into is not.
 */
void LoadTextData(const char *file)
{
    long size = limeFileSize(file);
    const unsigned char *data = (const unsigned char *)limeLoadFile(file);
    const unsigned char *src;
    char *dst;
    long count, i;

    count = (short)(data[6] | (data[7] << 8));

    LanguageTextData = (char *)limeMalloc("languagedata",
                                          size - (short)(data[0] | (data[1] << 8)));
    /* One pointer an entry -- four bytes in the image, eight here. */
    LanguageTextPtrs = (char **)limeMalloc("languagetextptrs",
                                           count * sizeof(char *));

    printf("Num Text strings Loading: %d\n", (int)count);

    src = data + 8;
    dst = LanguageTextData;

    for (i = 0; i < count; i++) {
        long len = src[0] | (src[1] << 8);
        long k;

        src += 2;

        LanguageTextPtrs[i] = dst;
        dst[0] = (char)0xFF;            /* the BOM, again */
        dst[1] = (char)0xFE;

        for (k = 0; k < len; k++)
            dst[2 + k] = (char)src[k];

        src += len;
        dst += len + 2;
    }

    limeFree((void *)data);
}


/* `putUnicodeChar` and `copyFloat` are declared above. copyFloat takes a
 * double, which under this soft-float ABI is the register PAIR the call site
 * loads with `ldm r1, {r1, r2}` -- the two words of a type-4 token. */
long copyUnicodeString(char *dst, const char *src);
long copyInt(char *dst, long v);


/* -------------------------------------------------------------- processString
 *
 * armv7 0x000a7600, 340 bytes.  **Complete.**
 *
 * The body of `usprintf`, run twice over the same format string: `pass != 0`
 * collects the tokens, `pass == 0` emits the result. It is one function with
 * two jobs and the flag is the only thing separating them.
 *
 *      pass 1:  walk, call getToken on every '%', record each token's TYPE in
 *               listOfTokens[index], count them, and write nothing
 *      pass 0:  walk, and for each token emit its VALUE through copyInt /
 *               copyFloat / copyUnicodeString; everything else goes through
 *               putUnicodeChar
 *
 * Returns the token count, which is what `usprintf` hands to `initArguments`.
 *
 * ### The cursor is rebased, not advanced
 *
 * The scan is `base + i`, and after a token it does **`base = foundToken.at;
 * i = 2`** rather than adding to `i`. `foundToken.at` is where getToken left
 * off -- on the type character -- so this resumes two bytes past it. Written as
 * an advancing index it looks like a reset bug; it is not, and the base moves
 * with it.
 *
 * ### An escape without a digit gets the next index automatically
 *
 * `getToken` leaves `index` at -1 when the escape carried no `1`-`8` digit.
 * This function then substitutes a running counter, so `%d%d%d` numbers itself
 * 0, 1, 2 while `#3d` addresses slot 2 explicitly. **The counter advances on
 * every token either way**, explicit or not, so mixing the two forms shifts
 * the implicit ones.
 *
 * ### Two arguments it never reads
 *
 * `len` and the varargs pointer arrive in r2 and r3 and are not touched. The
 * walk ends on the 16-bit NUL from `decodeLHWord`, not on the length, and the
 * values come from `listOfTokens` rather than from the block -- `initArguments`
 * has already moved them there between the two passes.
 *
 * ### The duplicate-slot diagnostic
 *
 * In pass 1, if the slot a token names already has a type, it prints
 *
 *      WRONG ARGUMENT! TOKEN #%d
 *
 * and **still counts the token and still advances**. So two escapes addressing
 * the same index inflate the count `initArguments` will loop to, and the second
 * type is discarded rather than overwriting the first.
 *
 * Both passes finish with `putUnicodeChar(dst, 0)` -- the terminator is written
 * even on the collecting pass, where `dst` has not moved.
 */
long processString(char *dst, const char *fmt, long len, const long *args,
                   int pass)
{
    const char *base = fmt;
    long i = 0;
    long count = 0;
    long autoIndex = 0;

    (void)len;                          /* r2, never read */
    (void)args;                         /* r3, never read */

    for (;;) {
        const char *p = base + i;
        uint32_t c = decodeLHWord(p);

        if (c == 0)
            break;

        if (c != '%') {
            if (!pass)
                dst += putUnicodeChar(dst, c);
            i += 2;
            continue;
        }

        if (getToken(p) == 0) {
            /* a '%' that starts no token: step over it.  The shared tail
             * below still runs, but foundToken.type is 0, so it emits
             * nothing. */
            i += 2;
        } else {
            /* a real token (0xa7694) */
            base = foundToken.at;       /* rebase, and i restarts at 2 */

            if (foundToken.index == -1)
                foundToken.index = autoIndex;

            if (pass) {
                if (listOfTokens[foundToken.index].type != 0)
                    printf("WRONG ARGUMENT! TOKEN #%d\n",
                           (int)foundToken.index);  /* type NOT overwritten */
                else
                    listOfTokens[foundToken.index].type = foundToken.type;
            }

            count++;
            i = 2;
            autoIndex++;
        }

        /* 0xa7650 -- the tail both paths share: on the emitting pass, write
         * the value of whatever getToken found. */
        if (pass)
            continue;

        switch (foundToken.type) {
        case 2:
            dst += copyInt(dst, listOfTokens[foundToken.index].value.word);
            break;
        case 4: {
            /* ldm r1, {r1, r2}: the two words ARE the double. */
            union { struct { long lo, hi; } w; double d; } u;
            u.w.lo = listOfTokens[foundToken.index].value.word;
            u.w.hi = listOfTokens[foundToken.index].value2;
            dst += copyFloat(dst, u.d);
            break;
        }
        case 1:
            dst += copyUnicodeString(dst,
                    (const char *)(uintptr_t)(unsigned long)
                        listOfTokens[foundToken.index].value.word);
            break;
        default:
            break;
        }
    }

    putUnicodeChar(dst, 0);             /* written on both passes */
    return count;
}
