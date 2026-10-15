# How the game works, and how to work on it

A guide for anyone who wants to help: what happens from the moment
`umk3-game.exe` starts to the end of an Arcade ladder, where each piece lives in
this repository, and the method that has fixed every bug found since the game
first ran. Written for the state of **alpha 0.0.5 (October 2026)**.

If you are new to reverse engineering, read [GETTING-STARTED.md](GETTING-STARTED.md)
first. For the rules of the project -- the binary is the only source of truth,
nothing from the game is ever committed -- read [AGENTS.md](../AGENTS.md) and
[CONTRIBUTING.md](../CONTRIBUTING.md).

---

## 1. What this project is today

Every one of the game's 2,572 functions has been rewritten by hand in C from the
iOS armv7 binary, and the result runs as a native Windows game:

```
your own UMK3 1.2.59 .ipa
        │  UMK3-Launcher.exe (launcher/)
        ▼
launcher/build_game.ps1
   1. download a pinned compiler (llvm-mingw) and Python into toolchain\
   2. check the binary (uuid 90d6f56a..., not encrypted)
   3. extract the data tables FROM YOUR BINARY (tools/logic_tables.py,
      level_info.py, seq_data.py) -- generated C, never committed
   4. compile decomp/ + runtime/ + those tables into umk3-game.exe (i686)
   5. copy res\ out of your .ipa
        ▼
umk3-game.exe + res\ + umk3.ini   -- the game reads only its own folder
```

Nothing of the game is in this repository or in a release: the launcher builds
the game on the player's machine from the player's own copy.

The game is **32-bit on purpose**: the fight engine stores addresses in 32-bit
words exactly as the ARM original did. Tools may be 64-bit; the game may not.

---

## 2. The game, from start to finish

### 2.1 The frame loop

`runtime/game_main.c` boots the game the way the iPhone does
(`-[UMK3AppDelegate startAppWithOptions:]`, then `GameCodeInit`), and then calls
**`GameCodeMain()` once per 60 Hz tick**. `GameCodeMain` runs one entry of
`TaskFunctionList[CurrentTask]`:

| `CurrentTask` | Function | What it is |
|---|---|---|
| 0 | `Task_LoadSplashScreen` | EA logo |
| 1 | `Task_LoadGeneralData` | fonts, text, settings, saves |
| 2 | `Task_FEInit` | loads the front end a step per frame (`FELoad_*`) |
| 3 | `Task_FEMain` | **the menus** (one front-end screen at a time) |
| 4 | `Task_FEDestroy` | frees the menus, hands over to a fight |
| 5 | `Task_GameInit` | loads a fight, one step per frame (`GameInit_LoadABit`, 53 steps) |
| 6 | `Task_GameMain` | **the fight** |
| 7 | `Task_GameDestroy` | frees the fight, picks the next screen |
| 8 | `Task_LoadingScreen` | the loading screen between the two |
| 9 | `Task_MultiplayerSync` | network play (not supported) |

