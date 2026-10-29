/*
 * omp_lite.c -- the few OpenMP runtime calls CCGSubSurf_legacy.c needs.
 *
 * Written here, not Blender's and not LLVM's libomp: the entry points clang
 * emits for `#pragma omp parallel`, `for schedule(static)` and `critical`
 * (the __kmpc_* interface), over a pool of threads that lives as long as the
 * game. zig cc compiles -fopenmp but ships no runtime, so without this the
 * Blender files were built without it and every subdivision ran on one core
 * (Mary: the game should use the PC's power; Scorpion's fatality smoothed a
 * 60,000-triangle mesh twice a frame on one core, ~6 ms each).
 *
 * Only what those pragmas generate: one team at a time (a second thread
 * forking while the team is busy -- SMOOTH MODELS builds a new mesh on a
 * thread of its own -- runs its region alone), no nesting, static schedule,
 * one lock for every `critical`. Without -fopenmp (the viewer's WebAssembly)
 * this file is empty.
 */
#ifdef _OPENMP

#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
typedef SRWLOCK            ol_mutex;
typedef CONDITION_VARIABLE ol_cond;
#define ol_lock(m)        AcquireSRWLockExclusive(m)
#define ol_unlock(m)      ReleaseSRWLockExclusive(m)
#define ol_wait(c, m)     SleepConditionVariableSRW(c, m, INFINITE, 0)
#define ol_wake_all(c)    WakeAllConditionVariable(c)
#define ol_yield()        SwitchToThread()
#define OL_MUTEX_INIT     SRWLOCK_INIT
#define OL_COND_INIT      CONDITION_VARIABLE_INIT
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
typedef pthread_mutex_t ol_mutex;
typedef pthread_cond_t  ol_cond;
#define ol_lock(m)        pthread_mutex_lock(m)
#define ol_unlock(m)      pthread_mutex_unlock(m)
#define ol_wait(c, m)     pthread_cond_wait(c, m)
#define ol_wake_all(c)    pthread_cond_broadcast(c)
#define ol_yield()        sched_yield()
#define OL_MUTEX_INIT     PTHREAD_MUTEX_INITIALIZER
#define OL_COND_INIT      PTHREAD_COND_INITIALIZER
#endif

#define OL_MAX_THREADS 32
#define OL_MAX_ARGS    16
#define OL_SPIN        20000            /* pauses before sleeping, ~0.2 ms */

#if defined(__x86_64__) || defined(__i386__)
#define ol_pause()     __builtin_ia32_pause()
#else
#define ol_pause()     ((void)0)
#endif

typedef void (*ol_task)();

static ol_mutex g_m = OL_MUTEX_INIT, g_crit = OL_MUTEX_INIT;
static ol_cond  g_go = OL_COND_INIT, g_done_cv = OL_COND_INIT;
static int      g_threads;              /* the team's size, the forker too */
static int      g_started;              /* -1: one core, no pool */
static volatile long g_busy;            /* a team is running */
static unsigned g_gen;                  /* a new region for the workers */
static int      g_done;                 /* workers finished with it */
static ol_task  g_fn;
static int      g_argc;
static void    *g_args[OL_MAX_ARGS];

static volatile long g_bar_count;       /* the barrier */
static volatile long g_bar_gen;

static _Thread_local int t_tid;         /* 0 outside a region */
static _Thread_local int t_team = 1;

static void call(ol_task fn, int gtid, int argc, void **a)
{
    int b = gtid;

    switch (argc) {
    case 0:  fn(&gtid, &b); break;
    case 1:  fn(&gtid, &b, a[0]); break;
    case 2:  fn(&gtid, &b, a[0], a[1]); break;
    case 3:  fn(&gtid, &b, a[0], a[1], a[2]); break;
    case 4:  fn(&gtid, &b, a[0], a[1], a[2], a[3]); break;
    case 5:  fn(&gtid, &b, a[0], a[1], a[2], a[3], a[4]); break;
    case 6:  fn(&gtid, &b, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 7:  fn(&gtid, &b, a[0], a[1], a[2], a[3], a[4], a[5], a[6]); break;
    case 8:  fn(&gtid, &b, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); break;
    default: fn(&gtid, &b, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]); break;
    }
}

