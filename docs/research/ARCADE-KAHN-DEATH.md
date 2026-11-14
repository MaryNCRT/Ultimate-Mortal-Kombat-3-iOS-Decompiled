# Shao Kahn's death in the arcade, as MAME plays it

— Source: observation in MAME 0.289, `umk3`, 2026-11-13. Black box: frames
(one every 0.05 s of emulated time) and the sound output (`-wavwrite`, 48 kHz
mono). Nothing of the ROM is in the repository; the lab (`LABORATORIO MAME/`)
is git-ignored.

Not used, and not to be used: any leaked source of the game (AGENTS.md §1).
Links to such a repository were offered during this work and not opened.

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
