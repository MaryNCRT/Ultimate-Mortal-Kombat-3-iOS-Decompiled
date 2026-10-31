# Changelog

Release history for the native PC port of *Ultimate Mortal Kombat 3* (iOS 1.2.59).

---

Newest first. Each release says what was added or fixed and whether the
tester (Mary) has played it yet. The technical detail of every change -- the
binary addresses, the checks run -- is in [PROGRESS.md](PROGRESS.md).

## Next (after 0.1.18) — accounts, MULTIPLAYER over LAN and Radmin VPN

On `main`, not released yet. **Played by Mary** in her test folder: works.

**Game**
- **PLAY > MULTIPLAYER works again**, on the local network alone (no Epic
  Online Services): the games with that screen open are listed by profile
  name, and choosing one starts the same fight as ONLINE. It reaches
  virtual LANs too (Radmin VPN, Hamachi, ZeroTier). ONLINE is unchanged.
  Port: the original listed players over Bluetooth.

**Launcher**
- **ACCOUNT.** Username and password; email optional. Making an account
  gives a recovery code, shown once (a new one each time it is used). The
  log-in screen warns not to use a personal email or a password used
  anywhere else. Without an account the player is a GUEST: the profile code
  for online play, and no leaderboards.
- **Progress in the cloud.** The profile (and its code) and the game's
  progress (save, tower, survival, achievements, stats, treasures) come
  down when PLAY is pressed and go up when the game closes; the controls and
  settings stay on each PC. Changed on both sides: the player chooses which
  stays, and the other is copied to `save\backup`.
- The server (accounts, progress, leaderboards) is a Cloudflare Worker kept
  in a private repository; only the client (`launcher/app/lib/account.js`)
  is here. Passwords and recovery codes are stored as salted PBKDF2 hashes,
  sealed with AES-GCM; emails are sealed too; each account reads only its
  own data. Leaderboards in the game are not hooked up yet.

## 0.1.18 beta (2026-10-30) — Noob Saibot on the roster, EXPORT ALL textures, settings by category

Published 2026-10-30 as the GitHub pre-release `v0.1.18-beta` (`UMK3-PC-0.1.18-beta.zip`, `UMK3-PC-0.1.18-beta-linux-x64.tar.gz`). **Played by Mary** in her test folder: works.

**Game**
- **Noob Saibot can be chosen** (umk3.ini `noob_saibot=1`: the launcher's
  PLAY page, or GAME in the Esc menu). The game already marks him
  available but its grid has no cell for him; he takes the empty one right
  of Smoke. Offline only: never in an online fight, whatever is set. Port,
  not in the original.
- **The Esc menu's categories:** VIDEO, GRAPHICS, TEXTURES and GAME are now
  separate (they were all under GRAPHICS); GAME holds the interface and
  what changes the game.
- **Resolutions from smallest to largest** in the Esc menu and the
  launcher (umk3.ini keeps width and height, so nothing saved changes).

