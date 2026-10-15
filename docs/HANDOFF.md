# Handoff

> **The status section below was rewritten on 2026-10-02 and updated on
> 2026-10-09.** Most of the rest
> of this file is a dated diary of what each finished module settled; it is
> kept because the facts in it are still true, but its counts ("1,752 of
> 2,172" and the like) are history. The live numbers are in
> [PROGRESS.md](PROGRESS.md) and come from `python tools/progress.py`.

Written for whoever picks this up next, human or model, with no prior context.
Read this, then [METHODOLOGY.md](METHODOLOGY.md). Everything else is reference.

---

## Releases

The current build is **0.0.8 alpha** ([notes](RELEASE-0.0.8-alpha.md)), announced as 100% playable; 0.0.7b, 0.0.7, 0.0.6c, 0.0.6b,
0.0.6, 0.0.5 and 0.0.4 are still published, 0.0.1-0.0.3 were deleted from GitHub when the history was
rewritten on 2026-10-09 (their notes stay in `docs/`). What each release fixed,
with addresses, is in PROGRESS.md, "Release history".

**Since 0.0.9 (2026-10-15):** texture quality option and texture fixes; faster launcher (unpacked, stub) and loading (all cores, several steps a frame); iPad UI size (`ui_size=`); launcher update check, download and install from GitHub releases (`lib/update.js`); aspect ratios 4:3 (the iPad's projection) / 16:9 / 21:9 (`aspect=`). Released as 0.1.0. Next on Mary's list: monitor choice, render backend (DX12/Vulkan/OpenGL), wide-menu stragglers. See PROGRESS.md.

## Where the project actually stands (2026-10-10, alpha 0.0.8)

**Start with [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md)** -- how the game
runs, the code map, the bug method, the debug menu and every test variable.

- Every function is decompiled (2,572 of 2,572); the game plays the menus,
  Arcade to the end (treasure screen) and whole fights on Windows.
- The open list is the README's *Known problems*. Done in 0.0.6: the
  Electron launcher, Shao Kahn's death and the ending texts, the treasure
  softlock, the pier floor. Next in Mary's queue: texture errors on other
  stages and fighters (Kung Lao's hat), crashes in modes other than Arcade,
  fatality animations, white frozen fighters.
- **0.0.9 released (2026-10-15):** gamepads -- `PLAT_PAD` / `plat_pad_read` in platform.h; win32: XInput + WinMM (not DirectInput: its EnumDevices took 40 s on Mary's PC), the WinMM search on a thread; `keyboard_buttons` ORs the pad in by position; `pad=`, `pad_dbg_menu=`, `UMK3_PAD`, `UMK3_LOG_PAD=1` (also reads the pad in a scripted run). Launcher: `pads:list` runs `umk3-game --list-pads`. Next: drive the game's menus with the pad.
- **Session of 2026-10-15 (Mary's list, not released, checked by Mary):** F2 menu TOOLS page -- HITBOXES (`hb_*` in game_main.c; body boxes from the fighter object's extents +0x34..+0x40 against the engine floor G+0xac; strike boxes through `PortStrikeHook` in other.c's `strike_check_regs`, drawn by `PortAfterScene` in GameCode.c), FIGHT SPEED, SAVE/LOAD STATE (`runtime/savestate.c`; see HOW-THE-GAME-WORKS 5.1: every new runtime file that holds game state must join the state list in CMakeLists.txt, build_game.ps1 and any rsp). Debug fights restore everything when they end (`g_dbg_saved`, `DbgFightActive`). `drawKodeTip` re-read from armv7: one row, plus the kode's name at y 202. Colour picker on the select screen (`palette_picker`, `PortPaletteOf` in FrontEnd.c; Players.c and the VS screen read it). The dizzy slide is no longer seen. Next: Ermac's friendship/animality and the Mileena combo by eye; use `UMK3_DBG_STATE` to repeat a moment instead of replaying a fight.
- **Session of 2026-10-14 (Mary's list, not released):** hold-and-release moves (Mileena's sai, Liu Kang's bicycle kick) fire: `QueueAndJump` (other.c) stores `last_switch_ram` only for press entries (0x572d4 `cbz` skips 0x57322/0x57326). Input lag gone, checked by Mary: `AnimSmoothWindowSize = 0` in game_main.c (port), `glFinish` in `plat_swap`, `keyboard_buttons()` feeds `PortKeyButtons`/`PortKeyDial` read at the end of ReadControls (GameCode.c), taps latched (`plat_input_used`), one tick per 60 Hz frame, late key read before a 60 Hz refresh (`pace_frame`, `UMK3_LOG_PACE=1`; Mary's screen is 180 Hz, where it stays off). Crash report: `on_crash` in game_main.c names functions from the exe's COFF symbols, writes `logs\crash-*.log`, shows a message box. Debug fights clear `Character2Override` at task 6. Next: debug-menu hitboxes (FrameInfo boxes per GameObjects entry, drawn at the end of RenderLevelPlayers), check Ermac's friendship/animality and Mileena's combo in game, the dizzy loser sliding after a debug-forced finisher (the menu calls `DoASpecial` outside `mk3_update`).
- **Session of 2026-10-10 (released as 0.0.8):** video options done, as Mary asked: real 3D resolution (FBO, `render_init` / `render_present` in game_main.c), antialiasing up to x16, exclusive fullscreen (`plat_fullscreen_exclusive`) and borderless fullscreen, aspect 3:2 only (launcher; the game always uses 3:2), `hide_controls=1` (GameCode.c `PortHideControls`, the layout is still computed for touch), game language in PLAY, CONTROLS split into PLAYER 1 / DEBUG, Mary's logo code restored. Before that: the MERCY banner and tune last their full time (DrawHUD 0x2844e adds 0.2 a frame, not 13). Mercy and animality checked by Mary; every finisher works. Mary rebuilt and published v0.0.7b herself: use her launcher as the base and keep her logo. Next, as she asked: game language to PLAY; a video section with render resolution separate from window resolution (really rendered at that size, an engine change); fullscreen and borderless as separate options. `UMK3_DBG_FIN` takes `tick:n;tick:n` (a mercy, then the animality).
- **Session of 2026-10-09 (after 0.0.6c), fixed and checked by Mary, not released yet** (branch `claude/survival-fix`): Survival crash, Scorpion's spear, Shao Karnage HUD and difficulty, finisher banners, finish timing, typed 5-button finishers. Also fixed after that: the BABALITY banner (8 bouncing blocks) and the missing babality/animality/mercy/friendship banner and tune (`create_fx_param` must use the code read on entry, 0x58b68); 6-button babality typed D,B,B,HK works (`UMK3_BUTTONS=6` for tests). Watch: the dizzy loser sliding toward the winner (seen once more by Mary, not reproduced since). Mary's next requests: debug menu with mouse, one page per category (Q/E to turn pages, keys configurable in the launcher), palette 1/2 per player when picking fighters, the debug box moved to the launcher's PLAY section. Fixed after that, checked by Mary: Kung Lao's hat on the select screen (StaticMeshAmbient is 0 outside a fight; game_main.c gives the front end the body's light), white frozen fighters and Sub-Zero's ice clone (LoadAnimatedCharacter now loads `_DIFFUSE_ICE` on the fight path too, as 0x5c7f8 does; the diffuse is loaded with (0, 0) as every arm does). REA (rea-agents 6.3.0) is built at `E:\rea-rea-agents-6.3.0` and registered in the worktree's `.mcp.json` (PowerShell: use `npm.cmd`, not `npm`). The launcher's *PLAY* section has a *Skip the intros* box (`skip_intro=1`, or `UMK3_SKIP_INTRO=1`): game_main.c jumps `SplashCount` to 491 after the first splash frame, so the menu is up at tick 94 instead of 584 (checked by Mary).
- **Start with [OPEN-ITEMS-STUDY.md](OPEN-ITEMS-STUDY.md)**: what the binary says about every open item, with the first step for each, and Noob Saibot's status.
- **Next (Mary's list after 0.0.6b, findings in the README table):**
  projectiles not drawn (start from the spear: `UMK3_DBG_SPECIAL="1000:0"`
  `--fight scorpion reptile 13`, frame lookup -1 in `RenderLevelPlayers`);
  typed finishers and the FATALITY banner/voice (force one with the F2
  FINISHER row or `UMK3_DBG_KEY="1300:2" UMK3_DBG_FIN="1300:2"`);
  Kung Lao's hat on the select screen; Survival crash after a win;
  Sub-Zero's ice clone textures.
- Two transcription traps found in 0.0.6 are worth checking elsewhere:
  armv6-era bodies that look complete but are not (`LIME_RenderEvents` drew
  with an uninitialised matrix), and helpers whose name misleads
  (`StringInString` is strcmp, not strstr).
- Re-run `python tools/progress.py` before quoting a percentage (set
  `UMK3_FUNC_TO_FILE` to `OUTPUT/func-to-file.txt`); it is 87.59%.
- Fix locally in a built game folder (the fast rebuild in the guide, section
  6), test with a scripted run, have a human check it, then commit.

---

## The five rules that produced everything here

These are not style preferences. Each came from a specific failure.

**1. Never trust a decompiler.** Ghidra silently mis-decompiles the 2-lane NEON
this compiler emits for scalar float maths — `_Len` returned an uninitialised
variable and compiled fine. Every accepted function is cross-checked.

**2. Read the armv6 slice, not armv7.** Same source, two code generators. armv7
is packed NEON and unreadable; armv6 is plain scalar VFP. This is now the
default route and it has never failed. `tools/slices.py` extracts both.

**3. Visual evidence terminates a hunt; numerical evidence only sustains one.**
Four times this project burned hours on something one glance at an image ended
— touchHLE's off-screen gamepad, MAME's door interlock, a PVRTC "bug" that
lived in the reference data, and a model that turned out to be Z-up.

**4. Question the reference, not only the code.** Three of thirteen PVRTC
reference images were different assets. The decoder was right the whole time.

**5. A file that will not parse is usually a variant, not corruption.** The tell
is that the alternative reading **divides exactly rather than nearly**. This
closed ROBO1/ROBO2, SINDEL, and CUTUP.

### And one anti-rule, learned the hard way this session

**Measure before changing.** A model lying on its back was confidently
diagnosed as a texture-orientation problem, on reasoning that sounded airtight
— PVR row 0 is the top, GL's V=0 is the bottom, therefore flip. The fix broke
textures that were already correct. The bounding-box extents named the real
cause in one line and had been available all along.

---

## Hard boundary: leaked source

Leaked UMK3 retail source has been offered to this project three times and
declined three times. **Continue to decline.** Using it destroys the clean-room
basis; possessing a copy grants no licence to make derivative works; the
contamination is irreversible.

This extends to **third-party write-ups that are themselves readings of leaked
source** — Ryiron's arcade process analysis is explicitly a "Source Code
Review". Building on someone else's reading of leaked material is the same
contamination one step removed.

Arcade behaviour comes from **observing the ROM under MAME**, which this project
has already done, and from **our own binary's symbol table**, which names 4,342
functions. That rule is why the findings here are reusable at all.

`.gitignore` carries broad patterns to catch renamed folders.

---

## Legal model

**Zero game assets in the repository.** Every build extracts what it needs from
a copy the user supplies. The `LABORATORIO MAME/` folder is fully gitignored and
the ROM is never committed.

Documentation **screenshots are fine** and are in `docs/img/` — a render depicts
geometry, is not an asset file, cannot be unpacked back into one, and is not
part of any build. Both READMEs state this. Reading "no assets" as "no pictures"
protects nothing and costs the project its only visible evidence.

---

## The route that got here (mid-September 2026)

> Kept as written then; the current next steps are in "Where the project
> actually stands" above.

### 1. `gamecode` is finished — read it before starting the logic module

**291 of 291**, ten files under `decomp/gamecode/`. Nothing is left there, but it
is the reference for everything the fight engine will need, and the headers are
where the hard-won facts live.

**Work smallest-first.** This has been repeatedly worth it: the small functions
keep turning up the constants, strides and struct offsets the large ones then
need, and going in size order means you never block on an unknown. `checkIfKode`
(136 bytes) answered a question `resetKodeSelector` had left open; `getToken`
(156 bytes) caught a misread jump table in `initArguments`; `limeUC` (184 bytes)
explained why `getToken` starts at `s + 2`.

**The strongest case for it so far**: the front-end task functions corrected
*seven* wrong type declarations, none of which was findable from inside the
function that declared them. `limeCreateFONT`'s last parameter is a float and
not an int, `SFXHandle` and `MusicVol` are arrays and not pointers,
`exitTimeout` and `KontinueTime` are floats and not longs, and both
`EASDK_LogEvent` variants had an argument typed wrong. Every one was settled by
reading a caller. Assume any signature that has never been read from a call site
is provisional.

The tail was all large and it is all landed. `GameInit_LoadABit` (11,700 B) is
the asset manifest for a fight and produced [the roster table](ROSTER.md);
`DrawHUD` (11,536 B) turned out to run the round and match state machine;
`FE_Task_About_Help` (10,360 B), the largest in the front end, is one text block
written out thirty-seven times with the spacing hand-tuned per block.

**A big function is often mostly one thing repeated.** `GameInit_LoadABit` is 53
steps of which twenty are two lines each, plus four 24-way switches that are one
table written out four times. Find the repetition first — extract the tables
mechanically with a script — and what is left to read by hand is small.

**`tools/annotate.py` truncates at the first literal pool** inside a function,
so for anything of this size its output stops early — and silently, which is
worse. Several functions in this block have their last third past a pool. Use
`tools/disasm_range.py` for those:

```bash
python tools/disasm_range.py work/UMK3.armv7 0x00010348 0x00010474
```

It steps over undecodable halfwords as `.word` and resumes, and resolves the
`ldr rN,[pc,#imm]` / `add rN,pc` pairs the same way `annotate.py` does. Reach
for `annotate.py` first — it names call targets, which this does not — and fall
back to this the moment the output ends somewhere a branch says it should not.

**The bar for landing one is not negotiable:** `bash tools/check.sh` at **zero
errors and zero warnings in `decomp/gamecode`** (the 13 `unused parameter`
warnings elsewhere are pre-existing), `python tools/symcheck.py <file>
work/symbols.txt` at zero unknown callees, then `python tools/sync_figures.py`
to republish the figures, then commit. A warning tolerated once becomes noise
that hides the next real one.

Write each function into its file with the armv7 address, the byte size, and
**what it confirms or contradicts**. That last part is where the value has
consistently been.

### 2. Then `gamecode/logic`

3 of 2,172, and the real mountain. **Its boundary with `gamecode` is already a
function pointer**: `mk3_init`'s third argument is `FrameID_GetBBox`, handed in
at init rather than called directly (see `decomp/gamecode/training.c`). Any
indirect call inside the logic module that takes a frame id and returns a box is
almost certainly that callback.

Everything learned in `gamecode` about
`PLAYER` (stride 0x5f0), `ANIMATEDCHARACTER` (character id at +8, frames at
+4 with an 88-byte stride) and the two frame tables feeds straight into it.
`moves_data.x` already names **92 `t_` handlers and 90 `q_` predicates** — see
[X-TABLES.md](X-TABLES.md) and [issue #1](../../issues/1).

This is the part the verification oracle cannot reach, so the standard of
evidence has to come from somewhere else: MAME observation and the binary's own
symbol table, never leaked source.

### 3. The platform layer

229 functions, of which **56 are a vendored MIT copy of zoul/Finch** and need no
reverse engineering. The GL target is measured, not guessed: **77 entry points**,
all ES 1.1 fixed function, three texture units, no shaders anywhere. See
[LIME-ENGINE.md](LIME-ENGINE.md).

What exists today (2026-10-02): window creation and a GL context on Windows
(`win32_gl.c`) and everywhere else (`sdl_gl.c`, selected by CMake, or
`-DUMK3_BACKEND=sdl2` to force it on Windows), the asset readers under
`runtime/lime/`, and all 49 C entry points of `lime.m` transcribed from the
binary -- sound, music, save files, the face-me billboards, the app lifecycle
on window focus (`runtime/lime_menu.c`, `draw_gl.c`, `lime_app.c`). **The gaps
are the fight's input mapping and music on SDL2.**

### 4. Renderer fixes whose causes are now known

Not open searches any more — each has a named function behind it:

- [#20](../../issues/20) a mirrored fighter needs `glCullFace(GL_FRONT)` to go
  with its negative X scale. `runtime/demo.c` enables culling and never calls
  `glCullFace`, which is correct only while one fighter is drawn.
- [#17](../../issues/17) the second background layer — settled: all three
  functions (`RenderLevelBG`, `MaintainLevelScenes`, `AnimateBG`) and
  `GameInit_LoadABit` are decompiled; the second scene is a second filename in
  the stage record, drawn instead of the first or during a blast.
- [#19](../../issues/19) Graveyard's floor gap.

## Things that will bite you

- **The engine does not use one matrix convention.**
  `CreatePerspectiveMatrix` needs transposing for GL; `LIMEDS_SetObjectOrientation`
  does not. Do not apply a blanket rule.
- **Geometry is Z-up.** GL is Y-up.
- **`LerpVector3` runs backwards** from its argument order: `t = 0` gives the
  *second* argument. `GetSlerpedQ` blends the same way round, and despite its
  name it is a **lerp with a shortest-arc sign flip, not a slerp** — no acos, no
  sin, no renormalisation. See [SKIN-FORMAT.md](SKIN-FORMAT.md).
- **Transparency is ADDITIVE with depth writes off, and deliberately unsorted.**
  Additive blending is commutative, so insertion order is correct order and the
  missing depth sort is not an oversight. Switching to standard alpha blending
  to make smoke look denser makes the result order-dependent and turns the
  absence of a sort into a real bug.
- **Lighting is monochrome and has no ambient term.** Two directional lights,
  a `pow()` falloff on each, negated dot products, clamped to 1. Substituting a
  plain `max(0, dot)` will look visibly wrong. See [LIGHTING.md](LIGHTING.md).
- **Thumb is marked by `N_ARM_THUMB_DEF` in `n_desc`**, not by bit 0 of the
  symbol value. `macho.py` handles it; anything new must too.
- **`IsWhirlwindScene` matches a filename substring.** Repacking assets breaks
  effects with nothing to warn you.

---

## Tools worth knowing about

| | |
|---|---|
| `tools/disasm.py` | disassemble by name; resolves import stubs |
| `tools/slices.py` | extract armv6/armv7, find NEON-affected functions |
| `tools/glsurface.py` | inventory every GL entry point the engine calls |
| `tools/meshview.py` | render a `.meshset` to PNG |
| `tools/pose.py` | pose a character; `idle` finds the stance |
| `tools/animate.py` | name the clips in an animation stream and play them |
| `tools/finishers.py` | the fatality/babality catalogue with frame indices |
| `tools/armrecomp/recomp.py` | the verification oracle |
| `tools/dumpfn.py` | disassemble one function by name -- the thing you will run most |
| `tools/pending.py` | what is left, smallest first: `--logic mkzap.c 40` |
| `tools/protos.py` | every declaration against its definition; `--fix` corrects return types |
| `tools/samefn.py` | group functions by shape, so identical bodies are read once |
| `tools/instck.py` | installs that refuse where the binary does not -- see below |

### The third detector: `tools/instck.py`

There are now three things that read our C and disagree with it, and each
found a class the other two are blind to.

1. **The compiler.** It caught a 33-bit constant, and an optimising build
   caught a nine-iteration walk off the end of an array.
2. **`tools/protos.py`.** Files are compiled one at a time, so a declaration
   that contradicts a definition in another file is invisible until something
   compares them. Five real bugs.
3. **`tools/instck.py`.** `mk3_push_handler` is two things at once: the state-0
   refusal *and* the install. A routine whose only state is entry wants both,
   and the binary agrees -- it tests the slot, returns -3 when it is dirty, and
   only then stores. But a dispatcher that has already matched a non-zero token
   cannot test that token for zero; it IS non-zero, and the guard would refuse
   every single time. Those routines branch into a bare store instead.

   **116 sites were wrong at once**, across twelve files. Every one of them
   compiled, every prototype agreed, and every one of them would have returned
   -3 where the game installs a handler -- a fighter that never leaves the
   state it is in. Use `mk3_install` there; `mk3_push_handler` keeps the guard.

   Told apart in the disassembly by what reaches the store: a `cbz` on the slot
   with `mvn r0, #2` on the other side is push_handler; a plain branch in is
   `mk3_install`. Both live in `mk3logic.h`, next to each other, with the
   difference written down.

The lesson generalises past this one helper. **A convenience that bundles a
test with an action will be reached for by name, and the test comes along
unnoticed.** When a helper is written for one shape, the next shape that looks
similar is where the bug goes. Whenever you add a helper to `mk3logic.h`, ask
what it silently asserts, and write a checker for the sites that cannot afford
that assertion.

### The four readers, and what each will not do

Most of `gamecode/logic` is one function written many times, so three programs
read the repetitive parts and refuse the rest. `tools/genfile.py <file>.c` runs
all three and appends what they proved.

| | takes | refuses |
|---|---|---|
| `tools/pushfn.py` | the frame-push shape: a guard, stores, a handler installed | one instruction it cannot model, a handler that is not an address, a store it cannot place |
| `tools/microfn.py` | fixed templates -- a tail call, a constant into 0x5c, a table handed to a search routine | anything with an instruction out of place |
| `tools/leaffn.py` | straight-line leaves: stores, calls, a return | anything that branches, any return value it cannot prove, any value read from a field the function also writes |
| `tools/parkfn.py` | the token dispatch -- `it ne` / `mvnne r0, #2` -- and its two states | a state that is not exactly one of park, descend or install; a token, duration or handler that is not constant |

**The refusals are the point.** Each was added because guessing there produced
something wrong:

- `leaffn` refuses a value read from a field the function also writes because
  that is the borrow-and-restore idiom -- a value saved at the top and put back
  at the end. Written as a re-read it becomes `obj->field40 = obj->field40`,
  which it did, once, before the rule existed.
- `leaffn` calls a function `void` only when it can PROVE the last thing to
  touch r0 was a call, so what is left there is the callee's leftover. Assuming
  otherwise is what hid `randper`'s return value and `DrawSkinnedMesh2`'s count
  for months.
- `pushfn` masks its arithmetic to 32 bits because Python's does not, and
  `0xffffffb8 + 0x5d` came out as a 33-bit constant in a `uint32_t` field.
- `parkfn` tests for the CLEAR before the token, because a zero into the token
  slot is the clear that ends an install. Testing for a token first ate every
  clear in the directory and the reader accepted nothing at all.

**Two of `parkfn`'s three bugs were in the parsing, not the logic**, and both
came from copying a piece of `microfn` without the pass that follows it: a
`bl` carries its target on the same line rather than as a `; ->`
continuation, and `ldr rX, [pc, #n]` with no `add rX, pc` after it loads a
word rather than an address. Ninety-five functions were lost to the first and
every 0x16462 park to the second. If a reader's yield is surprisingly low,
suspect the parser before the rules.

**If a reader accepts something, check one against `dumpfn.py` before trusting
a batch.** Every bug above was found that way or by a compiler, not by
reasoning about the tool.

---

## moves.c is finished, and what DoASpecial turned out to be

`moves.c` is **357 of 357**. `DoASpecial` -- armv7 `0x000517f0`, 3328 bytes --
was the last one, and reading it corrected three things the earlier map in this
file had guessed at. They are recorded here because the same guesses are easy to
make again on the next large dispatcher.

**It is three dispatchers, not one chain of 84 comparisons.** The earlier map
read the bulk of the function as a flat per-character override chain at
`0x518ee`. It is not. `0x518ee` is a single override, two comparisons long. The
3328 bytes are:

    which - 0xd <= 6 and RoundParam[14] != 0   tbh, 7 arms, gated, drone
    which - 0xd <= 6 and RoundParam[14] == 0   tbb, 7 arms, ungated, own
    anything else                              tbh, 23 arms, one per character

The 23 character arms are each themselves a switch on `which` -- eleven as
inline `b.w` jump tables reached through `mov pc, r3`, twelve as `cmp`/`beq`
pairs. **That** is where the comparisons are.

**RoundParam[14] is the finishing window.** Blood.c settles it without any
guesswork: its event 17/18, the FINISH HIM/HER prompt, sets `IsInFinishing = 1`
and `RoundParam[14] = 1` in the same breath. The two finisher dispatchers are
therefore the same seven requests in two modes, and their handlers pair up one
for one -- `t_drone_mercy` against `t_do_mercy`, `t_drone_babality` against
`t_do_baby`, and so on for all seven. Only the window-open path writes
`proc+0x80 = 1` and only it asks a predicate.

**The `which = 0xd` stub does ask a predicate** -- `q_pit_fatal_ez`. The earlier
map recorded it as asking nothing. And two of the four predicates guard a move
they are not named for, which is not a mistake in either: `0x11`, the animality,
is guarded by `q_mercy` because an animality requires a mercy first, and `0x12`,
the babality, shares `q_friend_ez` with the friendship because both need no
punches thrown.

**The lesson that generalises.** The map in this file was built from resolved
addresses and instruction counts without following the control flow, and it got
the shape wrong while getting every address right. Resolving the constants is
necessary and is not sufficient: decode the branch tables before describing what
a function does. A `tbh` whose table disassembles as `lsls` instructions is data,
and its seven or twenty-three targets are the outline of the routine.

## mkslam.c is finished, and four mechanisms it settled

`mkslam.c` is **60 of 60**. Four things it established generalise to the eight
files still open, so read this before starting any of them.

**1. 0x1c carries a FUNCTION POINTER into `call_a0_for_him`.** The field is a
duration in one line, a sound index in the next and an address in the third.
`call_a0_for_him` takes the address out of 0x1c and runs that routine on the
OTHER fighter. Seven sites across the file, and `t_do_back_breaker` alone hands
across five different routines -- `clear_inviso`, `clear_shadow_bit`,
`player_normpal`, `last_knockdown_frame`, `do_next_a9_frame`. Two of those are
also called directly on this fighter a line or two away, so one routine reaches
both fighters by two spellings inside one state. **If you see a pointer slot
loaded into 0x1c, look for the `call_a0_for_him` two instructions later before
assuming the value is a number.**

**2. The four flight fields, and what is now known about each.** Twelve routines
fill 0x1c, 0x20, 0x24 and 0x28 and descend into `t_flight`. 0x24 is 0x8000 at six
of the sites, each reached by different arithmetic off a different literal -- so
it looks like a fixed parameter of a flight, not a per-move number. 0x28 varies by
kind: 4 for a slam bounce, 6 for a throw, 2 for `t_lao_slam`, 0xfff for the
drop-downs. 0x1c and 0x20 are two components and which one a routine zeroes says
whether the flight is sideways or straight down; `t_lao_slam` is the only one that
sets both to one value. **`t_flight_call` is a second variant** that also reads a
per-frame callback out of **0x34** -- one site, `t_do_back_breaker`, parking
`t_bb_fall_call` there.

**3. The ground-slam template, and the three ways it appears.** Eleven
per-character slams share one six-state shape: `body_slam_init` and a grab, a
park, a shout and a double wait, the handover, a wait, the pop. What a character
carries is five durations, five token values and one victim handler -- everything
else is inside `body_slam_init`, which is where the per-character tables are read.
The template appears **split** across `t_indian_slam` and `t_slam_ani2`, **fused**
in `t_tusk_slam`, and **fused with an extra wait** in `t_jade_slam` /
`t_mileena_slam`. `t_njsl3` is the template with its first call missing, because
`t_noob_slam` has already called `body_slam_init` with the part's character number
temporarily set to 0x12.

The victim handler comes from a small set, not one per character:
`t_thrown_by_sonya` serves three slams and `t_thrown_by_lao` serves six.

**4. One register can carry two tokens, decided several branches earlier.**
`t_sz_slam` and `t_sg_slam` both load `r8` with a token at entry and reassign it
inside the dispatch, on the branch taken when the incoming token is above some
value. Two `str.w r8` instructions in different states then write different
numbers. **Transcribing the store without tracing back to which load reaches it
silently gives two states the same token.** Two sites makes it the compiler's
idiom, so expect it again.

### obj 0x40 is overloaded, and t_axeup3 settles how

The mkslam note below recorded that `obj->field40` is "read THROUGH" in two
routines without saying what that meant. `t_axeup3` in mkstat.c answers it: one
state dereferences 0x40 and loops while the word it points at is non-zero, and
another advances it by 4. **So in those routines it is a cursor walking an array of
words, terminated by a zero.**

That covers every dereferencing site found so far -- `t_robo2_slam` and
`t_jax_slam` dereference without advancing, `t_jk6` advances by 4,
`t_do_unblock_hi` steps back by 4 twice, `t_axeup3` does both.

**It does NOT cover the many routines that hand 0x40 to `get_char_ani`,
`get_char_ani2` or `find_ani2_part2` as a small index** -- `tl_do_reflect` sets it
to 3 for exactly that, in the same file. The field is genuinely overloaded and the
surrounding calls are the only thing that says which meaning applies. Do not
normalise one into the other.

**A third meaning is a packed pair of halfwords**, handed to `t_animate_a9` or
`t_animate2_a9`. Six sites: 0x0005000d, 0x00040021, 0x00040047, 0x00030021,
0x00030002, 0x00030001. Every one has a small high half and a small low half, and
two share a low half with different high halves, so the halves vary independently
-- which is enough to say it is two fields and not enough to name either.

### And two things transcribed rather than explained

`obj->field40` is read THROUGH in `t_robo2_slam` and `t_jax_slam` -- `ldr` then
`ldr` again -- where everywhere else in the file it is a small animation number.
Nothing in either routine says what the pointer points at, so both are written as
a dereference with a note and no name.

`proc+0x40` is read with `ldr`, a full word, by a fourth and fifth routine against
a field `mk3logic.h` declares as a halfword. The header already records the
disagreement; these are reached by byte offset for that reason, as
`tl_do_lao_tele` and `tl_do_robo_tele` are.

## mkstat.c is finished, and five things it settled

`mkstat.c` is **62 of 62**. Five findings generalise to the six files still open.

**1. There are THREE ways to run a routine on the other fighter, not one.** The
mkslam note above found `call_a0_for_him`, which takes the address out of 0x1c.
mkstat.c adds two more, and `tl_do_leg_throw` uses all three in one function:

    obj->field1c = fn;  call_a0_for_him(obj);   /* through 0x1c   */
    call_for_him(obj, fn);                      /* in a register  */
    obj->field38 = fn;  takeover_him(obj);      /* installed      */

The third one is a handover -- the routine becomes the other fighter's thread
handler -- where the first two are immediate calls. Do not read them as
interchangeable.

**2. `obj->field48` and `obj->a10` are borrowed constantly, and the thread's own
argument stack is how they are given back.** The push/pop idiom at 0xa8 with the
cursor at 0xf8 shows up eleven times in this file, spanning anything from a single
call (`t_grab_animation`) to four states (`t_edge_of_world_lineup`). **Any state that
pushes must be matched against the state that pops** -- `tl_do_shake` pushes and pops
on every pass of a loop rather than once around it, and `tl_do_noogy` pushes in one
state and pops in another two states away.

**3. Several rules are stated in the move, not in the shared helper.** A boss is
exempt from `strike_check_a0` in three separate routines, each testing
`q_is_he_a_boss` itself. An airborne opponent is exempt from the quake and shortens
the swat gun's recovery by twenty-four frames. Damage is gated on the `p_hit`
counter in `t_r_leg_slammed` and nowhere else. **Do not assume a gameplay rule lives
in one place** -- these are per-move and there is no central table.

**4. `obj->field18` is a modifier three routines key on**, and each pays a different
price for it: `tl_do_lao_spin` recovers in 0x30 frames instead of 0x20,
`tl_stat_do_fan_lift` in 0x50 instead of 0x10, and `tl_do_shake` and
`tl_do_leg_throw` skip their whole payload. Whatever it means, it is consulted after
a connection and it always costs the attacker.

**5. Imports are named through `tools/imports.py`, and the stub arithmetic is
checkable.** `t_stat_do_uppercut` calls `fflush` four times. The stub at
`0x000dd794` is ARM (`ldr ip,[pc,#0]; ldr pc,[ip]`), its pointer word sits at
**stub + 8**, and stubs are **12 bytes apart** -- so the neighbours should read as a
run of alphabetically ordered imports, and here they do (`_dlsym`, `_fflush`,
`_floorf`, `_free`). That cross-check is worth doing every time, because a
one-stub error still produces a plausible libc name.

## mkanimal.c is finished, and six things it settled

`mkanimal.c` is **63 of 63** -- the animality module, and the eighth logic file
closed. Twenty of its functions are the per-character drivers `t_do_animality`
installs out of `ochar_animalities`, and reading all twenty against each other is
what produced most of the findings below. They generalise to the six files still
open.

**1. The victim reactions are a shared POOL, not one per animal.** This is the
single most useful thing the module taught. A driver is a *schedule*; the routine
the victim runs is picked from about a dozen shared reactions:

    t_lion_mauled     tl_jax_lion, tl_sz_polar, tl_lao_cheetah, tl_indian_wolf
    t_bit_in_half     tl_liu_kang_dragon, tl_swat_dino
    t_dino_bucked     tl_kabal_skeleton  (and the dinosaur it is named for)
    t_eaten_by_snake  tl_shang_tsung_snake, and as t_eaten_by_shark's own tail

So four animals maul identically and differ only in the model on screen and one or
two calls. **Do not assume a `t_r_*` or `t_*_mauled` routine belongs to the
finisher it was first seen in.** The same warning applies to sounds: 0x92 looked
like the dragon's roar until the dinosaur and the kitty played it, and 0x95 looked
like the polar bear's until the cheetah and the wolf did.

**2. A field that has to survive a call gets parked, and 0x40 is the field that
needs it.** `t_mframew` and `t_animal_morph` walk `obj->field40` forward as a
cursor and leave it past the end, so every routine that descends into them twice
has to reset it first. Three different slots are used for the copy:

    tl_sheeva_scorpion   writes the constant `a_scorpion` again
    tl_mileena_skunk     saves it in obj->a10 (0x44) and reloads from there
    tl_jade_kitty        saves it in proc->field28 and reloads from there

The second `obj->field40 = a_<animal>` in most drivers is not redundant; it is this
reset written with a constant. **If you see a field written twice with the same
value around a descent, look for a cursor before calling it dead.**

**3. `proc->field28` now has four independent readings and none is wrong.** The
header calls it the shake target on the authority of the two shake routines.
`t_r_rabbit` reloads a frame countdown from it every frame. `tl_sektor_bat` parks an
x velocity in it. `tl_jade_kitty` parks a pointer in it. Four uses of one word, all
measured, none reconciled -- but every one of them is a place to put something that
has to outlive a call. Record which one your routine means; do not pick a name.

**4. Adds that overflow 32 bits are real and must be transcribed as adds.** Three
sites in this file compute a second field from the first with an `add` that wraps:

    t_hit_by_bull      0xfff80000 + 0x86000  -> 0x00006000
    tl_kitana_bunny    0xfff80000 + 0xe0000  -> 0x00060000
    tl_sektor_bat      0xffffe000 + 0x82000  -> 0x00080000

Writing the truncated value directly compiles and behaves identically, and hides
that the two fields come from ONE literal. Write the addition and note the wrap.
The same applies to the much commoner non-wrapping case (`adds r3, #5`,
`subs r3, #2`) -- one literal feeding two or three fields is the house idiom for
`t_shake_ob_up`, `t_flight` and `multi_adjust_xy` callers.

**5. Five routines place a body on the floor and no two agree.** All five write
floor-minus-height into the part's 0x12, and they differ in how they read the floor
and how they measure the height:

    ground_ob (here)          ldr  G+0xac   GetFrameHeight(part->field2c)   -9
    tl_reptile_monkey         ldr  G+0xac   mk3_getbbox, 0x40 - 0x38        none
    t_turn_into_a_baby (stat) ldr  G+0xac   mk3_getbbox                     none
    tl_kitana_bunny           ldrh G+0xac   GetFrameHeight(*list)           -9
    tl_jade_kitty             ldrh G+0xac   GetFrameHeight(part->field2c)   none

Every combination but one, in five hand-written copies. **This is not an oversight
in any single routine** -- it is what the codebase does instead of a helper, and a
port that unifies them will change behaviour. Issue-worthy if the placements ever
look wrong on screen.

**6. A predicate can be a parameter.** `t_animate_till_a11` calls `obj->field48`
through a register every frame and stops when the callee sets 0x5c.
`tl_sektor_bat` is the routine that exists for: it puts `q_bat_1`, `q_bat_2`,
`q_bat_3` and `q_bat_4` in 0x48 in turn, so the bat's four legs of flight end on
four different conditions rather than four different lengths. That also explains
why `q_bat_3` is behaviourally identical to `q_bat_1` at a different address --
two states use them, and a duplicate test costs nothing.

The same question -- "have I arrived?" -- is spelled five ways in this one file:
through a `q_*` predicate in 0x48, by calling `get_x_dist` and branching on 0x28
(`tl_smoke_bull_shit`, `tl_sonya_eagle`), by subtracting two objects' 0x0e by hand
with `rsblt` (`tl_scorpion_pengo`), by `distance_off_ground` and 0x1c
(`tl_sonya_eagle` again), and by not asking at all and teleporting with
`match_me_with_him` (`tl_cyrax_shark`). **Expect no shared helper where one
obviously belongs.**

**Two smaller things worth carrying forward.** `wfe_him` is 24 bytes that put
`t_wait_forever` into 0x38 and call `takeover_him` -- so a routine whose victim
reaction is an endless loop (`t_lion_mauled`, `t_stung_a_bunch`) does not need an
exit, because the attacker ends it from outside. And `obj->field54 = N` immediately
before `find_part_a14` is that routine's argument, on the authority of two files
agreeing (`tl_lao_cheetah`, `tl_kano_spider`, and mkprop.c with a different N).

**One thing this file made me delete twice.** Both `tl_kitana_bunny` and
`tl_sonya_eagle` first went in with an `extern` I had invented -- a word list the
driver does not have, and a name for a slot whose symbol is already
`death_scream`. Both compiled. **A clean compile is not evidence that a
declaration corresponds to anything**, and an invented name in this tree is worse
than a missing one, because the next reader will trust it.

## mkfatal.c is finished, and seven things it settled

149 of 149. The file is the finishers -- every fatality, the victim reactions
they hand over, and the small helpers both ends share -- and it was read from
the outside first, by `mkanimal.c`, which is why so many of its routines already
had names before they had bodies.

**Almost every chain in it closed inside it.** Six full chains and a dozen
orphaned helpers found their callers without leaving the file:

    t_jade_impale    -> t_r_impale_upcut -> t_flight_call -> t_impale_call
    t_jax_slice      -> t_get_sliced_up  -> t_post_sliced_up
    t_kano_lazer     -> t_local_r_laser
    t_smoke_arm      -> t_open_wide + t_smoke_dropping -> t_eat_this_shit
    t_ind_light      -> t_light_animator
    t_kitana_kiss    -> t_r_stretch
    t_kano_skeleton  -> t_skin_fall
    t_sz_lift_n_freeze -> t_liftshake, t_freeze_into_boomer, t_frozen_half_ani
    t_scorpion_hell  -> t_another_scorpion (six of them)

If you are opening one of the remaining files, expect the same: the small
routines are written first because they are small, and their callers turn up
two hundred functions later.

### 1. A handover can be revoked, two ways

`obj->field38 = fn; takeover_him(obj)` gives the other fighter a routine. An
**endless** victim routine is therefore safe to write, because the attacker owns
the schedule and 0x38 is a slot it keeps writing. Two ways to end one:

- **a second handover** -- `t_jax_slice` gives the victim `t_get_sliced_up`
  and then, forty-eight frames later, `t_post_sliced_up` through the same slot.
- **`wfe_him`** -- `t_kano_lazer` ends `t_local_r_laser` by parking the other
  side outright (`field38 = t_wait_forever` + `takeover_him`, which is all
  `wfe_him` is).

Neither is visible from the victim's side. `xfer_otherguy` is a third spelling
of the same 0x38 transfer; a port needs both it and `takeover_him`.

### 2. The comparison value doubles as the next token

This is the single most dangerous shape in the file, and it appears in almost
every large routine. The dispatcher loads a token into a register to compare
against, and a state later stores **that same register** as its successor:

    t_smoke_arm         r6 = 0x924, reloaded 0x946 on the greater-than path
    t_jax_slice         r1, ip and r6 hold three different successors
    t_cyrax_helecopter  three states share one `str.w r6` -- three tokens
    t_scorpion_hell     r6 is 0x30e for one state and 0x312 for another

**Reading a shared tail alone gives some states the wrong successor.** Trace
every store back to the load that reaches it. It is why the eleven-state chain
in `t_cyrax_helecopter` is written with its constants spelled out rather than as
`token + 1`: the binary never computes that.

### 3. The argument stack, seventeen sites, and when a register will do

`args[]` at 0x a8 with the cursor at `fieldf8` is a real stack -- `t_kissani`
pushes two values and pops them in order. It exists for one reason: **one object
field has to serve two callees with incompatible meanings.**

`t_sg_pound` settles the choice between it and a register. Its state 0xbac pops
a value into 0x40, **pushes it straight back**, and descends; the state on the
far side pops it again -- because a register cannot survive a descent, the
handler having returned. So:

- a span inside one state can use a callee-saved register (`t_ind_light` keeps
  0x40 and 0x08 in `sl` and `fp`, which is why it pushes `{r8, sl, fp}`)
- anything crossing a descent needs `args[]` or a spare object field

`t_sz_lift_n_freeze` uses both forms in one function, so the compiler picked
whichever it had room for.

### 4. Nine per-character tables, in three shapes

None was referenced anywhere in the tree before this file was opened:

| table | address | shape |
|---|---|---|
| `ochar_reached` | 0x1667c0 | words |
| `ochar_reached_kn` | 0x166a34 | words (animation cursors) |
| `ochar_wide_adjusts` | 0x166b54 | words, negated on use |
| `ochar_shocked_ani` | 0x166e08 | words |
| `ochar_staff_lineups` | 0x166af4 | words |
| `taser_lineups` | 0x166c44 | signed halfwords |
| `ochar_flesh_lineups` | 0x166bb4 | signed halfwords |
| `ochar_headrip_lineups` | 0x166a94 | packed halfword pairs |
| `ochar_skeleton_adj` | 0x166be4 | packed halfword pairs |
| `ochar_laser_lineups` | 0x166c78 | packed halfword pairs |

All are indexed by `part->field24`, the character number. The packed ones are
unpacked with `asrs #16` and `lsls #16; asrs #16` -- high half into 0x20, low
into 0x1c -- which is the same encoding `skinny_spawn` reads out of a field.

### 5. Cross-object data has one channel, and it has four sites

`proc->field00->field48 = obj->field40` -- fill the other fighter's 0x48 right
after handing it a handler that reads 0x48. `t_kabal_inflator` and
`t_kitana_kiss` both feed `t_r_stretch` this way; `t_lia_scream_rip` feeds
`t_r_scream_ripped`; `t_kano_skeleton` feeds `t_skin_fall`. **Two readers, two
writers each, one channel.** Hand over the handler, then fill the field.

### 6. Two or three agreeing sites settle nothing

Three readings recorded in this file did not survive their next counter-example,
and the notes were softened in place rather than deleted:

- `t_animate_a0_frames`' packed pair "always has 5 in the high half" -- four
  sites agreed, then ten sites read 5 5 5 5 8 3 3 2 4 3.
- `t_shake_ob_up`'s 0x1c as an encoded pair -- until `tl_r_scared_of_mileena`
  passed both kinds in three consecutive descents.
- `t_flight`'s 0x28 as a small "bounce kind" -- two sites had 4, then
  `t_soul_float` passed 0xfff.

**The routines are hand-written and the exceptions are not rare.** Say what a
field does at the sites you measured, and say how many there were.

### 7. What did NOT close

- ~~**`delete_slave` has seven sites and nothing in the tree creates the
  slave.**~~ **Answered by `get_proj_obj_m` in mkzap.c**, read the same day: it
  starts a thread on `t_wait_forever`, parks the new object in `proc->field64`
  and its part in `proc->slave`, and hands the part back in `a10`. A slave is a
  second object with no behaviour of its own. The set is complete -- **create**
  (`get_proj_obj_m`), **let go** (`detach_proj`), **destroy** (`delete_slave`).
- **Does `frame_a9` advance 0x40?** Two loops say it must: `t_scorpion_flame`
  descends into `t_double_flame_ani`, which does nothing but call `frame_a9`
  and pop, and then tests the word 0x40 points at. Neither `frame_a9` nor
  `do_next_a9_frame` is decompiled. Settling it also settles the loop in
  `t_sonya_kiss`, whose note was rewritten to say so.
- **Does `t_mframew` write 0x48 back?** `t_cyrax_self_destruct` captures a
  cursor once and runs `t_animate_a11` three times without recapturing.
- **Does `NewThread` carry the owner's `a10` to the new object?**
  `t_scorpion_hell` writes an x into its own `a10` before each spawn and
  `t_another_scorpion` reads one out of the spawn's.
- **`t_st_spike` does three quarters of `tl_do_swat_zoom`'s pose hand-off** and
  never writes the fourth part. `t_double_mframew` (slot 0x000f36a8) is the
  candidate reader and is not decompiled.

## Port-critical: eight hard-coded character numbers

The engine is almost entirely character-agnostic -- animations are looked up as
`base + part->field24`, tables are indexed by `part->field24`, and nothing else
in 1,400 read functions names a fighter. **Five places break that rule**, and a
port that renumbers the roster has to carry every one of them:

| where | file | test | what it changes |
|---|---|---|---|
| `tl_kano_spider` | mkanimal.c | opponent `== 0xb` | shifts the lineup by 0x20/0 |
| `t_ripped_skelton` | mkfatal.c | part `== 0xb` | skips one word in the cursor |
| `t_ripped_skelton` | mkfatal.c | part `== 0xb` | parks instead of running the blood |
| `t_remaining_skel` | mkfatal.c | part `== 0xb` | skips one word in the cursor |
| `t_kitana_kiss` | mkfatal.c | opponent in `{7, 8, 0xe}` | pose `0x00070025` instead of `0x25` |
| `proj_strike_check` | mkzap.c | opponent `== 0x18` | projectiles cannot hit Motaro |
| `t_sz_post_zap` | mkzap.c | part `== 0x15` | a different animation finder, with different arguments |
| `t_rocket_explode` | mkzap.c | opponent `== 0x18` | Motaro reflects the rocket back at its owner |

Four disjoint sets: **0xb** on its own in four of the eight, **{7, 8, 0xe}** in
`t_kitana_kiss`, **0x18** -- Motaro -- in `proj_strike_check`, and **0x15** in
`t_sz_post_zap`.

`t_sz_post_zap` is the easiest of the seven to miss: it runs the ordinary lookup
first and throws the result away for that one character, so the exception reads
as a wasted call rather than as a choice.

**mkzap.c also has a NAMED predicate for the same question**, `is_he_motaro`,
and `local_strike_check_box` calls it where `proj_strike_check` inlines the
comparison. A named predicate is not the hazard; the inlined one is, and it sits
five instructions from a routine that would have answered it. The `{7, 8, 0xe}` test is compiled as `cmp #0xe` plus an unsigned
`(char - 7) <= 1`, which is why it does not look like a set test in the
disassembly -- if you are scanning for these by eye, that is the shape to watch
for.

Note that 0xb is tested against **the part** in three of the four and against
**the opponent** in the fourth, so a port cannot fix them all in one place.

## Open questions worth someone's time

- **`.lighting` is a prelight bake and is not decoded.** 13 files, sizes scaling
  with vertices x frames at ~2 bytes each, bytes 62% zero — delta coding or
  compression. If it is what it appears to be, `LightVert` is the *fallback*
  path and most characters are lit from the table instead.
- **Each `*FRAMES.bin` is exactly 14,490 bytes** for every character with no two
  identical — a fixed-length table, presumably the compiled form of the text
  frame list. Layout undecoded.
- **`word0` of a bone record** is not the child count. Meaning unknown, and
  nothing needs it.
- **The hidden roster is reachable, and this is now settled.**
  `drawCharacterSelection` gives the exact routes:

  - **Smoke** — hold on Human Smoke (cell 14) for 180 frame-rate-corrected
    units, about three seconds. The code then turns the cell into character 22
    and writes `limeLastTouchScreenX = -1`, *fabricating a release* so the hold
    completes through the ordinary tap path.
  - **Ermac, Mileena, Classic Sub-Zero** — the three ten-digit kodes in
    `FE_Task_Enter_Kode`: `1234444321`, `2226422264` and `8183581835`, setting
    `ErmacUnlocked`, `MileenaUnlocked` and `ClassicSubZeroUnlocked`.

  Every cell is also matched against **two** tables, `CS_Layout` and
  `CS_Layout2`, so one grid position can stand for two fighters. See
  [HIDDEN-CONTENT.md](HIDDEN-CONTENT.md).

---

## Keep doing this

Update `PROGRESS.md` at the end of each block of work, keep the percentage bars
honest — **they did not move for several very productive sessions and that was
correct**, because the row those sessions advanced was already at 100% — close
issues that are resolved, open issues for what you find, and record the failures
alongside the results. Half the value in this repository is in the paragraphs
explaining what did *not* work.

## Where the work stopped (2026-08-29)

**`gamecode` reached 291 of 291.** The last twelve were all in `FrontEnd.cpp`:
the endings viewer, the versus screen, the treasure and kode rows, the lobby,
the character grid, the button editor, the tower and the help pages.

Eight declarations were corrected along the way, every one of them by finally
reading a *caller* — which remains the single most reliable way this project
finds its own mistakes:

| symbol | was | is | what proved it |
|---|---|---|---|
| `limeGetStringWidth` | `long` | `float` | `vmov s10, r0` with no `vcvt` |
| `VSWait` | `long` | `float` | `vcmpe.f32` against 30.0 |
| `Character_SelectWait` | `long` | `float` | `vldr` + `vcmp.f32` |
| `KodeSelectorParticle[]` | `int[]` | `float[]` | advanced by `0.05/fps` |
| `EASDK_LogEventEnumEnum` | 4 args | 5 args | callee reads `[sp, #0x34]` |
| `peerNames` | `[5][64]` | `[8][64]` | the symbol table's own extent |

Three things worth carrying forward into the fight engine:

- **A `= 0` store can never tell an `int` from a `float`.** Four of the six
  corrections above were invisible until a *read* appeared. Treat any type
  established only from stores as provisional.
- **Debug output shipped in retail** in at least four places:
  `printf("limeFPSScaleFactor:%f
")` on the endings screen's hot path,
  `printf("%d TOUCHED
")` in the lobby, `puts("PRESSED EXIT!")` on the network
  versus screen, and `puts("F")` / `puts("G")` inside the loader.
- **`FE_WidthScale` and `FE_HeightScale` are interchangeable on 480x320 and only
  there.** Four screens mix them — `FE_Task_VS_Screen` pulls the right-hand
  portrait back by a height-scaled 256 while drawing it width-scaled,
  `EditButtons` uses `FE_W(48)` for a vertical measurement, and
  `FE_Task_Select_Treasure` passes `FE_WidthScale` itself as a colour component.
  Every one of them is invisible at 4:3 and wrong at any other aspect. A
  widescreen port has to decide each case deliberately.

### The front is `gamecode/logic`, and it is at 1,446 of 2,172 (2026-09-10)

**`gamecode/logic` is twenty-one source files, not fourteen.** `tools/progress.py`
was skipping any file with nothing written, so its table showed 2,092 functions
against a TOTAL of 2,172 and hid seven files that had never been started. Fixed;
the rows add up now. **If a table cannot be added up, distrust it.**

**Nine of the twenty-one are closed**, and they are the reference for the rest:

    other.c      333/333      mkcombo.c     16/16
    moves.c      357/357      mkcanned.c    20/20
    mkprop.c      80/80       mkslam.c      60/60
    mkstat.c      62/62       mkanimal.c    63/63
    mkfatal.c    149/149

    mkdrone.c    154/394      mkzap.c       58/174
    mkreact.c     72/207      mkboss.c      29/104
    joy.c         19/73

    mkfriend.c     0/45       mk3.c          0/19
    mkbonus.c       0/8       playback.c     0/4
    a_robo.c        0/2       mkrepell.c     0/1
    a_fn.c          0/1

The seven at the bottom are 80 functions that no one has looked at. `mkfriend.c`
is the friendships and is the only one of any size; the other six total 35.

The tree is at **0 errors, 174 warnings, `instck` clean**, and `protos.py` is down
to the single known `LIME_RenderMeshSingleIndexed` float-ABI disagreement recorded
in issue #26.

**Do not use `tools/pending.py` to order the work -- its index is stale.**
`tools/progress.py` scans the tree for real definitions and is the authority; a
throwaway that lists one file's unwritten functions smallest-first, mirroring
`progress.py`'s own scan, is a ten-line script and worth rewriting rather than
trusting `pending.py`.

**The loop that has actually worked**, for eight files now, is one function at a
time and two or three per commit:

    python tools/dumpfn.py <name>          # the disassembly, with callees named
    <resolve every pointer slot and literal before writing a line>
    <write the batch; use the Write tool for the script, not a heredoc>
    gcc -std=c99 -Wall -Wextra -O2 -c -I runtime -I decomp/lime <file>
    bash tools/check.sh
    python tools/progress.py --write
    git commit && git push

`tools/decomp_loop.py` was the plan of record and is not what closed these files.
Reading them by hand is what did, and the paragraphs above each function are the
part that will still be worth something in a year.

**`mkzap.c` (32/174) is the one to do next.** Not because it is the smallest --
`joy.c` is -- but because `mkfatal.c` reached into it: `t_fat_robo_crush`
descends into `t_robo_open_chest` (0x00075290) and `t_robo_close_chest`
(0x00077eb4), both of which live there and bracket that whole fatality. The same
argument that made `mkfatal.c` the right file after `mkanimal.c`.

**Then `mkdrone.c` (154/394)**, the largest remaining, or `joy.c` (19/73) if the
goal shifts to playability.

**`joy.c` (19/73) is the one to do if the goal shifts to playability**, since it is
the gamepad hookup named in CLAUDE.md's Phase 9 and the smallest file left.
