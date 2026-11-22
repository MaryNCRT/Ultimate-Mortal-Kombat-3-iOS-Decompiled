# Graphics overhaul: audit, findings and architecture (phase 1)

**Status (2026-11-22):** phase 1 only -- an audit of what exists and a
proposed architecture. **No code was changed for it.** Nothing below is
implemented unless the "what exists" section says so with a file name.
Asked for by Mary: four presets (ORIGINAL, ENHANCED GRAPHICS, RTX,
RTX + DLSS 5), a character / environment split, HUD protection, launcher
management of optional components, in-game settings.

Everything here is port work (`Port, not in the binary`). It does not touch
`src/decomp/` logic: the fight engine, input, timing, rollback and the
online sync are out of scope and stay so. The few hooks the renderer needs
already exist as port lines in decompiled files (below).

---

## 1. What exists (audited in the tree)

### 1.1 How the game draws

- The decompiled code draws with **OpenGL ES 1.1 fixed-function calls**, as
  the iPhone did. Every `glX` the game calls is defined in
  `src/runtime/render/rgl_front.c`.
- **OpenGL** (`renderer=opengl`): the calls go straight to the driver.
  Windows creates a **legacy context** (`platform/win32_gl.c`,
  `ChoosePixelFormat` + `wglCreateContext`, no `wglCreateContextAttribsARB`);
  Linux asks SDL for a compatibility profile (`platform/sdl_gl.c`).
- **Vulkan 1.0** (`rgl_vk.c`) and **Direct3D 12** (`rgl_d3d12.c`):
  `rgl_ff.c` (3,500 lines) keeps the fixed-function state, **transforms every
  vertex on the CPU** into clip space and hands the backend `rgl_vtx`
  arrays plus a small state block (`rgl_pipe`, `rgl_frag`). The vertex
  already carries the **world position and normal** (`rgl_vtx.p`, `.n`) and
  the shadow-map coordinate. Fighters are sent indexed (`draw_indexed`).
- Every render target is **RGBA8**; there is no HDR target and no
  float colour buffer anywhere.
- Shaders: `render/shaders/ff.vert`, `ff.frag` (Vulkan GLSL 450),
  `ff.hlsl` (D3D12), compiled offline by `shaders/build.py` (glslang, fxc 5.1
  -- **Shader Model 5.1**) into `rgl_shaders_vk.h` / `rgl_shaders_d3d12.h`.
  Fragment behaviour is one uber-shader switched by push constants
  (`rgl_frag.shadow` modes 0..6).
- **Pipeline caches** compiled before play (`rgl_pipes_note` /
  `umk3-pipelines-*.bin`): no shader compiles during a fight. Must stay so.

### 1.2 ENHANCED LIGHTING (`rt_shadows=1`, Vulkan / D3D12 only)

All in `rgl_ff.c` + `ff.frag` / `ff.hlsl`:

| Feature | State | Where |
|---|---|---|
| Real-time shadow map (fighters + nearby stage, PCF 25 taps, per-caster ids, self-shadow skip) | working, played | `shadow_build`, `light_at` |
| Texture relief (Sobel slopes of the texture as a normal map, mip-aware) | working | `tex_bump`, `relief()` |
| Specular (Blinn-Phong on the relief; "specular map" from luminance/saturation) | working | `specular()` |
| Dynamic lights: point / spot / directional, 16 max, per pixel, linear | working | `lights_build`, `dynamic()` |
| Emissive draws turned into point lights ("every glow a light") | working | `rgl_mesh_fullbright`, `emit_note` |
| Point-light shadows (2 strongest, cube faces in an atlas) | working | `point_shadows` |
| Sky / ground ambient per stage | working | `sky_build`, `rgl_sky_stage` |
| Per-stage lighting files (9 stages) | working | `launcher/lighting/*.ini`, `cfg_load` |
| Mipmaps, anisotropy | working | `ff_glTexImage2D` |
| Ambient occlusion: Crytek SSAO / HBAO / Alchemy, depth pass, edge-aware blur, 5 debug views; fighters a layer of their own | working, played | `ao_passes`, `ao_pass`, `ao_blur`; `docs/research/AMBIENT-OCCLUSION.md` |
| Contact shadows | **research only** | `docs/research/CONTACT-SHADOWS.md` |
| Bloom, tonemapping, colour grading **during play** | **missing** | -- |
| Normal maps from real data | **missing** (the game ships none; relief is derived from the albedo) | -- |
| Any of the above on OpenGL | **missing** (README "Known problems") | -- |

