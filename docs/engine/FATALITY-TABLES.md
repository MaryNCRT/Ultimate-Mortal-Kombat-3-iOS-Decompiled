# The fatality tables, and the character roster they give away

`t_do_fatality_1` and `t_do_fatality_2` are one function written twice. Each
takes the character out of the other object's `+0x24` and uses it to index a
table of thread handlers:

```c
h = ochar_fatalitiesN[obj->field08->field24];
```

The tables are at **`0x00166e68`** and **`0x00166ed0`**. The gap between them is
`0x68` = 104 bytes = **26 pointers**, so the second begins where the first ends
and the roster is 26 entries.

Every entry is a named symbol, which makes this the clearest character-id list
in the binary — better than anything the animation code gives, because nothing
has to be inferred from a constant.

## Table 1 — `_ochar_fatalities1` (0x00166e68)

| id | symbol | id | symbol |
|---|---|---|---|
| 0x00 | `t_kano_lazer` | 0x0d | `t_kang_fire` |
| 0x01 | `t_sonya_kiss` | 0x0e | `t_smoke_blowup_earth` |
| 0x02 | `t_jax_slice` | 0x0f | `t_kitana_decap` |
| 0x03 | `t_ind_light` | 0x10 | `t_jade_shaker` |
| 0x04 | `t_sz_blow` | 0x11 | `t_mileena_nails` |
| 0x05 | `t_swat_taser` | 0x12 | `t_scorpion_hell` |
| 0x06 | `t_lia_scream_rip` | 0x13 | `t_reptile_tongue` |
| 0x07 | `t_fat_robo_crush` | 0x14 | `t_ermac_super_slam` |
| 0x08 | `t_cyrax_self_destruct` | 0x15 | `t_osz_head_rip` |
| 0x09 | `t_lao_tornado` | 0x16 | `t_ermac_decap_attack` |
| 0x0a | `t_kabal_inflator` | 0x17 | `t_non_violent_finish` |
| 0x0b | `t_sg_pound` | 0x18 | `t_non_violent_finish` |
| 0x0c | `t_st_spike` | 0x19 | `t_non_violent_finish` |

## Table 2 — `_ochar_fatalities2` (0x00166ed0)

| id | symbol | id | symbol |
|---|---|---|---|
| 0x00 | `t_kano_skeleton` | 0x0d | `t_kang_mk_game` |
| 0x01 | `t_sonya_kiss_crusher` | 0x0e | `t_smoke_arm` |
| 0x02 | `t_jax_grow` | 0x0f | `t_kitana_kiss` |
| 0x03 | `t_ind_zap_kill` | 0x10 | `t_jade_impale` |
| 0x04 | `t_sz_lift_n_freeze` | 0x11 | `t_mileena_suck_kiss` |
| 0x05 | `t_sw_plant_bomb` | 0x12 | `t_scorpion_fire` |
| 0x06 | `t_lia_hair_spin` | 0x13 | `t_reptile_vomit` |
| 0x07 | `t_robo_flame_throw` | 0x14 | `t_ermac_decap_attack` |
| 0x08 | `t_cyrax_helecopter` | 0x15 .. 0x19 | `t_non_violent_finish` |
| 0x09 | `t_lao_slicer` | | |
| 0x0a | `t_kabal_scare` | | |
| 0x0b | `t_sg_flesh_rip` | | |
| 0x0c | `t_st_suck` | | |

(`helecopter` is the original symbol's spelling.)

## What the tables say on their own

These follow from the tables and nothing else:

- **The roster is 26.** From the 0x68-byte gap, not from a count anywhere.
- **Ids 0x17, 0x18 and 0x19 have no fatality in either table.** `0x19` was
  already identified as Shao Kahn from a routine name elsewhere in the tree, so
  at least one of the three is a boss. Bosses having no fatality is the
  simplest reading of three consecutive `t_non_violent_finish` entries.
- **Id 0x16 has one fatality and it is Ermac's.** `t_ermac_decap_attack` is
  table 1's entry for 0x16 and table 2's entry for 0x14. Two ids sharing one
  routine is what you would expect of characters built from one another.
- **Id 0x15 has one fatality and none in the second table**, so the tables are
  not both full: 0x00–0x14 have two each, 0x15 and 0x16 have one, the last
  three have none.
- **`t_st_spike` and `t_st_suck` are at 0x0c**, which independently confirms
  the identification of 0x0c made earlier from `player_shang_ani`. Two
  unrelated pieces of the binary agreeing is worth more than either alone.

## Matching the abbreviations to characters

The binary gives the prefix; matching it to a character means knowing the
game's fatalities, so this half is identification and not a fact read out of
the file. Kept separate for that reason.

Unambiguous from the prefix alone: `kano`, `sonya`, `jax`, `cyrax`, `kabal`,
`kitana`, `jade`, `mileena`, `scorpion`, `reptile`, `ermac`, `smoke`.

The abbreviated ones, with what the routine names describe:

| prefix | ids | the routines describe |
|---|---|---|
| `sz` / `osz` | 0x04 / 0x15 | freezing and lifting; a head torn off. Two Sub-Zeros, which the game has |
| `st` | 0x0c | a spike, and something being sucked. Shang Tsung, agreeing with the earlier finding |
| `kang` | 0x0d | fire, and an arcade cabinet dropped on the loser |
| `lao` | 0x09 | a tornado and a slicer |
| `sg` | 0x0b | a pound and a flesh rip |
| `lia` | 0x06 | a scream that rips, and a hair spin |
| `ind` | 0x03 | light, and a zap |
| `swat` / `sw` | 0x05 | a taser and a planted bomb — one character, two prefixes |
| `robo` | 0x07 | a crush and a flame thrower, distinct from `cyrax` at 0x08 |

`swat` and `sw` being the same character across the two tables is a reminder
that these prefixes are whatever the original programmer typed, not a scheme.

## Reproducing

```bash
python tools/dumpfn.py t_do_fatality_1
```

The table address is the `add r2, pc` target in the disassembly; read 26 words
from it and look each up in `OUTPUT/func-to-file.txt`. Every pointer has the
Thumb bit set, so mask off bit 0 before matching.
