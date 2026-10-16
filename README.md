<div align="center">

<img src="docs/img/banner.jpg" alt="Ultimate Mortal Kombat 3 Recomp" width="880">

# Ultimate Mortal Kombat 3 — iOS Decompilation & PC Port

**A complete decompilation of the 2011 iOS release of Ultimate Mortal Kombat 3 — every one of the game's 2,572 functions is now readable C — and a native PC port for Windows and Linux that is still in progress.**

[Getting started](docs/GETTING-STARTED.md) · [How the game works](docs/HOW-THE-GAME-WORKS.md) · [Browser experiment](web/README.md) · [Methodology](docs/METHODOLOGY.md) · [LIME engine](docs/LIME-ENGINE.md) · [Asset formats](docs/X-TABLES.md) · [Mesh viewer](docs/MESH-VIEWER.md) · [Game bugs](docs/GAME-BUGS.md) · [Hidden content](docs/HIDDEN-CONTENT.md) · [Stages](docs/STAGES.md) · [Roster](docs/ROSTER.md) · [Move tables](docs/MOVES-TABLES.md) · [Lighting](docs/LIGHTING.md) · [Font format](docs/FONT-FORMAT.md) · [Scene format](docs/SCENE-FORMAT.md) · [PVR format](docs/PVR-FORMAT.md) · [Frame lists](docs/FRAMELISTS.md) · [MAME reference](docs/MAME-ARCADE.md) · [iPad build](docs/IPAD-BUILD.md) · [Architecture](docs/ARCHITECTURE.md) · [Progress](docs/PROGRESS.md) · [Handoff](docs/HANDOFF.md) · [Original brief](docs/ENCARGO.md) · [AI disclosure](AI-DISCLOSURE.md) · [Español](README.es.md)

**Companion project:** [**UMK3 — Godot Remake**](https://github.com/MaryNCRT/UMK3-IOS-GODOT-REMAKE) — a playable remake built on what this repository measures. [How the two fit together](#the-companion-repository).

</div>

---

## No copyrighted assets are distributed here

**This repository ships no game files.** No textures, no models, no audio, no compiled code — nothing you could extract from here and use. Every release is built against **a copy you supply yourself**.

The imagery here is worth being precise about. The banner combines fan art of the *Ultimate Mortal Kombat 3* wordmark with **renders of the game's models made by ermaccer**, licensed CC BY 4.0 — both credited [below](#the-banner). The screenshots in [the mesh viewer's documentation](docs/MESH-VIEWER.md) are our own, produced by our own tools. In both cases the same thing is true: a render depicts the game's geometry; it is not an asset file, it cannot be unpacked back into one, and it is not part of any build. The *Mortal Kombat* marks and the characters depicted belong to Warner Bros. Entertainment.

What lives here is *our own* work: analysis tools, documentation of file formats, hand-written C, and test harnesses. Everything that touches the original game reads it from **a copy you supply yourself** and produces its output locally, where `.gitignore` keeps it out of the repository.

You need a legally obtained copy of *Ultimate Mortal Kombat 3* for iOS (version 1.2.59) to use any of this. If you don't have one, nothing in this repository will do anything useful for you.

---

## Where the project stands — 16 October 2026 (beta 0.1.3)

