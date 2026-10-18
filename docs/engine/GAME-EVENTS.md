# The engine's event queue

**This is the seam.** The MK3 fight engine does not draw, play sound, or know
what a fatality is. It appends eight-byte records to `MKEventQueue`, and once a
frame `AddNewGameEvents` (armv7 `0x000732a8`, 6,644 bytes) drains that queue and
turns each record into blood, an FBX effect, a sound, a banner, an achievement,
a stat, or a change of round state.

Everything below the seam is arcade logic ported from the original game.
Everything above it is the iOS presentation layer. A port that wants to change
how the game *looks* changes `AddNewGameEvents`; a port that wants to change how
it *plays* does not touch it.

`AddNewGameEvents` is decompiled, in
[Blood.c](../decomp/gamecode/Blood.c). Landing it took that file to **8/8 — the
first complete file in `gamecode`**.

There are two unrelated event queues in `Blood.cpp`. `GameEvents` (stride 0x64)
is the file's own particle and screen-shake list, run by `RunGameEvents` and
already decompiled. `MKEventQueue` is the engine's, and is what this page is
about.

---

## The record

```
+0  int8   type       0..4, the class
+1  int8   player     which side the event is about
+2  int8   subtype    the event proper
+3  int8   (unused)
+4  int32  param
```

`MKEventQueue[0]` is the count and the records start at +4. The loop walks
exactly `count` of them and never looks at a used-flag, so the engine hands over
a fresh, packed array each frame.

## The five classes

| type | class | subtypes | live |
|---:|---|---:|---:|
| 0 | camera | 1 | 1 |
| 1 | position | 4 | 4 |
| 2 | sound | 5 | 5 |
| 3 | state | 14 | 10 |
| 4 | effects | **69** | 34 |

Type 4's table is an inline `.word` array the compiler jumps into with
`mov pc, r0`, not a `tbb`, so the dead subtypes are visible as explicit jumps
back to the loop tail. **Thirty-nine of the eighty-eight arms across all the
tables do nothing.**

```bash
python tools/disasm_range.py work/UMK3.armv7 0x732a8 0x74c9c > ev.txt
python tools/lits.py ev.txt > evl.txt
python tools/handlers.py evl.txt --table 0x733c0:69:1 --table 0x734ec:14:0 \
                                 --table 0x7353c:5:0 --tail 0x73388
```

---

## The finishers, named by the binary

Five subtypes carry the finisher set, and four of them log an EA analytics event
whose last argument is a plain ASCII literal — so these names are **read, not
inferred**:

| subtype | finisher | logged as | music |
|---:|---|---|---|
| 22 | **Fatality** | `"Fatality"` | — |
| 28 | **Animality** | `"Animality"` | `Animality.mp3` |
| 29 | **Mercy** | not logged | `Mercy.mp3` |
| 42 | **Friendship** | `"Friendship"` | `Friendship.mp3` |
| 43 | **Babality** | `"Babality"` | `Babality.mp3` |

Every logging call is
`EASDK_LogEventEnumEnumString(0x3f7, 15, DestinyNames[Destiny], 15, <name>)` —
the difficulty and the finisher. Mercy changes the music but is never logged.

The four logged ones share a shape that carries three facts:

- the log fires only when the **loser is actually at zero health**;
- in `GameMode == 0` it also unlocks an achievement and bumps a stat, which is
  why those two only ever count arcade play;
- in `GameMode == 1` the stat is bumped for whichever side the local machine is,
  decided by `isParent()`.

**Classic Sub-Zero (index 21) suppresses the friendship** on either side:
subtype 42 returns early if the performer is `0x15`. And `0x19` (Shao Kahn)
gates the whole Shao-Kahn death sequence in subtype 65. Both indices match
[ROSTER.md](ROSTER.md).

---

## Stats and achievements this function owns

| counter | what |
|---|---|
| `Stats[5]` | fatalities |
| `Stats[7]` | friendships |
| `Stats[8]` | animalities |

| achievement | earned by |
|---:|---|
| 4 | the blast, outside modes 1 and 6 |
| 5 | a mercy followed by a finisher |
| 9 | a babality |
| 0xa | an animality |
| 0xb | a friendship |
| 0xc | a fatality |

