# The move-input tables

The whole roster's move notation is **static data in `__DATA` with symbols on
it**. Nothing has to be captured, emulated or inferred: the in-game Moves Info
screen hands `DrawMoveListIcons` a pointer straight into a table, and the tables
are named.

```bash
python tools/moves.py work/UMK3.armv7            # all 48 tables
python tools/moves.py work/UMK3.armv7 Scorpion   # one character
python tools/moves.py work/UMK3.armv7 --json     # machine-readable
```

**48 tables, 673 rows.** Two per character — one for the five-button layout and
one for the six — plus a `Generic` pair for the moves everyone shares.

Twenty-three characters have tables, not twenty-four: **Noob Saibot (index 23)
has none.** The symbols run `Generic`, then `Kano` through `Humansmoke` in
address order, which is [the roster](ROSTER.md) 0 to 22 exactly, and then stop.
Whatever the Moves Info screen shows for Noob Saibot, it is not coming from a
table of his own.

---

## The layout

```
_Kano_Moves5    0x00103958
_Kano_Moves6    0x00103cd8
```

Each table is an array of **64-byte rows**: sixteen `int32`, terminated by `-1`.
`MovesList` indexes a row as `table + row * 64` and passes the row pointer as
`DrawMoveListIcons`'s `seq` argument, which walks it to the terminator.

Sixteen slots means a notation can be at most fifteen symbols long, and the
longest that ships is nine.

Which of the two tables is used comes from `Player1NumButtons`, which
`GameInit_LoadABit` sets from `Settings[4]` — so it follows the on-screen button
layout the player chose, not the character.

---

## `_MovesListTab` — the index

`MovesListTab` (`0x0010d918`) is **thirteen words per character**, indexed by
the character number (`MovesList` computes `character * 13`). It splits each
character's table into three sections:

| word | contents |
|---|---|
| 0 | rows in section 1 |
| 1 | `GameText` id base A for section 1 |
| 2 | `GameText` id base B for section 1 |
| 3, 4 | the section's Moves5 and Moves6 tables |
| 5 – 9 | the same five for section 2 |
| 10 | rows in section 3 |
| 11, 12 | its Moves5 and Moves6 |

Section 3 carries **no id bases**. All three sections point at the same pair of
tables; the counts are what slices them.

Row `k` of a section takes `GameText` id `base + k` — the ids run consecutively
straight through, so section 2's base is section 1's base plus section 1's
count. Two parallel id ranges per section, roughly 0x135–0x246 and 0x24a–0x35b.

**`MovesList` settles what the two ranges are.** Base A is drawn as text and
base B is handed to `DrawMoveListIcons` as its caption, which wraps it `"(%s)  "`
— so A is the **move's name** and B is the **parenthesised qualifier** that
follows it. One range per column of the screen.

**Section 3 is never displayed.** None of `MovesList`'s ten pages reads words 10,
11 or 12. Fourteen characters have a section 3 and no screen shows it.

**The three counts sum to the table's row count**, and `tools/moves.py` checks
that against the symbol gaps for all 23 characters. Section 2 is seven rows for
almost everyone, which is the size of the finisher list.

| character | rows | sections |
|---|---:|---|
| Kano | 14 | 7 + 7 + 0 |
| Sonya | 19 | 5 + 7 + 7 |
| Jax | 18 | 8 + 7 + 3 |
| NightWolf | 17 | 5 + 7 + 5 |
| SZ | 19 | 7 + 7 + 5 |
| Stryker | 17 | 6 + 7 + 4 |
| Sindel | 12 | 5 + 7 + 0 |
| Sektor | 11 | 4 + 7 + 0 |
| Cyrax | 13 | 6 + 7 + 0 |
| KL | 12 | 5 + 7 + 0 |
| Kabal | 12 | 5 + 7 + 0 |
| Sheeva | 14 | 4 + 7 + 3 |
| ST | 13 | 6 + 7 + 0 |
| LK | 17 | 6 + 7 + 4 |
| Smoke | 12 | 5 + 7 + 0 |
| Kitana | 14 | 5 + 7 + 2 |
| Jade | 17 | 7 + 7 + 3 |
| Mileena | 12 | 5 + 7 + 0 |
| Scorpion | 14 | 4 + 7 + 3 |
| Reptile | 14 | 7 + 7 + 0 |
| Ermac | 13 | 4 + 5 + 4 |
| Classic_SZ | 7 | 4 + 3 + 0 |
| Humansmoke | 8 | 4 + 4 + 0 |

**319 rows across the roster**, plus `Generic`'s 17. Ermac is the only character
whose second section is not seven.

### One row nobody can see