| | |
|---|---|
| **Decompiled** | ✅ **All of it.** 2,572 of 2,572 game functions have hand-written C: the LIME engine core (109), the game logic (291) and the fight engine (2,172). Nothing is left to transcribe. |
| **Verified** | ✅ The engine core passes differential tests against the recompiled original with zero divergences. The fight engine passes a behavioural differential test file by file, with the exceptions listed in [Verification](#how-much-of-it-is-verified) — every one of them a known limit of the test harness, not a known bug. |
| **Runs natively** | ✅ The real game runs natively on Windows (Linux/macOS from source): the front end with its 51 screens, Arcade to Shao Kahn and the treasure screen, whole fights with every finisher, sound, music and saves. How it fits together: [docs/HOW-THE-GAME-WORKS.md](docs/HOW-THE-GAME-WORKS.md). |
| **Playable** | ✅ **Beta 0.1.3** ([release](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/releases)): 100% playable. It can still have bugs, which are being fixed. You can play with the keyboard (rebindable), a gamepad (Xbox, PlayStation, Switch and others) or the mouse as the finger. The screen can be **3:2, 4:3 (the iPad's view), 16:9 or 21:9**. Texture quality goes up to uncompressed, and an iPad-size HUD is optional; 4:3 uses the iPad build's menus. PLAY > DUELO is two players on one PC, each with their own keys and gamepad. Esc opens a settings menu in the game (graphics and controls without restarting, quit), there is a player profile (name, ID, icon) for the leaderboards to come, a debug menu (hitboxes, fight speed, save states), and the launcher updates itself. Still wrong: see *Known problems*. The fight engine's 229 data tables are extracted from the player's own binary at build time and verified against it. |

### How to play (beta 0.1.3, Windows)

**Beta 0.1.3** ([notes](docs/RELEASE-0.1.3-beta.md); earlier: [0.1.2](docs/RELEASE-0.1.2-alpha.md), [0.1.1](docs/RELEASE-0.1.1-alpha.md), [0.1.0b](docs/RELEASE-0.1.0b-alpha.md), [0.1.0](docs/RELEASE-0.1.0-alpha.md), [0.0.9](docs/RELEASE-0.0.9-alpha.md), [0.0.8](docs/RELEASE-0.0.8-alpha.md), [0.0.7b](docs/RELEASE-0.0.7b-alpha.md), [0.0.7](docs/RELEASE-0.0.7-alpha.md), [0.0.6c](docs/RELEASE-0.0.6c-alpha.md), [0.0.6b](docs/RELEASE-0.0.6b-alpha.md), [0.0.6](docs/RELEASE-0.0.6-alpha.md), [0.0.5](docs/RELEASE-0.0.5-alpha.md), [0.0.4](docs/RELEASE-0.0.4-alpha.md), [0.0.3](docs/RELEASE-0.0.3-alpha.md), [0.0.2](docs/RELEASE-0.0.2-alpha.md), [0.0.1](docs/RELEASE-0.0.1-alpha.md)).
[Releases](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/releases)
has the launcher and only the sources the build needs, no game data.

1. Download the release zip and unpack it. Keep the folder together, and
   don't delete `launcher\bin\`: that is the launcher itself.
2. Run **`UMK3-Launcher.exe`** (from source: `npm install` and
   `npm run dist` in `launcher/app`, see its README). In *COMPILE YOUR .IPA*
   choose your own `.ipa` of UMK3 1.2.59 for iPhone and press **Compilar**.
   - It downloads a pinned compiler (llvm-mingw 20260616, SHA-256 checked) and
     Python 3.12.10 embeddable into `toolchain\`.
   - It checks the binary (uuid `90d6f56a…`, not encrypted) and extracts the
     data tables from it.
   - It compiles `umk3-game.exe` and copies `res\` out of the `.ipa`.
   
   About half a minute after the first download.
3. **Updates.** The same page shows the newest version's notes. The launcher
   checks GitHub when it opens and on **CHECK FOR UPDATES**; a new version
   gets a notification. **DOWNLOAD AND INSTALL** replaces the files, keeps
   `umk3.ini` and `res\`, reopens the launcher and recompiles.
4. **GRAPHICS:** window and 3D resolution, display mode, **aspect ratio**
   (3:2, 4:3, 16:9, 21:9), antialiasing, **texture quality** (IPHONE / IPAD /
   NO LIMIT), **interface size** (IPHONE / IPAD) and a picture for the
   fullscreen bars.
5. **CONTROLS:** the 6- or 5-button layout, keys for each, the gamepad, and
   the debug keys.
6. **PLAY:** language, skip the intros, debug mode, and the big **PLAY**
   button.

Everything is saved to `umk3.ini` the moment it changes. The launcher window
resizes freely, F11 makes it fullscreen, and the button at the top right
switches it between English and Spanish.

**Golden rule: the game depends only on its own folder.** `umk3-game.exe`
reads `res\` beside itself and nothing outside it (no `../` lookups, no
junctions; `Info.plist` is copied into `res\`). No game data and no game
executable are distributed: the exe only exists after the player's own
`.ipa` has been compiled. Build scripts: [`launcher/`](launcher/).

**Fight keys:**
- Move: W A S D or the arrows (two at once for diagonals).
- Attack: U high punch, I low punch, O block, J high kick, K low kick, L run (or numpad 7 8 9 / 4 5 6).
- **P** opens the pause menu (again: resume), **M** the moves list.
- The mouse is the finger.
- On a pad, the buttons go by position; START pauses and BACK opens the moves list.
- **The menus:** the d-pad, the left stick or the arrows move, cross / A or Enter accepts, circle / B or Backspace goes back; the right stick changes the fighter's colour. Player two: their movement keys, keypad 1 accepts, keypad / pauses, keypad * opens the moves list.

### Debug: straight into a fight

```
umk3-game.exe --fight kitana kunglao 0
```

skips the menus: two fighters by name (as in the select screen, case and
spaces ignored) or number 0-25, and an arena 0-15. `UMK3_FIGHT=kitana,kunglao,0`
does the same.

**Debug mode:** tick *Debug mode* in the launcher (it writes
`debug_keys=1` to `umk3.ini`; off by default). Then, in the game:

| key | does |
|---|---|
| **F2** (pad: L3+R3) | the debug menu, drawn over the frozen game. FIGHT: any fight (both fighters, Motaro and Shao Kahn included, any stage), win or lose the round or the match. SCREENS: any of the front end's 51 screens or the main menu. TOOLS: hitboxes, fight speed 10-400%, four save-state slots. Also the direct keys on/off and the info line. |
| F3 | the info line: task, screen, fighters, rounds |
| F6 / F7 | previous / next front-end screen |
| F8 | main menu |
| F9 / F10 | end the round (KO player 2 / player 1) |
| F11 / F12 | win / lose the whole match (F11 skips the fight) |

F3 and F6-F12 are the *direct keys*; the F2 menu turns them off and on.

**Logs and crashes:**
- **Logs:** every session started by double-click writes `logs/umk3-<date>-<time>.log` beside the exe. It holds the task changes and loading steps.
- **Crashes:** a crash writes `logs/crash-<date>-<time>.log` and shows it in a message box. It names the function, the stack, the task, the screen and the fighters. Attach it to an issue.

### Known problems (16 October 2026, beta 0.1.3)

What a player can still see. Everything fixed is in the release notes
([0.1.3](docs/RELEASE-0.1.3-beta.md) and earlier) and in
[docs/PROGRESS.md](docs/PROGRESS.md).

| Symptom | What is known |
|---|---|
| Some objects besides the fighters may not show. | Scorpion's spear and the stage effects are fixed; other props to check one by one. |
| Some fatalities lack animation (Sonya's does not show properly). | Not investigated. |
| Texture errors on some stages and fighters (parts of Kung Lao's hat black in a fight). | Not investigated stage by stage; report each with the stage and fighter. |
| Some modes besides Arcade can close the game; the menu is incomplete and some modes are missing. | Not inventoried yet. A crash log helps. |
| Some specials, projectiles above all, have no sound; Mercy's sound sometimes does not play. | Not investigated. Reptile's tongue fatality has no sound in the original either (Mary checked). |
| After a round a fighter can be drawn with another's model and a loose head (Ermac against Sub-Zero). | Seen once by Mary; cause not found yet. |
| Some attacks seem to connect from too far. | The collision code and frame tables match the binary; a concrete move is needed. |
| Pausing sometimes shows the fight shrunk into a corner behind the pause menu. | Not investigated. |
| Ermac's friendship and animality; Mileena's combo. | Probably fixed with the hold-and-release fix; to check in game. |
| In 16:9 and 21:9, a screen not yet checked may have one element off-centre. | Those screens place things with `FE_WidthScale` themselves; each is fixed when seen. Report it with a screenshot. |
| The game's ads open in separate windows. | Wanted: windows drawn inside the game. |
| Windows only for the launcher. | Linux/macOS build from source with CMake. |
| Only the iPhone 1.2.59 `.ipa` works. | The launcher refuses any other binary (uuid check); the iPad 1.2.56 build has different addresses. |
| The right stick of a PlayStation pad read through DirectInput (no Steam or DS4Windows) may do nothing. | Its axis (Z) is a guess; XInput and SDL pads are read directly. |
| Choosing the renderer (DirectX 12 / Vulkan / OpenGL). | On Mary's list, not started. The game draws with OpenGL. |

**What "decompiled" does and does not mean here.** It means every function the
game runs has a body that was written against the disassembly and checked
against an independent ARM→C recompilation of the same code. It does not mean
the game plays: code without its data tables and without a loop to drive it is
a complete engine sitting still. The [progress section](#overall-progress) puts
numbers on both halves, and is explicit about what the numbers leave out.

---

## What this project is

In 2011, EA Mobile released *Ultimate Mortal Kombat 3* for iPhone. It was built on an in-house 3D engine called **LIME**, and — like most iOS games of that era — it has been effectively unplayable for years: it requires an iPhone running iOS 3–6, and it was pulled from the App Store long ago.

This project is an attempt to bring it back properly, as **native PC software** rather than emulation: source code you can read, modify, and compile for Windows and Linux.

<div align="center">

<img src="docs/img/pose-cast.png" alt="Six UMK3 characters posed by tools/pose.py" width="860">

<img src="docs/img/viewer-graveyard.png" alt="The Graveyard stage rendered by tools/meshview.py" width="300">

<sub>Six of the roster in their fighting stances, and the Graveyard stage — drawn by [`tools/pose.py`](tools/pose.py) and [`tools/meshview.py`](tools/meshview.py). Bone tree, pose, skin weights, topology, UVs, PVRTC texture decoding and the projection matrix all come from this project's own parsers and decompiled code. No emulator, no engine binary. [How it works](docs/MESH-VIEWER.md).</sub>

<img src="docs/img/demo-graveyard.png" alt="The native demo rendering the Graveyard stage with an animated Sub-Zero" width="860">

<img src="docs/img/demo-balcony.png" alt="The same demo rendering the Balcony stage" width="430">

<sub>**The native demo** — [`runtime/demo.c`](runtime/demo.c), a real OpenGL window driven by this project's C, with no Python and no emulator anywhere in the picture. **All 18 arenas draw**, each running whatever effect its own files declare — Graveyard's seven drifting mist bands, Balcony's sixteen torches, Pit's seven spinning blades. Sub-Zero is skinned and animated from `.bones`, `.skin` and `.skinanim`; the stage is assembled by walking the `.scene` graph and placing each object with the matrix palette the file carries, sized by its own `boundsRadius`. Each mesh's blend mode comes from its **name** — `ATST_*` means alpha test, which is what the engine itself does. See [Scene format](docs/SCENE-FORMAT.md).</sub>

</div>

The long-term goals, in order:

| Goal | Status |
|---|---|
| Understand the binary and its file formats | ✅ done — every LIME asset format is specified |
| Recover readable C source, function by function | ✅ **done** — 2,572 of 2,572, behaviourally tested |
| Replace the iOS platform layer with a native PC one | 🔄 started — window, GL, textures, files, sound, music, saves and focus pause run natively; the fight's input waits for the fight runtime |
| Fight data: the 229 tables | ✅ **extracted and verified** ([#46](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/46), [#48](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/48)) — 1,118 objects, byte-exact, checked by `ctest` on every build |
| Run the fight: the hand-over from the front end to the engine | 🔄 **next** — engine runs headless ([#46](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/46)), booting to character select is in open PR [#43](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/43)/[#50](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/50) |
| Widescreen, gamepad support, modding | ⬜ planned |
| **Local two-player on one machine** | ⬜ planned — [the iPad build has it](docs/IPAD-BUILD.md) |
| Restore hidden and unreachable content | ⬜ after a playable build |
| 60 fps, modern netcode | ⬜ long term |

**Nothing here is playable yet.** What *is* here is the whole game as readable, tested C, a working method, a large amount of verified knowledge, and tooling that makes the remaining work tractable. The remaining work is integration, not decompilation: a runtime for the fight engine, its data tables, and the rest of the platform layer.

---

## The companion repository

There is a second project: [**UMK3 — Godot Remake**](https://github.com/MaryNCRT/UMK3-IOS-GODOT-REMAKE).
It is **playable now** — one character, two players on one machine — and it is
built entirely out of what this repository has measured.

They are not the same effort with two front ends. They answer different
questions and they are finished at different moments:

| | **This repository** — step one | [**Godot Remake**](https://github.com/MaryNCRT/UMK3-IOS-GODOT-REMAKE) — step two |
|---|---|---|
| **Question** | *What does the original do?* | *Can we play it again?* |
| **Output** | Readable C, checked against a static ARM→C recompiler | A running game |
| **Fidelity rule** | The C must match the disassembly | The *behaviour* must match the measurements |
| **Renderer** | The original's own GL calls, transcribed | Godot's, written fresh |
| **Finished when** | The C compiles and plays | It plays like the phone game |

**Why the split exists.** This project transcribes the original *including* the
way it talks to the hardware: 366 direct OpenGL calls, because the 2011 game
made them. Putting a modern engine underneath that would mean either writing a
fixed-function GL shim on top of it, or editing the transcription until it no
longer matches the disassembly — and the second destroys the only property that
makes a transcription worth having.

So the work divides along the one seam where nothing is lost:

> **This repository owns the ANSWERS. The remake owns the ENGINE.**

When this project reads `strike_check_regs` and works out that a strike box is
`[X + x - w, X + x]` rather than `[X + x, X + x + w]`, that fact is not C and it
is not GL. It is just true, and it is as useful to a remake as it is to a
transcription. Every measured constant in the remake carries the same hex
address that appears in the notes here, so the two are a reference and an
implementation of the same subject and can be checked against each other.

Neither replaces the other. A faithful native port is still the goal here; the
remake is how the knowledge gets played with while that work continues, and it
has already sent findings back — the animation streams turning out to have two
parts each was discovered by a fall that looked unfinished on screen.

---

## Why this one is unusually tractable

Most decompilation projects begin by spending years answering a single question: *where does each function start and end, and what was it called?* Retail binaries are stripped; you get addresses and nothing else.

**This binary is not stripped, and it still carries its STABS debugging table.** That single fact changes the nature of the project:

- **4,342 named functions** — the original C and C++ symbol names survive
- **135 translation units** across 19 directories — the original source tree of EA's build, recoverable
- Every function is **attributed to the `.cpp` or `.c` file it came from**
- The original build path is embedded in the binary:
  `/BuildServerX/reactive/mortalkombat_iphone/xcode/umk3_iphone_en/../../src/`
- **`cryptid = 0`** — no FairPlay DRM. The code is readable end to end.

So we are not decompiling in the dark. We know that `RenderMesh.cpp` had 19 functions and what they were called; we know `mkdrone.c` had 394. That is the starting point most projects need years to reach.

---

## How the work is divided

The binary's 4,342 functions split into four very different piles:

| Part | Functions | What happens to it |
|---|---|---|
| EA commerce & social SDK (store, Facebook, analytics, JSON) | ~1,412 (33%) | **Deleted / stubbed** — none of it is needed offline |
| iOS platform layer (`lime/iphone`, audio) | 229 (5%) | **Rewritten natively** — new code, no reverse engineering |
| Networked multiplayer (GameKit) | 126 (3%) | Stubbed |
| **The actual game** (`lime/common`, `gamecode`, fight logic) | **2,572 (59%)** | **Decompiled** |

A third of the binary is commercial scaffolding that gets thrown away. Only the last row is real work.

---

## The method: never trust a decompiler

The central technical decision of this project — and the one worth stealing if you are doing something similar — is that **decompiler output is treated as a draft, never as truth.**

We built a second, independent path from the same machine code:

- **`tools/armrecomp/recomp.py`** — a static recompiler that translates ARM/Thumb to C *literally*, one instruction at a time, with CPU state in an explicit `arm_ctx` struct. It doesn't interpret; it transcribes. The output is unreadable, and that's fine: it is faithful by construction.
- Ghidra produces **readable** C, which is what we actually want to ship.
- A human-written clean version of each function is only accepted once a **differential test** proves it behaves identically to the recompiled one across thousands of inputs.

This is not paranoia. It caught a real, silent failure almost immediately:

```c
/* What Ghidra produced for _Len() — WRONG */
float _Len(float *v)
{
  float in_s0;                    /* never assigned */
  FloatVectorMult(uVar1, uVar1, 2, 0x20);
  FloatVectorAdd(uVar1, uVar2, 2);
  return in_s0;                   /* returns garbage */
}
```

It compiles. It looks plausible. It returns an uninitialized variable, because EA's compiler used **2-lane NEON instructions to do scalar math**, and Ghidra models those as opaque vector operations, losing the `vsqrt` entirely.

**153 functions across the binary are affected**, 23% of the engine core — and, measured properly, more of them are in `FrontEnd.cpp` and `GameCode.cpp` than in the engine at all. Without a second source of truth, that bug — and however many like it — would have surfaced a year later as "the models look wrong," with no way to trace it back.

The full reasoning is in [docs/METHODOLOGY.md](docs/METHODOLOGY.md).

---

## Overall progress

```
███████████████████████████████████████░  98.30%
```

| Area | Weight | Done | |
|---|---:|---:|---|
| Binary analysis and source-tree mapping | 4% | 100% | `██████████` |
| Tooling and the verification oracle | 8% | 100% | `██████████` |
| Asset format specifications | 8% | 100% | `██████████` |
| `lime/common` — engine core (109 fn) | 12% | **100%** | `██████████` |
| `gamecode` — game logic (291 fn) | 18% | **100%** | `██████████` |
| `gamecode/logic` — fight engine (2,172 fn) | 28% | **100%** | `██████████` |
| Native PC platform layer (161 fn to rewrite) | 17% | 90% | `█████████░` |
| EA SDK stubs (27 fn the game calls) | 5% | 100% | `██████████` |

**98.30% of the total estimated effort. Alpha 0.1.0 is 100% playable:** whole
fights by the real path, with known problems (see above).

**Read that number for what it measures, and for what it leaves out.** It
weighs the eight areas in the table, and two pieces of work are in none of
them:

- **The fight runtime.** The table counts the fight engine's 2,172 functions
  as written; nothing in it counts the loop, the thread scheduler and the
  glue that make them run frame by frame on a PC. **That runtime now exists
  and runs the real game**: front end, tower, fights with every finisher,
  the end of the match, Continue and the next fight, to the end of Arcade.
- **229 data tables.** Linking the fight engine for the first time left 423
  undefined symbols, and 229 of them are not code but arrays in the binary:
  the special-move command lists (`sm_*`), per-character parameters
  (`ochar_*`), animation scripts (`a_*`) and engine-wide tables such as
  `reaction_table`. A fighter cannot throw a special move, react to a hit or
  animate without them. **All 229 are now extracted and verified** ([#46](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/46), [#48](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/48)). They are extracted from
  each user's own copy at build time, never committed — see
  [docs/PROGRESS.md](docs/PROGRESS.md#the-other-axis-229-data-tables-nobody-has-counted).

So the percentage is honest about functions and formats, and **silent about
the two things that turned the engine into a playable fight.** Both are now
done, and the weights were never revised to make room for them: the bar
measures the decompilation and the platform layer, not how playable the game
is. What a player sees is in *Known problems* above and in the release
notes.

**The middle three rows are counted, the rest are estimates.** `tools/progress.py`
reads the tree on every run for `lime/common`, `gamecode` and `gamecode/logic`;
the other five are judgement calls a person maintains.

Those three used to be hardcoded in the script too, and it showed: `gamecode`
sat at 0% and `gamecode/logic` at 4% long after both had verified bodies
committed. The overall figure it produced was 34.82% against a true 35.04% —
**right to within a fifth of a percent by accident**, because one number was
too low by seven points and the other too high by four, and the weights nearly
cancelled them. A progress script that needs editing by hand to reflect
progress will be wrong; that it was wrong in a flattering-looking direction is
what made it survive.

**Why the foundational areas count for something.** The first three rows are
finished, and they are what makes the rest tractable: the source tree is
recovered, every asset format is specified, and every function now has an
automated path from machine code to a differential test. That is real progress
even though it renders no pixels.

**The engine core is the fourth row, and it is done.** All 109 functions have a
body; all nine of its files are also verified against the recompiled original.

**Every function has a body.** All 2,172 functions of the fight engine are
written, including `mkdrone.c` (the AI opponent, 394 functions), the last file
to close. What is left is not decompilation: it is the fight runtime, the 229
data tables above, and the platform layer. The tables are extracted and
verified ([#46](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/46), [#48](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/48)), and the runtime plays whole fights in
the game. What is left is fixing the functions whose
transcription does not quite match the binary, as each symptom turns up.

**The EA SDK row is at 100% because of what the game calls, not because of
the SDK's size.** Of the ~1,412 SDK functions, the game reaches exactly 27
(`EASDK_*` logging, ticker and network queries; `EASOC_*` Mayhem and Facebook)
plus the `LocaleManager` constructor. All of them are stubbed with their real
signatures in `runtime/gamecode_stubs.c`, and the build links with no other SDK
symbol. The remaining ~1,385 are SDK-internal: nothing in the port references
them, so they are dropped rather than stubbed, and none of their code is in
this repository.

### How much of it is verified

Two independent checks run against the binary, and they see different things.

- **`tools/factdiff.py`, static.** For every logic function it compares the
  stores, handler installs, state tokens and calls in the C against the
  recompiled original. Every logic file passes, with the exceptions listed in
  `tools/factdiff_waivers.txt`. It cannot see which constant a return path
  yields, or a handler fetched through a pointer slot.
- **`tools/difftest/`, behavioural.** Each decompiled function and its
  recompiled original run from the same randomised state in one 32-bit
  process, and the return value and the **whole data image** are compared
  afterwards. This is the check that found about 80 real transcription bugs in
  the fight engine (a token read from the wrong register, a branch dropped, a
  pointer slot resolved to the wrong routine) and several bugs in the
  recompiler itself.

| File | Functions | Tested | Failing | Which |
|---|---:|---:|---:|---|
| `other.c` | 333 | 260 | 0 |  |
| `mkdrone.c` | 394 | 393 | 1 | `t_fatality_align` (harness limit) |
| `moves.c` | 357 | 193 | 0 |  |
| `mkreact.c` | 207 | 207 | 0 |  |
| `mkzap.c` | 174 | 172 | 3 | `t_summon_spawn`, `t_summon_proc`, `t_sky_ice_proc` (harness limits) |
| `mkfatal.c` | 149 | 149 | 0 |  |
| `mkboss.c` | 104 | 103 | 0 |  |
| `mkprop.c` | 80 | 80 | 0 |  |
| `joy.c` | 73 | 72 | 0 |  |
| `mkanimal.c` | 63 | 61 | 0 |  |
| `mkstat.c` | 62 | 62 | 0 |  |
| `mkslam.c` | 60 | 60 | 0 |  |
| `mkfriend.c` | 45 | 45 | 0 |  |
| `mkcanned.c` | 20 | 20 | 0 |  |
| `mkcombo.c` | 16 | 16 | 0 |  |
| `mkbonus.c` | 8 | 6 | 0 |  |
| `mk3.c` | 19 | 2 | 0 |  |
| `playback.c` | 4 | 0 | 0 | nothing the oracle covers |
| `mkrepell.c` | 1 | 0 | 0 | nothing the oracle covers |
| **Fight engine total** | **2,172** | **1,901** | **4** | all harness limits |

One full run on 2 October 2026, oracles regenerated from the fixed `recomp.py`. `mkzap.c` reported 4 in that run; the fourth, `tl_bomb33`, was a real bug (a crossed pointer slot) and has been fixed and re-tested since. The two files not listed (3 functions) have no tests.

"Tested" is the number of functions the oracle covers in that file; the rest
are reached only through their callers (data-driven dispatch, or functions the
recompiler leaves to a jump table). The failures that remain are **harness
limits, each checked by hand against the disassembly**: a field the harness
seeds with a handler address, which the code then adds to or truncates, cannot
hold the same value in its ARM and native forms. They are named in
[docs/VERIFICATION.md](docs/VERIFICATION.md).

### `lime/common` is complete — and here is what that does and does not mean

All **109 of 109** functions in the engine core have a body. Every one compiles,
every one passes the structural gate, and the whole module builds clean with
`-Wall -Wextra`.

**It does not mean all 109 are verified.** Four modules have differential tests
against the recompiled original; the rest have been read, written and compiled,
which is a weaker claim and an honest one:

| | |
|---|---|
| Behaviourally verified | `Matrix`, `limeVector`, the `RenderMesh` loader, the `RenderSkinned` maths, the `Events` pool, the `LIMEDS_Misc` conversions |
| Cases compared | 103,907 synthetic, plus 590 files and 7,327 meshes of real game data |
| Divergences | **0** |
| Written, compiled, not yet run against the oracle | the remainder |

Several bodies are **structural**: the call sequence and the field access are
recovered, and a branch condition or a GL enum inside them is marked in the
comment as not pinned down rather than guessed. Those markers are the interesting
part of the file — they are where the next person should look, and they are
deliberately not smoothed over.

The rule that got here is written into [ENCARGO.md](docs/ENCARGO.md): a body over
an unconfirmed layout is worse than no body. It was tested twice. `symcheck`
rejected a `LIME_RenderSceneOverrideTextures` built on two invented accessors,
and the count went **backwards** from 104 to 103 before the real layout was
found. `LIME_UpdateEvents` had a confidently wrong body that only a differential
test exposed.

---

## Current status

| Module | Body written | Differential test |
|---|---|---|
| `Matrix.cpp` (11 fn) | ✅ | **40,006 cases, 0 divergences** |
| `limeVector.cpp` (2 fn) | ✅ | **20,013 cases, 0 divergences** |
| `LIMEDS_Misc.cpp` (8 fn) | ✅ | **21,950 cases, 0 divergences** |
| `RenderSkinned.cpp` (20 fn) | ✅ | **18,780 cases, 0 divergences** |
| `Events.cpp` (22 fn) | ✅ | **2,224 cases, 0 divergences** |
| `limeFont.cpp` (6 fn) | ✅ | **896 cases, 0 divergences** |
| `RenderScene.cpp` (14 fn) | ✅ | **80 cases, 0 divergences** — helpers only, see below |
| `DS_DebugWin.c` (7 fn) | ✅ | **58 cases, 0 divergences** |
| `RenderMesh.cpp` (19 fn) | ✅ | **590 files, 7,327 meshes, 0 divergences** |
| `other.c` — `SwitchQueue` (1 of 333 fn) | ✅ | **500 pushes, 0 divergences** |

**Every file in `lime/common` now has a differential test.** 84,000 synthetic
cases plus 590 files and 7,327 meshes of real game data, zero divergences
throughout.

One caveat stated rather than buried: `RenderScene.cpp`'s test covers its
helpers — the transparent list, the palette lookup, the mesh search — and **not**
the two scene renderers. Those walk a two-level animation table and issue GL
calls whose enums are not all pinned down; their bodies are marked *structural*,
and a test driving them would compare this project's reading against itself
exactly where the reading is least certain.

The percentage bar has not moved: the row these tests advance was already at
100%. What moved is how much of it is *trusted*, which the bar does not measure
and this table does.

Detailed status, decisions and known technical debt: [docs/PROGRESS.md](docs/PROGRESS.md).

---

## Things discovered along the way

**A file that will not parse is usually a variant, not corruption.** Three formats turned out to have more than one layout, and in each case the giveaway was the same: the alternative reading divides *exactly* rather than nearly. `.meshset` has three variants; `.bones` has two, at 24 and 25 bytes per bone; and `SINDEL_STANDARD.skinanim` uses a 16-byte header where the other 28 files use 12 — its count field read `1065353216`, which is `0x3F800000`, the float 1.0 being mistaken for an integer. `.bones` and `.skinanim` both now walk **29 of 29** files.

The same reasoning collapsed four separate "known exceptions" into one. `ROBO1` and `ROBO2` were failing in `.bones`, `.skin`, `.scene` and by having no `.events` — they are simply **a different export**. `ROBO2_STANDARD.skin` is exactly four bytes shorter than `SEKTOR_STANDARD.skin`, the missing block count, and their first 1,276 bytes are byte-identical.

**Every LIME asset format is now solved.** `.scene` was the last, and it is the one that shows why the project refuses near-misses: an earlier attempt fitted a formula matching **71 of 92** single-object files and was rejected rather than published. It was wrong — each object carries its own animation tracks, and a third array follows them all. Reading the loader instead gives all three strides directly, and the piece that had been missing was hiding in an addressing mode: `ldr r3, [r1, #0x28]!`, a pre-indexed load *with writeback*, which advances the cursor 40 bytes as a side effect of reading. **545 of 547 files** now walk to their exact last byte, and the walk depends on three counts that vary independently across 63, 74 and 175 distinct values.

The two that do not parse are `ROBO1` and `ROBO2` — **the same pair that breaks every other format**, reading a 24-byte bone rather than 25 in `.bones` and using the unindexed `.meshset` variant. Four formats, one consistent anomaly.

**The PVRTC decoder works — and the bug was in the test data.** The game ships 38 textures twice, as `NAME.PNG` *and* `NAME.pvr`, which is a free reference implementation that made downloading a third-party converter unnecessary. Against it the decoder scores **1.5% mean error** — 0.6% on 2bpp, 2.4% on 4bpp — and the residual is *proven* to be compression rather than a bug: it rises with the image's local gradient (4.75 in flat areas, 30–51 at hard edges) and is flat across block position. A 4×4 block blending two colours cannot hold an edge inside itself; that is exactly how block compression fails.

Getting there cost three wasted rounds. The decoder scored 5.5% and fourteen careful hypotheses all made it worse — because **three of the thirteen PNG/PVR pairs are different assets sharing a name**. `FE_METAL_BG`'s PNG frames the art differently; `MYBLOOD`'s is the unprocessed source with a magenta chroma key. That one file inflated the score from 3.83 to 14.00. Rendering the images side by side ended it in a single glance, and it is the third time this project has paid for not looking at the picture.

**The renderers are Apple's sample code, and so is a third of the platform layer.** `ES1Renderer.m` has exactly the four methods of Apple's `GLES2Sample` template — `init`, `render`, `resizeFromLayer:`, `dealloc` — and `ES2Renderer.m` adds exactly the four shader ones. Together with `Finch/`, **68 of the 229 platform-layer functions (30%) need no reverse engineering at all**. It also explains why the binary imports both `glGenFramebuffers` *and* `glGenFramebuffersOES`: the ES 1.1 template uses the extension names and the ES 2.0 path uses the core ones, one set per renderer.

**The NEON problem is period-normal, not an EA quirk.** On the Cortex-A8 that shipped in the iPhone 3GS and 4, the scalar VFP unit is not pipelined and NEON is — so doing scalar float maths with 2-lane NEON was *faster*, even wasting a lane. That was standard practice in 2010. It also explains why the armv6 slice is clean: NEON arrived with armv7, and the ARM11 chips armv6 targets have none. `Info.plist` pins the toolchain exactly: GCC 4.2 (not clang), Xcode 4.0, SDK 4.3, built on Snow Leopard.

**The other slice of the binary decompiles cleanly where ours does not.** The fat binary ships armv6 and armv7; the project always used armv7, which is where EA's compiler emitted 2-lane packed NEON for scalar float maths — the pattern that makes Ghidra silently drop the arithmetic. ARMv6 has no NEON, so the armv6 slice is an independent compilation of the same source in plain scalar VFP. `_Len` reads there as nine obvious instructions computing `sqrtf(x*x+y*y+z*z)`. **107 functions are affected in armv7 and not in armv6**, and more than half of them are in `FrontEnd.cpp` and `GameCode.cpp` rather than the engine — so the long-quoted "27% of `lime/common`" both overstated the engine (it measures 23%) and looked in the wrong place. `tools/slices.py`.

**The audio engine was never EA's to begin with.** `lime/iphone/Finch/` is a vendored copy of [zoul/Finch](https://github.com/zoul/Finch), an OpenAL sound engine under the MIT licence — all seven classes present with their pre-refactor names. That is **56 of the 229 platform-layer functions, 24%, that need no reverse engineering at all**. The general lesson is cheaper than the finding: before decompiling any platform module, check whether the class name belongs to a known third-party library of the era. `GBMusicTrack.m` was checked the same way and could *not* be confirmed, so it stays on the list.

**Every asset format needed to draw an animated character is solved.** `.meshset` (geometry), `.skin` (skinning weights), `.bones` (skeleton), `.skinanim` (animation) and `.events` (effect tracks) all read correctly against the shipped data. `.scene` is the last one open, though `LIME_LoadScene` has now given up its field map and the rule that binds the files together: **a scene is a family of siblings derived by replacing the last six characters of the name**, with no index and no manifest anywhere. See [MESHSET-FORMAT.md](docs/MESHSET-FORMAT.md), [SKIN-FORMAT.md](docs/SKIN-FORMAT.md) and [EVENTS-FORMAT.md](docs/EVENTS-FORMAT.md).

**Landing on a file's last byte can prove nothing at all.** If every record is the same size, *any* split of that size walks the file perfectly — 324 bytes reads equally well as 268+56 or 324+0. `.events` was audited as resting on exactly that circularity, because `numEntries` looked constant at 1. Across the full corpus of 1,547 tracks it takes ten distinct values and 103 tracks are not 1, so the walk was real evidence after all. A constant makes a walk worthless; a constant seen on part of the data may not be a constant. Both halves matter, and the layout is now derived from the loader's own pointer arithmetic so it does not depend on the walk either way.

**The move tables were sitting in the binary with their names on.** The in-game moves list reprints a move's first input every frame, and for a while the plan was to recover the tables by scrolling through that list with a log running ([issue #5](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/5)). Decompiling `MovesList` made that unnecessary: the tables are static data in `__DATA` with symbols — `_Kano_Moves5`, `_Kano_Moves6` and so on, 48 tables and 673 rows — and `tools/moves.py` reads them directly. See [docs/MOVES-TABLES.md](docs/MOVES-TABLES.md).

**The `.meshset` model format is solved and verified.** Not by guessing — by running EA's own `LIME_LoadMeshSet`, recompiled, against the game's real data and comparing what it leaves in memory to our specification: **590 files, 7,327 meshes, 2.9M vertices, byte-for-byte agreement** on indices, vertices, bounds and per-vertex lighting. See [docs/MESHSET-FORMAT.md](docs/MESHSET-FORMAT.md).

**We found a bug in a shipped asset.** One file, `KANO_STANDARD.lighting`, is exactly one byte shorter than its mesh set needs — 42,867 bytes for 42,868 vertices. The retail game reads one byte past the end of that buffer every time it loads Kano. Every other lighting file in the game matches its vertex count exactly. It took running EA's loader and our own side by side to tell "we misread the format" apart from "the data is wrong."

**Version 1.2.59 now runs in touchHLE, with a 2-byte patch.** The compatibility database only ever listed 1.0.4; as far as we know nobody had the final version working. The cause turned out to be a two-part failure: touchHLE reports preferred languages as short codes (`["es","en"]`), EA's locale table only recognises long ones, `getLocaleIndex` returns −1, and an `assert(false)` fires — which kills the emulator outright, because **touchHLE does not implement `___assert_rtn`**. Patching `LocaleManager::setLocale` to return immediately is enough. Full write-up: [docs/TOUCHHLE-PATCH.md](docs/TOUCHHLE-PATCH.md).

That patch matters beyond convenience: a running copy of the game is a **behavioural reference** for the decompilation, and it is the only one that will work for the fight logic, where static recompilation runs into function-pointer tables.

**The EA SDK does not need to be neutralised.** The original assumption was that ~1,412 functions of commerce and analytics would have to be disabled. In practice exactly one function blocked startup. `Mayhem`, `EASDK_Handler` and even the achievement system initialised fine. The operational rule that came out of it, which now governs the whole port: **no stub may ever call `assert()`** — EA's code checks invariants that a port cannot satisfy.

---

## Repository layout

```
tools/
  armrecomp/recomp.py    ARM/Thumb → C static recompiler (the verification oracle)
  patch_ipa.py           applies binary patches and repackages an .ipa
  decomp_driver.py       ranks functions by difficulty, drives Ghidra, verifies
  macho.py               Mach-O parser: slices, symbols, sections, stub resolution
  stabs.py               rebuilds the original source tree from the STABS table
  disasm.py              disassembles a single function by name
  cd.py                  compact disassembly with literals resolved and idioms folded
  factdiff.py            static fact diff: decomp C vs recompiled original
  difftest/              behavioural differential test, decomp vs recompiled, same random state
  archstats.py           ARM/Thumb ratio and mnemonic inventory
  rank.py                scores functions by difficulty
  meshset.py             .meshset reader (all three variants)
  skin.py                .skin, .bones and .skinanim reader and validator
  events.py              .events reader and validator
  pvr.py                 .pvr header reader and block-geometry validator
  pvrtc.py               PVRTC decoder to RGBA (1.5% mean error vs EA's PNGs)
  pvrtc_diff.py          diffs the decoder against EA's own shipped PNGs
  slices.py              extracts armv6/armv7 and finds NEON-affected functions
  scene.py               .scene reader and validator
  meshview.py            renders a .meshset to a PNG -- software rasteriser
  pose.py                poses a character from .bones/.skinanim/.skin
  glsurface.py           inventories every GL entry point the engine calls
  finishers.py           extracts the fatality/babality catalogue with frame indices
  animate.py             names the clips in an animation stream and plays them
  thumb_scan.py          finds ARM/Thumb boundaries
  umk3paths.py           locates an extracted IPA's res/ directory
  xref.py                finds calls to an imported symbol; recovers assert() arguments
  ghidra/                headless decompilation scripts
  signatures/            function signatures and struct layouts fed to Ghidra

decomp/                  the hand-written C -- the actual product
  lime/                  the LIME engine core (109 functions)
  gamecode/              the game: front end, players, blood, HUD (291 functions)
  gamecode/logic/        the fight engine: moves, reactions, AI, fatalities (2,172)
runtime/                 the native port around the decompiled code
  platform/              the OS boundary: Win32 and SDL2 windows, GL, audio, input
  lime_menu.c, draw_gl.c the iOS platform layer, rewritten (sound, saves, sprites)
  lime_app.c             the app lifecycle (focus = iOS foreground)
  menu_main.c            umk3-menu: the real front end in a window
  fight_*.c, test_main.c umk3-fight / umk3-test: the arena and fighter test scene
  arm_runtime.c          CPU/memory runtime the recompiled oracle executes against
tests/                   differential test harnesses
docs/                    format specifications, methodology, progress
```

Anything derived from the retail binary — recompiled C, raw Ghidra output, symbol dumps — is generated locally and excluded by `.gitignore`.

---

## Getting started

If you have never worked on something like this before, read **[docs/GETTING-STARTED.md](docs/GETTING-STARTED.md)**, then **[docs/HOW-THE-GAME-WORKS.md](docs/HOW-THE-GAME-WORKS.md)** -- how the game runs from boot to the end of Arcade, where each piece of code lives, the method that fixed every bug so far, and the debug and test tools. It assumes no prior knowledge of reverse engineering and explains what each piece is for, why it exists, and what you would actually do first.

The short version, for the impatient:

```bash
# 1. Prerequisites: Python 3.10+, a C compiler (MinGW-w64 or gcc), Ghidra 11+, JDK 21+
pip install capstone

# 2. Extract the armv7 slice from YOUR OWN copy of the game.
#    An .ipa is a ZIP archive; the executable is at Payload/UMK3.app/UMK3
python tools/macho.py thin path/to/UMK3 armv7 work/UMK3.armv7

# 3. Dump the symbols and rebuild EA's original source tree from the debug table
python tools/macho.py syms  work/UMK3.armv7 work/symbols.txt
python tools/macho.py funcs work/UMK3.armv7 work/functions.txt
python tools/stabs.py work/UMK3.armv7 work

# 4. See which functions of a module are easiest to attack first
python tools/rank.py work/UMK3.armv7 Matrix.cpp

# 5. Generate the reference implementation — the oracle — for that module
python tools/armrecomp/recomp.py work/UMK3.armv7 \
    --file Matrix.cpp --out recompiled --name matrix --with-deps

# 6. Build and run its differential test
gcc -std=c11 -O1 -I runtime -I recompiled \
    tests/test_matrix_diff.c decomp/lime/Matrix.c recompiled/matrix.c runtime/arm_runtime.c \
    -o build/test_matrix_diff -lm
./build/test_matrix_diff
```

Everything derived from the binary lands in `work/`, which is git-ignored. Set
`UMK3_WORK` to put it elsewhere, and `GHIDRA_HOME` before using
`tools/decomp_driver.py`. All paths are resolved by `tools/umk3paths.py`.

### Build and run the native port

Players use the launcher (above), which runs `launcher/build_game.ps1`. By
hand, with CMake:

```bash
# Windows (MinGW-w64 i686 + Ninja) uses Win32; Linux uses SDL2 and SDL2_mixer.
# On Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-mixer-dev libgl-dev
# The whole game needs a 32-bit compiler and YOUR armv7 binary: the fight
# engine's data tables are extracted from it at build time.
cmake -S . -B build -G Ninja -DUMK3_BINARY=path/to/UMK3.armv7
cmake --build build --target umk3-game

# Put res/ from YOUR .ipa (Payload/UMK3.app/res) beside umk3-game.exe and run it.
build/umk3-game
```

`umk3-menu`, `umk3-fight` and `umk3-test` are older development programs:
the front end alone, the arena test scene, and both in one. The mouse stands
in for a finger. Save files go to `save/` beside the exe on Windows and
`~/.local/share/umk3` on Linux (`UMK3_SAVE_DIR` overrides both).
`UMK3_SHOT=<n>` runs n frames, writes a screenshot and quits; the other test
variables are listed at the top of `runtime/game_main.c`.

---

## Contributing

Contributions are welcome, and the project is structured so that people can work in parallel without stepping on each other — each module is independent, and the acceptance criterion is objective.

**One rule matters more than the rest: nothing is done until it is checked against the binary.** Readable code that behaves *almost* like the original is worse than no code at all, because it fails silently and much later.

The decompilation is finished, so the open work has changed shape. Where help is most useful now:

- **The bugs still open** — the *Known problems* table above. Each needs the same method: find the function, read it against armv7, fix the transcription. Logs (`logs\`) and screenshots help.
- **Pad-driven menus**, the **monitor choice** and **other renderers** (DirectX 12 / Vulkan), from Mary's list.
- **Linux and macOS**: the game builds from source with CMake and SDL2; the launcher is Windows-only.
- **Port ideas never in the original**: a higher frame rate ([#23](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/23)), mods ([#29](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/29)). Widescreen ([#22](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/22), [#24](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/24)) is done in 0.1.0.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the working rules, and [docs/PROGRESS.md](docs/PROGRESS.md) for the detail.

---

## AI disclosure

**Large parts of this project were produced with AI assistance** — specifically Anthropic's Claude, working through Claude Code. This includes analysis, tooling, decompilation work, documentation, and this README.

We state this plainly because the reverse-engineering community holds differing and strongly-held views on AI-assisted decompilation, and because you deserve to know how the code you are reading came to exist. The details — what was AI-generated, what was human-directed, and how correctness was established regardless — are in [AI-DISCLOSURE.md](AI-DISCLOSURE.md).

The short version: every claim in this repository that could be verified, was verified, mechanically, against the original binary's actual behaviour. The differential tests exist precisely because neither a decompiler nor a language model can be taken at its word.

---

## Credits

**The game itself was made by other people, and none of them are us.**

*Ultimate Mortal Kombat 3* was created by **Midway Games** in 1995, designed by
Ed Boon and John Tobias. The 2011 iPhone conversion that this project studies was
built by **EA Mobile**, on an in-house 3D engine their code calls **LIME**. The
engineers who wrote it left their names on the work by accident — the debug table
they shipped is what makes this project possible at all. Whoever forgot to strip
that binary: thank you.

This repository contains none of their code. It contains our description of what
their code does, and our own reimplementation of it.

**This project** is maintained by [MaryNCRT](https://github.com/MaryNCRT), who
sets the direction, makes the scope decisions, and supplies the legally obtained
copy of the game that all the analysis runs against.

The tooling, analysis, decompilation and documentation were produced with
**Anthropic's Claude**, via Claude Code, under that direction. Commits carry a
`Co-Authored-By:` trailer where that applies. See [AI-DISCLOSURE.md](AI-DISCLOSURE.md)
for the full account of what that means and how correctness was established
independently of it.

### The banner

The *Ultimate Mortal Kombat 3* wordmark was **redrawn in UHD by
[u/JuananoLaGarza](https://www.reddit.com/user/JuananoLaGarza/)** and posted to
r/MortalKombat as
*[Ultimate Mortal Kombat 3 logo redone in UHD](https://www.reddit.com/r/MortalKombat/comments/mvm4uo/ultimate_mortal_kombat_3_logo_redone_in_uhd/)*.
It is used here with credit. If you are the artist and would rather this project
did not use it, open an issue and it comes down.

The Sub-Zero figure and the stage behind him are **renders by
[ermaccer](https://github.com/ermaccer)**, from
*[UMK3 iOS MeshSet Tool](https://ermaccer.github.io/posts/umk3iosmeshsettool/)*
(the files `csubzero.png` and `m_balcony.jpg`), used under
**[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)** — the licence that
post is published under. They are output from ermaccer's own converter, which is
also the tool our `.meshset` parser was originally checked against, so the
banner is quite literally made of the thing this project studies.

The "RECOMP" word, the iOS badge and the composition are by
[MaryNCRT](https://github.com/MaryNCRT).

**Yes, It looks like a cheap, ugly 2011 design for a pirate app.**

It was put together quickly and on purpose in the visual language of the thing
it is about: a mobile port from the era when every game's key art was a
character standing in front of a stage with the logo dropped on top and a
platform badge in the corner. Something slicker would have looked like it
belonged to a different game. This looks like it belongs to *this* one — a 2011
iPhone conversion of a 1995 arcade game, which is exactly what is being taken
apart here.

It is a placeholder and nobody is precious about it. But a project with no face
at all is harder to care about than one with a slightly silly face, and this one
is going to run for a year or more. Identity is not the work, but it helps the
work get finished.

If it is ever replaced, the thing to reach for is a mark that does not lean on
the trademark at all — that would serve the project better the more visible it
becomes.

---

## Prior work and acknowledgements

This project stands on other people's work:

- **[UMK3 — Godot Remake](https://github.com/MaryNCRT/UMK3-IOS-GODOT-REMAKE)** — this project's companion: a playable remake built on these measurements.

- **[touchHLE](https://github.com/touchHLE/touchHLE)** — high-level emulator for iPhone OS applications. Used as a behavioural reference, and the target of our compatibility patch.
- **[N64Recomp](https://github.com/N64Recomp/N64Recomp)** and **[Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp)** — the static recompilation approach that `recomp.py` is modelled on.
- **[BattleShip](https://github.com/JRickey/BattleShip)** — a Super Smash Bros. 64 PC port whose repository structure and legal model this project follows.
- **[ermaccer](https://github.com/ermaccer)** — [UMK3IOS.MeshSetTool](https://github.com/ermaccer/UMK3IOS.MeshSetTool), the first public tool for this game's mesh format and the reference our own parser was checked against. The renders in this page's banner are also his, from [his write-up](https://ermaccer.github.io/posts/umk3iosmeshsettool/), used under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **[Ghidra](https://ghidra-sre.org/)**, **[Capstone](https://www.capstone-engine.org/)**, and **[GhidraMCP](https://github.com/13bm/GhidraMCP)**.

---

## Legal

*Ultimate Mortal Kombat 3* and all related assets are the property of their respective rights holders. This project is not affiliated with, endorsed by, or connected to Electronic Arts, Warner Bros. Interactive Entertainment, NetherRealm Studios, or Midway Games.

The work here is reverse engineering carried out for **interoperability and preservation**: making software that no longer runs on any current platform run again, on hardware its owners already have. No game code or data is redistributed. Every tool operates on a copy the user already owns.

The project's own code is released under the [MIT License](LICENSE).
