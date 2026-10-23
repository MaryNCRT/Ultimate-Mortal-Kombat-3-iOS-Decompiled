# AI Disclosure

This document exists because you have a right to know how the code in this repository was produced, and because AI-assisted reverse engineering is genuinely controversial in this community. We would rather be direct about it than have you find out from a commit log.

## The short version

**Most of this project was produced by Anthropic's Claude, working through Claude Code, under human direction.**

That includes the analysis tooling, the static recompiler, the decompilation work, the test harnesses, the binary patch, and the documentation you are reading now.

Claude appears in the commit history as a co-author, using the standard `Co-Authored-By:` trailer. Commits are attributed honestly: if a change was authored by an AI, the trailer says so.

## What was AI-generated and what was not

| Part | Who |
|---|---|
| Project direction, goals, scope decisions | Human |
| Which approach to take (decompile vs recompile vs emulate) | Human decision, AI analysis |
| Analysis tooling (`macho.py`, `stabs.py`, `rank.py`, `xref.py`, …) | AI |
| Static recompiler (`recomp.py`) | AI |
| Ghidra automation and signature files | AI |
| Hand-written clean C in `decomp/` | AI, verified mechanically |
| Test harnesses and differential tests | AI |
| The touchHLE compatibility patch and its diagnosis | AI |
| Documentation | AI |
| Verification that any of it is *correct* | **Machines, not opinions** — see below |

## Why we think this is defensible anyway

The objection to AI-assisted decompilation is not unreasonable. A language model will produce code that *looks* right with the same confidence whether it is right or not. In a domain where a subtly wrong function surfaces as a bug six months later, that is a real problem.

So the project was built around the assumption that **neither the AI nor the decompiler can be trusted**, and correctness has to come from somewhere else:

1. **A second, independent implementation.** `tools/armrecomp/recomp.py` translates the original machine code to C instruction by instruction. It does not interpret or infer — it transcribes. Whatever it produces behaves like the original binary because it *is* the original binary, mechanically restated.

2. **Differential testing as the acceptance criterion.** No hand-written function enters `decomp/` until it has been proven to behave identically to the recompiled reference across thousands of generated inputs. `Matrix.cpp` passed 40,006 cases; `limeVector.cpp` passed 20,013. Zero divergences, or it does not ship.

3. **Verification against the real game data.** The `.meshset` format specification was not checked against our own reader — it was checked by running EA's actual loader, recompiled, over 590 real game files and comparing 7,326 meshes byte for byte.

4. **Mathematical invariants, not just reference comparisons.** Matrix code is tested with properties like `A × I == A` and `R(a) · R(b) == R(a+b)`, which hold only if the arithmetic is genuinely correct and do not depend on having guessed the internal conventions right.

This method has already caught things a human reviewer plausibly would not have. Ghidra's decompilation of `_Len()` returns an uninitialized variable — it compiles, it reads plausibly, and it is completely wrong, because EA's compiler used 2-lane NEON instructions for scalar math. That pattern affects 27% of the engine core. It was caught in the first week, by a test, not by anyone's judgement.

**The honest framing: AI made this project fast enough to be worth attempting. Verification is what makes it trustworthy. Neither would be sufficient alone.**

## What this does not mean

- It does not mean the code is guaranteed correct. It means the parts marked verified have been mechanically checked, and the parts that have not are marked as such in [docs/PROGRESS.md](docs/PROGRESS.md).
- It does not mean review is unnecessary. Human review of this code is welcome and wanted.
- It does not mean we think this approach is right for every project, or that people who object to it are wrong to.

## If you would rather not use AI-assisted code

That is a legitimate position and we are not going to argue you out of it. Everything here is under the GPL (version 2 or later; MIT up to 0.1.9-beta), the methodology is documented in full, and the tooling is reusable — you can regenerate any of it yourself, or use the documentation and write your own implementation from scratch. The file format specifications in `docs/` are statements of fact about the binary; they hold regardless of what produced them.

## Prior art in being upfront about this

We are following the example of the [Super Smash Bros. 64 AI-assisted port](https://github.com/JRickey/BattleShip), whose author put it plainly: *"People may not like it because I used AI, and that's okay."*

We agree with both halves of that sentence.

## Agents, in relay

Since 2026-08-14 the project has been worked by **Anthropic's Claude** (via
Claude Code) and **DeepSeek**, handing over to each other as context runs out.
The working agreement between them is [AGENTS.md](AGENTS.md).

DeepSeek's contribution so far is research rather than code: mapping the
cooperative process scheduler in `gamecode/logic/other.c` — `StartThreadAt`,
`QueueAndJump`, `DoSwitchJump`, the circular queues and the function-pointer
dispatch tables — plus an independent run of the differential tests on Linux,
which confirmed they pass outside the machine they were written on.

The key and button pictures since 0.1.3 are Julio Cacko's
[Free Input Prompts](https://juliocacko.itch.io/free-input-prompts) (CC0,
credited in `launcher/aditional textures/glyphs/CREDITS.txt` anyway): drawn by
a person, not generated.

**Devin** (Cognition) joined the relay on 2026-08-20 and wrote the SDL2 backend
for the vertical slice, which is hand-written port code rather than anything
derived from the binary.

That research was audited against the binary before any of it was adopted:
258 symbol/address pairs checked, **223 correct, 15 wrong, 20 referring to
symbols that do not exist in the binary at all**. The errors were not evenly
spread — they clustered entirely in the notes that mapped iOS addresses to
names from other releases of the game. Those notes were rejected; the parts
derived from disassembling our own binary were kept, and they verified at
essentially 100%.

We mention the failure rate rather than only the contribution because it is the
strongest argument for the method: **an agent's output is worth exactly what it
can be verified against**, and here that is the binary, not another document.
