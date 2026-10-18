# The arcade original under MAME

*Ultimate Mortal Kombat 3* (Midway, 1995) runs on **Midway Wolf Unit**, and MAME
emulates it well. **It runs, and a scripted session reaches a real match.** This
records what observing it can contribute, what it cannot, and how far the
exploration got.

---

## Verified environment

MAME **0.289**, driver `williams/midwunit.cpp`, driver status **good**.

| | |
|---|---|
| Main CPU | TMS34010, 50 MHz crystal (6.25 MHz effective after the internal divider) |
| Sound CPU | ADSP-2105 @ 10 MHz |
| Security | Microchip PIC16C57 @ 4 MHz |
| Display | 400×254 @ **54.706840 Hz** |

The refresh rate is worth keeping in mind: the arcade original does **not** run
at 60 Hz, so "frames" in any arcade-derived timing are not the frames a modern
port would assume.

---

## The ROM set: an old-naming trap

The set verified as bad, and the diagnosis was not what it looked like.

`umk3.zip` reported only the PIC missing. Extracting to loose folders made
*more* files appear missing — because **the set uses MAME's old ROM naming**
(`mk3-u122.bin`, `umk3-u2.bin`) while 0.289 expects the modern descriptive names
(`l1_mortal_kombat_3_u122_game_rom.u122`). MAME hash-matches inside a zip and
tolerates it; with loose files it is stricter.

Matching every file by SHA1 against `-listxml` showed all 26 ROMs were correct
and only mis-named. After renaming: **`romset umk3 is good`**. The one genuinely
absent file was the security PIC.

## The security PIC

The one file that genuinely had to be sourced:

```
463_mk3_ultimate.u64   4,105 bytes
CRC32  4f425218
SHA1   7f26045ed2c9ca94fadcb673ce10f28208aa720e
region: serial_security:pic
```

The **PIC16C57 security chip** dump. The driver needs it to emulate Midway's
protection and refuses to start without it — fatal, not a warning. `umk3r11` is
a clone and inherits the parent's ROMs, so it needs the same file; MAME reports
`NOT FOUND (umk3)`, naming the set it searched. The hashes are recorded so a
candidate can be verified rather than assumed.

**The ROM is not in this repository and never will be.** It is Warner Bros.
property. `LABORATORIO MAME/` is excluded in full — emulator, ROMs, symbols,
NVRAM and save states.

---

## What observing the arcade could contribute

Narrow, but it lands in the one gap the verification oracle cannot cover.

### The process architecture

`gamecode/logic` is cooperative multitasking with per-process stacks and stack
jumping, inherited from the arcade's TMS34010. This is **established from our
own binary** — 1,172 `t_` functions, 139 `q_`, 105 `c_`, plus
`_reset_proc_stack`, `_UnstackSwitches`, `_stack_switch_bits`, `_DoSwitchJump`
and `_SwitchQueue`.

The oracle cannot follow that code by design: it dispatches through function
pointers. Watching the original scheduler run would show the *shape* of the
system — how control is yielded, what a process control block holds, how many
processes live during a fight — as observation rather than inference.

### A structure hypothesis for `frames.x` and `moves_data.x`

The most useful possible outcome. Those two files descend from the arcade's data
tables. **The values will have changed; the organisation may not have.** Seeing
how many fields a move record holds, in what order and of what types, would give
a starting hypothesis for two formats that have not been started.

**Structure only, never values.** iOS values come from the iOS binary and are
verified against the iOS game. Tuning mechanics against the arcade would produce
something that feels like the arcade rather than like the game being ported.

---

## What it cannot contribute

**The arcade is 2D sprites. The iOS version is 3D.** A character there is sprite
sheets and frame tables; here it is mesh, skin, bones and quaternion animation on
an engine that did not exist in 1995. LIME, `.meshset`, `.skin`, `.bones`,
`.skinanim` and the mesh viewer receive **nothing** from the arcade.

**The values do not match.** The lineage is arcade → Java ME/BREW → C++ iOS, with
rebalancing, touch adaptation and roster changes along the way. Frame data,
hitboxes and timings are not shared.

**Asset formats share nothing.**

---

## Provenance

Observing a running program is **black-box** reverse engineering — the cleanest
form there is, and this case is cleaner still: TMS34010 is not ARM, so there is
no expression that could transfer even in principle. What comes out is
conceptual understanding, not anyone's code.