`ST_Moves6` occupies 14 rows' worth of space but `MovesListTab` accounts for
13. The fourteenth is `17 11 18 3 3 19 16 11` — byte for byte Kano's last row.
Leftover, and unreachable: nothing indexes it. `tools/moves.py` flags it.

---

## The alphabet

`DrawMoveListIcons` (armv7 `0x0001e60c`) splits the values in two, and **this is
the part a decoder has to get right**:

| value | what it draws |
|---|---|
| 0 – 15 | one cell of the 4×4 `MOVES_ICONS.PNG` atlas |
| 16 | `GameTextNoHeader(0x3a8)` — a translated word |
| 17 | `GameTextNoHeader(0x3a9)` — a translated word |
| 18 | `"("` |
| 19 | `") "` |
| 20 | `"/"` |
| 21 | `GameTextNoHeader(0x3aa)` — a translated word |
| 22 | `GameTextNoHeader(0x3f9)` — a translated word |
| > 22 | falls back to cell 0 |

**16 and up are punctuation and words, not inputs.** A sequence like

```
17 7 18 0 3 3 0 19 16
```

is `<word 0x3a9>` `icon7` `(` `icon0` `icon3` `icon3` `icon0` `)` `<word 0x3a8>`
— a hold-something, a bracketed direction sequence, and a release-something. The
brackets and the two words are display furniture. Only the icons are inputs.

### The atlas

Values 0 to 15 map one-to-one onto the sixteen cells:

| | u=0 | u=0.25 | u=0.5 | u=0.75 |
|---|---|---|---|---|
| **v=0** | 0 | 1 | 2 | 3 |
| **v=0.25** | 4 | 5 | 9 | 8 |
| **v=0.5** | 13 | 7 | 6 | 14 |
| **v=0.75** | 12 | 11 | 10 | 15 |

The first row runs 0, 1, 2, 3 and the rest does not, so the sheet was laid out
by hand rather than generated.

**Naming the sixteen cells needs one look at `MOVES_ICONS.PNG`**, which is the
user's own asset and is not in this repo. Cut the image into sixteen and read
them off; the mapping above then names every value in every table. That is the
only manual step left, and it is one image.

---

## What this replaces

[Issue #5](../../issues/5) proposed capturing the tables by running the game
under touchHLE, reading a `printf` that fires every frame on the Moves Info
screen, and segmenting the output by periodicity.

That plan was sound when it was written, but the `printf` turns out to be the
first line of `DrawMoveListIcons` and it prints **`seq[0]` only**:

```c
void DrawMoveListIcons(int y, const int *seq, const char *caption, int rightAlign)
{
    printf("%d\n", seq[0]);
```

So the log's periodicity is the screen redrawing, not the sequence being walked
— a run of period 6 is six frames of animation in the caller, not a six-input
move. The capture would have yielded the first input of each move, repeated.

The tables were in the binary the whole time.

---

## Cross-checks

- The **character order** in the table symbols matches
  [the roster](ROSTER.md) exactly — `Kano`, `Sonya`, `Jax`, `NightWolf`, `SZ`,
  `Stryker`, `Sindel`, `Sektor`, `Cyrax`, `KL`, `Kabal`, `Sheeva`, `ST`, `LK`,
  `Smoke`, `Kitana`, `Jade`, `Mileena`, `Scorpion`, `Reptile`, `Ermac`,
  `Classic_SZ`, `Humansmoke` — in address order.
- **`Humansmoke` and `Classic_SZ` have their own tables**, which is the same
  conclusion the roster reached from a different direction: the classic pair are
  separate characters, not palette swaps.
- The list stops at `Humansmoke` (index 22), so **Noob Saibot has no table** —
  the one gap in an otherwise complete set, and worth knowing before anyone
  writes a loop over 24.
- Every value observed across all 673 rows is in `0 .. 22`, which is exactly the
  range `DrawMoveListIcons` handles.

---

## Still open

- **The sixteen icon names.** One look at `MOVES_ICONS.PNG`, as above.
- **The four text ids** (`0x3a8`, `0x3a9`, `0x3aa`, `0x3f9`) resolve through
  `GameTextNoHeader` into the language file. Reading them out needs
  `LANGUAGE_TEXT_EN` parsed, which `LoadTextData` describes.
- **What the three sections mean.** Section 2 being seven rows for twenty-one of
  twenty-three characters points at the finisher list (Fatality, Fatality 2,
  Animality, Babality, Friendship, Stage Fatality, Mercy is seven), but that is
  a reading and not yet a fact. `MovesList` shows sections 1 and 2 on their own
  pages and never shows section 3, which is consistent with 1 = specials,
  2 = finishers, 3 = something the shipped build dropped — still a reading.

Answered since this file was written: the two id ranges (above), and how the
tables are paged on screen — see `MovesList` in
[GameCode.c](../decomp/gamecode/GameCode.c).
