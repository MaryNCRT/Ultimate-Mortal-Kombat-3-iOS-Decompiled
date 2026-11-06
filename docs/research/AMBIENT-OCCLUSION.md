# Screen-space ambient occlusion: study and plan

**Status (2026-11-06):** phases 1-4 are in (`rgl_ff.c` `ao_passes`, the
three techniques in the shaders, the menu row and the `ao*` keys), Vulkan
and Direct3D 12. Normals come from the depth's derivatives; GTAO and
OpenGL are still plan. The fighters enter the depth pass from their base
meshes, not their smoothed ones, for speed.

Research only. Nothing here is implemented yet; no game code was changed
for it. Asked for by Mary on 2026-11-06, as a companion to
`DYNAMIC-LIGHTING.md` (dynamic point and spot lights, being built on
`claude/dynamic-lighting`) and `CONTACT-SHADOWS.md`.

The reference studied in depth is a local copy of
**shabaazxh/Real-Time-Ambient-Occlusion** (`WORK/Real-Time-Ambient-Occlusion-master`,
not in the repo): a C++ Vulkan renderer written for a bachelor thesis that
compares Crytek SSAO, HBAO and Alchemy AO on Sponza.

| Reference | License | Use here |
|---|---|---|
| shabaazxh/Real-Time-Ambient-Occlusion | **none**: no LICENSE file in the copy, none stated in the README | **ideas only.** With no license, all rights are reserved; not one line of it can be copied into this GPL-2.0-or-later tree. Its shaders also say they are built from other people's code (LearnOpenGL, jhk2/glsandbox, NVIDIA's Kubisch 2015, ZaOniRinku, Timurson), each with its own terms, so even a license from the author would not cover everything |
| myemural/VulkanCppExamples | MIT | SSAO, HBAO and GTAO examples; reusable with attribution if ever needed |
| HiveEngine "screen-space lighting" page | engine's own not stated; its GTAO says it is written after Intel XeGTAO (**MIT**) | measured costs, a history-free denoiser, parameter ranges |
| Intel XeGTAO (Jimenez et al. 2016, "Practical Real-Time Strategies for Accurate Indirect Occlusion") | MIT | the reference for a fourth, better technique later (§6, phase 5) |
| The papers: Mittring 2007 (Crytek), Bavoil & Sainz 2008 (HBAO), McGuire et al. 2011 (Alchemy AO) | papers | the algorithms themselves, which are free to implement |

**Bottom line:** we write our own shaders from the papers. The reference
repo is useful for its comparison and its pitfalls (§2.4), not as code.

---

## 1. The renderer as it is

(Fuller description in `DYNAMIC-LIGHTING.md` §1; what matters for AO here.)

**Draw path.** The decompiled code draws with OpenGL ES 1.1 fixed-function
calls. `src/runtime/render/rgl_front.c` sends them either straight to the
driver's fixed function (**OpenGL**: no shaders, nothing per pixel), or to
`rgl_ff.c`, which keeps the GL state, transforms vertices to clip space on
the CPU and hands each draw to `rgl_vk.c` / `rgl_d3d12.c`. One shader pair:
`shaders/ff.vert`, `ff.frag` (SPIR-V) and `ff.hlsl` (DXBC), built into
`rgl_shaders_*.h` by `shaders/build.py`.

**There is no depth texture and no normal buffer.** Depth exists only as an
attachment (`D24_UNORM_S8_UINT` on D3D12, the first of D24S8 / X8D24 /
D32F / D16 the device takes on Vulkan) created without
`VK_IMAGE_USAGE_SAMPLED_BIT` and without a typeless format on D3D12, so no
shader can read it. The only readable depth in the renderer is the shadow
map: an RGBA8 colour target with depth packed into RGB and a caster id in A,
written by `ff.frag` in `RGL_SHADOW_DEPTH` mode. That packing already works
on both backends and is the cheapest way to get a readable depth (§4.1).

