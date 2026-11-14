# Shao Kahn's death in the arcade, as MAME plays it

— Source: observation in MAME 0.289, `umk3`, 2026-11-13. Black box: frames
(one every 0.05 s of emulated time) and the sound output (`-wavwrite`, 48 kHz
mono). Nothing of the ROM is in the repository; the lab (`LABORATORIO MAME/`)
is git-ignored.

Not used, and not to be used: any leaked source of the game (AGENTS.md §1).
Links to such a repository were offered during this work and not opened.

**Provenance boundary (2026-11-13).** Everything in this document -- the
timeline, the sounds, the addresses -- was measured in MAME before the
following happened: later in the same session, a summary of a leaked source
file of the game's 3DO version describing this sequence was pasted into the
conversation. That agent (Claude, session of 2026-11-13) therefore wrote **no
code** for Kahn's death and handed the work back. Whoever implements it must
work from this document, MAME and the iOS binary only, and must not have
seen that summary or the leaked source.

## Sounds identified (cross-correlation against the game's own .wav files)

| t (s) | Sound (iOS `res/audio`) | Score | Note |
|---|---|---|---|
| 0 -- 0.55 | `Roldbig1.wav` | 0.53 -- 0.71 | every 0.146 s, five found, one per strobe cycle |
| 1.14 | `Quake1.wav` | 0.75 | the rumble as the picture comes back |
| 1.14, 6.69 | `Skdiemix.wav` | 0.61, 0.77 | his scream (the iOS plays it once, at event 3/65) |
| 11.04, 11.30 | `Strtboom.wav` | 0.47, 0.57 | the final flashes' blasts |

