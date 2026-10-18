# The fight's physics, and how fast it all runs

Everything here is measured out of the armv7 slice. Where a number is still a
choice it says so.

---

## The rate: 60 Hz, and the engine says so once

`limeBegin` (`0x00066a74`) is the frame opener, and it holds **the only `1/60`
double in the whole binary**, at `0x00066cc4`:

```
d5 = now                       ; a double, from the time call at 0xdd0d4
d7 = now - last
if (d7 <= 0) skip
d6 = 1.0/60.0                  ; <- the literal, and there is exactly one
d7 = d6 / d7
_limeFPS          = <literal>
_limeFPSScaleFactor = (float)d7 ; clamped below at 0.25
```

So `limeFPSScaleFactor` is **1.0 at 60 fps**, 0.5 at 30, floored at 0.25. The
engine's reference frame is a sixtieth of a second.

**The fight does not use that factor.** `gravity_n_bounds` and `DisplayUpdate`
are integer per-frame arithmetic and neither touches it; it is the renderer's,
for whatever it smooths. The arcade logic is a fixed step per drawn frame:

- `UpdateArcadeCode` calls `mk3_update` exactly **once** -- its two call sites
  are the one-player and multiplayer branches, not two ticks
- its caller at `0x0002b15a` runs it once a frame after the pause checks
- there is no accumulator and no fixed-step loop anywhere in that path

**So every per-frame number in this document is per 1/60 second.**

**`animationInterval` is `1.0/60.0`, and it is a literal, not a runtime
value.** `-[EAGLView initWithCoder:]` (`0x000613f0`) writes it directly into
the ivar at `0x00061546` once the GL context is up, whichever renderer version
it got:

```
mov.w r1, #0x11111111          ; the low word of the double
ldr   r2, [pc, #0x104]         ; the high word, 0x3f911111
str   {r1, r2}, [animationInterval]
```

`0x3f911111_11111111` is `1.0/60.0` to the bit -- read straight out of the
literal pool, not inferred from the disassembly printer's own resolution
(which only annotates the pc-relative `add`, not the plain `ldr r2, [pc, #N]`
one instruction earlier; the two literals have to be read as a pair by hand).
It is the same call site whichever context creation branch succeeds, so there
is no ES1-vs-ES2 split here either.

`-[EAGLView startAnimation]` then schedules an `NSTimer` from that ivar. So
the DESIGN rate and the REQUESTED rate are the same number, set once, as a
constant: **60 Hz, end to end, with nothing in the binary that ever asks for
30.** A brute-force search of the whole armv7 slice for the IEEE-754 double
`1.0/30.0` finds zero occurrences; `1.0/60.0` occurs exactly once, at this
call site (the `limeFPSScaleFactor` literal above is the same bit pattern
read a second, unrelated time out of `limeBegin`'s own literal pool).

What is still open is only what a specific piece of 2011 hardware actually
*delivered* once `NSTimer` and `CPU`/`GPU` load were in the loop -- iOS could
still miss the requested interval under load, and nothing in the binary
records what happened on a real device. That is a hardware question, not a
design one, and it does not change what a native port should target: the
game asks the system for 60, unconditionally, so a port asking for 60 is
matching the actual request, not guessing at one.

---

## Walking

`_walk_forward_info` (`0x0016ef6c`) and `_walk_backward_info` (`0x0016f03c`),
eight bytes per character, indexed by the character number.
`decode_walk_table` (`0x000552dc`) splits each entry: **`entry[0]` is the
animation rate** and **`entry[1] << 4` is the speed**, already 16.16.

| | forward | back | rate |
|---|---:|---:|---:|
| most of the cast | 3.0 – 3.5 | **2.25** | 5 |
| Scorpion | 3.25 | 2.25 | 5 |
| classic Smoke | 4.0 | 2.5 | 5 |
| Noob Saibot | 5.0 | 3.0 | 5 |
| Motaro | 4.125 | 4.0 | 4 |
| Shao Kahn | 5.0 | 4.0 | 3 |

`walk_flip_reverse` negates the speed on bit 4 of the part's `0x28` (the facing
flag) and `get_walk_info_b` negates it again -- that is the whole of what makes
backing up go the other way.

