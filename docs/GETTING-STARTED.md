# Getting Started

This guide assumes **no prior experience with reverse engineering**. If you know what a Mach-O binary is and how a decompiler works, skip to [Setting up](#setting-up).

The goal here is that someone who finds this repository cold can understand what the project is doing, why each piece exists, and where they could actually help.

---

## 1. The situation, in plain terms

There is a game — *Ultimate Mortal Kombat 3*, released for iPhone in 2011 by EA Mobile. It only runs on iOS 3 through 6, on hardware nobody owns any more, and it was removed from the App Store years ago. If you bought it then, you effectively cannot play it now.

We want it to run natively on a PC. Not emulated — actually compiled for Windows and Linux, as source code people can read and modify.

**The problem: we don't have the source code.** EA has it, and it is not public. What we have is the compiled game — a file full of ARM machine instructions.

**Decompilation** is the work of turning those machine instructions back into readable C, function by function, until you have something that can be compiled again for a different machine.

---

## 2. Why this particular game is a good candidate

When a program is compiled, the names of everything — functions, files, variables — are normally thrown away. What's left is addresses. A typical decompilation project spends *years* just working out where each function starts, what it was called, and which source file it lived in.

**This binary kept its debug symbols.** Whoever built it at EA shipped a build with the STABS debugging table still attached. That means:

- We know the name of all **4,342 functions** (`_LIME_LoadMeshSet`, `_CreatePerspectiveMatrix`, `_DoSpecial`, …)
- We know which of the **135 original source files** each one came from
- We can reconstruct EA's entire source tree layout
- The build path is even embedded in the file: `/BuildServerX/reactive/mortalkombat_iphone/xcode/umk3_iphone_en/`

There is also **no DRM** (`cryptid = 0`), so the code can be read directly.

In other words, the hardest and slowest part of a normal decompilation project is already done for us. That is the entire reason this is worth attempting.

---

## 3. The pieces, and what each one is for

### The binary

The game ships as an `.ipa` file, which is just a ZIP archive. Inside is `UMK3.app/UMK3` — the executable. It is a **fat binary**: it contains two versions of the same program, one for armv6 (older iPhones) and one for armv7. We work with the **armv7 slice**.

`tools/macho.py` reads this format and can pull out the slice, the symbol table, and the section layout.

### The symbol table

`tools/stabs.py` reads the debugging table and reconstructs the original source tree. This is what produces the mapping "function X lived in file Y", which drives everything else — we work through the game one original source file at a time, in the order EA wrote them.

### Ghidra — the decompiler

[Ghidra](https://ghidra-sre.org/) is a free reverse-engineering suite from the NSA. Point it at machine code and it produces approximate C. "Approximate" is the important word — see below.

`tools/ghidra/DecompileList.java` runs Ghidra without its GUI, feeding it a work list and a set of function signatures, and writes the resulting C to disk.

### The signatures

`tools/signatures/` contains two text files that transform Ghidra's output quality:

- `lime.txt` — what each function's arguments and return type actually are
- `structs.txt` — the memory layout of the game's data structures

Without them, Ghidra emits this:

```c
void _limeMatrixLoadIdentity(undefined4 *param_1)
{ *param_1 = 0x3f800000; param_1[1] = 0; ... }
```

With them, it emits this:

```c
void _limeMatrixLoadIdentity(float *m)
{ *m = 1.0; m[1] = 0.0; m[2] = 0.0; ... }
```

Same machine code. The difference is entirely in what we told Ghidra about the types. This is why signature files grow as the project learns, and why they are checked in as documentation of what has been figured out.

### The oracle — the important part

**Ghidra is often wrong, and it is wrong quietly.**

Real example from this project. Ghidra decompiled the function that computes a vector's length as:

```c
float _Len(float *v)
{
  float in_s0;                    /* never assigned anything */
  FloatVectorMult(uVar1, uVar1, 2, 0x20);
  FloatVectorAdd(uVar1, uVar2, 2);
  return in_s0;                   /* returns whatever was in that register */
}
```

That compiles. It looks reasonable at a glance. It returns garbage. EA's compiler had used NEON vector instructions to do ordinary scalar arithmetic, and Ghidra models those as opaque operations, losing the square root entirely.

If we had trusted it, we would have shipped a broken maths library and discovered it a year later as "the character models look wrong."

So the project has a second, independent path from the same machine code:

**`tools/armrecomp/recomp.py`** is a *static recompiler*. It converts each ARM instruction into one line of C that does exactly what that instruction does, keeping the processor's registers in a struct:

```
ARM:  add r4, r4, #0x20
C:    ctx->r4 = ADD32(ctx->r4, 0x20);
```

The result is unreadable — nobody wants to ship that. But it is **faithful by construction**: it does not interpret or infer anything, so it behaves exactly like the original.

That gives us a reference implementation to test against. We call it the **oracle**.

### Differential tests

This is where it comes together. For each function:

1. Ghidra gives us a readable draft
2. A human (or AI) writes clean, idiomatic C based on it
3. The clean version and the oracle version are run against **the same thousands of inputs**
4. If they ever disagree, the clean version is wrong and gets fixed

`Matrix.cpp` passed 40,006 cases with zero divergences. `limeVector.cpp` passed 20,013. That is the standard: **zero divergences, or it does not go in**.

Some tests go further and check mathematical properties instead of just comparing outputs — `A × I == A`, `R(a)·R(b) == R(a+b)`, rotation matrices being orthonormal with determinant +1. These are stronger, because they pass only if the arithmetic is genuinely right, and they don't depend on having guessed the internal conventions correctly.

### touchHLE — the behavioural reference

[touchHLE](https://touchhle.org/) is an emulator that runs old iPhone apps on PC. We use it as a **second opinion about what the game actually does**.

This matters most for the fight logic. That part of the game is a state machine built on tables of function pointers, and the static recompiler cannot follow those — it needs to know the jump target at translation time, and there it isn't known until runtime. For those functions, a running copy of the game is the only reference available.

Getting version 1.2.59 to run took a 2-byte patch. That story is in [TOUCHHLE-PATCH.md](TOUCHHLE-PATCH.md) and is worth reading — it is a good illustration of how these problems actually get diagnosed.

---

## 4. Setting up

### What you need

| Requirement | Notes |
|---|---|
| **Your own copy of UMK3 iOS 1.2.59** | Legally obtained. Nothing here works without it, and we cannot provide it. |
| Python 3.10+ | plus `pip install capstone` |
| A C compiler | MinGW-w64 on Windows, gcc or clang on Linux |
| Ghidra 11+ | https://ghidra-sre.org/ |
| Java 21+ | required by Ghidra |

### First steps

```bash
# Extract the armv7 slice from your copy
python tools/macho.py path/to/UMK3 --thin armv7 --out UMK3.armv7

# Rebuild EA's original source tree from the debug symbols
python tools/stabs.py UMK3.armv7 --tree

# Look at a function's disassembly
python tools/disasm.py UMK3.armv7 --func _CreatePerspectiveMatrix

# Generate the oracle for a module
python tools/armrecomp/recomp.py UMK3.armv7 --file Matrix.cpp --out recompiled --name matrix

# Build and run the differential test
gcc -std=c11 -O1 -I runtime -I recompiled \
    tests/test_matrix_diff.c decomp/lime/Matrix.c recompiled/matrix.c runtime/arm_runtime.c \
    -o build/test_matrix_diff -lm
./build/test_matrix_diff
```

If that last command prints zero divergences, your setup is correct and you are looking at a verified reimplementation of EA's matrix maths.

---

## 5. Where you could actually help

The decompilation is finished and the game runs: alpha 0.1.0 plays the menus,
Arcade to the end and whole fights on Windows, in 3:2, 4:3, 16:9 or 21:9. What is left is fixing the
places where the C and the binary still disagree, as players find them.

Read [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md) next -- it explains how the
game runs, where everything is, the method that fixed every bug so far, and the
debug and test tools. Then pick a row from the README's *Known problems*:

**If you are new to this:** a visual bug -- a texture or icon in the wrong place.
These are often a pair of swapped constants; the guide's section 4 walks
through the method, and `tools/cd.py` shows you the original code.

**If you know ARM assembly:** the behaviour bugs (a fighter moving when it
should not, a death scene that does not draw). They need reading the fight
engine's handlers against the disassembly.

**If you like tooling:** the Linux/macOS launcher, or better coverage in
`tools/difftest/`.

**If you want to work on the port itself:** widescreen, frame rate, and the
game's ads in in-game windows are features the original never had (issues
#22-#24).

---

## 6. Vocabulary

| Term | Meaning |
|---|---|
| **Mach-O** | Apple's executable file format |
| **armv7** | The 32-bit ARM instruction set the game was compiled for |
| **Thumb** | A compact ARM encoding. This binary is 100% Thumb except two functions. |
| **STABS** | An old debugging-symbol format. Its survival here is what makes this project feasible. |
| **NEON** | ARM's SIMD instructions. EA used them for scalar maths, which is what confuses Ghidra. |
| **Oracle** | Our recompiled reference implementation — faithful but unreadable |
| **Differential test** | Running two implementations on identical inputs and requiring identical output |
| **Stub** | A placeholder that satisfies a call without doing the real work |
| **AAPCS soft-float** | This binary's calling convention: floats travel in integer registers |
| **Literal pool** | Constants the compiler embeds inside code, which naive disassembly mistakes for instructions |
