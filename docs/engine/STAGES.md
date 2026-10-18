# Stages, and how finishers are organised

Seventeen stages ship, each as a `.scene` naming meshes, a `.meshset` holding
them, and an `.events` file driving whatever moves. All three formats are
solved, so this reads out of the data rather than out of the disassembly.

---

## Animated stage elements

An `.events` track on a stage scene is a **looping animated element**. The names
are self-describing:

| Stage | Tracks | Entries | Animated by |
|---|---:|---:|---|
| Scorpion's Lair | 29 | **142** | `lavapulse` |
| The Street | 20 | 20 | `newspaper1` — blowing newspapers |
| The Balcony | 16 | 16 | `torchfire2`, `fire1` |
| Noob's Dorfen | 12 | 12 | `torchfire2` |
| Waterfront | 8 | 8 | `WaterLoop`, `glowlight_red1`, `glowlight_red2` |
| The Graveyard | 7 | 7 | `gymist1` — graveyard mist |
| The Pit | 7 | 7 | **`Pit_Blades`** |
| Rooftop | 7 | 7 | `rooftop_clouds`, `glowlight_red1` |
| Cave | 4 | 4 | `lensflare` |
| Soul Chamber | 3 | 3 | `soul_mist`, `soul_fx` |
| Scislac Busorez | 2 | 2 | `vortex`, `lightning` |
| Jade's Desert | 1 | 1 | `desert_cyrax` |

**Scorpion's Lair is the most animated stage in the game** by a wide margin — 29
tracks and 142 entries of pulsing lava, against 20 for the next.

Four stages carry **no events at all**: Bell Tower, Bridge, Subway and Temple.
Their scenery is static.

## Stage fatalities are per-victim scene pairs

The Subway having no animated elements is not the same as having no stage
fatality. Those live elsewhere, as **`<STAGE>_<CHARACTER>` scenes**:

```
LAIR_KANO.scene      LAIR_KANO2.scene
SUBWAY_KANO.scene    SUBWAY_KANO2.scene
```

**24 victims × 2 stages × 2 phases = 96 scenes.** The `2` variant is the second
phase — the fall and then the landing.

So the game ships **two stage fatalities**, the Lair and the Subway, each
authored separately for every character who can be on the receiving end.

**The loader agrees, and names the two stages.** `GameInit_LoadABit`
(decomp/gamecode/GameCode.c) carries four 24-way switches that pick exactly
these scenes:

```
LevelSelect == 4    SUBWAY_<CHARACTER>.scene   -> TrainDie1Scene / TrainDie2Scene
LevelSelect == 11   LAIR_<CHARACTER>.scene     -> SLDie1Scene / SLDie2Scene
```

one switch per player slot per stage. Only the two loaded scenes are the
*first* phase; the `2` variants are loaded elsewhere. In endurance the second
slot is indexed by `LastEnduranceCharacter` rather than `Character2`, so the
death matches whoever is on screen.

Those switches are also where the [character index table](ROSTER.md) came
from -- all four order the roster identically, and they stop at 23 because
Motaro and Shao Kahn are not selectable.

## The finisher matrix

The same `<PREFIX>_<CHARACTER>` convention covers every finisher, and counting
the scenes gives the complete matrix without touching code:

| Finisher | Victims | Notes |
|---|---:|---|
| `BABALITY` | 24 | one per character |
| `CUTUP_BY_CYRAX` | 24 | |
| `CUTUP_BY_LAO` | 24 | |
| `CUTUP_BY_REPTILE` | 24 | |
| `LAIR` | 24 | **stage fatality** |
| `SUBWAY` | 24 | **stage fatality** |
| `CUTUP` | 23 | |
| `JAX_DICE` | 23 | |
| `MILEENA_SUK` | 23 | |
| `SHEEVA_SKINRIP` | 23 | |
| `NAILSCARED` | 22 | |
| `STRYKER_TAZER` | 11 | half the roster only |
| `ANIMALITY` | 3 | Kabal, Sheeva, Stryker |

`STRYKER_TAZER` covering only 11 of 24 and `ANIMALITY` only 3 are the two
asymmetries worth investigating; everything else is near-complete.

## Animalities work two different ways

The three `ANIMALITY_*` scenes are not the whole story. Most animalities are
**morph sequences in the character's own `.meshset`** — the transformation is
done by swapping whole meshes, because a skeleton cannot turn a man into an
animal:

| Character | Sequence | Frames | Becomes |
|---|---|---:|---|
| Smoke | `SmokeBull` | 14 | a bull |
| Jade | `JadeCat` | 11 | a cat |
| Cyrax | `SharkCyrax` | 10 | a shark |
| Nightwolf | `Raiden` | 11 | — |

Dedicated scenes exist only where the transformation needs more than a mesh
swap: `ANIMALITY_KABAL`, `ANIMALITY_SHEEVA`, `ANIMALITY_STRYKER`, plus
`ANIMALITY_HAWK` and `ANIMALITY_HAWK_FADEIN` — Kabal's bird, with its own fade.

See [SKIN-FORMAT.md §11](SKIN-FORMAT.md) for the morph-target mechanism.

## What this leaves open

- **What triggers a stage fatality.** The scenes exist, the events are timed
  and the loader is now decompiled -- but the input condition and the position
  check are in `gamecode/logic`, which is not.
- **Why `STRYKER_TAZER` covers 11 characters and `ANIMALITY` only 3.** Either
  the rest are handled by morph sequences alone, or they are incomplete. The
  scene count alone cannot tell them apart.
- The numbered stage variants — `STREET_LEVEL1`…`4A`/`4B`/`4C`/`4D`,
  `SUBWAY_LEVEL1`…`4`, `BALCONY_LEVEL1`…`4` — are not yet explained.