Is the AO visible? Yes on Vulkan / D3D12 with ENHANCED LIGHTING on;
`ao_debug=1..5` shows each stage of it in grey. On OpenGL it does nothing,
silently -- a preset system must say so rather than show a switch that has
no effect.

### 1.3 What the renderer already knows about the frame

This is the most useful finding: the hooks a character / environment / HUD
split needs are **already in the tree**, as port lines.

| Hook | Called from | What it tells the renderer |
|---|---|---|
| `rgl_shadow_camera(1)` | `decomp/lime/LIMEDS_Misc.c:564` (`LIMEDS_SetCameraOrientation`) | the 3D scene's camera starts |
| `rgl_shadow_camera(0)` | `decomp/gamecode/HudAnim.c:261`, `GameCode.c:1607` | the HUD (or a flat 2D effect) takes over |
| `rgl_shadow_caster(1/0)` | `GameCode.c:3828, 5787, 5845` | what is drawn now is a fighter, with **its own id per call** |
| `rgl_shadow_proxy` | smooth_models.c | a fighter's base mesh vs its smoothed mesh |
| `rgl_mesh_name(name)` | `decomp/lime/RenderMesh.c` via `lime_platform.c` | the stage mesh being drawn, by name |
| `rgl_mesh_fullbright` | same | an emissive / self-lit mesh |
| `rgl_sky_stage(name)` | meshset load event | which stage is loaded |

So "World", "Character" (with per-fighter ids), "Environment" (per mesh
name, i.e. floor / walls / props can be told apart by a data file) and
"HUD" are separable **from real renderer information**, not from screen
halves. The AO depth pass already stores a fighter / stage layer bit in
alpha.

### 1.4 Post-processing

PHOTO MODE's effects (`photo_mode.c`, `rgl_post.c`, `rgl_ff_post`,
`post_fx` in the shaders): bloom, depth of field, nine filters, vignette,
grain, chromatic aberration, highlights / shadows -- in **all three
renderers** (OpenGL through its own GLSL 1.20 program). They run only
inside PHOTO MODE. This pass is the natural home for play-time bloom and
tonemapping: reuse it, do not write a second one.

### 1.5 Configuration and UI

- `umk3.ini`, parsed in `game_main.c read_config` (`renderer`, `gpu`,
  `vsync`, `low_latency`, `antialiasing`, `render_width/height`,
  `rt_shadows`, `ao`, `ao_*`, `smooth_models`, ...).
- In game: `settings_menu.c` pages VIDEO (resolution, renderer, VSYNC, LOW
  LATENCY), GRAPHICS (antialiasing, smooth animations / models, graphics
  card, ENHANCED LIGHTING, AMBIENT OCCLUSION), TEXTURES.
- Launcher: `launcher/app/lib/ini.js` (same keys), `SecVideo.svelte`.
- There is **no preset** concept: each switch is independent.

### 1.6 Component detection that already exists (launcher)

- **GPUs:** the launcher runs `umk3-game --list-gpus` (`main.js listNames`,
  IPC `gpus:list`). On Windows the game enumerates DXGI adapters
  (`rgl_d3d12.c rgl_gpu_list`): names, software adapters skipped, sorted by
  dedicated VRAM. **The VRAM is read but not printed**; no vendor id, no
  driver version, no feature levels, no ray tracing tier, no OpenGL
  extension check. On Linux `rgl_gpu_list` is empty (`rgl_front.c:318`),
  although `rgl_vk.c` already enumerates physical devices.
- **VERIFY / REPAIR** (`lib/verify.js`, `lib/build.js`): every shipped
  source checked by its git blob SHA-1 against the launcher's own tag on
  GitHub, a manifest cached in `build/manifest-<version>.json`, missing or
  damaged files downloaded again and re-checked. This is the right
  mechanism to extend to optional graphics components (section 4.6).

