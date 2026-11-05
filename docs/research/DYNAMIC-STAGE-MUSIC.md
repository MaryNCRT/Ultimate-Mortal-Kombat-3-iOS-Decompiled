# Research: dynamic / adaptive stage music (Genesis style)

**Status: investigation only, nothing implemented.** Written 2026-10-30,
filed 2026-11-05 so it is not lost. Paused until Mary records the Genesis
version (see section I).

Each finding is marked **[F]** (fact checked in the repo or the files),
**[D]** (deduction) or **[?]** (hypothesis, not verified).

This is a port feature, not decompilation: the target behaviour is what the
1995 Sega Genesis release does, observed by playing it. No Genesis code or
data enters the tree; the iOS binary stays the reference for everything the
game itself does.

## The request

Stage music should react to the fight, with Soul Chamber as the first case:

1. Normal fight: the stage tune plays.
2. FINISH HIM/HER: the tune jumps to a "critical" section.
3. After it, an "ending" section plays, fatality done or not.
4. "X WINS": the music stops or pauses.
5. Next round/match: the tune starts again from its beginning.

Proposed times (unverified): critical 2:03–2:10, ending 2:10–2:16.

## Summary

- **Feasible with limited changes.** The audio architecture does not need
  rework.
- **What is missing is the Genesis evidence**: the repo cannot show how the
  Genesis does it.

Four findings change the brief:

1. **The Soul Chamber stage does not use `SoulChamber.mp3`; it uses
   `Tongue.mp3`.** [F] `SoulChamber.mp3` ships in the `.ipa` but the binary
   never names it (0 hits in `UMK3.armv7`). [F] `LevelMusic[10] =
   "Tongue.mp3"` (`src/runtime/gamecode_globals.c`). [F] The debug menu's
   comment puts level 10 at the Soul Chamber (`src/runtime/debug_menu.c`).
2. **The proposed times fit neither file.** [F] `Tongue.mp3` is 2:06.77, so
   2:10–2:16 does not exist in it; `SoulChamber.mp3` is 2:15.21, so 2:16 is
   past its end. [D] They probably come from another recording (a Genesis
   rip).
3. **[F] iOS already changes the music at the finish.** When a finisher
   starts, the stage tune is cut and `Fatal1.mp3` or `Fatal2.mp3` plays
   (~3 s, no loop). Animality, Mercy, Friendship and Babality have their own
   tune. A new system has to live with this.
4. **[F] The stage tune does not restart between rounds.** It starts once
   per fight and plays on through the rounds.

## A. Current architecture

**Start and stop:**
- `Task_GameInit` calls `limePlayTune(LevelMusic[LevelSelect], vol, 1)`
  (looping) — `src/decomp/gamecode/GameCode.c`.
- Online, the same call is in `Task_MultiplayerSync` (same file).
- `Task_GameDestroy` calls `limeStopTune()` at the end of the fight.

**The three tune functions** (`src/runtime/lime_menu.c`): `limePlayTune`,
`limeStopTune`, `limeSetTuneVol`. All respect `rb_quiet()`, so they do
nothing while rollback re-simulates frames. Beneath them is the platform API
`plat_music_play/stop/volume/playing` (`src/runtime/platform/platform.h`).

**Backends:**
- [F] **One music stream**, separate from the sound-effect voices.
- [F] **Windows** (`win32_audio.c`): decodes the whole MP3 to PCM at start
  (`g_mpcm`, `g_mframes`), plays from `g_mpos` through 4 buffers of ~90 ms;
  the loop always returns to frame 0.
- [F] **Linux** (`sdl_audio.c`): also decodes the whole MP3 (libmpg123),
  `g_mus` / `g_mus_pos`. With SDL2_mixer it uses `Mix_PlayMusic`.
- [F] There is no seek function.
- [D] Since the whole tune is in memory, seek or a section with its own loop
  is a small change: move `g_mpos` / `g_mus_pos` and the loop bounds.
- [D] Precision would be one buffer: ~90 ms on Windows, the device buffer on
  SDL.
