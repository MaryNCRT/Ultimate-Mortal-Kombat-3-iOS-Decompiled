/* par.h -- a small pool of worker threads for the loaders. Port only.
 *
 * lime_par_for(n, fn, ctx) calls fn(ctx, lo, hi) over [0, n) split into
 * chunks, on every core, and returns when all of it is done. The decoders use
 * it for rows of pixels: each chunk writes its own rows and nothing else, so
 * the result is the same as one thread's, byte for byte.
 *
 * One call at a time, from the main thread. Without threads (not Windows)
 * it runs fn(ctx, 0, n) in place. */
#ifndef LIME_PAR_H
#define LIME_PAR_H

typedef void (*lime_par_fn)(void *ctx, int lo, int hi);

void lime_par_for(int n, lime_par_fn fn, void *ctx);
int  lime_par_threads(void);            /* workers plus the caller */

#endif