### 1.7 Photo mode and replays

- PHOTO MODE freezes with the game's own pause (`GamePaused == 3`); nothing
  of the fight is copied. Captures up to 4K / supersampled
  (`game_main.c photo_capture_size`). A frozen frame with temporal effects
  must not keep accumulating -- relevant to any temporal upscaler.
- Replays seek by **re-simulating silently** (`rollback_probe.c rp_seek`,
  20 ms of ticks a frame). Every seek, step back and speed change is a
  discontinuity that must reset temporal history.

### 1.8 Online

No anti-cheat is used (`grep` finds no EOS Anti-Cheat in `src/` or the
launcher). Online fights force the game's own data except models and
textures. The renderer is not part of the rollback state. An injected
component would not break a check, but a crash in it still ends a ranked
fight, and a third-party DLL in the process is a support burden.

---

## 2. What the external references actually are (read 2026-11-22)

| Reference | What it is | License | Usable here? |
|---|---|---|---|
| Hero88go/melee-unlocked | Melee source port, **D3D12 / D3D11** (no OpenGL); DLSS SR / DLAA through Streamline 2.10.3 with engine motion vectors; "DLSS 5" as a separate model download placed as `nvngx_dlssnr.dll`; an "RTX Pack" DLL built with RTXDI / NRD / SHaRC loaded at run time; RTX listed as "not done" in its own log | **GPL-3.0-or-later** + a GPL-3 §7 exception for NVIDIA SDKs | Architecture ideas only. Its code cannot be copied into this GPL-2.0-or-later tree without relicensing the whole port to GPL-3 |
| jlrouzies-fr/DLSS5-Feeder (and the maddbuzz fork) | A **ReShade add-on** (`dlss5-feed.addon64`) + `DLSS5_Feed.fx`. Builds a DLAA request from colour, depth, motion vectors and an R8 trust mask, then a separate "neural consumer" add-on (RenoDX `renodx-dlss5`, OptiScaler fork, Deep Fried Chicken) runs NVIDIA's `nvngx_dlssnr.dll` ("feature 18"). OpenGL path through GL/D3D12 interop and a helper host process | MIT | Yes as an optional, user-installed layer. Beta; "the HUD is processed along with the scene"; HUD restore is a heuristic pass |
| itsmepixel-rgb/DLSS5-AutoBuilder | Python installer that stages ReShade + feeder + LumeniteFX | MIT | Not needed: the launcher does this job itself |
| umar-afzaal/LumeniteFX | ReShade FX, optical flow from luma, 1/8 resolution RG16F | **AGNYA licence**: no independent hosting, redistribution of modified versions or use in another project without the author's permission | **Cannot be bundled or auto-downloaded** without written permission. Not needed: we have real motion vectors (section 4.3) |
| vortigern11/vort_Shaders | ReShade FX motion effects (motion vectors, TAA, blur) | MIT | Usable, but also not needed |
| crosire/reshade | Injector + add-on API, D3D9-12 / OpenGL / Vulkan | BSD-3-Clause | Can be bundled with its notice. Add-ons load from a configurable search directory |
| NVIDIA Streamline 2.14.16 | DLSS SR / DLAA / RR / FG / Reflex | MIT for its code; the DLSS plugin DLLs under the NVIDIA RTX SDKs licence | D3D11 / D3D12 / Vulkan only. **No OpenGL. No public "DLSS 5" / neural rendering feature in it** |
| NVIDIA DLSS, RTXGI 2 (SHaRC, NRC), RTXDI 3.2, NRD 4.18, RTXPT 1.9 | SDKs / samples | **NVIDIA RTX SDKs licence** | D3D12 / Vulkan only, hardware RT. See 2.1 |

### 2.1 Two blocking licence questions for Mary

