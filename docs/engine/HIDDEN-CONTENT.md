# Hidden and possibly unreachable content

![The hidden tier: Noob Saibot, Human Smoke, Classic Sub-Zero, Motaro, Shao Kahn](img/hidden-tier.png)

<sub>Left to right: **Noob Saibot**, **Human Smoke**, **Classic Sub-Zero**,
**Motaro**, **Shao Kahn** — posed by [`tools/pose.py`](../tools/pose.py) from
their own `.bones`, `.skinanim` and `.skin`. Noob renders as a pure black
silhouette because that is his design, not because anything is missing.</sub>

---

## The roster table has a hidden tier

The binary carries the roster as a string table at `0x14ee34`:

```
KANO · SONYA · JAX · NIGHTWOLF · SUB-ZERO · STRYKER · SINDEL · SEKTOR · CYRAX ·
KUNG LAO · KABAL · SHEEVA · SHANG TSUNG · LIU KANG · SMOKE · KITANA · JADE ·
MILEENA · SCORPION · REPTILE · ERMAC · SUB-ZERO · SMOKE · NOOB SAIBOT ·
MOTARO · SHAO KAHN
```

Twenty-six entries, and **`SUB-ZERO` and `SMOKE` each appear twice**. The second
of each is Classic Sub-Zero and Human Smoke — the game ships them as
`OLDSUBZERO_STANDARD` and `OLDSMOKE_STANDARD`, and Human Smoke's versus art is
named `SMOKE_SECRET_VERSUS.PNG`.

So the last five entries form a distinct tier, and the game has UI for it:
**`HIDDENPORTRAIT.PNG`** ships alongside the normal character portraits.

## Noob Saibot is complete, not vestigial

Asked to check a forum rumour about leftover Noob Saibot assets, we found the
opposite of leftovers.

| | |
|---|---|
| Asset weight | **2,466,395 bytes** — within 1 KB of Scorpion's 2,465,648 |
| Animation | 347 frames over 48 bones |
| Full set | `.bones` `.skin` `.skinanim` `.scene` `.events` `.meshset` |
| Own stage | `NOOBS_DORFEN_LEVEL` + `NOOBSDORFEN_LEVEL_SCENE` |
| Own finishers | `BABALITY_NOOBSAIBOT`, `NOOBSAIBOT_DECAP`, `NOOBSAIBOT_FLOAT` |
| Deaths at others' hands | Cyrax, Kung Lao, Reptile, Jax, Mileena, Sheeva |
| Stage deaths | `LAIR_NOOBSAIBOT`, `SUBWAY_NOOBSAIBOT`, and `2` variants of each |
| UI art | iPad portrait, `NOOBSAIBOT_VERSUS`, `_VERSUS2`, `_LOW` |

The binary carries `NOOB SAIBOT WINS`, `_NoobSaibotIntroFrames`,
`_Player_NOOB_Idles`, `_t_noob_slam`, and at `0x162ea0` the pair `delete_slave`
and `noob_p` — his shadow clone.

### Why he looks cut, and is not

Searching `frames.x` and `moves_data.x` for "noob" or "saibot" returns **zero
hits**. That is the whole basis for thinking he was removed. He simply has his
own frame list, `res/framelists/noobsaibotframes.txt`, and its contents explain
everything:

```
SCBACKBREAK1 · SCBLOCK1 · SCBZZ1 · SCCOMBO1 · HALF_NJ · NJMOUTH7_KN
```

**`SC` is Scorpion's prefix and `NJ` is the shared ninja set.** Noob is built on
the male ninja rig and reuses Scorpion's frame naming, exactly as the 1995
arcade did with its palette-swap ninjas — his 347 frames over 48 bones match
Liu Kang's and Old Smoke's precisely. The naming convention hides him from a
text search; nothing is missing.

## The select-screen portraits give the tier away

![Character-select portraits: the hidden tier against the normal roster](img/hidden-portraits.png)

Every one of the **24 normal selectable characters has a 128x128 portrait**.
Exactly five files are **64x64**, and they are precisely the hidden tier plus
the two non-combatants:

```
DUMMYPORTRAIT · ENDURANCEPORTRAIT · NOOBSAIBOTPORTRAIT ·
OLDSMOKEPORTRAIT · OLDSUBZEROPORTRAIT
```

Half the resolution of everybody else, with no exceptions in either direction.
That is not a coincidence of art budgets; it is a category.

### Human Smoke never got a portrait at all

Noob Saibot and Classic Sub-Zero have **real, drawn portraits** — a masked
ninja face on green and a blue-masked one on purple. `OLDSMOKEPORTRAIT.PNG`
is the **red question mark**: the same placeholder art as `HIDDENPORTRAIT.PNG`,
downscaled to 64x64 and shipped as his actual portrait.

Measured against `HIDDENPORTRAIT` resampled to 64x64:

| File | Mean pixel difference |
|---|---:|
| `OLDSMOKEPORTRAIT.PNG` | **3.65** — the same image |
| `NOOBSAIBOTPORTRAIT.PNG` | 53.05 — a different, real portrait |
| `OLDSUBZEROPORTRAIT.PNG` | 48.99 — a different, real portrait |

A factor of fourteen. So the character-select art for Human Smoke is
**unfinished**, and the placeholder went out in the retail build.

This is the one place so far where the evidence supports the word *unfinished*
rather than merely *hidden*. Noob Saibot, by contrast, has everything.

## How they are reached — settled