Anything documented from it must be cited as observation:
`— Source: observed in MAME, umk3, <date>`.

One caution. The public analysis usually cited for the arcade's process
architecture is a **review of leaked source code**. Building on someone else's
reading of leaked material is the same contamination one step removed, and this
project does not use it. The architecture is documented here from our own symbol
table instead, and MAME observation would *confirm our own derivation* rather
than follow that reading — which is why it is worth doing directly rather than
citing.

---

## Priority

Low, and time-boxed by design. The mesh viewer and the animation formats remain
the priority, and MAME contributes to neither. The environment is now built and
the resume point is one specific problem — but if the next bounded look produces
nothing usable for `frames.x` / `moves_data.x` or the scheduler, it should be
dropped without further cost.

---

## What works, verified by running it

With the PIC in place, `romset umk3 is good` and the whole loop runs.

### Automation: Lua, not the interactive debugger

MAME's Lua interface drives everything from `-autoboot_script`, which is far
more productive than typing into the debugger console. Confirmed working:

- **`cpu.spaces["program"]:read_u16(addr)`** — memory sampling
- **`mach.ioport.ports[":IN0"].fields["P1 Right"]:set_value(1)`** — input
- **`mach.video:snapshot()`** — screenshots, which is how you confirm what state
  the machine is actually in rather than assuming
- **`cpu.debug`** — available when launched with `-debug -debugger none`, and it
  exposes `wpset`, `bpset`, `wplist`, `go`, `step`. A write watchpoint sets
  successfully and returns its index.

Useful flags: `-video none -sound none -nothrottle -seconds_to_run N`.

### The trap that cost the most time

**Coins do not register until `Door Interlock` is closed.** On this Midway
board the cabinet door switch gates the coin mechanism, and MAME models it. With
it open, the coin input is accepted silently and nothing happens — the machine
sits on the attract screen looking like the automation failed.

```lua
mach.ioport.ports[":IN2"].fields["Door Interlock"]:set_value(1)
```

Screenshots are what caught this. Sampling memory and reasoning about the
numbers would have gone on indefinitely against an attract loop.

### Reaching a match, scripted

Boot ≈20 s → close interlock → three coins (20-frame presses) → 1 Player Start
→ High Punch to pick a fighter → High Punch to confirm. Verified by screenshot:
**Kitana vs Reptile, "FIGHT!" on screen.**

### Memory map

The TMS34010 **addresses in bits, not bytes** — consecutive 16-bit words are
`0x10` apart, and the map's ranges are bit counts.

| Range | |
|---|---|
| `0x00000000-0x003FFFFF` | delegate (video window) |
| `0x01000000-0x013FFFFF` | **main RAM** (0x400000 bits = 512 KB) |
| `0x01880000-0x018FFFFF` | RAM |
| `0xFF800000-0xFFFFFFFF` | ROM |

Regions: `:maincpu` 1 MB, `:video` 32 MB, `:dcs` 8 MB, `:serial_security:pic`
4,105 bytes.

---

## Task 2 — player state: narrowed, not found

Sampling 65,537 words of RAM at rest, after walking right, after walking left,
and after walking right again, keeping only addresses whose deltas reverse with
direction and then reverse back: **24 coherent candidates** out of 65,537.

A first attempt produced 260 candidates that were all noise — `A` and `C`
identical and `B` at `0xFEFE`. That is a fill pattern sampled during a screen
transition, and it is why the walk-left-then-right-again check was added.

A jump was used to try to separate X from Y, but walking and jumping both
perturbed most candidates, so the discrimination is not clean yet. The most
interesting pair is `0x010304E0` and `0x010306F0`, which move under both and in
different directions.

### Resume point

**A write watchpoint sets successfully but its `printf` action produces no
output**, because `-debugger none` leaves no console for the debugger's own
command interpreter to print to. Two ways forward:

1. Let the watchpoint **halt** execution and read `cpu.state["PC"].value` from
   Lua on the break, then `cpu.debug:go()`. This keeps everything in one script.
2. Launch with a real debugger console and `-debugscript`, where `printf` and
   `trackmem` work as documented.

Either gives what Task 2 is actually after: **the PC that writes the player's
position**, which is the entry to the movement system.