**The scene is held back.** With ENHANCED LIGHTING (`rt_shadows=1`) every
3D draw under the scene camera is held (`hold_draw`, vertices in world
space) until something else needs the backend; `scene_flush` then draws
the 4096² shadow map from the held casters and replays the held draws. This
is the hook AO needs: **all the opaque geometry of a frame is in hand before
any of it reaches the screen**, so a depth pre-pass costs no extra game
work and no change to the decompiled code.

**The frame target.** The game's area is drawn into a framebuffer object at
the 3D resolution (`render_width`/`render_height`), multisampled when
`antialiasing=2..16`, then resolved and scaled to the window
(`game_main.c`). AO must work at that resolution, not the window's.

**Lighting there is to occlude** (`docs/formats/LIGHTING.md`):

- **Fighters**: `LightVert` on the CPU, two directional lights with a power
  falloff, **no ambient term at all** (the accumulator starts at `0.0f`).
- **Stages**: lighting **baked** into vertex colours from `.lighting`
  files; at draw time `CreateFadedRGBS` adds the stage's ambient.
- Per pixel: the texture (relief and specular derived from it, ENHANCED
  LIGHTING), the shadow map's darkening.

So there is no separate ambient term in the shader to multiply by AO, as a
textbook deferred renderer has. AO has to be applied to the final colour
of a 3D draw, weakened and floored (§4.3). Once the dynamic lights land,
it applies to the base (baked / LightVert) colour and not to the added
dynamic light, which is the physically right split.

---

## 2. The reference: Real-Time-Ambient-Occlusion

### 2.1 Architecture

A plain Vulkan 1.2 renderer (GLFW, GLM, tinyobjloader, Dear ImGui), one
class per Vulkan object (`Device/`, `Swapchain/`, `Renderpass/`,
`Framebuffer/`, `Pipelines/`, `Descriptors/`, `Commands/`, `Resources/`).
`Renderer::RecordCommandBuffer` records, every frame:

1. **Shadow pass**: Sponza from the light into a 2048² depth image.
2. **Forward pass**: Sponza with its textures into a colour target.
3. **G-buffer pass**: view-space position and view-space normal into two
   `R32G32B32A32_SFLOAT` targets (32 bytes a pixel), plus depth.
4. **AO pass**: a full-screen quad into an `R16G16B16A16_SFLOAT` target. The
   technique is chosen by **binding one of three pipelines** that share one
   layout and one descriptor set (`ssaoPipeline`, `hbaoPipeline`,
   `alchchemyPipeline`); the parameters come from one UBO
   (`SSAOubo`: a 64-vector kernel, radius, HBAO directions/step size/step
   count/strength, Alchemy sigma/kappa, time).
5. **Blur pass**: a 4×4 box blur into another target.
6. **Lighting pass**: Blinn-Phong from the G-buffer, a single-tap shadow
   test, ambient = `0.3 × AO`.
7. **Present quad**.

The selector-by-pipeline idea is the part worth keeping: one layout, one
parameter block, one pipeline per technique (§4.4). Everything is full
resolution; there is no resolution scale. The SPIR-V is built offline by
`glslc` (`Shaders/compile.bat`).

The bindings are: 0 the UBO, 1 a 4×4 `R32G32B32A32_SFLOAT` noise texture
(random XY rotations, `GenerateSSAONoise`), 2 the view-space position
target, 3 the **depth attachment itself, sampled** in
`DEPTH_STENCIL_READ_ONLY_OPTIMAL` (the thing our backends cannot do yet),
4 the camera matrices.

### 2.2 The three techniques, as the repo writes them

