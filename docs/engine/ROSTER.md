# The roster, and what a character index means

Character indices turn up everywhere in this codebase — `PLAYER1MODEL`,
`Character2`, `TowerRand`, `Stats`, the achievement counters, the stage-fatality
scene tables. Until now they were numbers. They are not any more.

---

## The table

| # | Character | | # | Character |
|---:|---|---|---:|---|
| 0 | KANO | | 13 | LIU KANG |
| 1 | SONYA | | 14 | SMOKE |
| 2 | JAX | | 15 | KITANA |
| 3 | NIGHTWOLF | | 16 | JADE |
| 4 | SUB-ZERO | | 17 | MILEENA |
| 5 | STRYKER | | 18 | SCORPION |
| 6 | SINDEL | | 19 | REPTILE |
| 7 | SEKTOR | | 20 | ERMAC |
| 8 | CYRAX | | 21 | SUB-ZERO (classic) |
| 9 | KUNG LAO | | 22 | SMOKE (classic) |
| 10 | KABAL | | 23 | NOOB SAIBOT |
| 11 | SHEEVA | | 24 | MOTARO |
| 12 | SHANG TSUNG | | 25 | SHAO KAHN |

**Twenty-six entries, 0 through 25.**

---

## How it was established

Not by one reading. Six, from four different directions, and they agree.

1. **`GameInit_LoadABit` contains four 24-way switches** (armv7 `0x0002b940`),
   one per player slot per stage-fatality stage, each selecting a
   `SUBWAY_<NAME>.scene` or `LAIR_<NAME>.scene`. All four order the characters
   identically. Twenty-four cases, so indices 0–23.

2. **`_CharacterNames`** at `0x0014fe54` is an array of `char *` and names all
   twenty-six in the same order. That is where MOTARO and SHAO KAHN come from —
   the switches stop at 23 because bosses have no stage death.

3. **`FE_CHARACTER_SLOTS` is 26.** `Players.c` measured that from the gap
   between `_FECharacters` and the next symbol: `0x0020e634` to `0x00218cc4` is
   `0xA690`, which is `26 × 0x668` exactly.

4. **`Task_FEDestroy` measured it again**, from `CharacterVSTexture` being
   `0x68` bytes — 26 pointers — and from its loop walking 1 to 25 after doing 0
   by hand.

5. **`AnimateFECharacters` compares its index after the body**, so it processes
   slot 25 — which is only correct if 25 exists.

6. And the numbers scattered through the game logic land on the right names:

   | site | test | reads as |
   |---|---|---|
   | `QuitAsLose` | `PLAYER2MODEL == 25` increments `defeatedBySK` | three consecutive losses to **Shao Kahn** |
   | `QuitAsWin`, `FE_Task_Main_Menu` | `PLAYER1MODEL == 5` increments `winningStryk` | five wins in a row as **Stryker** |
   | `FE_Task_Character_Select` | `Character2 = 0x19` in karnage and mode 5 | **Shao Kahn** as the fixed opponent |
   | `GameInit_LoadABit` step 50 | either fighter is 5 → preload `{2, 3, 13}` | **Stryker** pulls in Jax, Nightwolf and Liu Kang |
   | `GameInit_LoadABit` step 42 | either fighter is 0, 8 or 11 → load `PURPLEHAZEDEATH.scene` | **Cyrax**'s self-destruct, and the two it is used on |

   `defeatedBySK` and `winningStryk` were named by whoever wrote the original.
   Both now check out against the table rather than against a guess.

---

## Things that follow from it

### Bit 7 is "this side is the CPU"

`GameInit_LoadABit` step 52 hands the fight engine its fighters:

```c
AIOn == 0   mk3_init(P1,        P2,        FrameID_GetBBox, AIOn)
AIOn == 1   mk3_init(P1,        P2 | 0x80, FrameID_GetBBox, AIOn)
AIOn == 2   mk3_init(P1 | 0x80, P2 | 0x80, FrameID_GetBBox, AIOn)
```

**The fourth argument is `AIOn` itself**, which this table first recorded as a
constant `1`. It is not: `r3` is loaded from `AIOn` at `0x0002d974` for the
switch that picks between the three calls and is never reassigned before any of
them, so each call passes the value it just tested. `DrawHUD` does exactly the
same thing at its own three call sites, which is what prompted the re-read.
The `| 0x80` bits and the argument carry the same information twice.

which is exactly the three states `ShowDebugInfo` prints as "Human vs Human",
"Human vs CPU" and "CPU vs CPU". So **a model index is seven bits wide** and
twenty-six entries leave the top bit free for the flag.

### Noob Saibot can never be your most-played character

`Stats[index + 15]` is the per-character play counter — `GameInit_LoadABit`
increments it once per fight. `FE_Task_Stats` finds the "favourite" by scanning
**23** counters from word 15, so indices 23, 24 and 25 are counted and never
looked at. Noob Saibot is countable and unrankable; Motaro and Shao Kahn are
never player one anyway.

### The classic pair are separate characters, not skins

21 and 22 are their own indices with their own stage-death scenes
(`SUBWAY_OLDSUBZERO.scene`, `LAIR_OLDSMOKE.scene`), not variants of 4 and 14.
Anything that maps indices to art has to treat them as distinct.

---

## Stage indices, as far as they go

The same function pins down two of them, because only two stages have
per-character deaths:

| `LevelSelect` | stage |
|---:|---|
| 4 | **Subway** — `SUBWAY_<CHARACTER>.scene` |
| 11 | **Scorpion's Lair** — `LAIR_<CHARACTER>.scene` |

and `GameInit_LoadABit` step 2 maps four stages onto a `RoundParam` value:

| `LevelSelect` | `RoundParam[9]` |
|---:|---:|
| 3 | 1 |
| 4 | 3 |
| 8 | 2 |
| 11 | 4 |

The rest leave it at 0. What that value selects is not established here.

See [STAGES.md](STAGES.md) for the stage scene and finisher inventory, which was
worked out from the asset files rather than from code — the two agree.

---

## Game modes

`GameMode` is the other index that turns up everywhere, and `DrawHUD` is where
the six values disambiguate each other -- each mode reaches something only it
reaches.

| `GameMode` | mode | what identifies it |
|---:|---|---|
| 0 | **Arcade / tower** | `winStreak`; the tower-completion logging |
| 1 | **Network** | `isParentBasedOnSpeed`, `updateMPWins`, `isParent` |
| 2 | **Training** | the only caller of `TrainingMessages` |
| 3 | **Karnage** | draws `KarnageScore`; the one mode that never draws player 2's plate |
| 4 | **Survival** | `survivalWinStreak` |
| 6 | **Two players, one device** | logged as the literal `"2 Players on 1 iPad"` |

Mode 5 never appears in `DrawHUD`. `FE_Task_Character_Select` uses it, so it
exists; what it is has not been established here.