1. **NVIDIA SDKs inside a GPL program.** The RTX SDKs licence §4(e) forbids
   using the SDK "in any manner that would cause it to become subject to an
   open source software license". Linking DLSS / NRD / RTXDI into
   `umk3-game` (GPL-2.0-or-later) conflicts with that. Melee Unlocked
   answers it with a GPL-3 §7 additional permission -- which only the
   copyright holders of *all* the code can grant. This tree contains
   Blender's GPL-2.0-or-later subdivision code (`src/thirdparty/blender`),
   whose authors have granted no such exception. **Recommendation: no
   NVIDIA SDK is compiled into the game.** RT is written in-house (it does
   not need the SDKs, section 4.4); DLSS reaches the game only through
   components the player installs, kept in a separate process-loaded
   layer the game does not link (section 4.5). This needs Mary's decision,
   and ideally a lawyer's, before phase 5.
2. **The neural model and runtime.** `nvngx_dlssnr.dll` is not in any
   public NVIDIA SDK. The community gets it from Discord servers and DLSS
   Swapper; the feeder's own installer "fetches both". Redistributing it,
   or downloading it automatically from those channels, is redistribution
   of NVIDIA's proprietary binary without authorisation. **The launcher
   must not ship or fetch it.** It can detect it, verify it, and guide the
   player to place a copy they obtained themselves (section 4.6).

### 2.2 What "DLSS 5" can honestly mean here

- Runs only on **RTX 50** GPUs per the feeder (RTX 20/30/40 fail with
  `0xbad00001` unless a modified DLL is used -- which we will not support).
  Mary's reference RTX 5070 qualifies.
- Neural rendering is a whole-image pass. It takes colour, depth, motion
  vectors and a trust mask; **it does not take object ids or material
  masks**. Character / environment control therefore has to be done by us
  around it (section 4.5), never claimed as an input it reads.
- It is beta software on the community side, with known driver faults
  (616.64). It stays labelled experimental until it has run end to end
  here, with a log line proving the feature was created and evaluated.

---

## 3. Gap list