- [D] Every `plat_music_play` cuts dead and decodes again: no crossfade.

## B. Relevant code

| What | Where |
|---|---|
| Event list (8 bytes: type, player, subtype, param) | `MKEvent_Add`, `src/decomp/gamecode/logic/mk3.c`; queue `MKEventQueue` 0x0038cf80, at most 10 per frame |
| Event consumer | `AddNewGameEvents`, `src/decomp/gamecode/Blood.c` (armv7 0x000732a8) |
| FINISH HIM / FINISH HER | event 4/0x11 or 4/0x12 from `finish_him_or_her` (`logic/other.c`), received in `Blood.c`: sets `FinishHimHer = 1/2`, `IsInFinishing = 1`, `RoundParam[14] = 1`, `RoundParam[13] = 0` |
| Finisher starts: music cut | `t_make_db_tone` (`logic/mkfatal.c`) sends event 2/4 param 0x36 → `PlayFatalityVoice` (`Blood.c`): `limeStopTune()` then `Fatal1/2.mp3`. Raised by the common finisher, the Pit fatality (`mkfatal.c`) and `t_fx_mercy` (`other.c`) |
| Fatality completed | event 4/22: `FatalityMessage`; music unchanged |
| Animality / Mercy / Friendship / Babality | events 4/28, 29, 42, 43: `PlayFinisherTune(...)` (`Blood.c`) |
| Finish time runs out | `t_wait_fatality_finish` (`other.c`) exits to `t_finish_him_exit` with no event |
| "X WINS" | `_do_winner_char` (`logic/mkbonus.c`) sends 3/3, which writes `WinnerMessage` (`Blood.c`) |
| Round end | `t_play3`, token 0x12d0 (`other.c`), sends 3/4 (`RoundHasEnded = 1`) |
| Round start | 2/0 param 0x11..0x14 (`RoundMessage`, "ROUND N") and 4/11 (`FightMessage`, "FIGHT") |
| End of the Mercy banner | restarts the stage tune from the top (`GameCode.c`) |
| Music switched back on from options | `GameCode.c` |

## C. Audio files

[F] The `.ipa` has 28 MP3s, all 44.1 kHz.

**Stage tunes** (stereo):
- 320 kbps CBR: Pit 1:54, Church 1:51, Grave 1:57, Train 1:56, Old 2:21,
  Frenzy 1:19, Coolt 1:46, Tongue 2:06.77, Wacker 1:59, Slow 2:16.
- **Shipped but unused** (VBR, not named by the binary): SoulChamber 2:15.21,
  Bank, Bridge, GraveYard, Roof, Street, Subway, NoobDorfen, Select.

**Stingers** (mono, 128 kbps): Fatal1 3.34 s, Fatal2 3.11 s, Animality
3.37 s, Babality 3.29 s, Friendship 2.90 s, Mercy 2.19 s.

**Not there:**
- [F] No separate "critical" or "ending" files per stage; each tune is one
  continuous track.
- [?] Whether `Tongue.mp3` or `SoulChamber.mp3` have recognisable transition
  points: needs listening or analysis (no ffmpeg on the machine then).

## D. Real event flow (iOS)

1. `Task_GameInit` starts the stage tune, looping.
2. Rounds 1 and 2: the tune plays on. Each round sends "ROUND N" (2/0) and
   "FIGHT" (4/11); its end sends 3/4.
3. The deciding round, with time left (`t_play3` requires it), sends FINISH
   HIM/HER (4/0x11 or 0x12); the tune plays on.
4. Four ways out:
   - **Fatality:** `t_make_db_tone` (2/4, 0x36) cuts the tune, `Fatal1` or
     `Fatal2` plays, then event 4/22.
   - **Animality, Friendship, Babality:** their own tune.
   - **Mercy:** `Mercy.mp3`; after the banner the stage tune restarts from
     the top.
   - **Failed or timed out:** no event; the tune plays on.