| | Crytek SSAO (`SSAO.frag`) | HBAO (`HBAO.frag`) | Alchemy AO (`AlchemyAO.frag`) |
|---|---|---|---|
| Paper | Mittring 2007 | Bavoil & Sainz 2008 | McGuire et al. 2011 |
| Idea | count random points of a ball around the pixel that are behind the depth buffer | march N directions across the depth buffer, keep the highest horizon angle, occlusion = sin(horizon) − sin(tangent) | an estimator of the obscurance integral: sum of max(0, n·v + β)/(v·v + ε) over disc samples |
| Depth | view position from depth × inverse projection; sample depth from the **position** target | depth only, unprojected per step | depth only, unprojected per sample |
| Normals | from depth: `cross(dFdx(p), dFdy(p))`, faceted | same, then `y` flipped | same |
| Sampling | 64-vector kernel (a sphere, not a hemisphere, as Crytek did; lengths lerped towards the centre), reflected by a 4×4 noise vector, through a TBN | `hbaoSampleDirection` directions × `hbaoNumberOfSteps` steps of a fixed **UV** size, rotated and jittered by the 4×4 noise | `sample_amount` points on a disc from a per-pixel `sin` hash, re-seeded with `time` **every frame** |
| Range test | hard step (`< radius ? 1 : 0`) | length(diff) < radius, attenuation 1 − (d/R)² | Heaviside step |
| Output | 1 − hits / N | mean over directions of 1 − occlusion × strength | max(0, 1 − (2σ/N · Σ)^κ) |
| Controls (ImGui) | samples 0-64, radius 0-2 | radius, directions 0-20, step 0-0.08, steps 0-20, strength 0-5 | radius, samples 0-64, σ (intensity) 0-2, κ (contrast) 0-5 |
| Filter | 4×4 box blur (shared) | same | same |

### 2.3 How they look (the repo's own captures, `Pictures/`)

- **Crytek SSAO**: the strongest and flattest. Because the kernel is a full
  sphere, a flat floor or wall has half its samples below the surface and
  comes out mid-grey everywhere; creases are only a little darker than open
  surfaces. This is the well-known "grey wash" of first-generation SSAO,
  and it would darken the game's stages uniformly, which is exactly the
  over-darkening Mary does not want.
- **HBAO**: the cleanest. Open surfaces stay white; the darkening is where
  geometry actually meets (column bases, curtain folds, under ledges),
  with a soft falloff. The closest of the three to how AO should read in a
  fighting game: under the fighters' feet, in the stage's corners.
- **Alchemy AO**: contact darkening as good as HBAO's, slightly
  stronger in creases, but **grainy**: the per-pixel hash is re-seeded with
  time each frame, and a 4×4 box blur cannot hide it, so it shimmers in
  motion. Fine with a temporal filter, which we do not have.

### 2.4 Pitfalls found in the code (why it is a study, not a source)

None of this matters to the thesis's captures, but each would show in a
game:

- `inverse(camera.proj)` is computed **per pixel, per sample** (Alchemy
  calls it twice per sample, HBAO per step): a matrix inverse in the inner
  loop. Ours goes in the parameter block once a frame.
- Normals from `dFdx/dFdy` of position are **faceted** and wrong at
  every depth edge (the derivative straddles two surfaces), which shows as
  a halo around silhouettes. The fighters' silhouettes against the stage are
  the most visible edges of this game.
- Alchemy's `depthToNormal` drops the perspective divide
  (`viewSpace.xyz / viewSpace.w;` is a statement with no effect), and its
  Heaviside compares `length(dot(V,V))`, a **squared** distance, with the
  radius.
- HBAO's step size is in **UV units**, not a world radius projected to the
  screen, so its reach changes with distance from the camera and with the
  resolution; its noise scale is hard-coded for 1920×1080; attenuation
  uses only the last sample in range.
- Crytek's range check is a hard step (visible rings at the radius) and it
  reads sample depth from the position target and the centre from depth.
- The blur is a 4×4 box from −2 to +1: **half a texel off centre**, and
  not depth-aware, so AO bleeds across silhouettes (a dark halo round a
  fighter on a bright floor).
- 64 bytes a pixel of G-buffer in RGBA32F is far more than AO needs.

### 2.5 What can be reused, and what needs adapting