| Area | Exists | Missing |
|---|---|---|
| Presets | individual switches | `graphics_preset=`, the mapping, "custom" when a switch is moved |
| ENHANCED on OpenGL | photo-mode post only | shadows, relief, lights, AO (the GL path draws through the driver's fixed function, so this means routing GL through `rgl_ff` too -- section 4.2) |
| Bloom / tonemap in play | photo-mode `post_fx` | an HDR (RGBA16F) scene target, play-time use of `post_fx`, exposure |
| Contact shadows | plan | implementation (screen-space march in the AO pass) |
| Motion vectors | none | per-vertex previous clip position in `rgl_ff`, an RG16F target |
| Object / material ids | caster ids, mesh names | an R16UI id target, a per-stage / per-fighter classification file |
| HUD separation | the exact call that starts the HUD | an offscreen scene target and a composite step |
| Hardware RT | none (SM 5.1, Vulkan 1.0) | Vulkan 1.2 + ray query, D3D12 DXR 1.1 / SM 6.5 via DXC, a per-frame acceleration structure |
| GPU detection | names, VRAM (unprinted) | vendor, driver, VRAM, RT tier, Vulkan / GL extension checks, Linux |
| Optional components | VERIFY / REPAIR of the sources | a component manifest, user-supplied file import, removal |

---

## 4. Proposed architecture

### 4.1 Presets

One key, `graphics_preset=original|enhanced|rtx|rtx_dlss5|custom`, read in
`read_config`, which **sets the existing switches** -- no parallel system.
Moving any switch by hand turns the preset into `custom`. The menu shows
only presets the machine can run (section 4.6's capability probe); RESET
puts `original` back.

| Preset | Sets |
|---|---|
| ORIGINAL | `rt_shadows=0 ao=0`, no post, any renderer |
| ENHANCED | `rt_shadows=1`, `ao=2`, contact shadows, bloom + tonemap; Vulkan or D3D12 (OpenGL once 4.2 lands); NVIDIA / AMD / Intel |
| RTX | ENHANCED + RT shadows / AO / reflections / one-bounce GI; Vulkan 1.2 or D3D12 with hardware RT (NVIDIA RTX, AMD RDNA2+, Intel Arc) -- "RTX" is the name in the menu, the code is vendor-neutral |
| RTX + DLSS 5 | RTX + the neural layer; D3D12, NVIDIA RTX 50, components present and verified |

### 4.2 ENHANCED GRAPHICS consolidation (phase 2)

- Play-time bloom and ACES-style tonemap through `rgl_ff_post` /
  `rgl_post.c`, with the scene drawn into **RGBA16F** so highlights exist to
  bloom. The HUD is drawn after the pass, as photos already do.
- Contact shadows in the AO pass, per `CONTACT-SHADOWS.md`, multiplied
  into the sun's shadow term only.
- A metal / skin / cloth classification **by texture name**, in data files
  beside `launcher/lighting/` (the game has no material data; this is an
  artist mapping, said so in the menu's help).
- OpenGL: the honest choice is to route OpenGL draws through `rgl_ff` as
  well, with a small GL 3.3 backend, so one shader set serves all three.
  The current direct-GL path stays as `renderer=opengl_legacy` for
  drivers without GL 3.3. This is a larger change than the rest of the
  phase and is its own step.

### 4.3 Motion vectors, depth and ids (phases 6-7, needed by RTX too)

`rgl_ff` already has every vertex's clip position. It keeps, per **draw
identity** -- fighter caster id + mesh + vertex index for fighters, mesh
name + draw order for the stage -- last frame's clip positions, and emits
`prev` beside `pos`. A second colour attachment (RG16F) takes
`(pos - prev)` in pixels; a third (R16UI) the id:
`0 HUD / unknown, 1..15 fighters (caster id), 16+ stage meshes` (a hash of
the mesh name, classified by the stage's data file as floor / wall / prop /
light / animated). These are **real** vectors from the geometry, not
optical flow: no LumeniteFX needed.

Reset rules (history cleared, vectors zero, the neural pass told to drop
its history): the first frame after a load, a camera cut, PHOTO MODE
entering / leaving, any replay seek / step back / speed change, the SMOOTH
ANIMATIONS interpolated frame is the frame (vectors from the drawn pose).

### 4.4 RTX (phase 3)

No NVIDIA SDK is needed for the scale of this game (a few tens of
thousands of triangles a frame):

- **One acceleration structure per frame**, built from the world-space
  triangles `rgl_ff` already has (`rgl_vtx.p`), split into a static BLAS
  for the stage (rebuilt on stage load) and a dynamic BLAS for the fighters
  (refit / rebuilt every frame).
- **Inline ray queries** (`VK_KHR_ray_query` on Vulkan 1.2; DXR 1.1
  `RayQuery` on D3D12 with SM 6.5 compiled by DXC) in the existing uber
  fragment shader: RT shadows for the sun and the strongest point lights,
  RT AO (a few short rays, replacing SSAO when on), mirror / glossy
  reflections on surfaces the classification marks as floor / metal /
  water, one-bounce diffuse GI from the dynamic lights.
- Denoising: a small spatio-temporal filter of our own, reusing the AO
  blur's depth / layer weights plus the motion vectors. NRD only if the
  licence question in 2.1 is settled in its favour.
- RTXGI's SHaRC / NRC, RTXDI and RTXPT are path-tracer infrastructure
  (hash-grid radiance caches, ReSTIR for thousands of lights). With 16
  lights and a box-shaped 2.5D stage they are more than the game needs;
  they are studied, not adopted.

### 4.5 RTX + DLSS 5 (phases 4, 5, 7)

**Backend: Direct3D 12, not OpenGL.** The game already has a D3D12
renderer; the feeder works natively on D3D12. The GL/D3D12 interop route
(`GL_EXT_memory_object_win32`, `GL_EXT_semaphore_win32`, a helper
process) exists for games that have nothing else -- here it would only add
a legacy-context limitation and a cross-process fence. OpenGL stays the
compatibility backend.

Frame order with the preset on:

```
scene (RGBA16F) + depth + motion vectors + ids
  -> RT / lighting / post (bloom, tonemap)          -> scene_ldr
  -> copy scene_ldr -> original
  -> neural pass (the feeder + consumer, via ReShade's add-on runtime)
  -> layer blend: mix(original, neural, w[id])      CHARACTER / ENVIRONMENT
  -> HUD drawn on top, never processed              HUD PROTECTION
  -> present
```

- **HUD protection** follows from drawing the HUD after the neural pass --
  the split point is `rgl_shadow_camera(0)` in HudAnim.c. This requires
  ReShade's effects to run at a point the game chooses rather than at
  present (ReShade's API exposes `effect_runtime::render_effects` for
  applications that are their own add-on). **Phase 4 must prove this works
  with the feeder before anything else is built on it.** If it does not,
  the fallback is the feeder's trust mask set to 1 over HUD pixels plus a
  HUD re-composite after present, which is weaker and will be called so.
- **Character / environment control** is the `w[id]` blend above (Neural
  Intensity per layer), plus the R8 trust mask the feeder does accept. It
  is never presented as "DLSS 5 receives our object ids".
- **Isolation:** ReShade and the add-ons live in `graphics/neural/`, loaded
  explicitly by the game with a fixed add-on path and an empty effect
  search path except our own `.fx`, never as a proxy `dxgi.dll` /
  `opengl32.dll` in the game folder, never picking up a player's other
  ReShade add-ons. Off by default in online fights until tested
  (section 4.7).

### 4.6 Launcher: components (phase 8)

Extend what exists instead of adding a second system:

- **Capability probe:** `umk3-game --gpu-caps` printing, per adapter,
  `name, vendor id, VRAM, driver version, D3D12 feature level, DXR tier,
  Vulkan version, VK_KHR_ray_query, GL version, GL_EXT_memory_object_win32`.
  `--list-gpus` stays for old launchers. Linux through Vulkan.
- **Component manifest** (`graphics/components.json` in the repository,
  checked by VERIFY): for each component its version, source URL **on the
  original project's GitHub releases only**, SHA-256, licence, and whether
  it may be downloaded (`fetch`) or must be supplied (`user`).
  - `fetch`: ReShade (BSD-3, from reshade.me's official release), the
    feeder (MIT, its GitHub release) -- verified by SHA-256 pinned in the
    manifest, unpacked into `graphics/neural/`, never over an existing
    third-party file without a backup.
  - `user`: `nvngx_dlss.dll`, `nvngx_dlssnr.dll`, the consumer add-on if
    its licence forbids redistribution. The launcher offers "Select file",
    checks the Authenticode signature (NVIDIA Corporation) and version,
    copies it into `graphics/neural/`. It does not download them.
- REPAIR re-checks hashes; REMOVE deletes `graphics/neural/` and sets the
  preset back to RTX; settings in `umk3.ini` are kept.

### 4.7 Online

The renderer is not in the rollback state, so presets cannot desync a
fight. Still: the neural layer (third-party code in the process) stays off
in online fights until it has been played online between two machines;
ORIGINAL / ENHANCED / RTX are first-party code and allowed.

---

## 5. Phases and what "done" means for each

| Phase | Work | Proof required |
|---|---|---|
| 1 | this audit | -- |
| 2 | presets, bloom + tonemap in play, contact shadows, material files | screenshots ORIGINAL vs ENHANCED on 3 stages, frame times logged, OpenGL says what it cannot do |
| 2b | OpenGL through `rgl_ff` (GL 3.3 backend) | `tests/rgl_test.c` on GL, the same screenshots on GL |
| 3 | Vulkan 1.2 / DXC, acceleration structures, RT shadows / AO / reflections / GI, denoiser | screenshots, frame times on the RTX 5070, AMD / Intel fallback to ENHANCED |
| 4 | **spike, outside the main build**: the feeder on our D3D12 renderer, then effects run before the HUD | a log line "feature 18 created / evaluated"; a screenshot pair. Go / no-go for 5-7 |
| 5 | ReShade + feeder loaded from `graphics/neural/`, isolated | no foreign add-ons load; game runs with the folder absent |
| 6 | motion vectors, depth, id targets, reset rules | debug views of each; a replay seek with no ghosting |
| 7 | layer blend, HUD after the neural pass | HUD pixels identical with the pass on and off (image diff) |
| 8 | `--gpu-caps`, manifest, user-supplied import, repair / remove | launcher tests in `launcher/app/test` |
| 9 | menu: presets + advanced options, only the supported ones shown | Mary plays it |
| 10 | measurements, docs | README / PROGRESS / HANDOFF |

Each phase on its own branch, released only after Mary has played it.