`set_x_vel_player` (`0x00055a68`) then writes it to **`G + 0xb8`** for player
one and **`G + 0x210`** for player two -- the `0x158` stride `repell_func`
uses -- with the part pointers beside them at `+0xbc` and `+0x214`.

---

## The leash, and why a port without it feels different

**The velocity is not applied raw.** `repell_func` (mkrepell.c, fully
decompiled) reads both velocities out of `G` every frame and arbitrates:

- **closer than `0x3c` (60 units)**: driven apart at a fixed **3.0 a frame**
- **walking into each other while close**: each velocity **halved**, or both
  stopped outright
- **further apart than `0x130` (304 units)**: half the excess is taken out of
  the gap, both fighters moving, unless a fighter has opted out with bit 10 of
  the part's `0x30`

So fighters are **leashed to about 304 units**, pushed apart when they overlap,
and slowed when they close. A port that applies the table speed directly gets
the top speed right and the FEEL wrong, because the arbitration never happens.
Neither this repository's C scene nor the Godot port runs it yet: in the C one
`repell_func` is a deliberate empty stub.

---

## Jumping

`t_do_jump_up` (`0x000580e8`):

```
obj->field1c = 1;  group_sound(obj)      ; the grunt
obj->field40 = 0x16;  get_char_ani(obj)  ; animation 22 -- SCJUMP
obj->field20 = -655360                   ; the literal at 0x000581d0
obj->field24 = field20 + 0xa8000
obj->field28 = 4
```

`t_flight_call` (`0x00055aec`) then copies **`field20` into the part's `0x1c`**
-- the y velocity -- and **`field24` into `0x20`**, the gravity. `0xd` in
either field means "leave this one alone"; it is a sentinel, not a value.

    jump velocity  -655360 = -10.0 exactly, in 16.16
    gravity        -10.0 + 0xa8000 = +0.5

Both ports had guessed these: `-10.0` was right and `0.40` was not.

---

## Size, and the scale between the two worlds

`_ochar_ground_offsets` (`0x0016ef04`), one int32 per character:

| | | | |
|---|---:|---|---:|
| Shang Tsung | 136 | Scorpion and the ninjas | 139 |
| Liu Kang, Kitana, Jade, Mileena | 139 | Kung Lao | 140 |
| Sub-Zero | 142 | Sonya | 143 |
| Kano, Nightwolf, Sindel | 144 | Stryker, Sektor, Cyrax, Smoke | 147 |
| Kabal | 148 | Jax, Sheeva | 158 |
| Motaro | 168 | Shao Kahn | 173 |

Scorpion's skinned model measures **140.1 scene units** against this table's
**139**, so **one engine unit is one scene unit** to within a per cent.

That matters more than it looks. Both ports derived the scale from
`mk3_getbbox`'s hard-coded `56 x 72` box -- the only one in the binary, and it
belongs to a single animation -- giving 1.946. At 1.946 the arena comes out
2,713 scene units wide against Graveyard's 2,839-unit ground plane, so a
fighter can walk to the edge of the world. At 1.008 the arena is 1,405: half the
stage, centred.

**There is no per-character weight table.** Gravity is one literal for
everyone. What varies per character is the walk speed, the walk's animation
rate, and this height.

---

## The animation rate outside the walk

`init_anirate` loads `Pp->field1c` with a rate and starts the countdown at 1;
`next_anirate` decrements and steps a frame at zero. The basic moves set **no
rate at all** -- `t_joy_hi_punch` writes `field40 = 0xe`, calls `get_char_ani`
and never touches `init_anirate` -- so they inherit whatever the last state
left.

Of the 70 calls to `init_anirate`, 57 carry a small literal immediately before:

| rate | sites | rate | sites | rate | sites |
|---:|---:|---:|---:|---:|---:|
| 0 | 2 | 3 | **16** | 6 | 5 |
| 1 | 4 | 4 | **17** | 8 | 1 |
| 2 | 8 | 5 | 3 | 15 | 1 |

Three and four are 33 of the 57. The walk's own 5 is three of them.