**Launcher**
- **MODS → TEXTURES: EXPORT ALL** exports every texture of the fighter or
  stage shown, and **EXPORT EVERY FIGHTER / STAGE** the whole group, each
  to its place in `exports\`.
- **GRAPHICS** has a page of its own beside VIDEO, TEXTURES and INTERFACE.
- **CHARACTER EDITOR:** the note now says it as it is: character changes
  (attacks, hitboxes, kombos, data) never apply online; online only models
  and textures count (runtime/mods.c already worked that way).

**Known problems closed** (Mary no longer sees them): the dropped fight
intro frame, the repeated death scream, the Motaro softlock, SCULPT's
"500 Internal Error"; duplicating a fighter is off the list.

## 0.1.17 beta (2026-10-30) — Kombos that share a start, KOMBOS in the CHARACTER EDITOR

Published 2026-10-30 as the GitHub pre-release `v0.1.17-beta` (`UMK3-PC-0.1.17-beta.zip`, `UMK3-PC-0.1.17-beta-linux-x64.tar.gz`). Both changes were **played by Mary** in her test folder: work.

**Game**
- **Kombos that start alike can now be finished either way.** Mileena's
  HK, HK, LK, HK and HK, HK, U+LK, HK share their first hits, and the game
  always took the first: the third hit's list holds LK twice and the
  original picks the first entry for the button pressed, without looking at
  the stick. Holding **up** on that hit now takes the second. The same in
  the five other lists of the game built that way (Kabal, Sonya twice,
  Stryker, Kano). Port, not in the original.

**Launcher**
- **CHARACTER EDITOR → KOMBOS** (new page): every kombo of the fighter, hit
  by hit, as the game's tables have it; each hit's button and strike can be
  changed into the mod.

## 0.1.16 beta (2026-10-30) — MOD HUB

Published 2026-10-30 as the GitHub pre-release `v0.1.16-beta` (`UMK3-PC-0.1.16-beta.zip`, `UMK3-PC-0.1.16-beta-linux-x64.tar.gz`). The MOD HUB was **played by Mary** in her test folder: works.

**Launcher**
- **MOD HUB** (new tab in MODS): lists the mods published in the companion
  repository [MaryNCRT/UMK3-IOS-Mod-Hub](https://github.com/MaryNCRT/UMK3-IOS-Mod-Hub)
  with their pictures, downloads one into `mods\<mod>\` (checked against
  its SHA-256, turned on at the top of the MOD MANAGER) and offers UPDATE
  when a newer version is published. A mod already in `mods\` (copied by
  hand) is recognised by its folder and compared file by file with the
  published zip; UPDATE then downloads only the files that differ, not the
  whole mod. The MODS tab row wraps in a small window instead of pushing
  the page past it (Mary's report). Only the repository's owner and
  collaborators can publish there. First mods: HD NINJAS, Sonya Blade MKX,
  Rain.

## 0.1.15 beta (2026-10-30) — STAGE VIEWER with Blender-style editing, shape keys, stage textures, select-screen music, 4:3 menus

Published 2026-10-30 as the GitHub pre-release `v0.1.15-beta` (`UMK3-PC-0.1.15-beta.zip`, `UMK3-PC-0.1.15-beta-linux-x64.tar.gz`). **Not yet played by Mary** as a whole; the items she reported are marked.

**Game**
- **STAGE SELECT** keeps the select screen's music playing without a break
  (no fade, no stop on the way), and its CANCEL goes back to the select
  screen instead of starting the fight (Mary's report). Online, the host's
  CANCEL still takes the game's own stage, since the guest waits on it.
  STAGE SELECT names and pictures all 16 stages apart (the Subway, Soul
  Chamber and Lair were named Street, Balcony and Kave).
- **4:3:** the ONLINE code box no longer runs off the screen, and the
  settings / Esc menu is sized to the 4:3 picture instead of the 16:9
  window around it (Mary's reports).
- **`repell_func`**, the rule that pushes two close fighters apart, now
  follows the binary when one is above the other (a juggle); checked
  against the binary's own code on 200,000 states. The slight slide in a
  kombo (issue #114) was checked too: it is the original game's rule.

**Launcher**
- **STAGE VIEWER** (new): a stage played as the engine plays it, with the
  effects its events fire, the game's blending, element picking,
  properties and isolate; exported with its animation and effects.
  Elements are edited as in Blender: G / R / S with handles, axis locks and
  typed values, I / Alt+I keyframes, REC, linear or bezier, a dope-sheet
  timeline, and a model editor with shape keys and proportional sculpting.
  Edits are kept per stage (`stage edits\`) and exported as GLB morph
  targets, FBX blend shapes and USD BlendShapes. A **TEXTURES** tab shows
  the stage's textures and replaces them into a mod.
- **REPLACE / EDIT MODEL:** shape keys for fighters (Basis plus keys,
  values keyed per animation frame, sculpting); the T-pose shown too.
- **The timeline** under both editors is Blender-like: transport buttons,
  frame field, a tick a frame, a Summary row, the playhead's frame.
- **CHARACTER VIEWER** can put a playing stage behind the fighter.
- **TARGET MOD** lists every installed mod (it showed only the one typed).
- Editor windows are no longer covered by the launcher's bar or the
  player card; the menu's description text shrinks to fit its box.

**Known problems:** see the [README](../README.md#known-problems-0115-beta).

## 0.1.14b beta (2026-10-30) — Liu Kang's endless scream, SMOOTH WEIGHTS, imported models lit

Not yet played by Mary. 0.1.14 plus:
- **Liu Kang's death scream no longer plays forever.** The Windows mixer
  kept a sound's place as 16.16 in 32 bits, which wraps past 65,536 frames
  (4.1 s); Lkdeath, Skdiemix and Vpdeath are longer. Now 64-bit.
- **Linux sound:** a sound played again starts over instead of doubling.
- **REPLACE MODEL:** SMOOTH WEIGHTS (Blender's Smooth Vertex Weights); GLB
  bone names read from the file (dots kept); kept-proportion models bound
  to bones along their own limbs; Blender's gizmo and G / R keys; FBX read
  as Blender reads it.
- **Imported models are shaded in the fight:** their `.lighting` bake is
  made at load, byte-exact with the game's own.

## 0.1.14 beta (2026-10-30) — REPLACE MODEL like Blender, corrective animation layers

Not yet played by Mary.
- **REPLACE MODEL:** bones moved and rotated with the mouse or G / R and
  X / Y / Z, LOCATION / ROTATION fields mirrored left / right, WEIGHT PAINT,
  Ctrl+Z / Ctrl+Y, reset to a clean T or A pose, Blender's camera (axis
  gizmo, numpad views, orthographic), the file's own rig by default, EDIT
  MODEL on the fighter's current model, a model put over an earlier one
  fitted to the game's rig, the mod's own animations in the preview,
  Mixamo / AccuRig / ValveBiped rigs, KEEP PROPORTIONS, a texture per
  material.
- **Model viewer:** tabs, derived models, layers, thumbnails, floor, solid
  and weight views, CORRECTIVE ANIMATION LAYERS, GLB export, a
  `mod.meta.xml` in every mod.
- **Replays** record and load the rival actually fought; the player can be
  hidden and resized.
- **STAGE SELECT** after the fighters, with a picture for every stage;
  VSYNC option.
- **Custom models:** no black fighters with a large model, no SMOOTH
  MODELS on them, no frames lost to their material parts.

## After 0.1.13: an update stopped by a leftover update\new (2026-10-29, released in 0.1.14)

Mary's update failed with "ENOTEMPTY: directory not empty, rmdir
...\update\new\UMK3-PC-0.1.11-beta\launcher\bin\resources". `unpack`
(`launcher/app/lib/update.js`) cleared `update\new` with one `fs.rmSync`.
An earlier update's copy was still there: apply.ps1 removes it with
`-ErrorAction SilentlyContinue`, and Windows would not let a file in it go
(the usual holder is an antivirus still reading what was just unpacked).
So the update threw before it began.

Now:
- `unpack` retries the removal for two seconds (`maxRetries`).
- What still will not go is left, and the update unpacks into
  `update\new-<time>` beside it.
- Every leftover `new*` is tried again at the next update.
- apply.ps1 / apply.sh remove the folder that update used.

Checks: a test case in `test/update.test.js` (a removal that throws
ENOTEMPTY, then the next update clearing both); launcher tests 14 pass.

The fix runs in the launcher doing the update, so it helps from the next
release on: a launcher of 0.1.13 or older still stops on a leftover. Until
then, close the launcher, delete the `update` folder by hand and update
again (or download the release zip).

## Release 0.1.13 beta (2026-10-29): replays, online delay by ping, Vulkan / Direct3D 12 stutters, 4 GB

Published as the GitHub pre-release `v0.1.13-beta`, packaged as 0.1.12 was:
`UMK3-PC-0.1.13-beta.zip` (`launcher/make_release.py` over `electron-builder
--win dir`) and `UMK3-PC-0.1.13-beta-linux-x64.tar.gz` (`electron-builder
--linux dir`, then `launcher/make_release_linux.py`). It carries the five
"After 0.1.12" sections below. Before publishing, the game was built from
the 1.2.59 .ipa in the unpacked Windows package. Known problems taken off at
Mary's word: Reptile's fatalities (it was not Reptile's alone) and the
Vulkan / Direct3D 12 stutters (fixed below). **Mary has not played this
build yet.**

## After 0.1.12: REPLAYS (2026-10-29, in 0.1.13)

Not yet played by Mary. Asked for so that a fight can be watched again to
find bugs and take logs of them (Melee Unlocked's replays as the model).

**Recording** (`rollback_probe.c`, `GameCode.c`). The probe's recorder
(`UMK3_REPLAY_RECORD`, which replayed a fight identically over 3038 ticks)
now records every fight but an online one, by default, to
`replays/recent/<date>_<p1>_vs_<p2>.umkr` beside the exe; only the newest 5
stay, and one shorter than a second is not kept. umk3.ini `replays=0` turns
it off. The file is the setup -- the fighters, the level, the mode, the ten
`Settings` words and training (`PortPractice`), 18 words now -- the two
seeds, then each tick's two engine words and its result's hashes. A replay
takes the recorded settings back for its one fight, since the buttons and
the difficulty change how it plays. While recording the frame step is pinned
to 1, as online: below 60 FPS the game slows instead of skipping ticks.
Online fights are not recorded yet (their ticks go through `rollback.c`).

**Playing** (`umk3-game --replay replays/<folder>/<file>`, a path inside the
exe's folder). The game goes straight into the fight, as `--fight` does, and
`rp_frame` steps it in `GameCodeMain`'s place. A panel along the bottom
(`debug_menu.c`, drawn like the launcher: violet glass, light rules, buttons
with a rim, play lit) holds the status line, a bar to click or drag, and
buttons: to the start, back 5 s, back a frame, play / pause, on a frame, on
5 s, slower / faster (x1/4 .. x4), and the settings menu; each names itself
when the mouse is over it. Keys: Enter or P play / pause, left / right 5 s
(paused, a frame; held, they repeat), up / down speed, M the start, Esc the
settings. Going back loads the nearest keyframe (a rollback snapshot,
`st_rb_*`, every 120 ticks or more, 47 at most) and runs the ticks between
silent and undrawn, 20 ms of them a frame. Pausing draws the next frame and
puts the state back. At the last tick the player stops. Each tick's result is
compared with the recorded one: a difference shows on the panel and goes to
the log (`replay: tick N DIFFERS`, then `replay: the end ... the same as the
recording` or `DIFFERS`). Windows only for going back and pausing: the
snapshots are Windows only.

**Launcher.** A REPLAYS line on the menu (the eight lines re-centred on the
ring) opens the page (`SecReplays.svelte`, `lib/replays.js`): the keep-the-
last-5 switch, the last fights and the kept ones, each with WATCH, KEEP FOR
GOOD (recent to `replays/saved/`), EXPORT… and DELETE, and IMPORT… and OPEN
FOLDER for the kept ones. `play` passes `--replay` to the game.

**The player's top corners** (asked after the first look at the plan). A
REPLAYS button at the top left opens a list of the last fights and the kept
ones (the newest 8), the one playing marked; picking another switches to it
in the same game: this replay ends (logged as `switched to another
replay`), the game leaves the fight as the debug menu's FIGHT does and the
`--fight` path starts the new one's fighters and level, behind a LOADING
screen with its name; the sticks rest and the keys and mouse do nothing on
the way (`rp_switching`). (The first version started a second copy of the
exe instead; Mary asked for no restart.) An X at
the top right closes the game. Nothing else is drawn across the top, so the
health bars stay in sight; the replay's name moved into the status line.

Checks: Windows build without warnings in the new code, Linux compile 84/84,
launcher tests pass, the page and the menu seen in launcher captures. The
panel and playback have not been seen in the game yet.

## After 0.1.12: online lag over the internet (2026-10-29, in 0.1.13)

Not yet played by Mary. She saw lag in an online fight against another PC.
No 0.1.12 online log was on hand; the last one (0.1.11, over EOS) showed, per
600 frames, 75 to 128 holds (a frame shown again while waiting for the other
side), rollbacks always at the deepest the window allowed (7), and the other
side 6 to 9 frames behind. The input delay was fixed at 2 frames, enough on a
local network, not over the internet. Changes (`rollback.c`, `mp_net.c`):

- Inputs go on the wire when they are taken (`mpnet_flush` ticks EOS), not at
  the next frame's poll: about a frame less each way.
- The input delay follows the round trip, measured from the other side's
  acknowledgements: once a second, one frame toward the delay that keeps
  rollbacks near 3 frames, between 2 and 6. Each side's delay is its own (the
  other only receives inputs by frame), so it can move mid-fight: frames it
  skips take the same input, a frame it overlaps drops the newer one.
  `UMK3_RB_DELAY` still fixes it.
- Rollback window 8 -> 12 frames (4 more snapshots, ~25 MB).
- The log's `rollback: frame ...` line and NET STATS show the ping, the delay
  and the longest rollback in ms.

Also seen in that 0.1.11 log, not touched: the second fight (this side player
2) logged a DESYNC every second. Built into Mary's 0.1.12 test folder; not
yet tried between two PCs.

## After 0.1.12: REAL-TIME SHADOWS' note on a row of its own (2026-10-29, in 0.1.13)

Mary saw the note beside REAL-TIME SHADOWS' value ("WITH VULKAN OR DIRECT3D
12") drawn over the label: the row shrinks its text to fit, down to a floor,
and this one was still too long. The note is now a dimmed row of its own under
the option, shown, not chosen (`settings_menu.c`): ONLY WITH VULKAN OR
DIRECT3D 12 on OpenGL, EXPERIMENTAL on the others. Seen right by Mary.

## After 0.1.12: the game's memory below 4 GB, limeMalloc's heap 2 GB (2026-10-29, in 0.1.13)

Not yet played by Mary.

The 64-bit build kept the game's heaps below 2 GB, because a `long` holding an
address was feared to sign-extend. That fear was checked two ways.

**The compiler.** Clang's `-Wint-to-pointer-cast` was run over every source
of the 64-bit build. It found 320 integer-to-pointer casts, and all of them
are from `uint32_t`, which zero-extends: `P32` is `__ptr32 __uptr`, and the
game's `uintptr_t` is `uint32_t`. None are from a signed type, and
`-Wint-conversion` found no implicit ones.

**Running it.** With `UMK3_HIGH_MEM=1`, a test switch in `savestate.c`'s
`reserve_low`, every region starts at 2 GB: the arena at 0x81000000 and the
heap at 0xC1000000. With all of the game's memory above 2 GB, the game:
- booted;
- fought;
- went through FINISH HIM, the continue screen, character select and the main
  menu (8,000 ticks);
- ran a 6,000-tick fight with a finisher request, with no fault.

What changed:
- `reserve_low` now searches below 4 GB, still first fit from the bottom. The
  game's memory sits where it was; only what it never used before can go past
  2 GB.
- limeMalloc's region (`lime_platform.c`) now tries 2048 MB first (was 768) and
  gets it: `lime_heap: 2048 MB`. It is a reservation, and pages are committed
  as they are used. A fight with SMOOTH MODELS uses about 386 MB in all.
- The save-state arena stays at 1024 MB.

Linux keeps `MAP_32BIT`, so it stays below 2 GB. It falls back through the
size list to what fits. Linux compile: 84/84.

## After 0.1.12: Vulkan / Direct3D 12 stutters, compiled shaders, Sub-Zero's strap (2026-10-29, in 0.1.13)

Not yet played by Mary.

**The stutters on Vulkan and Direct3D 12 (Mary's 180 Hz screen).** Each 60 Hz
tick was presented with a swap interval of one refresh (5.6 ms at 180 Hz).
The frame's work (10-12 ms) varies by a few ms, and a frame that ended just
past a refresh was shown one refresh later. The result was 22 ms, then 11 ms,
several times a second. That is what was measured on 0.1.12: 150-330 of
~2,400 frames late. Changing vsync made no difference, because the late
refresh came from where the frame ended, not from vsync.

Now, on a 120/180/240 Hz screen, each frame stays up 2/3/4 refreshes
(`rgl_swap_interval`, set in `game_main.c`'s `pace_frame`):
- Direct3D 12 presents with that sync interval.
- Vulkan's FIFO has no interval, so `rgl_vk.c` keeps a copy of the frame and
  presents it again once for each extra refresh.
- The loop is paced as on a 60 Hz screen.

Measured on Direct3D 12 and Vulkan at 180 Hz:
- Swaps hold at 16.66 ms with 0 late frames a second through a fight.
- A few frames are still late when the frame's work goes past 16.7 ms.
  `UMK3_LOG_PACE=1` now prints the late frames a second.

OpenGL is unchanged.

**Where a fight's frame time goes (measured, D3D12, SMOOTH MODELS 2).**
- `rgl_ff` (transform, index gather, hand-off): ~3 ms.
- Catmull-Clark of the two fighters: ~7 ms.
- With 1 / 4 / 8 threads, the intro had 64 / 38 / 23 frames over 13 ms, with
  smoothing averaging 12.4 / 9.9 / 9.7 ms. The pool stays at 4.
- Open: a fight intro still has frames of 15-19 ms of CPU work. The next
  step is to make the smoothing cheaper.

**COMPILE SHADERS (Khronos' pipeline_cache sample, Microsoft's
D3D12PipelineStateCache).**
- Every pipeline a backend makes is a line in `umk3-pipelines-vk.txt` /
  `umk3-pipelines-d3d12.txt` beside the game, and the driver's compiled
  pipelines are kept in `umk3-pipelines-vk.bin` (`VkPipelineCache`) /
  `umk3-pipelines-d3d12.bin` (`ID3D12PipelineLibrary`, names carrying the
  shaders' hash).
- When the backend opens, it makes them all again, from its own seed list
  (`render/rgl_pipes_*.h`: menus and two fights, 18 / 17 pipelines) and the
  file. This takes 1-7 ms, and no pipeline is compiled in the middle of a
  fight.
- `umk3-game --compile-shaders` does only that and exits.
- The launcher's PLAY has COMPILAR SHADERS ANTES DE JUGAR
  (`compile_shaders=1`), which runs it before the game.

**Sub-Zero's strap stretched to his fist (Mary, his intro, player two, with
SMOOTH MODELS).** The weld was measured on the first pose a skin is drawn in.
In Sub-Zero's intro his hand touches the strap there, and the two were welded
for good. `DrawSkinnedMesh2` (armv7 0x608d8) unwelds the skin by UV: each
record vertex is a copy of the skin vertex its corner names (SKININFO +0x18).
The weld now joins two vertices only when, besides being close, their skin
vertices have the same four bone bytes and weights (+0x20, +0x24):
`umk_subsurf_new_keyed`. For his mesh, 3 of 670 welds were refused, and the
strap stays on his back (captures at ticks 105, 108 and 159). With SMOOTH
MODELS off the strap was always right.

Checks:
- rgl_test: 0 failed on OpenGL, Vulkan and Direct3D 12.
- Linux compile: 84/84.
- Launcher tests: pass.

## Release 0.1.12 beta (2026-10-29): fatality lag, several cores, no vertex-buffer ceiling, shadows, flawless

**Scorpion's fatality lag (Mary: lag in that fatality only).** Measured
with a per-second log of the draws (Scorpion vs Sonya, practice, finisher 2):
in the late phase ~150 draws of 180,216 vertices a frame (the copies of
Scorpion, each a smoothed body expanded to one vertex per triangle corner),
~1 GB sent to the GPU, frames of 889-968 ms (651-679 ms in the scene's
draws). Two causes:
- SMOOTH MODELS' budget of smoothed triangles read `limeRenderedPolyCount`,
  which the game's tick clears; a slow frame runs several ticks between its
  draws, so the budget never filled (logged: 0 draws over it). Now its own
  count, cleared before each present (`port_smooth_frame`, game_main.c),
  250,000 triangles: the two fighters and a third body; copies past it are
  drawn as the model is.
- Direct3D 12 and Vulkan grew their vertex buffer up to 256 MB; past that,
  every overflow waited for the GPU and made a new 256 MB buffer, 3-4 times a
  frame. Now the buffer grows to what the frame needs with no ceiling, and the
  old one is kept until the next flush instead of waiting partway.
After: one 52 ms frame in the whole fatality, the rest under the slow-frame
threshold. rgl_test 0 failed on OpenGL, Vulkan and Direct3D 12; Linux
compile 84/84. Not yet played by Mary.

**Open: random stutters on Vulkan and Direct3D 12 (Mary, RTX 4060: OpenGL
smooth).** Measured on Direct3D 12 (Liu Kang vs Sonya, practice, 40 s, a
180 Hz screen): ~150-330 of ~2,400 frames take 20-23 ms instead of 16.7,
then one ~11 ms frame (the next tick is already due); they come about every
100 ms. Ruled out: vsync (`Present(0)` changed nothing, 289 vs 318), GPU
waits (2 a frame, 0.6 ms), textures rewritten mid-frame or read back (0).
The frame work before the present is ~10-11 ms on Direct3D 12 against
~5.5 ms on OpenGL (the `pace:` log), so the CPU side of rgl_ff (transforming
and expanding every vertex) is the next place to look. Not fixed.

**Several cores.** zig cc ships no OpenMP runtime, so Blender's subdivision
ran on one core: `src/thirdparty/blender/omp_lite.c` implements the
`__kmpc_*` calls (a pool of at most 4 threads, measured fastest; `UMK3_THREADS`
overrides), `CCG_OMP_LIMIT` lowered to 20000, rgl_ff's indexed expansion in
parallel, a ring of 8 recent poses per smoothed mesh.

**Engine memory (Mary asked for more).** The game keeps pointers in 32-bit
words and longs that sign-extend, as on iOS, so its two heaps must sit below
2 GB: the save states' arena reserves 1024 MB and limeMalloc's heap 768 MB
(read at start). A 1024 or 896 MB heap was tried and does not fit beside the
exe; that space is full. Going past 2 GB would mean every pointer read in the
decompiled code zero-extended instead, a change across the whole game.
Measured use is ~386 MB for the whole process, so the heap is not what
limits the frame rate. Everything else (textures, vertex buffers, smoothed
meshes, shadows) is ordinary 64-bit memory with no limit of the engine's.

**Also since 0.1.11:** REAL-TIME SHADOWS drawn the same frame with a steady
light box; Kung Lao's hat in the fight, not only the intro; round end as the
binary's two blocks (FLAWLESS VICTORY on match point, DrawHUD 0x2902c /
0x290a6); Linux keys mapped by scancode (not tested on Linux);
`UMK3_PRACTICE=1` runs a `--fight` as TRAINING.

---

## 0.1.11-beta (2026-10-23) — updates that finish, window resize, loads, profile picker, sharp edges

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary.

- Launcher updates: every Windows update since 0.1.6 ended as "could not be
  copied" (robocopy on folders no release has) and deleted `tools\eos\`;
  now only `src\`, `launcher\` and `toolsuild\` are mirrored, the rest
  copied, the downloaded archives removed.
- Window resize and move: Direct3D 12 and Vulkan keep working through resizes
  that fail or change the buffers; the render buffer is remade once the size
  settles, not every frame.
- Online over the internet: the EOS SDK's headers and libraries are in the
  repository (`tools/eos`), and the release scripts refuse to package without
  them and `eos.ini` (0.1.10b went out without them).
- The home screen ring's picture (`image.png`) comes with the launcher.
- Load times: texture uploads batched on Direct3D 12 and Vulkan; load screens
  draw half as often; the launcher's compile is incremental (13.9 s to 1.2 s
  with nothing changed).
- Long sessions: textures found by a hash, not a list walk that grew all
  session; a memory line in the log every five minutes.
- Character editor: Motaro's page no longer fails (a strike named three times).
- PROFILE: a gradient picker (two colours, an angle, presets) and the
  fighter's second outfit as the player's picture, in the launcher and the game.
- SMOOTH MODELS: automatic sharp edges, with two sliders (which edges, how
  much) in GRAPHICS and the model viewer, and in the game's own GRAPHICS.
- SMOOTH MODELS: Kung Lao's hat is no longer pierced by his head -- the skin
  under a piece on its bones gets no correction pass (game and viewer).
- The game's Esc menu: PROFILE's background is the launcher's gradient picker,
  drawn the same, with the mouse, the keys and the pad; OUTFIT 1 / 2.
- Launcher version 0.1.11-beta.

## 0.1.10b-beta (2026-10-23) — video settings, settings in two columns

Built for Windows (64 and 32 bits) and Linux. Mary tried the two-column
menus and the new box before the last two changes below; those are untested.
Mary has now run the Linux package on a Linux machine, and played the
Vulkan and Direct3D 12 renderers: all three work.

- **VIDEO SETTINGS** on the settings screen, in the empty top-right box above
  SOUND VOLUME: it opens the settings menu's GRAPHICS page in the game's own
  panel, as BUTTON SETUP does, with the on-screen controls on or off.
- The settings in **two columns** side by side: the Esc menu, the debug menu
  (F2) and the game's own panels. The columns break only at a section's
  heading, so a section never runs into the other column; a row whose label
  and value would overlap is drawn smaller.
- CONTROLS: the keys and the pad's buttons split into FIGHT and MENU, each
  in its own column.
- Launcher: its own version `0.1.10-beta.b` reads as 0.1.10b, so it no
  longer offers the 0.1.10b release as an update to itself (`lib/update.js`).

## 0.1.10-beta (2026-10-23) — Kombos, SMOOTH MODELS, GPL

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary at release; the Linux package is untested on a Linux machine.

- Kombos (issue #114): when a player's button matched any entry of a combo
  table but the first, the port went on with the FIRST entry's next move.
  `combo_scan_a11` put the list back at its start on a match; the binary
  keeps the entry that matched, and `t_comb2` takes the next move from it
  (armv7 0x32776..0x3278e). Likely the cause of Liu Kang's 7-hit kombo
  breaking with the opponent sliding away; not yet tried in the game.
- Fixed: `t_r_combo0` called `set_no_block` on a path where the binary skips
  it (armv7 0x45da8).
- **SMOOTH MODELS** (off, level 1 or 2): the fighters drawn through Blender's
  Weld and Subdivision Surface (Catmull-Clark), in the game (GRAPHICS > VIDEO,
  `smooth_models=` in `umk3.ini`) and the model viewer, which can also play
  animations smoothed. The code is Blender's (`src/thirdparty/blender`).
- **The project is now GPL-2.0-or-later**, because it contains Blender's code.
  Releases up to 0.1.9-beta stay MIT.
- Crash reports and the session log name the version, the system, the
  renderer, the GPU and the settings in use.
- Linux: sound and music (the Linux game was silent); music needs libmpg123.
- SMOOTH ANIMATIONS also on the select screen and in the intro.
- Faster drawing of large models with Vulkan and DirectX 12.
- Fixed: SMOOTH MODELS drew dark wedges and black spots on some fighters
  (Jax, Sektor, Cyrax, Kano, Kitana, Mileena, Jade, Liu Kang).
- MODS > EXPORT: stages with their animation (FBX, USD); every export in its
  own folder: Characters, Stages, Models, and textures in their fighter's or
  stage's folder, Interface or Effects.
- MODS > TEXTURES: replace or export any texture of a fighter, a stage, the
  interface and menus, or the effects; a mod keeps each in that place's
  folder.

- **SMOOTH ANIMATIONS** (off by default): the launcher's GRAPHICS > VIDEO, or
  Esc > GRAPHICS > VIDEO in the game (applied at once), `smooth_anims=1` in
  `umk3.ini`. A fighter's frames are held for several ticks and drawn as hard
  cuts; with this on, each new frame is reached by blending from the one
  before. Drawing only: the game logic, hit boxes and events are unchanged.
  It applies to every fighter: fights, the select screen and the intro.
- Fixed: the game crashed when a fighter took Kabal's projectile (the
  reaction asked for a sound before setting its number; armv7 0x46396).
- Fixed: `tools/decomp/progress.py` looked one folder above the repository
  and counted every module as 0%.

## 0.1.9-beta (2026-10-22) — Vulkan and Direct3D 12, Linux Fixes

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary at release; the Linux package is untested on a Linux machine.

- **Vulkan and Direct3D 12 renderers** beside OpenGL (`src/runtime/render/`): every GL call goes through `rgl_front.c`; with Vulkan or Direct3D 12 a fixed-function layer (`rgl_ff.c`) keeps OpenGL 1.1's state and hands transformed vertices to `rgl_vk.c` or `rgl_d3d12.c`. Chosen in GRAPHICS > VIDEO or the in-game menu (`renderer=` in umk3.ini); a renderer that cannot start falls back to OpenGL. `tests/rgl_test.c` checks all three against the same calls (22 checks, 64 and 32 bits).
- **Move list:** the fatality page drew the special moves' buttons (section 2 starts after section 1's rows in the shared table, as the binary reads it).
- **Linux:** the game reads `umk3.ini` and `res/` beside its executable; it no longer links against libGL (loaded at run time); `text.c` includes `<stdarg.h>`; `build-game.sh`, `install-shortcut.sh`, `README-linux.md`, a CI job compiling every source with the bundled headers only.
- **Build from a terminal:** `launcher/app/cli.js`, run by `build-game.sh` / `build-game.cmd` with the launcher's own Electron.
- **eos.ini.example** for internet play.

- **Language in the in-game settings menu**, changed at once (texts and menus).
- **Mod textures reach the game's smaller copies** (`_LITE`, `_LOW`): a replaced fighter skin now shows at IPHONE texture quality.
- **Launcher:** smaller text everywhere; MODS fills the window; the model viewer has its options beside the model, a full screen button, REFRESH FOLDERS, textures by fighter, REPLACE TEXTURE from any picture (PNG, JPEG, WebP, GIF, BMP, ICO, AVIF, SVG, TGA) with the LOW and LITE copies made automatically; covers from any picture.
- **Linux:** the link no longer takes a 32-bit `libGL.so.1` from `/usr/lib` (Fedora and its kin): 64-bit directories first, each library checked for its width.
- **Viewer:** Kung Lao's hat (and anything else riding a fighter's bones), with ACCESSORIES to hide it; Reptile and Noob Saibot no longer show Scorpion's texture; texture quality NORMAL / LOW / LITE; playback speed.
- **[GOALS.md](GOALS.md):** what works and what comes next.

---

## 0.1.8-beta (2026-10-22) — Mods, Character Editor, Blender Round Trip

Built for Windows (64 and 32 bits) and Linux; not yet played by Mary at release. The Linux package is untested.

- **MODS in the launcher:** a mod manager (`mods/<mod>/`, files named like the game's replace them; priority, on/off, categories, covers), a model viewer that shows the game with or without mods, a character editor (each fighter's strikes with damage, chip and a visual hitbox editor, specials and finishers with the missing ones marked, per-fighter data, the game's texts, the fighter's textures), and export to FBX, USD (USDA/USDC/USDZ) and OBJ with a corrected T-pose.
- **From Blender back to the game:** an FBX animation replaces one of a fighter's clips, an FBX model on the same rig becomes a new `.skin`, both written into a mod.
- **The game reads mods:** `src/runtime/mods.c` lays `mods/` over `res/` and applies each mod's `data.ini` over the fight tables (`umk3_logic_names`, a new registry at the end of `logic_tables.c`).
- **Motaro and Shao Kahn** picked as FIGHTER 1/2 in the debug menu no longer crash the game (button rows, animation tables and strike rows stop before the bosses).
- **Linux:** bundled GL/KHR/SDL2 headers (`GL/gl.h` not found), soname link fallback, and a self-update that takes the Linux `.tar.gz`.

---

## 0.1.6-beta (2026-10-22) — Internet Play, Debug Menu, Linux Package

Tested by Mary on Windows (64 and 32 bits). The Linux package is built but untested.

- **ONLINE over the internet:** Epic Online Services (`src/runtime/mp_eos.c`), loaded at run time from the SDK beside the exe; 64-bit only. The release ships the SDK in `tools/eos` and the launcher copies it next to the game. LAN play works without it.
- **Rollback netcode** for online fights (`src/runtime/rollback.c`); each player picks their fighter's colour online.
- **Debug menu (F2)** reorganised like the settings menu: FIGHT, MENUS, TOOLS; new infinite health, freeze clock, FPS and online numbers (all off in an online fight).
- **Mouse wheel** in the debug and settings menus; **alerts answer the pad**.
- **iPhone-style boxes on Linux:** `src/runtime/platform/ui_alert.c` is shared by both backends (GDI on Windows, stb_truetype on Linux).
- **64-bit white textures:** the texture cache's entries come from below 2 GB (`umk3_alloc_low`); the C heap sometimes sat above 4 GB.
- **Linux package:** `UMK3-PC-0.1.6-beta-linux-x64.tar.gz`, the Electron launcher for Linux; needs SDL2 and OpenGL. Untested.
- **Release tooling:** `launcher/make_release.py` copies all of `src/` and the EOS SDK, and builds the stub with zig when llvm-mingw is missing.

### Also in 0.1.6 — Mileena's Fatalities

- **Nail fatality:** `RenderAnimatedCharacter` (0x5c2f4) now poses the skeleton on frames that draw no skinned body, as the binary does. Mileena's frames 310..334 are baked meshes, so her BIG CUP (bone 24) was riding the opponent's skeleton instead of her hand.
- **Suck-kiss fatality:** the port's "hold the last pose" for frames missing from a fighter's frame list now applies only to Classic Sub-Zero, the case it was written for. Held for everyone, it kept the victim on frame 327 after `set_inviso`, and the `MILEENA_SUK_<victim>` absorption replayed every eleven frames until the round ended.
- **Debug finisher on x64:** `opponent_dizzy` reads the engine's 32-bit words instead of 8-byte pointers (it crashed the scripted finisher test).

## 2026-10-18 — Repository Reorganization & Unified `.ipa` Build

- **Directory layout:** Separated source into `src/decomp/` (1:1 decompiled C) and `src/runtime/` (PC port layer), and tools into `tools/build/` (build-time `.ipa` & table extractors) and `tools/decomp/` (reverse engineering, Ghidra, `armrecomp`, `difftest`).
- **Single-command `.ipa` build (`build.py`):** Added `build.py` and `tools/build/unpack_ipa.py` + `CMakeLists.txt` (`-DUMK3_IPA=<path.ipa>`) to validate the 1.2.59 `.ipa`, unpack `res/`, extract the 229 data tables, and compile `umk3-game` and `umk3-fight-headless`.
- **Removed dead code:** Removed unused legacy prototype executables (`umk3-menu`, `umk3-fight`, `umk3-test`), old Win32 `launcher.c`, experimental `web/` wrapper, and one-off scaffolding scripts.

## 0.1.5-alpha (2026-10-16) — Online Update

- **PLAY > ONLINE:** Transcribed `limeMPGameSpecific.c` (48 functions, 14 packet types, 120-entry sprite-list queue) and added `src/runtime/mp_net.c` (UDP + LAN discovery) and `FE_Task_Online` (FE task 51).
- **Fixes:** Fixed `UpdateArcadeCode` guest/host `GameObjects` pointer, `Task_GameInit` online task transition (`Task_MultiplayerSync`), `dummyObjects` head pointer relink, and 64-bit `DrawAnimAsSprite` texture table width.

## 0.1.4-beta (2026-10-16) — 64-Bit Build & Arcade Crash Fix

- **64-bit build (`UMK3_LOW32`):** Added 32-bit pointer support (`__ptr32`) keeping game heaps and static data below 2 GB.
- **Fixes:** Fixed crash on winning Arcade / leaving stage 14 caused by non-NULL initial scene handles in `gamecode_globals.c`; added fallback animation frames for Classic Sub-Zero's elbow combo (`RPCOMBO6..9`).

## 0.1.3-beta (2026-10-16) — Controller Menu Navigation & Console Launcher

- **Menu navigation (`src/runtime/pad_nav.c`):** Full gamepad and keyboard navigation across all 51 front-end screens, two-player simultaneous character select, right-stick palette switching, and input prompt icons.
- **Settings & Launcher:** Added monitor selection (`monitor=`), player 2 pause/moves-list keys, and redesigned Electron launcher.

## 0.1.2-alpha (2026-10-15) — In-Game Settings & Player Profiles

- **In-game settings menu (`Esc`, `src/runtime/settings_menu.c`):** Live graphics reconfiguration (resolution, aspect ratio, texture quality) and live keyboard/gamepad rebinding without restarting.
- **Player profiles (`src/runtime/profiles.c`):** Persistent player ID, name, and customizable fighter badge (`profiles.ini`).

## 0.1.1-alpha (2026-10-15) — Player 2 Controls for Local Versus

- **Two-player controls:** Added Player 2 keyboard and gamepad bindings (`p2key_*`, `p2pad_*`) and dual-controller polling (`plat_pad2_read`).

## 0.1.0 / 0.1.0b-alpha (2026-10-15) — Widescreen, iPad Layout, HD Textures & Auto-Updater

- **Aspect ratios:** Added `3:2`, `4:3` (iPad camera projection), `16:9`, and `21:9` support.
- **iPad UI & DUELO mode:** Added `ui_size=ipad` (960×640 virtual layout) and local 2-player `DUELO` mode (`GameMode 6`).
- **Texture quality & fast loading:** Added `IPHONE`, `IPAD`, and `MAX` (4× Catmull-Rom upscale) texture modes, parallel PVRTC decoding (`src/runtime/lime/par.c`), and fast table-driven PNG inflate (`src/runtime/lime/png.c`).
- **Auto-updater:** Added GitHub release checker and in-place updater to the launcher.

## 0.0.8 / 0.0.9-alpha (2026-10-10 – 2026-10-15) — Gamepads, Save States, Hitboxes & All Finishers

- **Controls & zero input lag:** Added XInput/WinMM gamepad support, fixed hold-and-release special moves (`QueueAndJump`), and removed frame smoothing lag during fights.
- **Debug tools (`F2`):** Added hitbox viewer, fight speed control (10%–400%), and 4 in-memory save-state slots (`src/runtime/savestate.c`).
- **Finishers:** Fixed `MERCY` banner timing and verified all Fatalities, Animalities, Babalities, and Friendships.

## 0.0.1 – 0.0.7b-alpha (2026-10-08 – 2026-10-09) — Initial Playable Native Port

- **Full decompilation & integration:** Connected all 2,572 decompiled functions (`lime/`, `gamecode/`, `gamecode/logic/`) with the cooperative coroutine scheduler (`src/runtime/fight_runtime.c`) and the 229 extracted binary data tables (`tools/build/logic_tables.py`).
