# The iPad build, 1.2.56

There is a second retail build: `com.ea.umk3.ipad.bv`, version **1.2.56**,
`UIDeviceFamily [2]`, minimum iOS 3.2. It is reputed to have better graphics and
models than the iPhone release. Both halves of that turn out to need unpacking:
its **asset files are byte-identical**, so there are no better models -- yet it
genuinely does look sharper, because it renders the same art to five times as
many pixels. And it carries one thing the iPhone build simply does not have: a
**local two-player duel mode**.

---

## Every asset is byte-identical

All 5,092 files under `res/` were hashed in both builds.

| Category | Files | Identical | Different |
|---|---:|---:|---:|
| `.meshset` `.skin` `.bones` `.skinanim` | 692 | **692** | **0** |
| `.pvr` and `.PNG` | 1,783 | **1,783** | **0** |
| everything else | 2,617 | **2,617** | **0** |

No file exists in one build and not the other. The two `res/` trees are the same
397.3 MB down to the byte.

So there are **no higher-poly models and no higher-resolution texture files**.

### But it does look sharper, and that is not a contradiction

The textures were always 1024x1024. What changes is the screen they land on:

| | pixels |
|---|---:|
| iPhone, 480x320 | 153,600 |
| iPad, 1024x768 | **786,432** |

**Five times as many.** The iPhone was throwing most of every texture away on
the way to the panel; the iPad shows more of what was already in the file. Same
art, far more of it visible — which is exactly what "HD" meant in the marketing,
and why the difference is obvious on screen and invisible to a hash.

Worth being precise about, because the two statements sound contradictory and
are not: **identical files, better output.** For the port it means the ceiling on
visual quality is set by the assets, and the assets are already better than the
iPhone build ever displayed.

### Where the impression probably comes from

Both builds carry 32 `IPAD`-prefixed assets — 27 character portraits and an
`IPAD_GRAVEYARD_LEVEL_SCENE`. The iPad build presumably selects them and the
iPhone build does not.

But measured, they are barely different:

- **The portraits are 128x128 either way**, except for the five whose base is
  64x64 — Endurance, Dummy, and the [hidden tier](HIDDEN-CONTENT.md). So the
  iPad "advantage" is that the hidden characters get a full-resolution portrait.
- **`IPAD_GRAVEYARD_LEVEL_SCENE.meshset` has identical geometry** to the base:
  58 meshes, 2,355 triangles, 3,591 vertices, differing by 305 bytes of names.

Nothing here is worth switching builds for, and both builds contain all of it
already.

---

## Switching would cost the armv6 slice

| | iPhone 1.2.59 | iPad 1.2.56 |
|---|---|---|
| Binary | **FAT: armv6 + armv7** | **thin armv7 only** |
| Size | 5,006,704 | 2,363,680 |
| Symbols / STABS | 29,076 / 21,112 | 29,081 / 21,114 |

The iPad build has **no armv6 slice**. That slice is this project's single most
valuable technical asset: it is the same source compiled without NEON, and it is
the only reason `RenderSkinned.cpp`, the lighting model and the skinning maths
are readable at all — armv7 compiles them to packed NEON that Ghidra
mis-decompiles silently.

The iPad build is also **older**: 1.2.56 against 1.2.59.

**Recommendation: keep the iPhone build as the reference.** Switching gains
nothing measurable and loses the production route.

---

## It has a local two-player mode, and that is the reason to keep it

**An earlier version of this page said it did not. That was wrong**, and the
mistake is worth recording because the reasoning looked sound: every *networked*
multiplayer function is the same size in both builds, so I concluded there was
no extra multiplayer code. But a local two-player mode does not need new
multiplayer functions -- it reuses the fight code with both players on one
device. The difference was never going to be in `FE_Task_Multiplayer`.

Looking in the right place settles it immediately. The **Play menu** has five
entries on iPad and four on iPhone:

| iPhone | iPad |
|---|---|
| `_Touch_PlayArcade` | `_Touch_PlayArcade` |
| — | **`_Touch_Play2P`** |
| `_Touch_PlayMultiPlayer` | `_Touch_PlayMultiPlayer` |
| `_Touch_PlaySK` | `_Touch_PlaySK` |
| `_Touch_PlaySurvival` | `_Touch_PlaySurvival` |

`_Touch_Play2P` and `_LastTouch_Play2P` exist **only** in the iPad build, and
they sit *beside* the networked entry rather than replacing it. In game it is a
**duel mode** — two players on the one device, no network involved.

The code is there too. `_FE_Task_Play` is **2,188 bytes on iPad against 2,048 on
iPhone** -- 140 bytes and 60 instructions more -- and the extra work is exactly
one more menu item:

| Call, from `FE_Task_Play` | iPhone | iPad |
|---|---:|---:|
| `_DrawOptionAsButton` | 4 | **5** |
| `_GameText` | 3 | **4** |
| `_FE_W` | 6 | **7** |

One more button drawn, one more label, one more width query.

### The second player exists in **both** builds