`UMK3_LOG_TASKS=1` prints every change of `CurrentTask` and of the front-end
screen; every session started by double-click writes it to `logs\`.

### 2.2 The front end (`decomp/gamecode/FrontEnd.c`)

`Task_FEMain` draws one of **51 screens**, `FETaskFunctionList[FE_CurrentTask]`
(main menu = 0, Play = 1, Settings = 9, Character select = 27, Tower = 28,
Continue = 29, Select treasure = 36 ...; the names are in `FETaskNames`).
Screens are a **stack**: `PushFETaskDeferred(n)` starts a fade and pushes `n`,
`PopFETask` goes back. `FE_Special_Inits` / `FE_Special_Destroys` run on entry
and exit.

Two globals drive every transition: `FE_Fade` (0 black .. 1 visible) and
`FE_FadeAdd` (the step per frame, ±1/30). A screen ignores input while a fade
runs. When a fight-side fade reaches 0 with `DontQuitAfterFade` clear,
`Task_GameMain` ends the fight. **While an achievement banner shows, fades run
ten times slower** (`areAchievementsViewing`) -- worth remembering when a fade
looks wrong.

Buttons are `BUTTONNEW` structs drawn by `DrawButtonNew`, which cuts nine styles
out of the `FE_BUTTONS_01` atlas. Text is `limeDrawFONT` with `GameText(id)`
strings from the language files.

Arcade flow: Play → Arcade → **Character select** → **Tower** (choose a ladder,
`Destiny` 0..3, then one rung per fight, `Stage`) → VS screen → fight →
Continue / next rung → ... → Shao Kahn → **Select treasure**. The ladder is
`OpponentTowerList[Destiny * 11 + Stage]`; Motaro is always rung `Destiny + 6`,
Shao Kahn `Destiny + 7` (`PopulateTower`).

### 2.3 The fight

`Task_GameInit` loads the arena (`Level_Info[LevelSelect]`), both fighters
(meshes, skins, animations), sounds (`LoadAllSounds`) and effect scenes, then
`Task_GameMain` runs every frame:

```
Task_GameMain                      decomp/gamecode/GameCode.c
  ├─ input: keyboard / touch → the on-screen dial and buttons
  ├─ UpdateArcadeCode → mk3_update      the FIGHT ENGINE, one tick
  │      decomp/gamecode/logic/*.c (2,172 functions, the arcade game's logic)
  ├─ AddNewGameEvents                   what the engine asked for this tick
  ├─ camera, RenderLevel, RenderLevelPlayers, effects
  ├─ DrawHUD (health, clock, round coins, banners, the round end)
  └─ the fade, and the end of the fight
```

**The fight engine** is the arcade game's logic, rewritten for the phone. It is
a set of **cooperative threads** (`MK3THREAD`): each fighter, projectile and
effect is a process whose code is a chain of handlers. A handler returns how
many ticks to sleep, and parks a **token** in its frame to know where to resume
(`mk3_frame`, `mk3_install`, `mk3_unwind` in `decomp/gamecode/logic/mk3logic.h`).
That is why the logic files read as `if (token == 0x11aa) {...}` blocks.

Its state lives in a few big blocks shared with the front end
(`runtime/fight_runtime.c`): **`G`** (game state: health at `G+0x368` for player
1 and `G+0x36c` for player 2, 166 = full), **`H`** (round wins per player,
compared with 2 to end a match), **`GrObj`** (one 76-byte record per object; the
fighter's character at `+0x24`), `Plyr`, `Pp`. Its ~229 data tables
(`sm_*`, reaction tables, boss branches...) come from your binary through
`tools/logic_tables.py`.

**The engine never draws or plays anything itself.** It posts events --
`MKEvent_Add(type, subtype, param, player)` -- and `AddNewGameEvents`
(`decomp/gamecode/Blood.c`) turns them into the phone's effects:

| type | meaning | examples |
|---|---|---|
| 0 | blood | splats |
| 1 | camera | shake, centre on a fighter |
| 2 | sound | character voices (`ochar_sound`), groups |
| 3 | round and HUD state | health (`subtype 0`), clock, winner text, round end, FIGHT |
| 4 | effects (`create_fx`) | projectiles, FINISH HIM/HER (17/18), Shao Kahn's death (65) |

So a missing sound or effect is either an event never posted (engine side) or
an event mishandled (`Blood.c` side) -- and `Blood.c`'s switch is a jump table
you can read in the binary to settle which.

The **HUD keeps its own copies** of what the engine knows (`Health[]`,
`RoundWins[]`, refreshed by type-3 events). The round ends in two places that
must agree: the engine (`t_clock`, `t_round_is_over`, its tally `H[]`) and the
HUD (`RoundEndedAgainst`, `RoundSummaryUpdate`). Changing one without the other
is a classic source of bugs (see section 5).

At the end of a ladder, `t_results_retp` starts **`t_game_finished`** (Shao
Kahn's death: white flashes, then effect 65 plays `SK_ENDING.scene`) when
`RoundParam[0x3c]` is set -- only in Arcade, on the last rung -- and the
engine's tally reaches 2.

### 2.4 The engine underneath (LIME)

`decomp/lime/` is EA's engine (109 functions, all verified against the original
by differential tests): meshes (`.meshset`), skinning, scenes (`.scene`, `.events`),
animation, the font, matrices. Formats are documented in `docs/*-FORMAT.md`.

`runtime/` replaces what was iOS: `lime_platform.c` (files, saves, the clock),
`lime_menu.c` (touches, the 512-slot sound table, music), `draw_gl.c`
(textures -- the `.pvr` is tried before the `.png`, as the device does -- and the
2D drawing), `runtime/platform/` (window, OpenGL, keyboard and gamepad, audio
mixing, MP3 music; Win32 and SDL2 backends), `gamecode_globals.c` (generated
storage for every global the decompiled code declares, see 4.2).

---

## 3. Where things are

| Path | What it holds |
|---|---|
| `decomp/gamecode/` | the game: `FrontEnd.c` (menus), `GameCode.c` (fight loop, HUD, loading), `Blood.c` (engine events), `Players.c` (fighters), `achievements.c`, `sound.c`, `text.c`, `HudAnim.c` |
| `decomp/gamecode/logic/` | the fight engine, one file per original source file (`mkreact.c`, `mkfatal.c`, `mkboss.c`, `joy.c`, `other.c` ...) |
| `decomp/lime/` | the engine (LIME) |
| `runtime/` | the PC side: `game_main.c` (boot, frame loop, test aids), `debug_menu.c` (F2 menu), `fight_runtime.c`, `lime_*.c`, `draw_gl.c`, `platform/` |
| `launcher/` | `launcher.c` (the Windows launcher), `build_game.ps1` (the player's build), `make_release.py` (the release zip) |
| `tools/` | analysis and build tools: `cd.py` (annotated disassembly of a function), `macho.py`, `stabs.py`, table extractors, `difftest/`, `mkglobals.py`, `check_banned_words.py` |
| `tests/` | differential and format tests |
| `docs/` | this file, formats, history (`PROGRESS.md`), hand-over notes (`HANDOFF.md`), release notes |

Every function's comment starts with its **armv7 address and size**. Keep it
that way: it is how the next person finds the original.

---

## 4. How a bug gets fixed

Since the game first ran, **every bug has been a place where a transcription
and the binary disagree** -- never a "design" problem. The method is always the
same:

1. **Reproduce it** and note exactly what is seen (a screenshot helps).
2. **Find the code path.** Task and screen numbers from the log, the debug menu's
   info line (F3), and the function that draws or decides the thing.
3. **Read the original.** `python tools/cd.py <Function>` prints the annotated
   armv7 disassembly (pointer slots, literals and calls resolved). Compare it
   with the C line by line around the symptom. For pointer slots, compute
   `literal + pc` and look the address up in the symbol table.
4. **Fix the C to match**, and write *in the comment* the addresses that prove
   it, so the next reader does not have to redo the work.
5. **Test.** Rebuild (section 6) and reproduce with a scripted run; then a human
   checks it in the game.
6. **Document** the row in the README's *Known problems*, then commit.

### 4.1 The bug classes that keep coming back

All found in the last weeks, all invisible to a casual reading:

| Class | Example (fixed) |
|---|---|
| **A table declared as a pointer** | `spotlight_SpriteDef` was `void **`: the generated store was empty and callers passed the pointer's address -- eight screens crashed |
| **A raw iPhone address left in the C** | `t_rst5` stored `0x0017b8d0` / `0x0017b884` (`motaro_branches`, `sk_branches`) and read them: bosses crashed |
| **A symbol that does not exist** | `UnLoadSoundList` searched `SoundListUniqueIds` (not in the binary): no fight sound was ever freed, audio died after two fights |
| **Two branches merged into one** | FINISH HIM / HER have separate copies with `TriggerAnim(2)` / `(3)`; merged with a constant 3, men got HER |
| **Swapped constants** | `DrawButtonNew` had u/v transposed (every menu button wrong); achievement banners had x/y swapped |
| **A wrong loop bound** | `areAchievementsViewing` counts 20 slots (`cmp r2, #0x50`), not 24: five-second fades |
| **Swapped call arguments** | `DrawAnimAsSprite` called `__modsi3(n + 1, m)` instead of `(m, n + 1)` |
| **Two copies of one state** | debug wins changed the HUD's `RoundWins` but not the engine's `H[]`, so the ladder never ended |

When something looks wrong, check these first.

### 4.2 Globals and pointer slots

The decompiled files only *declare* globals; `tools/mkglobals.py` generates
`runtime/gamecode_globals.c` with storage for each, taking initial values from
the binary. The binary reaches most globals through a **pointer slot** (a word
holding the global's address). If the C declares `extern T *x` but the slot
points at a **table**, the generator makes an empty store -- declare it as an
array (`extern T x[]`) and regenerate:

```
UMK3_SYMBOLS="<symbols.txt>" python tools/mkglobals.py > runtime/gamecode_globals.c
```

Diff the result: only the globals you changed should move.

---

## 5. Testing without playing

### 5.1 Debug mode (for everyone)

Tick *Debug mode (F2 menu)* in the launcher. In the game:

| Key | Does |
|---|---|
| **F2** | the debug menu, over the frozen game: any fight (26 fighters, bosses included, any stage), any of the 51 screens, main menu, win/lose round or match, *Arcade: next is Motaro / Shao Kahn*, direct keys on/off, info line |
| F3 | info line: task, screen, fighters, rounds |
| F6 / F7 / F8 | previous / next screen, main menu |
| F9 / F10 | end the round (KO player 2 / player 1) -- only while a round is in play |
| F11 / F12 | win / lose the whole match (also counted by the engine) |

The menu's TOOLS page (Q / E turn pages): **HITBOXES** (each fighter's body
box, blue and green; every strike test's attack box in red and target box in
yellow for 20 ticks, white on a hit), **FIGHT SPEED** (10% to 400%, the fight
only), and **STATE SLOT / SAVE STATE / LOAD STATE** (below). A fight started
from the menu gives back the fighters, stage, mode, Arcade rung and AUTO
palettes when it ends; the PALETTE rows apply to debug fights only.

#### Save states (port only)

A save state is the exact moment, restored without restarting the game --
in a fight or on the select screen. How it can work at all:

- Every file is compiled with `-include runtime/savestate.h`. The game's own
  files -- `decomp/`, `runtime/lime/`, the generated tables in
  `build-game/`, and the runtime files that hold game state (listed in
  `CMakeLists.txt` and `launcher/build_game.ps1`) -- also get
  `-DUMK3_STATE_FILE`, which puts their globals and statics in the sections
  `.umkstd` / `.umkstb` and sends their `malloc`/`free` to an arena
  (`runtime/savestate.c`: one reserved range, power-of-two classes, its
  bookkeeping inside it). `tools/logic_tables.py` puts the engine's tables
  (where `G` and the players live) in `.umkstd$l`.
- A state is a copy of the two sections and the arena up to its top (about
  130 MB); loading copies them back, stops the sounds and restarts the tune.
  Four slots, in memory only: they die with the process.
- Textures are never deleted (`limeDeleteTexture` keeps its cache) and the
  cache itself is outside the arena, so a loaded state never points at a
  texture that is gone.
- Other platforms build the same code with save states off.

The colour picker on the select screen ("< COLOR 1 >" under each fighter,
`PortPalette` / `PortPaletteOf` in FrontEnd.c) is a port feature too: the
3D model (`RenderFECharacters`), the versus portrait and the fight
(`LoadLevelCharacters`) follow it, and with nothing picked they keep the
binary's rule. The pick is kept in `save\palette.txt` (runtime/game_main.c,
`palette_load` / `palette_sync`), so it survives a restart. An Arcade
ladder keeps the palette it began with (`PortArcadePalette`, saved in the
same file), so a ladder continued after a restart uses it too.

### 5.2 Scripted runs (for the person fixing)

Environment variables read by `runtime/game_main.c` (all listed in its header):

| Variable | Does |
|---|---|
| `UMK3_SHOT=<tick>` | write `umk3-game.ppm` at that tick and quit (`tools/ppm2png.py` converts it) |
| `UMK3_SHOTS=<t,t,...>` | several screenshots in one run, `umk3-shot-<t>.ppm` |
| `UMK3_TAPS="tick:x,y[,hold];..."` | scripted touches in 480x320 game units (a hold in ticks is optional) |
| `UMK3_SCREEN=<n\|name>` / `--screen` | open a front-end screen once the menu is up |
| `UMK3_SCREENS="tick:n;..."` | walk several screens in one session |
| `--fight p1 p2 [stage]` / `UMK3_FIGHT` | straight into a fight |
| `UMK3_ARCADE="destiny,stage"` | with `--fight`: that rung of an Arcade ladder |
| `UMK3_DEBUG_KEYS=1`, `UMK3_DBG_KEY="tick:k;..."` | debug keys, and F9+k pressed by script |
| `UMK3_DBG_OPEN=<tick>`, `UMK3_DBG_BOSS="tick:24\|25"` | open the menu / the Arcade boss jump by script |
| `UMK3_LOG_TASKS=1`, `UMK3_LOG_SOUND=1`, `UMK3_LOG_FADE=1` | logs: tasks, sounds loaded/played, fades |
| `UMK3_KEYS="tick:keys:hold;..."` | player one's keys by script; keys joined with `+` from `up down left right hp lp bl hk lk run s`, or `p b k r` for five buttons; held `hold` ticks (default 4) |
| `UMK3_LOG_P1=1` | in a fight, every change of player one's frame and of the engine's button word |
| `UMK3_LOG_PACE=1`, `UMK3_FRAME_DELAY=0`, `UMK3_PACE_HZ=<hz>` | input-lag pacing: log it once a second, turn it off, or pace as if the screen ran at that rate |
| `UMK3_CRASH_TEST=<tick>` | crash on purpose, to check the crash report |
| `UMK3_PAD=<auto\|off\|xinput\|vvvv:pppp>`, `UMK3_LOG_PAD=1` | the pad over `pad=`; log its buttons (and read it even in a scripted run). `umk3-game --list-pads` prints the pads attached |
| `UMK3_DBG_HITBOX=1`, `UMK3_DBG_SPEED=<percent>` | the TOOLS page's hitboxes and fight speed |
| `UMK3_DBG_STATE="tick:s1;tick:l1"` | save / load state slot 1-4 at those ticks: test a moment again without replaying up to it |
| `UMK3_DBG_FIN="tick:n;..."` | the FINISHER row (0-6) at the first FINISH HIM from that tick |

A scripted run ignores the real mouse and keyboard, so it is not disturbed by
someone typing in another window. Example -- the moves list in a fight:

```
UMK3_SHOT=1450 UMK3_TAPS="1350:10,10" umk3-game.exe --fight kitana sub-zero 2
```

A crash writes `logs\crash-<date>-<time>.log` and shows the same text in a
message box: the reason (access violation, division by zero...), the function
and offset by name (the exe keeps its symbols; outside it, the DLL), the call
stack by name, and the task, screen, fighters and stage. `UMK3_CRASH_TEST=<tick>`
crashes on purpose to check it.

### 5.3 Differential tests

For a function that computes rather than draws, `tools/difftest/` runs your C
and the recompiled original (`tools/armrecomp/`) on thousands of inputs and
reports any divergence. See [METHODOLOGY.md](METHODOLOGY.md) and
[VERIFICATION.md](VERIFICATION.md).

---

## 6. Building

**As a player:** the launcher (section 1).

**As a developer, fast:** after one launcher build, the object files and
response files are in `build-game\` next to the game. Recompile only what you
changed and relink:

```
<toolchain>\bin\i686-w64-mingw32-clang.exe -w @build-game\obj\<NNN_file>.o.rsp
<toolchain>\bin\i686-w64-mingw32-clang.exe @build-game\link.rsp
```

(Copy the changed source into the game folder's tree first; the `.rsp` files
point there.) Linking fails while the game is running -- close it, or link a
second exe for tests by changing the `-o` line of a copy of `link.rsp`.

**From source on Linux/macOS:** CMake, target `umk3-game` (see the README).

**A release:** `python launcher/make_release.py <launcher.exe> <out dir> --zip`,
then build that zip once from an `.ipa` to check it before publishing.

---

## 7. Where help is wanted

The open list is the README's **Known problems** table and the end of the latest
release notes. Each row says what was seen and what is known. Good first
contributions:

- **A visual bug** (textures, misplaced icons): find the draw call, compare its
  constants with `cd.py`. Several such fixes were one-line swaps.
- **A missing sound**: is the event posted (`UMK3_LOG_SOUND=1`)? Is the file in
  the sound table?
- **Port features** that were never in the original (widescreen, frame rate,
  in-game windows for the ads): issues #22, #23, #24 discuss them.
- **Linux/macOS launcher.**

Open an issue saying what you are taking. Send logs (`logs\`) with bug reports.