| Part | Verdict |
|---|---|
| Any code, shader or C++ | **Not reusable as code** (no license; mixed provenance) |
| The three-technique comparison, the UI controls, the defaults ranges | reusable as **knowledge**: they shaped §4.4 |
| One pipeline per technique, a shared layout and parameter block | the **design** carries over, written in our style for both backends |
| Kernel generation (sphere vs hemisphere, lengths lerped inward) | the **algorithm** (from Mittring / LearnOpenGL) we implement ourselves, as a hemisphere |
| 4×4 rotation noise | replaced by interleaved gradient noise computed in the shader (no texture, no binding, stable on both backends) |
| Sampling the depth attachment | **does not carry over**: our depth is not sampleable; we write a linear depth target (§4.1) |
| Box blur | replaced by a depth-aware separable or 4×4 bilateral blur |
| Vulkan specifics (render passes, image layouts, descriptor sets) | **do not carry over** to D3D12 or OpenGL at all, and our Vulkan backend has its own pass and layout handling (`get_pass`, `to_layout`) |

---

## 3. Compatibility with our three backends

| Need | Vulkan (`rgl_vk.c`) | Direct3D 12 (`rgl_d3d12.c`) | OpenGL (`rgl_front.c` → driver) |
|---|---|---|---|
| A readable depth | write linear depth to a colour target (RGBA8 packed, as the shadow map, or R16F/R32F with a new format flag); sampling the real depth needs `SAMPLED_BIT`, a depth-only aspect view and layout changes | same choice; sampling the real depth needs an `R24G8_TYPELESS` resource with a DSV and an SRV, more work than a colour target | **impossible today**: the fixed-function path has no shaders. Needs the GL 3.3 backend behind `rgl_ff.c` proposed for ENHANCED LIGHTING (`DYNAMIC-LIGHTING.md` phase 6) |
| Full-screen passes | a draw of a big triangle with its own pipeline; our pipelines are keyed by `rgl_pipe` and use `ff.vert/ff.frag`, so AO pipelines are a second family | same, a second root signature or the same with more slots | GL 3.3 backend |
| A third texture in the scene shader (the AO result) | binding 2 in set 0 | `t2`, one more descriptor in the table | GL 3.3 backend |
| Screen position of a pixel in the scene shader | `gl_FragCoord` minus the draw's viewport (push constant) | `SV_Position` minus viewport (root constant) | GL 3.3 backend |
| Derivatives, loops, `textureLod` | SPIR-V from glslang, as now | DXBC SM 5.1 from fxc, as now | GLSL 330 |
| Compute shaders (optional) | yes | yes | GL 4.3 only: **avoid**; everything below is fragment shaders so the GL 3.3 backend can share it |

Conclusions:

- **Vulkan code does not carry over** to D3D12; the backend interface is
  the shared layer. AO must be expressed as **new `rgl_backend` calls**
  that both backends implement, with the algorithm in `rgl_ff.c` and the
  shaders (as `ff.frag` and `ff.hlsl` are now, kept in step by hand).
- **OpenGL gets AO only with the GL 3.3 backend.** Until then, the option
  is greyed out with OpenGL, as ENHANCED LIGHTING is today, and
  `rgl_ao_active()` returns 0.
- Everything uses **fragment shaders and colour targets only**: the formats
  and features already proven on both backends (RGBA8 targets made
  readable, as the shadow map). No compute, no depth sampling, no UAVs.

---

## 4. Proposed design: a modular AO system

### 4.1 Passes, inside `scene_flush`

`scene_flush` today: shadow map → replay held draws. With AO:

1. **Shadow map**, unchanged.
2. **Depth pre-pass** (new, ~0.1-0.3 ms): the held **opaque, depth-writing**
   draws (the same test the shadow casters use: no blend, depth write on;
   alpha-tested ones with their texture so a fence's holes do not occlude)
   drawn into an **AO depth target** at the AO resolution, one sample, with
   a new fragment mode `RGL_SHADOW_DEPTH`'s sibling that writes **linear
   view depth** packed in RGBA8 (the same pack/unpack code as the shadow
   map, so no new format). Later, when the dynamic-lighting work gives each
   vertex a world normal, the pre-pass can also write an octahedral normal
   into a second RGBA8 target; until then normals are rebuilt from depth
   (with the edge-aware 3-tap reconstruction, not raw `dFdx`).
3. **AO pass** (new): one full-screen draw with the chosen technique's
   pipeline, reading the AO depth (and normal), writing **AO** (R in an
   RGBA8 target; G free for the contact-shadow term, `CONTACT-SHADOWS.md`).
4. **Blur** (new, optional): two passes of a depth-aware separable blur,
   or one 4×4 bilateral pass (HiveEngine's cheaper choice) and an
   **upsample** if the AO runs below full resolution.
5. **Replay held draws**, as now, with the AO target bound as a third
   texture; each 3D receiver reads AO at its screen position.

The HUD, 2D, the menus and everything not under the scene camera is not
held, never reaches the pre-pass, and is never darkened. Blended draws
(blood, fire, smoke, glass) are not in the pre-pass and **do not receive**
AO (they would show the surface behind them), which is how every engine
treats them.

### 4.2 Techniques and the selector

`umk3.ini ao=0..3` (`0` Disabled, `1` Crytek SSAO, `2` HBAO, `3` Alchemy AO),
one pipeline per technique, one shared parameter block. Written from the
papers, with the reference's pitfalls fixed:

- **Crytek SSAO**: a **normal-oriented hemisphere** kernel (16-32 points,
  lengths lerped inward), rotated per pixel by interleaved gradient noise,
  a smooth range check. Kept for its classic look and as the cheapest.
  With a hemisphere instead of Crytek's sphere it loses the grey wash.
- **HBAO** (default when AO is on): 4-8 directions × 4-6 steps, the world
  radius projected to pixels (so the reach is the same near and far),
  per-step distance falloff, a tangent bias, jittered start. The best
  quality per millisecond of the three, and the cleanest look (§2.3).
- **Alchemy AO**: the obscurance estimator with σ (intensity), κ
  (contrast) and β (bias), disc samples on a spiral with a **fixed**
  per-pixel rotation (no time seed, so no shimmer without a temporal
  filter), and the projected radius.

A fourth, **GTAO** (XeGTAO, MIT), is the natural upgrade later (phase 5):
it is HBAO done right (cosine-weighted, slices), and the HiveEngine figures
show it at ~0.7 ms at 1080p.

### 4.3 How AO is applied (no over-darkening)

The final colour of a 3D receiver, in `ff.frag` / `ff.hlsl`:

    ao   = texture(aomap, screen_uv).r                // 1 open, 0 fully occluded
    ao   = mix(1.0, ao, intensity * draw_strength)    // strength per draw class
    ao   = max(ao, floor)                             // never below, e.g. 0.55
    base = base * ao                                  // baked / LightVert colour
    out  = base + dynamic_light                       // when DYNAMIC-LIGHTING lands

- **Only the base colour is occluded**, never the added dynamic lights or
  specular: AO is an ambient term. Until the dynamic lights land, the base
  colour is all there is.
- `draw_strength`: stages 1.0; **fighters 0.5** by default (no ambient in
  `LightVert`, light painted into their textures; full AO on them reads as
  dirt), per-fighter override later with the per-fighter lighting file
  `DYNAMIC-LIGHTING.md` proposes. Fullbright textures
  (`IsTextureFullBright`: ICE fighters, blood, acid) get **0**.
- **Floor** (`ao_floor`, default 0.55): the darkest AO can make anything,
  so a corner never goes black, and the original look stays the floor of
  what you see.
- **Shadow and AO do not stack**: where the shadow map already darkens,
  AO is reduced: `ao = mix(ao, 1, shadow_amount * 0.5)`, so a crease in
  shadow is not black twice.
- **Distance fade**: AO fades out towards the far plane and at the screen's
  edges (where its samples leave the screen), so the backdrop is untouched.

### 4.4 Controls

`umk3.ini`, the settings menu (`settings_menu.c`) and the launcher's video
section (`SecVideo.svelte`, `i18n.js`, `lib/ini.js`), all under ENHANCED
LIGHTING:

| Key | Range | Default | Meaning |
|---|---|---|---|
| `ao` | 0 Disabled, 1 Crytek SSAO, 2 HBAO, 3 Alchemy AO | 0 | technique (one pipeline each) |
| `ao_radius` | 0.1-3.0 world units | 0.5 | reach of the occlusion; fighters are ~2 units tall in the scene camera's space (to be measured before setting the default) |
| `ao_intensity` | 0-2 | 1.0 | strength (σ for Alchemy, the multiplier for the others) |
| `ao_bias` | 0-0.2 | 0.03 | depth / tangent bias against self-occlusion on flat ground |
| `ao_samples` | 4, 8, 16, 32 | 16 | Crytek and Alchemy: points; HBAO: directions × steps from it (8 → 4×2, 16 → 4×4, 32 → 8×4) |
| `ao_blur` | 0 off, 1 bilateral 4×4, 2 separable 2×7 | 1 | the filter |
| `ao_scale` | 0.5, 0.75, 1.0 | 0.5 | AO resolution relative to the 3D resolution, with depth-aware upsampling |
| `ao_floor` | 0.3-1.0 | 0.55 | the darkest AO can go (§4.3) |

The menu shows the technique and three presets (Low: 8 samples, ½ res;
Medium: 16, ½; High: 32, full) with the individual keys for whoever edits
the ini, as `antialiasing=` and the shadow options are now.

### 4.5 What it must not touch

- **No decompiled code changes.** AO lives entirely in `rgl_ff.c`, the
  backends and the shaders; the game's draws are the same calls they are.
- **Models, animations, gameplay**: untouched. AO reads the depth of what is
  drawn; it moves nothing and changes no timing (it runs in `scene_flush`,
  on the render side of the frame).
- **The existing lighting**: `LightVert`, the baked stage colours, the
  shadow map, relief and specular stay as they are; AO is one multiplier
  more, off by default, and `ao=0` makes no pass and binds a 1×1 white
  texture, so the frame is bit-for-bit what it is now.

---

## 5. Files involved

| File | Change |
|---|---|
| `src/runtime/render/rgl.h` | `rgl_ao` settings, `rgl_ao_active()`; backend calls: a full-screen `pass(pipeline, inputs[], target, params)` and a third texture for `draw` (or a `screen_maps(ao_target)` call as `shadow_map` is) |
| `src/runtime/render/rgl_ff.c` | in `scene_flush`: the depth pre-pass from the held opaque draws, the AO and blur passes, the parameter block (inverse projection once a frame), the per-draw strength (fighter / stage / fullbright) |
| `src/runtime/render/rgl_vk.c`, `rgl_d3d12.c` | AO targets (RGBA8, scaled), a second pipeline family for full-screen passes, binding 2 / `t2` in the scene pipeline, the viewport in the push / root constants |
| `src/runtime/render/shaders/ao.vert`, `ao_crytek.frag`, `ao_hbao.frag`, `ao_alchemy.frag`, `ao_blur.frag` and their `.hlsl` (new) | the techniques |
| `src/runtime/render/shaders/ff.frag`, `ff.hlsl` | the linear-depth pre-pass mode, the AO read and the §4.3 application |
| `src/runtime/render/shaders/build.py` | the new shaders into `rgl_shaders_*.h` |
| `src/runtime/game_main.c`, `settings_menu.c`, launcher `SecVideo.svelte`, `i18n.js`, `lib/ini.js` | the `ao*` keys |
| `tests/rgl_test.c` | an AO scene (a box on a floor) on Vulkan and D3D12: corner darker than the open floor, open floor unchanged, HUD unchanged |
| `docs/formats/LIGHTING.md`, README, PROGRESS, HANDOFF, CHANGELOG | as each phase lands |

**Dependencies**: none new at run time. Build time: glslang
(`WORK/tools/glslang`) and fxc (Windows SDK), as now. Vulkan 1.0 and D3D12
feature level 11 are enough (fragment shaders, RGBA8 targets, derivatives).
OpenGL needs the GL 3.3 backend.

**Coordination with `claude/dynamic-lighting`**: both change `rgl_vtx`,
the push constants and `ff.frag` / `ff.hlsl`. AO should start **after**
that branch is merged, take its world normals for the pre-pass, and apply
AO to the base colour before its `rgl_light_block` lights are added.

---

## 6. Performance

Baseline (`docs/PERFORMANCE-AUDIT.md`): a fight is dominated by SMOOTH
MODELS on the CPU (6-11 ms a frame); the GPU presents in 2-3 ms; the shadow
map and held draws take a few ms.

| Piece | Estimated cost (mid-range GPU, 1080p 3D resolution) | Note |
|---|---|---|
| Depth pre-pass | 0.1-0.3 ms GPU, ~0 CPU | the held draws again, colour only, one sample; vertices already transformed and uploaded |
| Crytek SSAO, 16 samples, ½ res | 0.2-0.4 ms | one tap a sample |
| HBAO, 4×4, ½ res | 0.3-0.5 ms | 16 taps; HiveEngine measures GTAO (4 slices × 3 steps, full res, 4× MSAA) at 0.63-0.71 ms on an RTX 5060 Ti |
| Alchemy AO, 16 samples, ½ res | 0.3-0.5 ms | |
| Bilateral blur + upsample | 0.1-0.2 ms | HiveEngine: 0.10-0.14 ms |
| Scene shader AO read | < 0.05 ms | one tap a pixel |
| 4K 3D resolution | ×3-4 | ½ res keeps it near the 1080p full-res cost |
| Memory | 1080p: depth + AO, RGBA8, ½ res: ~2 MB; full: ~16 MB | the shadow map is 64 MB |

Total at Medium: **~0.7-1.2 ms GPU**, nothing on the CPU, which is where the
game is bound. Low (8 samples, ½ res) about half.

---

## 7. Phased integration plan

| Phase | What | Done when |
|---|---|---|
| 0 | Measure the scene camera's units (a fighter's height, the floor's distance), so `ao_radius` and the bias have real defaults | numbers recorded in `docs/formats/LIGHTING.md` |
| 1 | Backend groundwork, both backends: full-screen pass call, scaled RGBA8 targets, third scene texture, viewport in the constants. `ao=0` binds white | `rgl_test` passes unchanged on all three renderers; frame identical with `ao=0` |
| 2 | Depth pre-pass from the held opaque draws (linear depth packed RGBA8), debug view `ao_debug=1` showing it | depth view correct on Vulkan and D3D12, fighters and stage in it, HUD not |
| 3 | HBAO + bilateral blur + upsample + §4.3 application, fighter / stage / fullbright strengths, floor, distance fade | Mary's test build: corners and feet darker, open floor and HUD untouched, no halo round fighters |
| 4 | Crytek SSAO and Alchemy AO pipelines, the selector, menu and launcher controls, presets | all four selector values work on both backends |
| 5 | Optional: GTAO (after XeGTAO, MIT, with attribution); normals from the dynamic-lighting vertices instead of from depth | quality pass |
| 6 | OpenGL: comes with the GL 3.3 backend (`DYNAMIC-LIGHTING.md` phase 6); the AO shaders compiled as GLSL 330 | `rgl_test` AO scene passes on OpenGL |

Each phase: `tests/rgl_test.c` on all three renderers before shipping, a
local build for Mary first, docs updated as it lands.

---

## 8. Risks and open questions

- **Baked stage lighting already has occlusion painted in** in places
  (dark corners baked into vertex colours): AO on top may double it. The
  floor and per-stage strength (in the per-stage file `DYNAMIC-LIGHTING.md`
  proposes) are the answer; tune per stage.
- **Low-poly stages**: depth-reconstructed normals of large flat triangles
  are good; the risk is at the fighters' silhouettes, which is why the blur
  must be depth-aware and why the dynamic-lighting normals help (phase 5).
- **Fighters as sprites-on-models**: their textures carry painted light;
  0.5 strength is a guess to be judged in Mary's build.
- **Multisampling**: the pre-pass is one sample; AO edges at silhouettes
  are then one pixel per AO texel. With ½-res AO this is hidden by the
  depth-aware upsample; to be checked at 4× and 8× AA.
- **Held-draw ordering**: the pre-pass must use the same viewport and
  scissor per held draw as the replay, or AO is offset in split / picture
  screens (the versus screen, the select screen's 3D fighter).