---

## What else the map named

- **`groundoffsets` (0x001725bc)** is a per-character ground offset table.
  Subtype 54 computes `blast_player_height = 0xf7 - groundoffsets[model]`, a
  fixed 247 minus the character's offset, reading player 1's entry or player 2's
  depending on which side blasted. First use of that table found.
- **The scene globals** all resolve through pointer slots and are named:
  `BloodScene`, `PitDeathScene`, `SZEffectScene`, `SwatEffectScene`,
  `CyraxSelfDestructScene`, `SKEffectScene`, `XeroxScene`, `RocksScene`,
  `TrainScene`, `TrainDie1Scene`, `TrainDie2Scene`, `SLDie1Scene`,
  `SLDie2Scene`. The stage-death scenes pair with
  [STAGES.md](STAGES.md)'s inventory.
- **`ScorpionFade`, `ScorpionFadeAdd`, `ScorpionFlash` and `lightsOn` are
  floats**, written here as `1.0f` and `1.0f/30.0f`. That is an independent
  confirmation of the type correction made when `DrawHUD` landed — two
  functions, neither of which had been read when the wrong `long` was written
  down.
- **Health arrives as a bar length, not a percentage.** Type 3 subtype 0
  computes `Health[p] = 100 * (param + 1) / 166`, the exact inverse of the
  `* 166 / 100` that `GameCode.c` uses to draw the bar. The run meter is
  `100 * param / 48`.

---

## Two shared tails

- **the sound tail** (`0x0007417e`) —
  `limePlaySound(id, MusicVol[Settings[3]] / 100.0f, 1.0f, 0)`, and every arm
  that reaches it is gated on `Settings[3]` before the lookup even happens.
- **the finisher tail** — stop the tune, start the finisher's own.

The effect spawn *looks* like a third but is not; see below.

---

## What the finished read added

- **Type 0 and the shake name three `GAMEEVENT` fields.** Type 0 stores the
  fighter's object at **+8** and the 3D position at **+0xc**; the shake takes
  event id 13 and writes **+0x24**, **+0x28** and **+0x2c** — magnitude, count,
  tick. `RunGameEvents` reads exactly those, from the other end of the same
  struct. Two functions decompiled independently, agreeing field for field.
- **The stage death picks its scene from the stage.** Subtype 58 branches on
  `LevelSelect`: 4 (Subway) plays `TrainScene` then `TrainDie1Scene` or
  `TrainDie2Scene`; 11 (Scorpion's Lair) plays `SLDie1Scene` or `SLDie2Scene`.
  Nothing else. Those are exactly the two stages [ROSTER.md](ROSTER.md) found
  per-character deaths for, reached from a different direction.
- **FINISH HIM and FINISH HER are different voice lines** — `get_tsound(0x15)`
  and `get_tsound(0x16)`. They also set `RoundParam[14] = 1` and
  `RoundParam[13] = 0`.
- **The combo counter starts at 192 and 256 and holds for 300**, not the 128 a
  reading of `DrawHUD` alone would suggest. `DrawHUD` draws it at
  `(128 - slider)`, so the counter starts a full screen-width off and slides in.
- **The blood spray is six calls at three heights**, `z + 1.5`, `z + 1.0`,
  `z + 0.5`, each twice.
- **The effect matrices are not uniform.** Every arm starts the same — identity,
  `RotMatrixX(m, pi/2)`, `limeScaleMatrix(m, 1 / WorldScaleAdjust)` — and then
  they differ in what goes at `m[0x30]` and `m[0x38]`, in whether a second scale
  follows (subtype 3 scales again by 0.25), and in the 2.2 and 2.8 offsets some
  add and others do not. Folding them into one helper is where a reading of this
  function goes wrong; `Blood.c` writes them out per arm.

## Two things the shipped code does that look wrong

- Subtype 13 **stores `ev->player` back over itself**, predicated on the combo
  damage exceeding 100. A genuine no-op in the shipped binary.
- Type 1 subtype 1 calls `SetCameraOverridePosFrom2d` with **two stack words the
  function never writes**. Safe only because the callee ignores both — which
  `SetCameraOverridePosFrom2d` had already been decompiled as doing, before this
  call site was read.
