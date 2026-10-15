/* par.c -- see par.h. Port only, nothing of this is in the binary: the
 * iPhone decoded every texture on its one core while the loading screen
 * waited. */
#include "par.h"

/* runtime/lime/ is built as a save-state file (savestate.h puts its globals
 * in the sections a save state copies). The pool's handles are not game
 * state and must not be rolled back: back to the default sections. */
#ifdef UMK3_STATE_FILE
#pragma clang section data="" bss=""
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define PAR_MAX 64

static int            g_workers = -1;   /* -1 = not started */
static HANDLE         g_go;             /* semaphore, one count per worker */
static HANDLE         g_done;           /* the last chunk sets it */
static lime_par_fn    g_fn;
static void          *g_ctx;
static int            g_n, g_chunk, g_chunks;
static volatile LONG  g_next;           /* next chunk to take */
static volatile LONG  g_left;           /* chunks not finished */

static void run_chunks(void)
{
    for (;;) {
        LONG c = InterlockedIncrement(&g_next) - 1;
        int lo, hi;

        if (c < 0 || c >= g_chunks)     /* parked, or all taken */
            return;
        lo = (int)c * g_chunk;
        hi = lo + g_chunk < g_n ? lo + g_chunk : g_n;
        g_fn(g_ctx, lo, hi);
        if (InterlockedDecrement(&g_left) == 0)
            SetEvent(g_done);
    }
}

static DWORD WINAPI worker(LPVOID arg)
{
    (void)arg;
    for (;;) {
        WaitForSingleObject(g_go, INFINITE);
        run_chunks();
    }
    return 0;
}

static void start(void)
{
    SYSTEM_INFO si;
    int i, n;

    GetSystemInfo(&si);
    n = (int)si.dwNumberOfProcessors - 1;   /* the caller works too */
    if (n > PAR_MAX - 1) n = PAR_MAX - 1;
    if (n < 0) n = 0;
    g_go   = CreateSemaphoreW(NULL, 0, PAR_MAX, NULL);
    g_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_workers = 0;
    if (!g_go || !g_done)
        return;
    for (i = 0; i < n; i++) {
        HANDLE t = CreateThread(NULL, 64 * 1024, worker, NULL, 0, NULL);
        if (!t)
            break;
        CloseHandle(t);
        g_workers++;
    }
}

int lime_par_threads(void)
{
    if (g_workers < 0)
        start();
    return g_workers + 1;
}

void lime_par_for(int n, lime_par_fn fn, void *ctx)
{
    int chunks;

    if (n <= 0)
        return;
    if (g_workers < 0)
        start();
    if (g_workers == 0 || n < 2) {
        fn(ctx, 0, n);
        return;
    }
    /* four chunks a thread, so a slow one does not hold the rest */
    chunks = (g_workers + 1) * 4;
    if (chunks > n) chunks = n;
    /* A worker woken by a count left over from an earlier call may run at
     * any moment: park the chunk counter out of range while the job is
     * written, and open it last (InterlockedExchange is a full barrier). */
    InterlockedExchange(&g_next, 0x40000000);
    g_fn = fn;
    g_ctx = ctx;
    g_n = n;
    g_chunk = (n + chunks - 1) / chunks;
    chunks = (n + g_chunk - 1) / g_chunk;
    g_chunks = chunks;
    g_left = chunks;
    ResetEvent(g_done);
    InterlockedExchange(&g_next, 0);
    ReleaseSemaphore(g_go, g_workers < chunks - 1 ? g_workers : chunks - 1, NULL);
    run_chunks();
    WaitForSingleObject(g_done, INFINITE);
}

#else

int lime_par_threads(void) { return 1; }

void lime_par_for(int n, lime_par_fn fn, void *ctx)
{
    if (n > 0)
        fn(ctx, 0, n);
}

#endif
