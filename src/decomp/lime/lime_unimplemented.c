/*
 * lime/common — bodies that are deliberately not written yet.
 *
 * Several functions in decomp/lime/ are documented declarations with no body,
 * because what they do is understood but some field layout inside them is not,
 * and writing a plausible body over an unconfirmed layout is the failure this
 * project guards against hardest. See docs/ENCARGO.md.
 *
 * That leaves the linker with undefined references, which would stop any test
 * from building even when the test never calls them.
 *
 * This file resolves them the same way tools/armrecomp/recomp.py resolves an
 * import it has no shim for: with a definition that ABORTS AND NAMES ITSELF if
 * it is ever reached. Fail loudly at run time, not at link time. A test that
 * does not touch these links and runs; one that does gets told exactly which
 * missing body it needed, instead of a wall of linker output.
 *
 * **Deleting an entry here is how you finish a function.** Write the real body
 * in its own file and remove the stub; the linker will pick up the real one.
 */

#include <stdio.h>
#include <stdlib.h>

#include "lime.h"

static void unwritten(const char *name)
{
    fprintf(stderr,
            "\nlime: %s has no body yet.\n"
            "It is a documented declaration in decomp/lime/ -- the behaviour is\n"
            "described in the comment above it, but a field layout inside it was\n"
            "not confirmed, so no body was invented. Reaching this means a test\n"
            "now needs it. See docs/ENCARGO.md.\n", name);
    abort();
}