#ifdef _WIN32
static DWORD WINAPI worker(LPVOID p)
#else
static void *worker(void *p)
#endif
{
    int me = (int)(long)(size_t)p;
    unsigned seen = 0;

    for (;;) {
        int spin;

        /* a subdivision is dozens of regions back to back: waiting awake
         * for a moment finds the next one without the system's wake-up
         * (tens of microseconds a region, which cost more than the work) */
        for (spin = 0; spin < OL_SPIN && *(volatile unsigned *)&g_gen == seen; spin++)
            ol_pause();
        ol_lock(&g_m);
        while (g_gen == seen)
            ol_wait(&g_go, &g_m);
        seen = g_gen;
        ol_unlock(&g_m);

        t_tid = me;
        t_team = g_threads;
        call(g_fn, me, g_argc, g_args);
        t_tid = 0;
        t_team = 1;

        ol_lock(&g_m);
        if (++g_done == g_threads - 1)
            ol_wake_all(&g_done_cv);
        ol_unlock(&g_m);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* The pool, at the first region: a thread a core, the forker one of them. */
static int pool(void)
{
    int n, i;

    if (g_started)
        return g_started > 0;
#ifdef _WIN32
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        n = (int)si.dwNumberOfProcessors;
    }
#else
    n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    /* four at most: a subdivision's regions are short, and past four the
     * threads cost more to gather than they save (measured: 1, 4 and 8
     * threads on Scorpion's fatality, 4 the fastest) */
    if (n > 4)
        n = 4;
    if (getenv("UMK3_THREADS"))         /* a test's count */
        n = atoi(getenv("UMK3_THREADS"));
    if (n > OL_MAX_THREADS)
        n = OL_MAX_THREADS;
    if (n < 2) {
        g_started = -1;
        return 0;
    }
    g_threads = n;
    for (i = 1; i < n; i++) {
#ifdef _WIN32
        HANDLE h = CreateThread(NULL, 0, worker, (LPVOID)(size_t)i, 0, NULL);
        if (h == NULL)
            break;
        CloseHandle(h);
#else
        pthread_t t;
        if (pthread_create(&t, NULL, worker, (void *)(size_t)i) != 0)
            break;
        pthread_detach(t);
#endif
    }
    g_threads = i;
    g_started = g_threads > 1 ? 1 : -1;
    return g_started > 0;
}

int __kmpc_global_thread_num(void *loc)
{
    (void)loc;
    return t_tid;
}

void __kmpc_fork_call(void *loc, int argc, ol_task fn, ...)
{
    void *a[OL_MAX_ARGS] = { 0 };
    va_list ap;
    int i;

    (void)loc;
    va_start(ap, fn);
    for (i = 0; i < argc && i < OL_MAX_ARGS; i++)
        a[i] = va_arg(ap, void *);
    va_end(ap);

    /* one core, inside a region already, or the team taken by another
     * thread: this region alone */
    if (t_team > 1 || !pool()
#ifdef _WIN32
        || InterlockedCompareExchange(&g_busy, 1, 0) != 0
#else
        || !__sync_bool_compare_and_swap(&g_busy, 0, 1)
#endif
        ) {
        int team = t_team;
        t_team = 1;
        call(fn, t_tid, argc, a);
        t_team = team;
        return;
    }

    ol_lock(&g_m);
    g_fn = fn;
    g_argc = argc;
    for (i = 0; i < OL_MAX_ARGS; i++)
        g_args[i] = a[i];
    g_done = 0;
    g_bar_count = 0;
    g_gen++;
    ol_wake_all(&g_go);
    ol_unlock(&g_m);

    t_tid = 0;
    t_team = g_threads;
    call(fn, 0, argc, a);
    t_team = 1;

    for (i = 0; i < OL_SPIN && *(volatile int *)&g_done < g_threads - 1; i++)
        ol_pause();
    ol_lock(&g_m);
    while (g_done < g_threads - 1)
        ol_wait(&g_done_cv, &g_m);
    ol_unlock(&g_m);
    g_busy = 0;
}

void __kmpc_serialized_parallel(void *loc, int gtid) { (void)loc; (void)gtid; }
void __kmpc_end_serialized_parallel(void *loc, int gtid) { (void)loc; (void)gtid; }

/* schedule(static): the iterations in one even block a thread */
void __kmpc_for_static_init_4(void *loc, int gtid, int sched, int *last,
                              int *lower, int *upper, int *stride,
                              int incr, int chunk)
{
    int n = t_team, me = t_tid;
    long long trip, each, rem, from, count;

    (void)loc; (void)gtid; (void)sched; (void)chunk;
    if (incr == 0)
        incr = 1;
    trip = incr > 0 ? ((long long)*upper - *lower) / incr + 1
                    : ((long long)*lower - *upper) / -incr + 1;
    if (trip <= 0 || n <= 1) {
        if (last)
            *last = trip > 0;
        if (stride)
            *stride = (int)(trip > 0 ? trip * incr : incr);
        return;
    }
    each = trip / n;
    rem = trip % n;
    from = me * each + (me < rem ? me : rem);
    count = each + (me < rem);
    if (last)
        *last = count > 0 && from + count == trip;
    if (stride)
        *stride = (int)(trip * incr);
    *upper = (int)(*lower + (from + count - 1) * incr);
    *lower = (int)(*lower + from * incr);
}

/* the same over an unsigned range (a loop whose bound is unsigned); the
 * game's ranges fit an int */
void __kmpc_for_static_init_4u(void *loc, int gtid, int sched, int *last,
                               unsigned *lower, unsigned *upper, int *stride,
                               int incr, int chunk)
{
    __kmpc_for_static_init_4(loc, gtid, sched, last, (int *)lower,
                             (int *)upper, stride, incr, chunk);
}

void __kmpc_for_static_fini(void *loc, int gtid) { (void)loc; (void)gtid; }

void __kmpc_barrier(void *loc, int gtid)
{
    long gen;

    (void)loc; (void)gtid;
    if (t_team <= 1)
        return;
    gen = g_bar_gen;
#ifdef _WIN32
    if (InterlockedIncrement(&g_bar_count) == t_team) {
        g_bar_count = 0;
        InterlockedIncrement(&g_bar_gen);
        return;
    }
#else
    if (__sync_add_and_fetch(&g_bar_count, 1) == t_team) {
        g_bar_count = 0;
        __sync_add_and_fetch(&g_bar_gen, 1);
        return;
    }
#endif
    while (g_bar_gen == gen)
        ol_yield();
}

void __kmpc_critical(void *loc, int gtid, void *lock)
{
    (void)loc; (void)gtid; (void)lock;
    ol_lock(&g_crit);
}

void __kmpc_end_critical(void *loc, int gtid, void *lock)
{
    (void)loc; (void)gtid; (void)lock;
    ol_unlock(&g_crit);
}

#endif /* _OPENMP */