This is the part that matters for the port, and it is better news than "the iPad
has a feature we lack".

Running the mode and reading the game's own telemetry gives it a name:

```
LOGGING (50013): MAIN MENU, PLAY
LOGGING (30038): 2 Players on 1 iPad, 0      <- mode selected
LOGGING (30041): 2 Players on 1 iPad
LOGGING (30042): 2 Players on 1 iPad, 0      <- match
LOGGING (20002): 1, 0                        <- result
```

**"2 Players on 1 iPad."** The engine's own words, and the string sits in the
front-end label table alongside `ROUND 1`…`ROUND 4` and the pause-menu entries.

And the machinery behind it is in the **iPhone binary too**, byte for byte the
same size:

| Symbol | Bytes | In iPhone | In iPad |
|---|---:|---|---|
| `_ButtonStatesP2` | 28 | yes | yes |
| `_LastButtonStatesP2` | 28 | yes | yes |
| `_ButtonsPosP2` | 120 | yes | yes |
| `_ButtonsPosP2_1` / `_2` | 120 each | yes | yes |
| `_ButtonsPos6P2_1` / `_2` | 120 each | yes | yes |
| `_JoystickStateP2` + X/Y | 4 each | yes | yes |
| `_P2Controls` | 4 | yes | yes |
| `_Player2NumButtons` | 4 | yes | yes |
| `_PLAYER2MODEL` | 4 | yes | yes |
| `_Player2Pos` | 12 | yes | yes |
| `_isp2` | **48 (code)** | yes | yes |
| `_gup2` | **368 (code)** | yes | yes |

Seven button states, **five separate button layouts** including two six-button
variants, a joystick with its own position, a controls block, a player model and
two functions. That is a complete second-player input path, and the iPhone build
carries all of it.

**Only `_Touch_Play2P` is missing from the iPhone build.** The mode is fully
implemented there and simply unreachable — no menu entry leads to it.

#### What this means for the port

**Local two-player does not have to be written.** It is already in the code being
decompiled, with a second control layout and a second player model, and it works
— a full match was played through it. The work is exposing it, not building it.

It also means the feature is **not** a reason to prefer the iPad build as a
decompilation source. The iPad binary is the reference for *what the menu should
offer*; the iPhone build, which still has the armv6 slice, remains the source
the code is read from, and it contains the same second-player code.

### Why the search missed it

The first pass searched the binary's own string table for a two-player
vocabulary and found nothing. That was the wrong place twice over: the menu
labels come through `_GameText` rather than sitting as literals, and the
`.lproj` bundles contain only a `dummy.txt`. Absence of a string was taken as
absence of a feature.

**This is the port's most valuable inheritance from the iPad build.** Local
two-player on one machine is far more useful to a PC port than GKSession
peer-to-peer, which needs two devices and an Apple networking stack that will
have to be replaced wholesale. It is now [a stated goal](../README.md).

## What the iPad build *is* good for

**A second fully-symbolised build of the same source.** 29,081 symbols with
21,114 STABS entries, from a different compile of a slightly earlier version.

That makes it usable as a **cross-check**: function boundaries, struct layouts
and call graphs that agree across two independent builds are more trustworthy
than ones seen once.

So it is worth keeping for two reasons -- the two-player menu path above, and
that cross-check. It cannot *replace* the iPhone build, because it has no armv6.

## Running it

`tools/patch_ipa.py` takes a `--build` flag, and the three patches that make the
game reach its menus are expressed **relative to symbols** rather than as
addresses, because every function in the iPad binary is relocated:

```bash
UMK3_IPA_DIR=/path/to/ipas UMK3_APPS_DIR=/path/to/touchHLE_apps   python tools/patch_ipa.py --build ipad     --apply setlocale_nop --apply mp_disable --apply eula_exit
```

The older hardcoded-address patches are **refused** for any build but the iPhone
one; they would silently hit the wrong bytes.

touchHLE runs the result and reports `Device family: iPad`. Its options file
needs its own entry for `com.ea.umk3.ipad.bv`, and the controls have to be
**re-measured rather than copied** -- the iPhone entry's coordinates were taken
against 480x320 and would land in the wrong places on a 1024x768 layout:

```
com.ea.umk3.ipad.bv: --fullscreen --scale-hack=4 --ignore-gl-errors
  --stabilize-virtual-cursor=0.1,8 --deadzone=0.15
  --dpad-to-touch=39,530,212,213  --stick-to-touch=39,530,212,213
  --button-to-touch=X,712,686            (S)
  --button-to-touch=A,834,686            (P)
  --button-to-touch=B,953,686            (K)
  --button-to-touch=LeftShoulder,822,562 (R)
  --button-to-touch=Y,951,562            (B)
  --button-to-touch=Start,990,21         (pause)
```

The on-screen layout matches the iPhone one in *shape* -- three buttons on the
lower row, two above, pause top right, stick bottom left -- so the mapping is
the same idea at 1024x768 coordinates. `--scale-hack=4` renders internally at
4096x3072.
