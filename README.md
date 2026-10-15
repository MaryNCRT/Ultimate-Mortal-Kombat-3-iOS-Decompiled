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

## Where the project stands — 10 October 2026 (alpha 0.0.8)

| | |
|---|---|
| **Decompiled** | ✅ **All of it.** 2,572 of 2,572 game functions have hand-written C: the LIME engine core (109), the game logic (291) and the fight engine (2,172). Nothing is left to transcribe. |
| **Verified** | ✅ The engine core passes differential tests against the recompiled original with zero divergences. The fight engine passes a behavioural differential test file by file, with the exceptions listed in [Verification](#how-much-of-it-is-verified) — every one of them a known limit of the test harness, not a known bug. |
| **Runs natively** | ✅ The real game runs natively on Windows (Linux/macOS from source): the front end with its 51 screens, Arcade with the tower to the end of the ladder, fights, sound, music and saves. How it fits together: [docs/HOW-THE-GAME-WORKS.md](docs/HOW-THE-GAME-WORKS.md). |
| **Fight** | ✅ **Fights play to the end by the real path** (0.0.2): round 1, round 2, the end of the match, `Task_GameDestroy`, Continue and the next fight, with the camera following both fighters and the voices playing. Before that, **the first fight ran by the real path.** After the tower (or straight from the menu with `--fight`, below) the game runs `Task_GameInit` and `Task_GameMain`: the arena draws, both fighters fight with the CPU playing, with the HUD and touch controls, for thousands of frames without a crash ([#54](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/54), [#57](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/57)). |
| **Playable** | ✅ **Alpha 0.0.8: 100% playable** (it can still have bugs; they are being fixed) ([release](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/releases)): menus, Arcade to Shao Kahn and the treasure screen, whole fights, from the keyboard (rebindable in the launcher), a gamepad or the touch controls, with a debug menu (F2). Still wrong: see *Known problems*. The fight engine's 229 data tables are extracted from the user's own binary at build time and verified against it. |

### How to play (alpha 0.0.7, Windows)

**Alpha 0.0.8** ([notes](docs/RELEASE-0.0.8-alpha.md); earlier: [0.0.7b](docs/RELEASE-0.0.7b-alpha.md), [0.0.7](docs/RELEASE-0.0.7-alpha.md), [0.0.6c](docs/RELEASE-0.0.6c-alpha.md), [0.0.6b](docs/RELEASE-0.0.6b-alpha.md), [0.0.6](docs/RELEASE-0.0.6-alpha.md), [0.0.5](docs/RELEASE-0.0.5-alpha.md), [0.0.4](docs/RELEASE-0.0.4-alpha.md), [0.0.3](docs/RELEASE-0.0.3-alpha.md), [0.0.2](docs/RELEASE-0.0.2-alpha.md), [0.0.1](docs/RELEASE-0.0.1-alpha.md)):
[Releases](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/releases)
has the launcher and only the sources the build needs, no game data. Its
known problems are the table below.

1. Download this repository (or the alpha release) and keep the folder together.
2. Run **`UMK3-Launcher.exe`** (in a release; from source, `npm install`
   and `npm run portable` in `launcher/app`, see its README). In *COMPILE
   YOUR .IPA* choose your own `.ipa` of UMK3 1.2.59 for iPhone and press
   **Compilar**. The launcher downloads a pinned compiler
   (llvm-mingw 20260616, SHA-256 checked) and Python 3.12.10 embeddable into
   `toolchain\`, checks the binary (uuid `90d6f56a…`, not encrypted), extracts
   the data tables from it, compiles `umk3-game.exe` and copies `res\` out of
   the `.ipa`. About half a minute after the first download.
3. In *GRAPHICS* pick the 3D resolution (480×320 up to 3840×2560),
   fullscreen, language, debug mode and an optional frame picture for the
   fullscreen bars; in *CONTROLS* the 6- or 5-button layout (the game starts
   with it) and each layout's keys. Everything is saved to `umk3.ini` the
   moment it changes. In *PLAY* press the big **PLAY** button; the box above
   it, *Skip the intros*, starts the game past the two publisher logos
   (`skip_intro=1`). The launcher
   window resizes freely (4:3 by default) and F11 makes it fullscreen; the
   button at the top right switches it between English and Spanish.

**Golden rule: the game depends only on its own folder.** `umk3-game.exe`
reads `res\` beside itself and nothing outside it (no `../` lookups, no
junctions; `Info.plist` is copied into `res\`). No game data and no game
executable are distributed: the exe only exists after the player's own
`.ipa` has been compiled. Build scripts: [`launcher/`](launcher/).

**Fight keys:** W A S D or arrows to move (two at once for diagonals); U high
punch, I low punch, O block, J high kick, K low kick, L run (or numpad
7 8 9 / 4 5 6). **P** opens the pause menu (again: resume), **M** the moves
list. Esc no longer quits. The mouse is the finger. Player 1's keys can be
changed in the launcher (box 3; saved as `key_*` lines in `umk3.ini`).

### Debug: straight into a fight

```
umk3-game.exe --fight kitana kunglao 0
```

skips the menus: two fighters by name (as in the select screen, case and
spaces ignored) or number 0-25, and an arena 0-15. `UMK3_FIGHT=kitana,kunglao,0`
does the same.

**Debug mode:** tick *Debug mode (F2 menu)* in the launcher (it writes
`debug_keys=1` to `umk3.ini`; off by default). Then, in the game:

| key | does |
|---|---|
| **F2** | the debug menu, drawn over the frozen game: any fight (both fighters, Motaro and Shao Kahn included, any stage), any of the front end's 51 screens, the main menu, win/lose the round or the match, the direct keys on/off, the info line |
| F3 | the info line: task, screen, fighters, rounds |
| F6 / F7 | previous / next front-end screen |
| F8 | main menu |
| F9 / F10 | end the round (KO player 2 / player 1) |
| F11 / F12 | win / lose the whole match (F11 skips the fight) |

F3 and F6-F12 are the *direct keys*; the F2 menu turns them off and on. A
fight or screen picked while the game is still loading starts once the main
menu is up. For scripts: `--screen <n|name>` (or `UMK3_SCREEN`) opens one
screen at start, `UMK3_DBG_OPEN=<tick>` opens the menu.

Every session started by double-click writes `logs/umk3-<date>-<time>.log`
beside the exe: task changes, loading steps and, on a crash, the addresses to
symbolise. A log is deleted once the error it shows is fixed.

### Known problems (9 October 2026, alpha 0.0.7)

Release notes: [0.0.9](docs/RELEASE-0.0.9-alpha.md), [0.0.8](docs/RELEASE-0.0.8-alpha.md), [0.0.7b](docs/RELEASE-0.0.7b-alpha.md), [0.0.7](docs/RELEASE-0.0.7-alpha.md), [0.0.6c](docs/RELEASE-0.0.6c-alpha.md), [0.0.6b](docs/RELEASE-0.0.6b-alpha.md), [0.0.6](docs/RELEASE-0.0.6-alpha.md), [0.0.5](docs/RELEASE-0.0.5-alpha.md), [0.0.4](docs/RELEASE-0.0.4-alpha.md), [0.0.3](docs/RELEASE-0.0.3-alpha.md), [0.0.2](docs/RELEASE-0.0.2-alpha.md), [0.0.1](docs/RELEASE-0.0.1-alpha.md).

What a player sees today, and what is known about each.

| Symptom | What is known |
|---|---|
| **No fight gets past round 1.** | **Fixed for 0.0.2** (checked in game by Mary, 2026-10-08): round 2, the end of the match, Continue and the next fight all work. Five causes, each against armv7: `RoundSummaryUpdate` returned early on `WinnerMessage`/`IsInFinishing` (the binary keeps the timer running, 0x29a64/0x2a2d6); the fade-in after the summary went to `InfoScaleAdd` instead of `FE_FadeAdd` (0x2abaa); `GetScenePointingTo` returned the last node instead of NULL, so `Task_GameDestroy` turned the scene list into a ring and hung (0x5ef4c; `LIME_FreeScene` rewritten from 0x5efe0); `MeshSetLayers` was never initialised, so stage 1 crashed on load; `WinnerMessage` was 2 bytes, not 128. |
| **Pause menu options invisible; moves-list pages wrong; HUD "i"/pause buttons missing.** | **Fixed for 0.0.3.** `processString` (`usprintf`'s engine, 0xa7600) only emitted a token's value on the no-token path; both paths share the tail at 0xa7650 that emits it, so every templated string lost its `%s`/`%d`. The generic moves-list pages index the table by absolute row (`row << 6`, 0x1efb0) and pass a NULL caption. `DrawHUD` draws both corner buttons in every mode while not paused (0x28910, 0x2a170); the C only did for `GameMode > 1`, and drew pause on one pulse frame (the pulse is a growing, fading second INFO icon, 0x2ab2a). New keys: P pause, M moves list; Esc no longer quits; player 1 keys configurable in the launcher. |
| **The game crashes easily.** | User report, alpha 0.0.1; `logs/` beside the exe has the addresses. Not investigated as a whole. |
| **Several texture errors.** | Many fixed in 0.0.2 (black fighters, hats, Sindel's hair -- row below); several remain. Report each with the stage and fighter. |
| **The camera angles in the fight look wrong.** | **Fixed for 0.0.2** (checked in game by Mary, 2026-10-08): `UpdateArcadeCode` converted each arcade object at `GameObjects[0] + 16*i` instead of `GameObjects + 16*i` (0x21fde-0x22026), so one fighter's 3D position was garbage and the camera, which centres and zooms on both, followed only one. `TrackCam` also passes a difftest against the oracle (0 divergences / 20,000 cases) after keeping its `x + (y - x)` look-at commit. |
| **The on-screen joystick does not animate.** | **Fixed for 0.0.2** (checked in game by Mary, 2026-10-08): `DrawControls` offsets the knob by `JoyOffset[JoystickState]`, a function-local static the binary names `_JoyOffset.11128` (0xde09c). `tools/mkdata.py` did not match the `.NNNN` suffix, so the table was generated as zeros; it now aliases function statics, and the nine offsets come from the image. |
| **The menus are not complete; some menu sections crash.** | Seen by Mary, 2026-10-08. Not investigated section by section yet. |
| **Sindel's hair missing; fighters, Kung Lao's hat and props drawn black.** | **Fixed for 0.0.2** (checked in game by Mary, 2026-10-08). Two causes, both against armv7: `LIME_LoadSkin` dropped the second block of a two-block `.skin` (0x6067c, `skin_containerSECOND`), so no character had a second skin -- Sindel's hair; and `LightPlayers` writes the player's texture every frame, `+0x530` or else `anim[0x14]` (0x1c0d8 / 0x1c2d8), where the C only wrote it for an alternate costume -- a fighter or attachment the intro had not textured was drawn with no texture, solid black. |
| **Sounds play at the wrong moments; character voices missing.** | **Fixed for 0.0.2** (checked in game by Mary, 2026-10-08): `AddNewGameEvents` passed `get_gsound` its voice group and random seed swapped (binary 0x7368a: `get_gsound(arg & 0xf, arg >> 4, limeRand())`), so every attack/jump/grab/hit grunt read past its table and played a stray sound or none. |
| **Sometimes every fighter in the tower is Jade; the tower animation is wrong.** | **Fixed for 0.0.3** (checked in game by Mary, 2026-10-08). `Load_Tower` (0x23314) wrote the saved ladder into a `TowerData` table nothing read; the binary writes `OpponentTowerList` (0x14fcb4) itself, so every rung kept the image default, 16 (Jade). `FE_Task_Tower` (0x8310): states 2 and 4 bias x by `Destiny`, not `Stage`; state 2 fades into the fight once settled (0x8c56); state 4 fades after 360 units (0x8f6e); the climb entry snaps the camera and sets `MoveUpTower = JustWon ? 0 : 1` (0x8d98); survival picks `TowerRand[abs(rand) % 22]` and the boss rungs force their arenas (0x932e). |
| ~~The arena is always the same in Arcade.~~ | Not seen any more: the arena changes (checked by Mary, 2026-10-09). |
| **Reported by Mary after 0.0.3 (2026-10-09), not investigated yet:** | |
| ~~Audio stops working properly after two fights in a row~~ (Arcade). | **Fixed** (checked by Mary, 2026-10-09). `UnLoadSoundList` (0xa7f08) searches `SoundListUniqueHandle` (0x38b8b0, the table `LoadSoundList` fills); the transcription read a `SoundListUniqueIds` that is not in the binary, so no fight sound was ever deleted and limeLoadSound's 512 slots were full after about two fights. Voices still playing a deleted sound are now stopped first (`plat_audio_stop_pcm`). |
| The Motaro fight has audio problems. | Seen by Mary 2026-10-09. Probably the `t_rst5` fix below; to confirm. |
| Some special attacks have no sound, projectiles especially. | Not investigated. |
| Texture errors remain on some stages and in some modes; the menu has texture errors too. | Not investigated stage by stage. |
| ~~Some menu sections crash.~~ | **Fixed** (checked by Mary, 2026-10-09: no menu section crashes). `FE_Task_Treasure` (18) and `FE_Task_Stats` (15) were spotlight screens; see the unlockables row. |
| ~~Shao Kahn may be crashing the game.~~ | **Fixed** (checked by Mary, 2026-10-09). `t_rst5` (0x473d0), the hit-reaction dispatcher, stored the bosses' reaction tables as raw iOS addresses (0x17b8d0 `motaro_branches`, 0x17b884 `sk_branches`) and read them: the first reaction against Motaro or Shao Kahn crashed or ran garbage. A 4,000-tick fight against each now runs clean. |
| ~~The winner's name ("X WINS") is still not shown.~~ | Shown (seen in 0.0.6 tests: "KITANA VENCE" at the end of Arcade, "SMOKE VENCE" in a fight). |
| The menu is not complete (target: 100%), and some modes are missing. | Not inventoried yet. |
| ~~The game's alerts (no connection, "are you sure?") are Windows message boxes, hidden behind the game in fullscreen.~~ | **Fixed for 0.0.6b** (checked by Mary, 2026-10-09). `plat_ask` draws them inside the game window as an iPhone OS 3 `UIAlertView`: on the device `+[modalAlert askFull:textOK:textCANCEL:]` (0xb5444) passes the whole text as the title, message nil, OK as the cancel button (index 0, left) and CANCEL as the other (index 1, right), and blocks in a run loop; the port blocks the same way, over a copy of the frame. The F2 debug menu wears the same style (`plat_ui_menu`), and it no longer clears the fullscreen frame picture (`glClear` ignores the viewport). |
| The game's ads open in separate windows, which forces leaving fullscreen. | Wanted: show them in windows drawn inside the game, in the same executable. |
| **Reported by Mary 2026-10-09, after the debug menu:** | |
| ~~The end of Arcade does not go on to the unlockables screen~~ (it crashed). | **Fixed** (checked by Mary, 2026-10-09). `FE_Task_Select_Treasure` and the seven other screens that draw the spotlights: `spotlight_SpriteDef` / `spotlight_Anim` are the tables themselves (0x175188, 0x175608), not pointers -- the port had an empty store; `DrawAnimAsSprite` (0x1c8bc) takes the texture from its sixth argument (0x1c956), computes `abs(counter) % frames` (0x1c8f6) and draws `record[2..3]` as the size with corner+extent UVs (0x1ca00). Menu textures now load the `.pvr` first, as the device does: `FE_METAL_BG.PNG` holds its art in a 480x320 corner and drew a white L. |
| ~~Achievements draw wrong, with overlapping text.~~ | **Fixed** (2026-10-09). `achievementsDraw` (0xa09xx) draws both lines at x = 20, the heading at y + 4 and the name at y + 14; the constants were swapped. |
| ~~A slow black fade at the start and end of rounds, in some fights.~~ | **Fixed** (checked by Mary, 2026-10-09). The fade runs ten times slower while an achievement banner shows; `areAchievementsViewing` (0xa02ac) counts 20 slots (`cmp r2, #0x50`), the transcription 24, and a save with slot 21 at 1 kept a banner "showing" forever: five-second fades. |
| ~~FINISH HER shown for male fighters.~~ | **Fixed** (checked by Mary, 2026-10-09). The voice was right; `AddNewGameEvents` has two copies, FINISH HIM = `TriggerAnim(2)` (0x73f2a) and FINISH HER = `TriggerAnim(3)` (0x73d28), merged into one with a constant 3. |
| ~~The icons on the loading screen are misplaced.~~ | **Fixed**: the Kombat Kode row below. |
| ~~The moves list ("i") icons are still misplaced.~~ | **Fixed** (2026-10-09). The rows zig-zag: rows 0, 2, 4 right-aligned at FE_X(432), rows 1, 3, 5 left-aligned at FE_X(48), name and icons on the same side (0x1eed2 / 0x1efee); the name stayed right on every row. |
| ~~Menu buttons drawn wrong (Facebook art on the settings boxes, frame slivers on Stats, Achievements, Leaderboards, Share info...).~~ | **Fixed** (2026-10-09). `DrawButtonNew` (0x57d8): u0 is `fp` and v0 `[sp+0x28]` (style 0: fp = 0, v0 = 0x3f020000, 0x58d4); every style had them swapped and drew the wrong window of `FE_BUTTONS_01`. |
| ~~Frozen fighters (Sub-Zero's freeze) draw completely white.~~ | **Fixed** (checked by Mary, 2026-10-09). `LoadAnimatedCharacter` loaded `<name>_DIFFUSE_ICE` on the front-end path only; the binary loads it on both (the fight path's arms all return to 0x5c7f8), so in a fight `anim[0x18]` was NULL and a frozen fighter or the ice clone drew untextured. |
| Sonya's fatality does not show properly. | Seen by Mary 2026-10-09. Not investigated. |
| Random crashes. | Seen by Mary 2026-10-09; logs needed. |
| ~~Human Smoke cannot be chosen by holding Smoke's portrait.~~ | Works (checked by Mary, 2026-10-09): hold the click on Smoke for three seconds (`drawCharacterSelection`, `SmokeCounter` > 180). |
| ~~The debug menu's win/lose round can give the round to both fighters.~~ | **Fixed** (2026-10-09): the round keys act only while a round is in play (`dbg_round_live`: no intro, round summary, finisher or pause, both fighters up); pressed during a round's end they ended it again with the other fighter. Win match on the last round leaves the fight, as a won match does. New: *Arcade: next is Motaro / Shao Kahn* (Arcade only). |
| ~~Shao Kahn's death at the end of Arcade shows an empty arena and its sound repeats.~~ | **Fixed for 0.0.6** (checked by Mary, 2026-10-09). `LIME_RenderEvents` (armv7 0xa4a3c), which draws every scene-driven effect, was an armv6 transcription that multiplied an uninitialised matrix and drew with it, so no event was placed where it belonged. Rewritten from armv7: an event that follows a matrix gets `+0x68 = +0xa8 * follow` (and dies with it), mirrored events negate x and swap the culled face, the event's colour goes into `SceneTint` (16 bytes; it was declared 12). `SK_ENDING` / `SK_LOOP` now play (Shao Kahn in green light, beams), and stage effects drawn as events (the graveyard's moon and sky) appear too. The death sound is `Skdiemix.wav` (tsound 0x88, 4.2 s), played once (0x73b7a); the scream repeats inside the file itself. The binary starts no tune there (this version has no "No More" track). Once the winner banner has slid up, `DrawHUD` (0x2a5e0) draws game texts 0x39c-0x39e (SHAO KAHN IS NO MORE / YOU ARE THE / ULTIMATE MK3 CHAMPION) at y 112, 144, 176; that block was missing and is restored. Arcade only: outside Arcade Shao Kahn has no death scene, as in the binary. Test: `UMK3_ARCADE="0,7" UMK3_DEBUG_KEYS=1 UMK3_DBG_KEY="1400:2" umk3-game.exe --fight kitana "shao kahn" 2`. |
| ~~A custom frame (fullscreen) showed through the stage where it draws nothing (the sky).~~ | **Fixed for 0.0.6** (checked by Mary, 2026-10-09): the game's own area is cleared to black after the frame is drawn (`glScissor` + `glClear` in `runtime/game_main.c`). |
| ~~In a finisher against Reptile, the dizzy opponent walked towards the player instead of standing still.~~ | Not seen any more (Mary, 2026-10-15); a forced fatality against Reptile left him standing. Cause not found; probably one of the debug-fight fixes. |
| ~~One stage (the spiked bridge, against Nightwolf) draws no background.~~ | **Fixed for 0.0.6** by the `LIME_RenderEvents` rewrite: the background draws again (all 16 stages checked). |
| ~~The loading screen's Kombat Kode icons are misplaced (two rows; the lower one covers "Loading").~~ | **Fixed** (checked by Mary, 2026-10-15). `drawKodeTip` (0xa0530) draws one row of six at y 160: each loop pass draws seq[i] at x and seq[i+3] at x + 0x74 (`add.w r3, r4, #0x74` is the X argument); the transcription put the 0x74 on Y. The kode's name (word 6 of its entry, at y 202) was missing too, and the texture is deleted at the end of every call. |
| Pausing sometimes shows the fight shrunk into a corner behind the pause menu. | Seen by Mary 2026-10-09. Not investigated. |
| ~~Kitana kept blocking and behaved oddly in the fight against Motaro.~~ | **Fixed** with `t_rst5` (checked by Mary, 2026-10-09). |
| **Mary's queue of 2026-10-09, after the launcher:** | |
| ~~Choosing a reward on the treasure screen softlocks: it never returns to the main menu.~~ | **Fixed** (checked by Mary, 2026-10-09). `FE_Task_Select_Treasure` runs its 300-unit clock once a tile is picked: `(sel == -1) ? allDone : 1` at 0x113ba; the transcription had it inverted, so the clock stopped at the pick. |
| ~~The Waterfront (pier) stage has a black floor.~~ | **Fixed for 0.0.6** (checked by Mary, 2026-10-09). `StringInString` (0x5e27c) is an exact comparison, not a substring search; `LIME_FindMeshByName` used `strstr`, so the floor node "Object04" found mesh "Object040", the floor (mesh 3, `WATERFRONT_NEWFLOOR`) was never marked visible and `LIME_FreeNonVisibleMeshes` freed it. All 16 stages checked after the fix. |
| Texture errors on some stages and fighters; Kung Lao's hat black in places. | Partly fixed in 0.0.6 (stage effects, the pier floor); the rest not investigated stage by stage. |
| Crashes in the modes other than Arcade. | Not investigated. |
| Fatalities with missing animations; fighters frozen by Sub-Zero drawn white. | Not investigated. |
| **Mary's list of 2026-10-09, after 0.0.6b** (study: [docs/OPEN-ITEMS-STUDY.md](docs/OPEN-ITEMS-STUDY.md)): | |
| Typed finishers (fatality, friendship, babality, animality, mercy) do not seem to fire; after a fatality the announcer and the FATALITY HUD banner are missing. | Debug tool added in 0.0.6c (checked by Mary: the fatality runs). The banner and voice come from `FatalityMessage` (Blood.c's finisher event, `LogFinisher("Fatality")`); why they do not show after a forced one is not found yet. Tool: the F2 menu's FINISHER row, enabled during FINISH HIM once the loser is dizzy, calls `DoASpecial` (moves.c, 0x51830) for player one with `which` 0xd..0x13 (pit, mercy, fatality 1, fatality 2, animality, babality, friendship) -- the call the joystick code makes; scripted: `UMK3_DBG_FIN="tick:n"`. Finding: every finisher goes through `mercy_xfer` (0x54ac4), which starts nothing unless G+0x45c == 3, G+0x450 == 0, the winner is on the ground and the loser's thread sits in `t_dizzy_sleep`; a request made before the loser is dizzy is dropped silently. Forced this way, Scorpion's fatality 1 runs (`t_drone_do_fatality1` -> `t_fatality_align` -> `t_do_fatality_1`). Why typed ones fail is not found yet. |
| After the last hit the loser keeps acting for a moment during FINISH HIM. | In a debug win the loser (CPU) enters `t_finish_him` and `t_dizzy_dude` about 20 engine passes after the winner -- it finishes what it was doing first. Mary reports the walking-towards-you case fixed in her test; to watch. |
| ~~Shao Karnage: player one's bar covers the score; difficulty broken.~~ | **Fixed** (checked by Mary, 2026-10-09). `DrawHUD` skips the whole plate block in mode 3 (0x284ec), player one's included; and Karnage's difficulty is `GameMode - 3` = 0 (0x2db1c: r3 still holds GameMode), not `Destiny - 3` (-4 outside a ladder). Time out ends the mode into its summary, as designed. |
| ~~No FATALITY / ANIMALITY / FRIENDSHIP banner; the fight ends before the announcer finishes.~~ | **Fixed** (checked by Mary, 2026-10-09). The banners were never drawn: transcribed from DrawHUD 0x2a3c0..0x2ab26 (`DrawFinisherBanner`); their counters are floats. The round summary timer adds 0.7 a tick while `IsInFinishing` (0x2a2d6), not 1.25. The BABALITY banner is the eight bouncing blocks (0x29d56, `BabalityVel` / `BabalityHeight`). Babality, animality, mercy and friendship had no banner or tune because `create_fx_param` re-read `obj->field1c` after `NewThread` changed it; the binary keeps it in r5 from entry (0x58b68). Both checked by Mary, 2026-10-09. |
| ~~MERCY banner flashes for a moment and Mercy.mp3 is cut off.~~ | **Fixed** (checked by Mary, 2026-10-10). DrawHUD's mercy counter rises 0.2 a frame (0x2844e, the double at 0x286f4); it rose 13, so the banner was gone in three frames and the stage music came back over the tune at once. Mercy itself was already right: the loser gets up with a sliver of health and the same round goes on (`t_master_proc_mercy`); the animality, which needs a mercy first (`q_mercy` in `DoASpecial`), then works with its banner and model. Scripted: `UMK3_DBG_FIN="1950:1;2450:4"` with KO keys for a round each, then a third. |
| ~~Typed finishers never fire.~~ | **Fixed for the 5-button layout** (checked by Mary, 2026-10-09). `GetArcadeJoyBits` compares later table entries against the word WITH the finishing bit 0x2000 (0x1b830 `mov r3, r0`); it reset to the bare bits, so no finisher entry could match. 5-button finishers are one gesture with S during FINISH HIM: toward+S fatality 1, toward+down+S fatality 2, down+S animality, away+S babality, S friendship, away+down+S pit, S+run mercy (`FourButtonMoves`). The 6-button layout feeds the arcade code directly: Sub-Zero's babality typed as down, back, back, HK fires (scripted test, `UMK3_BUTTONS=6`). Test tool: `UMK3_KEYS="tick:keys:hold;..."`. |
| ~~Kung Lao's hat black on the character select screen.~~ | **Fixed** (checked by Mary, 2026-10-09). The hat's colour is its baked lighting (average 15/255) plus `StaticMeshAmbient`, which only `LightPlayers` writes, in a fight; the select screen has no stage, so it stayed 0 (__common). Port fix in `runtime/game_main.c`: in the front end it gets the light `RenderFECharacters` gives the body (0.65 0.65 0.7) x 255. |
| ~~Survival mode crashes after winning a fight.~~ | **Fixed** (checked by Mary, 2026-10-09). `QuitAsWin` / `QuitAsLose` reach SurvivalStage and DisplaySurvivalStage through pointer slots (0x26804, 0x26b8c, 0x26b92); `SurvivalStageP` was left NULL and `DisplaySurvivalStage` was used as a pointer. Both slots now hold the addresses. |
| ~~Sub-Zero's ice clone drawn white.~~ | **Fixed** (checked by Mary, 2026-10-09): the same missing ice sheet as the frozen fighters. |
| ~~Scorpion's spear is not drawn.~~ | **Fixed** (checked by Mary, 2026-10-09): `RenderExtras`' mirrored test was inverted (0x20ffc `bpl`), and `RenderLevelPlayers` set `DrawSpear` on one of the five spear frames only (every arm returns to 0x24962). Other props: to check one by one. Earlier notes: Started. Scorpion's spear (`UMK3_DBG_SPECIAL="tick:0"` throws it) is projectile slot 4 with frames 0x1a7e then 0x1a80; in `RenderLevelPlayers` its frame lookup through the owner's animation record gives -1 (`w[0x14]`), so the object draws nothing, and `RenderExtras` gets `DrawSpear[0] = 1` for 0x1a7e but draws no visible shaft either. Next: check the projectile's frame table (`owner->anim + 0x2c`, `HavePreloadedCharacter`) and `limeDrawFaceMeSpriteWH` against armv7. |
| ~~Controls: the 5- and 6-button layouts need their own key settings, and a choice of layout that the game follows.~~ | **Done for 0.0.6** (checked by Mary, 2026-10-09): the launcher's CONTROLS tab picks the layout (`buttons=` in `umk3.ini`, written into `Settings[4]` after `Load_SettingsData`), with separate keys (`key_*` for six buttons; `key5_p/b/k/r` and `key_special` for five). |
| **Mary's list of 2026-10-14, after 0.0.8:** | |
| ~~Hold-and-release moves never fire (Mileena's sai, Liu Kang's bicycle kick; Ermac's hold-RUN finishers too).~~ | **Fixed** (checked by Mary, 2026-10-14: sai and bicycle kick). `QueueAndJump` stores `last_switch_ram` (a button's press time, which `get_tsl_px` reads to see how long it was held) only for a press entry: `cbz` on `e[3]` at 0x572d4 skips the stores at 0x57322/0x57326 for a release. The transcription stored on release too, so every hold measured 0 ticks. |
| ~~Input delay; the victim of a combo (Mileena's HP,HP,HK,HK,U+LK,U+HK, Liu Kang's) reacts before the blow lands.~~ | **Fixed** (checked by Mary, 2026-10-14: "no input lag"). Port changes, none in the binary: `AnimSmoothWindowSize` (2 at 0x171368) drew each fighter's pose two ticks behind its position, hits and sounds -- set to 0 in a fight; `glFinish` after the swap so the driver queues no frames; the keys drive `ButtonStates` / `JoystickState` directly (held keys stay held) instead of through the touch buttons; a tap shorter than a tick still counts; a frame of one tick's time counts as exactly one tick; on a 60 Hz screen the keys are read just before the refresh (`UMK3_LOG_PACE=1`). A press now changes the move on the same tick. The combo's sync itself is still to be re-checked by eye. |
| Ermac's friendship and animality do not fire. | Probably the hold-and-release fix above (his inputs hold RUN); to be checked in game. |
| ~~A fight started from the debug menu kept its fighters in the Arcade fights after it.~~ | **Fixed** (2026-10-15). Only the VS screen resets `Character2Override` to -1; game_main.c clears it once the debug fight is up, and when the fight is over it puts back the fighters, the stage, the mode, the Arcade rung and AUTO palettes. The menu's PALETTE rows apply to debug fights only (`DbgFightActive`). |
| ~~The dizzy opponent slides toward the winner after a finisher forced from the debug menu.~~ | Not seen any more (Mary, 2026-10-15). |
| ~~A crash just closes the game.~~ | **Done**: a crash writes `logs\crash-<date>-<time>.log` (reason, function by name from the exe's own symbols, stack, task, screen, fighters, stage) and shows it in a message box. `UMK3_CRASH_TEST=<tick>` crashes on purpose. |
| ~~Debug menu: show hitboxes.~~ | **Done** (Mary saw them, 2026-10-15; placement fixed after). F2 > TOOLS > HITBOXES: each fighter's body box (player one blue, player two green) from the engine's own extents; every strike test (`strike_check_regs`, other.c) shows the attack box in red and the target's box in yellow for 20 ticks, white on a hit. `UMK3_DBG_HITBOX=1`. |
| Debug menu: fight speed and save states. | **Done** (save states checked by Mary, 2026-10-15). TOOLS > FIGHT SPEED, 10% to 400% (`UMK3_DBG_SPEED`); STATE SLOT / SAVE STATE / LOAD STATE, four slots in memory that restore the exact moment without restarting (`UMK3_DBG_STATE="tick:s1;tick:l1"`). How: [HOW-THE-GAME-WORKS](docs/HOW-THE-GAME-WORKS.md#save-states-port-only). |
| ~~Pick palette 1 or 2 on the character select screen.~~ | **Done** (checked by Mary, 2026-10-15). A "< COLOR 1 >" box under each fighter (player two's in 2-player versus only); a click switches it, and the 3D model, the versus portrait and the fight follow. Port only; nothing picked = the binary's rule (player two takes the second palette in a mirror match). |
| ~~Gamepad support; keyboard and gamepad in separate launcher tabs.~~ | **Done for 0.0.9** (checked by Mary, 2026-10-15). XInput for Xbox pads, WinMM for PlayStation 4/5, Switch Pro and generic pads (DirectInput's EnumDevices took 40 s on one machine; WinMM is searched on a thread). `pad=` picks the pad, `pad_dbg_menu=` the combination that opens the debug menu (L3+R3), `umk3-game --list-pads` feeds the launcher's list. The menus are still mouse-only. |
| **Windows only for the launcher.** | Linux/macOS still build from source with CMake. |
| **Only the iPhone 1.2.59 .ipa works.** | The launcher refuses any other binary (uuid check); the iPad 1.2.56 build has different addresses. |

Fixed on 8 October, each against the armv7 binary: the black arena and the
crash before the fight (scene loader, scene renderer, events module); hats
and attachments and their textures; player two's intro (placed and mirrored);
the HUD text (limeDrawFONT reads ASCII); the FIGHT overlay's texture and
alpha; "ROUND 1" staying up; mirrored diagonals (the dial's up/down); the
keyboard for player 1 (W A S D or arrows, U I O J K L); several fight-logic
crashes (button handlers' second argument, `t_rup3`, `seq_lookup`).

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
███████████████████████████████████░░░░░  87.59%
```

| Area | Weight | Done | |
|---|---:|---:|---|
| Binary analysis and source-tree mapping | 4% | 100% | `██████████` |
| Tooling and the verification oracle | 8% | 100% | `██████████` |
| Asset format specifications | 8% | 100% | `██████████` |
| `lime/common` — engine core (109 fn) | 12% | **100%** | `██████████` |
| `gamecode` — game logic (291 fn) | 18% | **100%** | `██████████` |
| `gamecode/logic` — fight engine (2,172 fn) | 28% | **100%** | `██████████` |
| Native PC platform layer (161 fn to rewrite) | 17% | 27% | `███░░░░░░░` |
| EA SDK stubs (27 fn the game calls) | 5% | 100% | `██████████` |

**87.59% of the total estimated effort. Alpha 0.0.8 is 100% playable:** whole
fights by the real path, with known problems (see above).

**Read that number for what it measures, and for what it leaves out.** It
weighs the eight areas in the table, and two pieces of work are in none of
them:

- **The fight runtime.** The table counts the fight engine's 2,172 functions
  as written; nothing in it counts the loop, the thread scheduler and the
  glue that make them run frame by frame on a PC. **That runtime now exists
  and runs the real game** (alpha 0.0.2 and 0.0.3): front end, tower, fight
  load, both rounds, the end of the match, Continue and the next fight.
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
the windowed game (alpha 0.0.3). What is left is fixing the functions whose
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

```bash
# Windows (MinGW-w64 + Ninja) uses Win32; Linux uses SDL2 and SDL2_mixer.
# On Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-mixer-dev libgl-dev
cmake -S . -B build -G Ninja
cmake --build build

# The real front end, in a window. Point it at res/ inside YOUR extracted .ipa.
build/umk3-menu  path/to/Payload/UMK3.app/res

# The arena and fighter test scene: umk3-fight <res> [character] [stage]
# (Windows backend only for now, like umk3-test)
build/umk3-fight path/to/Payload/UMK3.app/res

# Both in one program: F2 enters the test scene, F3 returns to the menu.
build/umk3-test  path/to/Payload/UMK3.app/res
```

The mouse stands in for a finger. Save files go to `save/` beside the exe on Windows
and `~/.local/share/umk3` on Linux (`UMK3_SAVE_DIR` overrides both).
`UMK3_SHOT=<n>` runs n frames, writes a screenshot and quits.

---

## Contributing

Contributions are welcome, and the project is structured so that people can work in parallel without stepping on each other — each module is independent, and the acceptance criterion is objective.

**One rule matters more than the rest: nothing is done until it is checked against the binary.** Readable code that behaves *almost* like the original is worse than no code at all, because it fails silently and much later.

The decompilation is finished, so the open work has changed shape. Where help is most useful now:

- **The fight runtime** — running the decompiled fight engine frame by frame: its thread scheduler, the per-frame logic and the bridge from `Task_GameInit`. A headless driver runs it in [#46](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/46); open PR [#43](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/43) boots the game to character select. Still missing: the hand-over from the front end to `Task_GameInit`/`Task_GameMain`.
- **The 229 data tables** — ✅ done in [#46](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/46) and [#48](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/pull/48). `tools/logic_tables.py` extracts 1,118 objects (4,291 relocated words) from the user's binary at build time; `tools/check_logic_tables.py` checks them against it: byte-exact round trip, every relocation's target, no pointer in an `int16_t` table, no missed pointer, and the image's layout kept in the linked program. A second, independently written generator agreed on every shared word but 53, all settled by the code that reads them. `cmake -DUMK3_BINARY=...` + `ctest -R logic` runs it ([docs/PROGRESS.md](docs/PROGRESS.md#fight-data-tables-how-they-were-verified-2026-10-08)).
- **The platform layer** — MP3 music on the SDL2 backend, the fight's keyboard and gamepad input.
- **Port decisions already written down** in the open issues: widescreen ([#22](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/22), [#24](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/24)), frame rate ([#23](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/23)), mods ([#29](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp/issues/29)).

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
