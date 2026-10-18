# The animation streams: how a fighter knows what to show

Every animation in this game is a **bytecode stream**, walked one step per
displayed frame, exactly the way `Playback_Update` walks a recorded demo. This
page is the whole of it: the tables that find a stream, the opcodes that make
one up, and the two files in `res/` that turn its numbers into frames.

Recovered from `do_next_a9_frame_pxob` (armv7 `0x00059c74`), `get_char_ani`
(`0x0005520c`), `init_anirate` (`0x000553a0`) and `next_anirate`
(`0x0005a680`).

---

## Finding a character's animation

```c
obj->field40 = <animation id>;
get_char_ani(obj);              /* turns the id into a stream pointer */
obj->field1c = <rate>;
init_anirate(obj);
```

`get_char_ani` is four instructions and they say everything:

```
r3 = obj->field08              ; the part
r1 = obj->field40              ; the animation id
r3 = part->field24             ; the CHARACTER NUMBER
r3 = _character_anitabs1[r3]   ; 0x0016ee34 -- one table per character
r3 = r3[r1]                    ; one pointer per animation
obj->field40 = r3
```

`_character_anitabs1` has 28 entries and **six of them are the same pointer**:

| characters | table | |
|---|---|---|
| 18–23 | `_nj_ani_data` `0x0015978c` | Scorpion, Reptile, Ermac, classic Sub-Zero, classic Smoke, Noob Saibot |
| 15–17 | `0x00172884` | Kitana, Jade, Mileena |
| 7, 8, 14 | `_robo_ani_data` `0x0015b1d0` | Sektor, Cyrax, Smoke |
| 24 | `_mot_ani_data` `0x00159398` | Motaro |
| 25 | `_sk_ani_data` `0x0015e29c` | Shao Kahn |

**The palette swaps share their animations**, which is the check that the table
is the right one: the six ninjas fall out of the data rather than being assumed.

`_nj_ani_data` runs to `0x0015b1d0`; its first `0x170` bytes are **92 pointers**
and the streams follow.

---

## The opcodes

A word **above 0x12 is a frame**. The interpreter shows it and returns, so one
word is one step of the animation clock. Everything at or below 0x12 is an
instruction:

| | | |
|---:|---|---|
| 0 | `END` | the animation is over, return 0 |
| 1 | `JUMP p` | continue at `p` — **this is how a clip loops** |
| 2 | `FLIP` | `part->0x28 ^= 0x10`, the facing flag |
| 3 | `ADJXY a` | `multi_adjust_xy_ob(a, 0)` |
| 4 | `ADJXY2 a b` | the same with a second component |
| 5 | `LASTFRAME f` | show `f` and return 1 |
| 6, 7 | `SET f` | `obj->field1c = f`, no frame shown |
| 8 | `CHARSW c p` | if the character is `c`, continue at `p` |
| 9 | `NOP` | |
| 10 | `OFFSETXY p` | `do_ani_offset_xy` |
| 11 | `GETPRC_Z` | attaches a second object; see `match_ani_points_ob_ob` |
| 12 | `SLAVE p` | `slave_ani`, continue |
| 13 | `FRAMECONT f` | show `f` and **continue** — a frame that costs no time |
| 14 | `SLAVERET p` | `slave_ani`, return 2 |
| 15 | `CHARSND s` | `ochar_sound` |
| 16 | `ADJ a b` | `multi_adjust_xy_ob`, then skip a word |
| 17 | `CALL fn` | `blx fn` — the stream can call code |
| 18 | `SND s` | `rsnd_func` |

---

## The frame numbers are global

A stream holds **global frame ids**, not a character's own. Two files in `res/`
close the gap:

- `framelists/<NAME>FRAMES.bin` — 14,490 bytes for every character, 7,245
  `uint16` slots, `0xFFFF` where that character has no such frame. It maps a
  global id to that character's own frame number.
- `framelists/<name>frames.txt` — names those frames, one per line.

Scorpion's animation 0 holds 351..359; through his map that is 216..224, and the
text file calls them `SCSTANCE1`..`SCSTANCE9`.

---

## The clock

```c
init_anirate:   Pp->field1c = rate;  Pp->field20 = 1;
next_anirate:   if (--Pp->field20 > 0) return;
                Pp->field20 = Pp->field1c;
                if (obj->field40 && *obj->field40) do_next_a9_frame(obj);
```

So an animation advances **one frame every `rate` game frames**, and the rate is
a literal the starting state sets. Three are known:

| | rate | from |
|---|---:|---|
| the walk | 5 | `_walk_forward_info`, the other half of the speed entry |
| the run | 3 | `run_setup` |
| `t_r_rabbit` | 4 | mkanimal.c |

Every other rate lives in a per-move state that is not decompiled.

---

## What this corrected

Both ports had a clip table read **by eye** out of `scorpionframes.txt`: a name
like `SCHIPUNCH1`..`SCHIPUNCH7` became the range 80..86. Against the streams:

- **`SCHIPUNCH` is three frames**, 80, 81, 82. The other four belong to nothing
  the high punch plays.
- **The streams are not ranges.** `SCHIHIT` is 72, 73, 72, 71 — it comes back.
  `SCDUCKHIT` is 31, 32, 31, 30. Played as a range these reverse.
- **Backing up is the walk cycle REVERSED**, 290 down to 282, not the same nine
  frames played forwards.
- **There are two jump flips**, ids 26 and 27, one turning each way.
- Scorpion has 92 animations, including a spear (82), a summon (85), the mask
  coming off (86) and the fire breath (88), none of which either port plays.

`umk3/umk3_scorpion_ani.gd` in the Godot project is generated from this, so the
numbers are never typed by hand again.