`FrontEnd.cpp` is now fully decompiled, and the selection path is no longer a
gap. Every one of these characters is reachable in the shipping build.

**Smoke: hold on Human Smoke.** `drawCharacterSelection` special-cases cell 14:

```c
if (c == 14) {
    if (the touch is inside this cell) SmokeCounter += 1 / limeFPSScaleFactor;
    else                               SmokeCounter = 0;

    if (SmokeCounter > 180) {
        SmokeCounter = 0;
        c = 22;                          /* the cell BECOMES character 22 */
        *limeLastTouchScreenX = -1.0f;   /* and a release is fabricated */
    }
}
```

180 frame-rate-corrected units is about three seconds, and the counter is reset
by any frame the touch leaves the cell, so the hold has to be continuous. The
`limeLastTouchScreenX = -1` is the interesting part: it is exactly the condition
the tap test below is waiting for, so the hold finishes by selecting character 22
through the ordinary path, as though the cell had been tapped.

Its hit box is not the cell's. The hold runs from `FE_Y(32 + row * 63)` to
`FE_Y(row * 63 + 91)`; the tap box starts four units higher. The two agree on the
bottom and the right and differ on the top.

**Ermac, Mileena and Classic Sub-Zero: three ten-digit kodes.**
`FE_Task_Enter_Kode` holds all three as chains of ten compares:

| kode | sets |
|---|---|
| `1 2 3 4 4 4 4 3 2 1` | `ErmacUnlocked = 1`, `KodeSuccess = 2` |
| `2 2 2 6 4 2 2 2 6 4` | `MileenaUnlocked = 1`, `KodeSuccess = 1` |
| `8 1 8 3 5 8 1 8 3 5` | `ClassicSubZeroUnlocked = 1`, `KodeSuccess = 3` |

`KodeSuccess` then doubles as the message id — `GameText` 0x5f, 0x60 and 0x61 —
and the wheels freeze once it is set. The dispatch keys on `KodeSelector[0]`, so
one wrong digit early abandons the whole chain rather than falling through to
the next kode.

**One grid cell can stand for two fighters.** Every highlight test in
`drawCharacterSelection` matches against **both** `CS_Layout[row][col]` and a
second 4x7 table `CS_Layout2` at `0x001016a0`, which is how the alternates share
positions with the characters they replace.

So: the assets are complete, the roster table lists them, the hidden-portrait
asset is slot 27 (`HIDDENPORTRAIT.PNG`, drawn for any cell whose
`CharacterAvailable` entry is neither 1 nor 2), and the selection path is read.
Nothing here is unused.

## Rain: one string, and nothing else

Rain is the other famous UMK3 rumour -- he appears in the arcade attract mode
and was not playable until Trilogy. Checked the same way Noob Saibot was, the
answer is the exact opposite.

The binary carries **two roster tables**, and they disagree:

| Table | Entries |
|---|---:|
| Character names, at `0x14ee34` | **26** |
| Win messages, at `0x173a80` | **29** |

They are otherwise in identical order. The three that appear only in the win
messages are:

```
RAIN WINS          between SCORPION and REPTILE -- his arcade roster slot
TOBIAS WINS        John Tobias, one of Mortal Kombat's creators
OON WINS
```

Alongside them sit `JOHNNY CAGE LOSES BIG TIME !!` and
`JOHHNY CAGE TRANSFORMATION ACTIVATED` -- the typo is the binary's -- which are
arcade-era developer jokes for a character who is not in UMK3 at all.

So the win-message array was carried over from the arcade **verbatim**, jokes
included, while the actual roster was trimmed to 26.

### There is nothing else of him

Every asset category searched, excluding the Subway `TRAIN` and training-mode
matches that share the substring:

| Looked for | Found |
|---|---:|
| `*RAIN*.pvr` / `.PNG` | 0 |
| `*RAIN*.meshset` / `.skin` | 0 |
| `*RAIN*frames.txt` | 0 |
| `*RAIN*PORTRAIT*` | 0 |

**No model, no animation, no texture, no portrait, no frame list, no scene.**

### Why this is the useful comparison

| | Noob Saibot | Rain |
|---|---|---|
| Assets | 2.47 MB, complete | none |
| In the name table | yes | **no** |
| Win message | yes | yes |
| Select portrait | a real drawn face | none |
| Verdict | present, reachability unknown | **absent** |

Noob Saibot looked cut and is complete; Rain looks present and is a leftover
string. Either one alone would mislead. Checking both is what makes the method
worth anything.

## Genuinely unused: build leftovers

- **`res/framelists/subzeroframes.txt.tmp`** — a temp file that shipped. The
  binary contains zero occurrences of `.tmp`, so nothing loads it.
- **SINDEL's duplicate animation half** — ~265 KB never read; see
  [GAME-BUGS.md](GAME-BUGS.md).

---

## On restoring it later

This is a **post-playable** goal, deliberately. Restoration means changing
behaviour, and behaviour cannot be judged before there is a running port to
judge it against. It sits on the roadmap after the decompilation and a playable
build, not before.

One boundary to state in advance, because it will come up: reconstructing
arcade behaviour must come from **observing the arcade ROM** — which this
project has already done under MAME — and from **our own binary's symbol
table**, which names 4,342 functions. It must not come from leaked retail
source, or from third-party write-ups that are themselves readings of leaked
source. That rule is why this project's findings are reusable at all, and
restoration work does not get an exception from it. See
[METHODOLOGY.md](METHODOLOGY.md).
