# `res/framelists/` — the animation stream, named frame by frame

A directory nobody in this project had opened. It holds **28 per-character frame
name lists**, in plain text and in a fixed-size binary form, and it is the thing
[issue #13](../../issues/13) was hunting: **the segmentation of the animation
stream.**

```
res/framelists/
    allframes.txt          75 KB, the global list
    kanoframes.txt         361 lines
    noobsaibotframes.txt   338 lines
    ...  28 characters
    KANOFRAMES.bin         14,490 bytes -- one per character, all the same size,
    ...                    all different content: a fixed-length record table
    subzeroframes.txt.tmp  a build leftover that shipped
```

---

## The line number *is* the frame index

One-based line number, zero-based frame index — so **frame `N` is line `N+1`**.

The proof is the `.events` frame indices this project established separately.
Kano's events say his babality fires at frame 311, his ice-shatter death at 324,
his explosives vest at 336, and his cut-up by Reptile at 340:

| Event slot | Frame | Line | Frame name |
|---|---:|---:|---|
| `KANO_BABALITY` | 311 | 312 | `BABKANO//doFatal` |
| `ICETHUD_DEATH_ANIMATION` | 324 | 325 | `ICETHUD2//doFatal` |
| `KANO_EXPLOSIVES VEST` | 336 | 337 | `EXPLO1//doFatal` |
| `KANO CUTUP BY REPTILE` | 340 | 341 | `CUTUP_BY_REPTILE_KANO//doFatal` |

The last row is a **literal string match** between the event track name and the
frame name. And the pattern holds across characters: the event `nailscared_X`
lands on the frame named `NAILSCARED` for **twelve different characters at
twelve different indices** — 485 for Cyrax, 351 for Ermac, 429 for Jade, 382 for
Jax, 340 for Kano, 350 for Kitana. Twelve independent hits on the same name is
not coincidence.

Measured over all 307 catalogued events: 13% match the frame name exactly or by
containment, 51% within fuzzy tolerance. The remainder are event tracks named
for the **effect they spawn** (`smokeparticle`, `bloodSplatUp`) rather than for
the frame, which is expected and not a failure.

## `//doFatal` marks the finisher block

Frames carry inline annotations. From `kanoframes.txt`:

```
305  KNSABER18
306  KNMOUTH3_KN//doFatal
...
312  BABKANO//doFatal
313  SPINNER2
314  KNBROKEN1
315  x
316  event_SONYA_KOD_DEATH//doFatal
```

So the stream is not merely ordered with finishers at the end — **it is
labelled**. That is the last piece a playback implementation needs: which frames
belong to a clip, and what that clip is.

Note line 315 is literally `x`, and Noob's list ends with `x2 x3 x4 x5` and
`xxx`. Padding, to keep indices aligned.

---

## Noob Saibot is not a trace — he is a complete character

Asked to look for rumoured leftovers, we found the opposite of leftovers.

| | |
|---|---|
| Asset weight | **2,466,395 bytes** — within 1 KB of Scorpion's 2,465,648 |
| Animation | 347 frames over 48 bones, his own `.skinanim` |
| Full set | `.bones` `.skin` `.skinanim` `.scene` `.events` `.meshset` |
| Own stage | `NOOBS_DORFEN_LEVEL` + `NOOBSDORFEN_LEVEL_SCENE` |
| Own finishers | `BABALITY_NOOBSAIBOT`, `NOOBSAIBOT_DECAP`, `NOOBSAIBOT_FLOAT` |
| Deaths at others' hands | by Cyrax, Kung Lao, Reptile, Jax, Mileena, Sheeva |
| Stage-specific deaths | `LAIR_NOOBSAIBOT`, `LAIR_NOOBSAIBOT2`, `SUBWAY_NOOBSAIBOT`, `SUBWAY_NOOBSAIBOT2` |
| UI art | `IPADNOOBSAIBOTPORTRAIT.PNG`, `NOOBSAIBOT_VERSUS.PNG`, `_VERSUS2`, `_LOW` |

The binary carries him too: `NOOB SAIBOT` in the name table beside Motaro and
Shao Kahn, the victory line `NOOB SAIBOT WINS`, and the functions
`_NoobSaibotIntroFrames`, `_Player_NOOB_Idles` and `_t_noob_slam`. At `0x162ea0`
sit `delete_slave` and `noob_p` — his shadow clone.

### Why he is absent from `frames.x` and `moves_data.x`

Searching those two files for "noob" or "saibot" returns **zero hits**, which is
what makes him look cut. He is not: he simply has **his own frame list**,
`noobsaibotframes.txt`, and its contents explain the rest —

```
SCBACKBREAK1
SCBLOCK1
SCBZZ1
SCCOMBO1
```

**`SC` is Scorpion's prefix.** Noob Saibot is built on the male ninja rig and
reuses Scorpion's frame naming, exactly as the 1995 arcade original did with its
palette-swap ninjas. `HALF_NJ` and `NJMOUTH7_KN` are shared ninja frames. His
347-frame stream matches Liu Kang's and Old Smoke's at 347 over 48 bones — one
skeleton, several characters.

So the forum rumour has it backwards. There is nothing vestigial about him; the
naming convention just hides him from a text search.

---

## The `.bin` form

Each `*FRAMES.bin` is exactly **14,490 bytes**, for every character, and no two
are identical. A fixed-length record table rather than a serialised list —
almost certainly the runtime form the text file is compiled into. Its layout is
not yet decoded and is the obvious next thing to read here.

## A build leftover that shipped

`subzeroframes.txt.tmp` sits in the directory in the retail app. The binary
never references a `.tmp` — there are zero occurrences of the string — so it is
dead weight, not a dependency. Filed with the other
[shipped-game defects](GAME-BUGS.md).

---

## Playing it back: `tools/animate.py`

The framelists turn the stream into named clips, and clips are what an animation
player needs. Consecutive frames whose names share a stem and count up are one
clip — `KNHIPUNCH1` … `KNHIPUNCH7` — while padding entries (`x`, `x2`, `xxx`)
are skipped.

```bash
UMK3_RES=/path/to/res python tools/animate.py KANO --list
UMK3_RES=/path/to/res python tools/animate.py KANO out.gif --clip KNHIPUNCH
```

Kano's 362 frames resolve into **54 named clips**: `KNDUCK`, `KNDUCKHIKICK`,
`KNFLIP`, `KNGETUP`, `KNHIKICK`, `KNHIPUNCH`, `KNJUMPFLIP`, `KNKNOCKDOWN`,
`KNLASER` — the character's complete move breakdown, read out of the data.

![Kano's high punch, frames 58-64](img/anim-punch-strip.png)

<sub>`KNHIPUNCH1`…`KNHIPUNCH7`, rendered frame by frame: guard, wind-up, full
extension, retraction, recovery, guard. ([animated](img/anim-kano-punch.gif))</sub>

**This is the first moving output in the project's life**, and it is the proof
the segmentation is real rather than inferred. A wrong clip boundary produces a
sequence that jumps; this one reads as a punch.