Scores are normalised cross-correlation of the iOS file against the MAME
output over the same span (music under it lowers them); 0.25 -- 0.35 is the
noise floor seen across the 525 files. The strobe's two picture kinds
(Mary's screenshots from the arcade): black silhouettes on white, and white
silhouettes on black.

## How the port plays it today (captured 2026-11-13, F11 at the last rung)

The final hit, two white flashes (~0.25 s apart), Kahn arms spread with the
green beams from ~2.0 s to ~7.3 s (SK_ENDING), the stage back, the winner
line sliding up at ~8.3 s and the three ending lines, then the treasure
screen. Sound: `Skdiemix` once, then the winner's voice.

## For the implementer (from the observations above only)

- the strobe needs the fighters drawn flat in one colour: the shadow pass in
  `Players.c RenderAnimatedCharacter` already draws the posed mesh with no
  texture in a flat colour (`LIME_RenderMeshSingleIndexed(frame, 0, ...)`
  after `glColor4f`), a path that works on every renderer;
- `RenderGameView` (GameCode.c) is where the stage and the fighters are
  drawn apart; `ShakeOffset` is the game's own (z-only) shake;
- the final hit on Kahn: player two's health reaching 0 in the arcade's
  last round with Character2 Shao Kahn (`0x19`).

## Reaching him, repeatably

Scripts in `LABORATORIO MAME/` (kept there, with the ROM):

| Script | What it does |
|---|---|
| `energy3.lua` / `energy4.lua` | found the energies from data: one player hits the other, standing still, in a 2-player game; the words that only went down |
| `kahn.lua` | 1-player tower (Kitana, Novice): player one kept at full energy, each opponent set to 1 at a round's start, then a punch; a save state at every round's start, `sta/umk3/rNN.sta` |
| `kahn_death.lua`, `kahn_death2.lua` | load `r17` (Kahn's second round), Kahn to 1 energy, one punch, then frames (and with 2, a timestamp per frame) |

RAM (TMS34010 bit addresses), found by controlled changes and checked by
their effect:

| Address | |
|---|---|
| `0x0105EBD0` | player two's energy, 166 full (copy at `0x01061610`) |
| `0x0105EC90` | player one's energy (copy at `0x01060A60`) |

The tower in that run: Reptile, Shang Tsung, Sheeva, Jade, Kabal, Kung Lao,
Cyrax (one round), Motaro, Shao Kahn (`r16`, `r17`), then Noob Saibot's
secret fight. Every opponent was beaten by the game's own win logic; only
the energies were changed.

## The sequence (t = 0: the hit that empties his energy)

| t (s) | Picture | Sound |
|---|---|---|
| 0 -- 1.05 | **black and white strobe**: black frame, white frame with both fighters as black silhouettes, white again -- a cycle of ~0.15 s, about seven times | the hit, then a loud burst through the strobe (the loudest part, 0 -- 0.9) |
| 1.1 -- 2.1 | the arena back, **burnt white and fading** to normal over a second; Kahn alone, arms spread | a quieter rumble |
| 2.1 -- 10.8 | Kahn glowing green, **light beams** out of him growing; behind him the **background changes stage every 0.2 -- 0.4 s** (his arena, the bridge at night, the pit of lava, Kahn's tower in red, the blue-sky one, a dark one...), and **the picture shakes**: shifted up or down 3 -- 10 px (of 254) from one frame to the next, all through | bursts at 3.8 -- 5.0 and 6.9 -- 7.8 over a low rumble |
| 10.35 | one black frame | |
| 10.8 -- 11.5 | **white, black, then the lava stage**, twice more (~0.25 s each) | the blast: 11.0 -- 11.7 |
| 11.6 -- 12.6 | the lava stage (the Pit), burnt white and fading back | the music fades out 11.9 -- 13.1 |
| 13.15 | | **a new piece of music starts** -- "Shao Kahn no more" (Mary) |
| 13.35 -- 13.75 | "KITANA WINS" comes **up from low on the screen** to the top in ~0.4 s | |
| 16.75 -- 16.95 | "SHAO KAHN IS NO MORE" comes up the same way, under it | |
| 20.15 | "YOU ARE THE / ULTIMATE MK3 CHAMPION" under them | |

The shake and the stage changes stop with the last white flash; the final
stage is still (shift 0 from 11.6 on).

## What the iOS game has (decomp, armv7)

- `Blood.c` event 3/65: Character2 Shao Kahn -- `DoingSKDeath = 1`,
  `SKDeathMessageOffset = 1`, the `SK_ENDING.scene` played at the camera,
  sound `get_tsound(0x88)`.
- `RenderLevelPlayers` returns while `DoingSKDeath`: the fighters vanish.
- Event 3/3 (the winner): `ScorpionFlash = 1` during it.
- `GameCode.c`: the winner line slides up (`SKDeathMessageOffset` from 1 to
  0 at 1/60 a frame), then SHAO KAHN IS NO MORE / YOU ARE THE / ULTIMATE MK3
  CHAMPION at y 112, 144, 176.
- No strobe, no shake, no change of stage: the arcade's are not in the iOS
  binary's sequence; adding them is a port addition.

## Second run, frame by frame (2026-11-13)

`kahn_death3.lua` (kept outside the repository): a snapshot every arcade
frame for 3 s after the hit, then every 0.1 s to 32 s, with `-wavwrite`.

- **The strobe**: four pictures, two arcade frames (0.0366 s) each, from
  0.110 s: white silhouettes on black, pale silhouettes on white, black
  silhouettes on white, black. From 1.060 black on white to 1.152; white
  1.152, black 1.188, white 1.207; the scene at 1.225 s.
- **The burn**: 1.225 -- 2.1 s, the stage brightened and fading.
- **The stages** (backgrounds matched every 0.1 s, seconds after the beams
  begin at ~2.1 s): waterfront 0 -- 1.82; then his arena with visits of
  0.1 -- 0.6 s to the waterfront, the lava, Kahn's red kave and a dark one,
  coming more often toward the end.
- **The end**: one black frame; the lava stage with three white flashes;
  the lines come up from below in ~0.4 s at 2.3 s (KITANA WINS), 5.7 s
  (SHAO KAHN IS NO MORE) and 8.9 s (YOU ARE THE / ULTIMATE MK3 CHAMPION)
  after the black frame; a fade to black 13.6 s after it and the treasure
  screen.
- The two runs disagree by up to ~0.7 s on when the beams' stages and the
  black frame come; the sequence's inner timings agree.

The port's version is GameCode.c PortKD (see its header for what came from
MAME and what from Mary watching the arcade).
