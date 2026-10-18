# Contributing

Contributions are welcome. This document covers the rules that matter, how to pick up work, and what "done" means here.

If you are new to reverse engineering, read [docs/GETTING-STARTED.md](docs/GETTING-STARTED.md) first — it explains the whole pipeline from nothing. Then read [docs/HOW-THE-GAME-WORKS.md](docs/HOW-THE-GAME-WORKS.md): how the game runs, where each piece lives, how bugs are found and fixed, and the test tools.

---

## The two hard rules

### 1. Never commit anything derived from the retail binary

No `.ipa`. No extracted bundles. No game assets. No symbol dumps. No recompiler output. No raw Ghidra output.

`.gitignore` is set up to catch all of this, but check your `git status` before committing anyway. If you are unsure whether something counts, it counts.

**What is fine to commit:** tools, documentation, hand-written C, test harnesses, signature files. In other words: descriptions of what the binary does, and original code that reimplements it — not copies of it.

### 2. A function is not done until its differential test passes with zero divergences

Not "looks right." Not "compiles." Not "matches the decompiler output."

Zero divergences against the oracle, across thousands of inputs.

This is not bureaucracy. Decompiler output for this binary is *wrong* about 27% of the time in the engine core, and it is wrong in ways that read perfectly well. A plausible-looking function that is subtly incorrect is worse than no function at all, because the failure surfaces months later somewhere unrelated.

---

## How the pipeline works

```
Ghidra  ──────────►  draft C in decomp/lime/_raw/     (regenerable, never edited)
                              │
                              ▼
                     hand-written clean C in decomp/lime/
                              │
recomp.py  ────────►  oracle in recompiled/            (faithful, unreadable)
                              │
                              ▼
                     differential test  ───►  zero divergences  ───►  accepted
```

The raw Ghidra output is a **draft**. It gets regenerated whenever signatures improve, so never edit it by hand. The clean C in `decomp/lime/` is what people write, review, and eventually compile into the port.

---

## Picking up work

**Every function is decompiled** (2,572 of 2,572) and **the game is playable**
(alpha 0.0.5: menus, Arcade to the end, whole fights). The work now is fixing
the places where a transcription and the binary disagree, one symptom at a
time, and the port features the original never had. The open list is the
README's *Known problems* table; open an issue saying what you are taking.

| Area | What it is | Where to start |
|---|---|---|
| **Known problems** | Visual, audio and behaviour bugs seen in the game, each with what is known | README, *Known problems*; [docs/HOW-THE-GAME-WORKS.md](docs/HOW-THE-GAME-WORKS.md), sections 4 and 5 |
| **Port features** | Widescreen, frame rate, the game's ads in in-game windows, mods | issues #22, #23, #24, #29 |
| **Platforms** | A launcher for Linux and macOS (the game itself builds there with CMake) | `launcher/`, `CMakeLists.txt` |
| **Verification** | Raising coverage where the difftest reports `LOWCOV`; the prototype disagreements `tools/protos.py` lists | `tools/difftest/`, issue #26 |

Correcting a decompiled function follows the method in
[HOW-THE-GAME-WORKS.md](docs/HOW-THE-GAME-WORKS.md) section 4: reproduce, read
the original with `tools/cd.py`, fix the C to match, write the proving
addresses in the comment, test. For functions that compute rather than draw,
the differential workflow below still applies.

### The workflow

```bash
# 1. Generate or regenerate the draft
python tools/decomp_driver.py --file YourModule.cpp

# 2. Generate the oracle
python tools/armrecomp/recomp.py UMK3.armv7 --file YourModule.cpp \
       --out recompiled --name yourmodule --with-deps

# 3. Write clean C in decomp/lime/YourModule.c

# 4. Write a differential test in tests/

# 5. Build and run it — it must report zero divergences
```

---

## Writing a good differential test

Comparing against a reference implementation is the baseline. Two things make a test genuinely strong:

**Cover the edge cases the code actually has.** Zero vectors, denormals, very large and very small magnitudes, negative values, and — importantly — any conditional path. The `limeVector` test specifically exercises the zero-length branch, because that path goes through an IT block, which is the riskiest part of the recompiler.

**Test invariants, not just outputs.** For anything mathematical, properties are stronger than comparisons:

```c
assert_matrix_eq(mul(A, identity), A);           /* A × I == A            */
assert_matrix_eq(mul(rotZ(0.3), rotZ(0.4)),
                 rotZ(0.7));                      /* R(a)·R(b) == R(a+b)   */
assert_close(determinant3x3(rotX(t)), 1.0f);      /* rotations preserve    */
```

These pass only if the arithmetic is genuinely correct, and — unlike a direct comparison — they do not depend on having guessed the internal row/column convention right.

Aim for thousands of generated cases with a **fixed seed**, so failures are reproducible.

---

## Things worth knowing before you start

**The calling convention is AAPCS soft-float.** Floats travel in integer registers: a `float` comes back in `r0`, a `double` in `r0:r1`. Ghidra does not know this and will emit `/* WARNING: Unknown calling convention */`. Fifth arguments and beyond are on the stack.

**The binary is 100% Thumb** — only 2 of 4,342 functions are ARM. You do not need to handle mode switching.

**Thumb is marked unusually here.** This binary does *not* use bit 0 of the symbol value to flag Thumb; it uses the `N_ARM_THUMB_DEF` flag (`0x0008`) in `n_desc`. Getting this wrong produces garbage disassembly. `macho.py` already handles it.

**Watch for post-indexed loads.** `ldr r3, [r1], #4` accesses `[r1]` and *then* adds 4. Capstone does not put that 4 in `mem.disp` — it appears as a third operand. This caused a silent pointer-corruption bug in the recompiler that only surfaced because a test caught a wrong value.

**Literal pools sit inside functions.** Thumb-2 PC-relative loads only reach ±4 KB, so the compiler drops constants in the middle of large functions. A linear disassembly sweep will read them as instructions. 852 of 4,342 functions are affected.

---

## Commit conventions

Keep commits scoped to one module or one tool. Write messages that say what changed and why:

```
decomp(lime): rewrite RenderScene.cpp clean, verified

12 of 14 functions rewritten from disassembly. Differential test
passes 25,000 cases with 0 divergences.

Two functions still pending: _SceneSortNodes and _SceneCullVisible
both use indirect calls the oracle cannot resolve yet.
```

If AI assistance was involved in a commit, say so with a trailer:

```
Co-Authored-By: Claude <noreply@anthropic.com>
```

We are explicit about this — see [AI-DISCLOSURE.md](AI-DISCLOSURE.md). Being upfront costs nothing and lets people evaluate the code on informed terms.

---

## Reporting findings without code

Not every useful contribution is a patch. All of these are valuable and welcome as issues:

- A file format you have worked out (the `frames.x` table is the one still open)
- A correction to something in `docs/` — including things that are simply wrong
- A function where the decompiler output is misleading, so others don't lose time to it
- Behavioural observations from running the game

Documentation corrections are treated as first-class contributions. Several specifications in `docs/` have already been fixed by testing them against reality, and finding the next error is genuinely useful work.