5. Winner announced (3/3, "X WINS").
6. `Task_GameDestroy` stops the tune.
7. The next fight starts the tune from the top.

## E. Feasibility

**Feasible with limited changes.** Every moment needed already has an event:
FINISH HIM, finisher start, winner, round end and start. A failed finish is
detected without a new event: 3/3 arrives with no 4/22 before it.

## F. Recommended design

**A + D:** one controller that listens to the events and seeks inside the
continuous track.

- **A state machine per fight:** NORMAL → CRITICAL → ENDING → STOPPED; back
  to NORMAL at the next round or fight.
- **Fed from one line** in `AddNewGameEvents`, marked as port code, on
  events 4/0x11, 4/0x12, 3/3, 4/11 and 2/4 with 0x36.
- **No duplicates:** transitions only move forward. The latch is the same
  `FinishHimHer` flag, which only `ResetFightData` clears.
- **A data table per stage** (`data.ini` or a port `.ini`) with the sections
  in samples. A stage with no entry behaves exactly as now. At first only
  Soul Chamber has one.
- **One new platform function**, `plat_music_seek` / `set_loop(start, end)`,
  in `win32_audio.c` and `sdl_audio.c`, moving `g_mpos` / `g_mus_pos` over
  the PCM already in memory.
- **The clash with `PlayFatalityVoice` is a design decision (Mary's):**
  either the fatality stinger replaces the ENDING section (as on iOS), or it
  is skipped on adaptive stages.

Rejected:
- **B, extend `limePlayTune`:** mixes port logic into functions transcribed
  from the binary.
- **C, separate segments:** cutting audio files per mod, and each switch
  decodes again (pause and audible jump).
- **D alone, no controller:** does not solve duplicates.

Both platforms share the logic; only the backend differs.

## G. Minimal changes (all outside fight logic)

1. `platform.h`, `win32_audio.c`, `sdl_audio.c` including the SDL2_mixer
   branch (`Mix_SetMusicPosition`): seek and per-section loop.
2. `lime_menu.c`: a wrapper that respects `rb_quiet()`.
3. A new `src/runtime/adaptive_music.c` with the state machine and the
   per-stage table.
4. `Blood.c`, `AddNewGameEvents`: one call marked as port code. No game
   decision changes.
5. `Task_GameDestroy`: reset the state.

`src/decomp/gamecode/logic/` is not touched.

## H. Tests

**Soul Chamber first.** Mary plays; the log must show every transition, one
`music:` line per state change. Cases: fatality done; finish failed; time
out before FINISH HIM (not sent); draw (3/3 with −1); Mercy; Animality; Esc
to the menu mid-finish; rematch; online (rollback).

**Every other stage** must log exactly as now: no adaptive `music:` lines,
the same `limePlayTune` / `limeStopTune` sequence.

## I. Open questions

1. **[?] What the Genesis really does.**
   - [F] The Genesis ROM is by Avalanche Software / Williams. It has none of
     the known sound-driver signatures searched for (GEMS, SMPS, Echo).
   - [F] It has a SOUND TEST ("A-START SOUND / B-RESUME MUSIC").
   - [?] Whether it uses separate tracks or one sequence with jumps cannot
     be told statically.
   - [F] Gens 2.14 has a **WAV dump** and a **GYM log** (its HISTORY.txt).
     The GYM keeps the YM2612 writes frame by frame, which shows whether the
     sequence jumps.
   - **Needed:** Mary records (WAV + GYM) a Soul Chamber fight through
     FINISH HIM, a fatality done (and a second one failed), "WINS" and the
     next round, into `E:\MK3 PROJECT\sega genesis test\` (git-ignored, not
     in the repo), noting roughly when FINISH HIM, the fatality and "WINS"
     happened.
2. **[?] Whether the Genesis tune matches `Tongue.mp3` or
   `SoulChamber.mp3`**, and where the sections are in each file.
3. [D] Level 10 is Soul Chamber per the code; confirm in the game.
4. [?] What to do with the `Fatal1/2` stinger (section F): Mary decides.
