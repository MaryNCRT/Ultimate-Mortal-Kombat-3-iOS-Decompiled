/*
 * test_dsdebugwin_diff.c — clean DS_DebugWin.c against the oracle.
 *
 * The debug overlay's window state: clearing, scrolling and line advance.
 *
 * ## What made this test worth writing
 *
 * The window record size could not be read from the disassembly. The literal
 * pools in this module disassemble as `0xe12fff1e` — `bx lr` mistaken for
 * data — so the multiply's operand never resolved, and the decompiled comment
 * said so and refused to guess.
 *
 * The recompiler resolves it: `movw r3, #0xcf20`. Running ClearDebugWindow
 * against a poisoned arena then confirms it from the other side — window 1's
 * writes land at exactly +0xcf20, and clearing one window touches 102 words:
 * the two cursors plus a pair inside each of fifty 0x420-byte line records.
 *
 * So this test did not just check a body, it **measured a constant that reading
 * could not reach**. That is worth knowing about differential testing in
 * general: the oracle is a second, independent decoding of the same bytes, and
 * where one decoding is blind the other sometimes is not.
 *
 * ## What is compared
 *
 * The whole window, byte for byte, on both sides. There is no float in this
 * module and no data file — the state is the test.
 *
 * The guest reaches `_DebugWindows` through its non-lazy pointer slot, so the
 * test points that slot at an arena it controls and reads back through the same
 * offsets. The clean side is the array itself. Neither can see the other's.
 */

#include "arm_runtime.h"
#include "dsdebugwin.h"                 /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (16u << 20)
#define STACK_TOP  0x007F0000u

/* The pointer variable the oracle loads through, computed from the generated
 * code: `ldr r3, [pc, #0x30]` gives 0x000d0f1a, plus the pc of 0x000a7cba. */
#define WINPTR     0x00178BD4u
#define ARENA      0x00500000u

#define WINDOWS    4                    /* enough to prove the stride */

static int  g_fail  = 0;
static long g_cases = 0;

static DEBUGWINDOW *g_clean;

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static void call0(void (*f)(arm_ctx *))
{
    arm_ctx c; ctx_reset(&c); f(&c);
}

static void call1(void (*f)(arm_ctx *), uint32_t a0)
{
    arm_ctx c; ctx_reset(&c); c.r[0] = a0; f(&c);
}

/* Both sides to the same starting pattern, so a function that writes nothing is
 * distinguishable from one that writes zeros. */
static void seed(uint8_t byte)
{
    memset(g_clean, byte, (size_t)WINDOWS * DEBUG_WINDOW_SIZE);
    for (uint32_t a = 0; a < (uint32_t)WINDOWS * DEBUG_WINDOW_SIZE; a += 4u)
        MEM_ST32(ARENA + a, (uint32_t)byte * 0x01010101u);
}

static void cmp_windows(const char *what)
{
    const uint8_t *clean = (const uint8_t *)g_clean;
    g_cases++;
    for (uint32_t a = 0; a < (uint32_t)WINDOWS * DEBUG_WINDOW_SIZE; a++) {
        uint8_t o = MEM_LD8(ARENA + a);
        if (clean[a] != o) {
            printf("  DIVERGE %s  byte 0x%x (window %u, +0x%x): clean=0x%02x  oracle=0x%02x\n",
                   what, a, a / DEBUG_WINDOW_SIZE, a % DEBUG_WINDOW_SIZE,
                   clean[a], o);
            g_fail++;
            return;
        }
    }
}


/* ------------------------------------------------------- ClearDebugWindow */

static void test_clear(int index)
{
    char label[64];

    seed(0xA5);
    ClearDebugWindow(index);
    call1(func_000a7ca4_ClearDebugWindow, (uint32_t)index);

    snprintf(label, sizeof(label), "ClearDebugWindow(%d)", index);
    cmp_windows(label);
}


/* ------------------------------------------------------------ DW_NewLine
 *
 * Advances the line and scrolls at the end. Driven from a range of starting
 * cursors, including the last valid line and past it, because the scroll is the
 * only interesting branch and it only fires at the boundary.
 */
static void test_newline(int start_line)
{
    char label[64];

    seed(0x00);
    g_clean[0].line = start_line;
    g_clean[0].column = 7;
    MEM_ST32(ARENA + 0x04, (uint32_t)start_line);
    MEM_ST32(ARENA + 0x00, 7u);

    DW_NewLine(&g_clean[0]);

    {
        arm_ctx c;
        ctx_reset(&c);
        c.r[0] = ARENA;
        func_000a7e00_DW_NewLine(&c);
    }

    snprintf(label, sizeof(label), "DW_NewLine(line=%d)", start_line);
    cmp_windows(label);
}


/* --------------------------------------------------------- LIME_KillSliders
 *
 * Six unrolled ClearDebugWindow calls on slots 10 to 15. The other tests
 * compare four windows; this one compares sixteen -- which is also a check that
 * the slider slots really are 10..15 and not something the earlier reading
 * rounded.
 */
static void test_killsliders(void)
{
    /* Slots 10..15 are all that is touched; sixteen windows cover them. */
    const size_t bytes = (size_t)16 * DEBUG_WINDOW_SIZE;
    const uint8_t *clean;
    uint32_t a;

    memset(DebugWindows, 0xA5, bytes);
    for (a = 0; a < (uint32_t)bytes; a += 4u)
        MEM_ST32(ARENA + a, 0xA5A5A5A5u);

    LIME_KillSliders();
    call0(func_000a7ce8_LIME_KillSliders);

    clean = (const uint8_t *)DebugWindows;
    g_cases++;
    for (a = 0; a < (uint32_t)bytes; a++) {
        if (clean[a] != MEM_LD8(ARENA + a)) {
            printf("  DIVERGE LIME_KillSliders  byte 0x%x (window %u, +0x%x)\n",
                   a, a / DEBUG_WINDOW_SIZE, a % DEBUG_WINDOW_SIZE);
            g_fail++;
            break;
        }
    }

}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    g_clean = DebugWindows;             /* the clean side's array */
    MEM_ST32(WINPTR, ARENA);            /* the guest's pointer to its own */

    printf("=== clean DS_DebugWin.c vs the recompiled original ===\n");
    printf("window stride: 0x%X (%d bytes), %d lines of 0x%X\n\n",
           DEBUG_WINDOW_SIZE, DEBUG_WINDOW_SIZE, 50, DEBUG_LINE_STRIDE);

    /* -1 is the "no window" sentinel and must write nothing at all */
    test_clear(-1);

    for (int i = 0; i < WINDOWS; i++)
        test_clear(i);

    /* every line, plus the boundary and one past it */
    for (int l = 0; l <= 0x33; l++)
        test_newline(l);

    test_killsliders();

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean debug overlay matches the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
