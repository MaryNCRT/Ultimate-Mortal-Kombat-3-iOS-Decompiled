# `frames.x` and `moves_data.x` — plain text, not a format

Two files sat on the "unsolved formats" list for the whole project. They needed
no reverse engineering at all: **they are text**, shipped readable inside the
app bundle.

They live in `Payload/UMK3.app/`, not in `res/`.

---

## `frames.x` — 6,831 animation frame names

A comma-separated list of quoted names, one per line:

```
  	"TSTHROWPRO1"
, 	"KLSTUMBLE1"
, 	"TSTHROWPRO2"
, 	"OBLOPUNCH1"
, 	"BIGXPLOD1"
```

6,831 names in 6,831 lines. The naming is `<character prefix><action><index>` —
`KLSTUMBLE1` through `KLSTUMBLE8`, `OBLOPUNCH1` through `OBLOPUNCH6`. This is the
frame-name table the animation system indexes into.

## `moves_data.x` — 144 secret-move tables, as C

Not data *about* C. Actual C array declarations:

```c
int sm_ermac_bc[]=
{
   (int) p1b2                 //; player 1 requirements
 , (int) p2b2                 //; player 2 requirements
 , (int) x_fatal
 , (int) q_ermac_fatal        //; yes/no routine
 , (int) t_do_fatality_1      //; address of secret move
 , (int) 0x30
 , (int) sw_down
 , (int) sw_down
 , (int) sw_down
 , (int) sw_up
 , (int) sw_down
 , (int) 0
 , ...
};
```

**144 arrays across 25 characters.** Each is one secret move: two player
requirement flags, a class, a predicate, the move's handler, and then an input
sequence terminated by `0`.

### Move classes

| Class | Count |
|---|---:|
| `x_ground` | 113 |
| `x_fatal` | 44 |
| `x_friend` | 22 |
| `x_baby` | 21 |
| `x_animal` | 19 |
| `x_airborn` | 11 |
| `x_close_animal` | 1 |
| `x_mercy` | 1 |

### The input vocabulary is ten symbols

| Symbol | Uses |
|---|---:|
| `sw_down` | 350 |
| `sw_left` | 349 |
| `sw_right` | 349 |
| `sw_run` | 206 |
| `sw_block` | 102 |
| `sw_up` | 100 |
| `sw_lo_kick` | 28 |
| `sw_hi_kick` | 18 |
| `sw_lo_punch` | 14 |
| `sw_hi_punch` | 2 |

Note this is a **different table** from the one behind
[issue #5](../../issues/5). That one is the in-game *moves list display*, whose
printed values span 0–22. This is the *secret move* table, and its alphabet is
ten symbols. Both are worth having; they are not the same data.

---

## The part that matters most: it names the binary's functions

`moves_data.x` references **90 distinct `q_` predicates and 92 distinct `t_`
move handlers by name** — `q_ermac_fatal`, `t_do_fatality_1`, `t_do_animality`,
`t_do_baby`, `t_do_ermac_slam`.

**Those are functions this project already has addresses for.** The binary
carries 1,172 `t_` and 139 `q_` symbols; `t_do_fatality_1` is one of the six
verified in the dispatch table at `0xf3150` in `__DATA,__nl_symbol_ptr`.

So this file is a **hand-written map from move semantics to function names**,
covering 182 of the fight engine's functions. For `gamecode/logic` — the part
the verification oracle can never reach, because it dispatches through function
pointers — that is a substantial head start on what those functions *are*, from
a source that is neither guesswork nor a leak.

---

## Provenance and how the port should use this

**These files ship inside the IPA.** They are app-bundle content in a copy the
user legally owns, exactly like `.meshset` or `.pvr`. They are not leaked
material, and reading them carries none of the contamination problem that
[leaked retail source does](METHODOLOGY.md).

But they *are* source form, so the distinction the project already draws applies
with full force:

- **The values are data.** Move classes, input sequences and frame names get
  extracted at build time from the user's own copy, the same as every other
  asset. That is the existing legal model, unchanged.
- **The C text is not ours to ship.** None of it goes in this repository, and
  the port does not paste these declarations into its source. It reads the file.

The function *names* are a different matter again: they are already in the
binary's symbol table, which this project has been reading from the start.

---

## Status

Nothing to reverse engineer, so nothing to specify. Both files are readable with
any text editor, and the structure above is all there is to say about their
layout.

What is **not** established is the semantics of the numeric fields — the
`(int) 0x30` after the handler, the two `p1b*` / `p2b*` requirement flags, and
what separates a `0` terminator from a `0` that means something. Those are
questions for whoever decompiles the move system, and they now start with a
named map rather than a blank page.
