# Screen-space contact shadows: study and plan

Research only. Nothing here is implemented yet; no game code was changed
for it. Asked for by Mary on 2026-11-06, as a companion to
`AMBIENT-OCCLUSION.md` and `DYNAMIC-LIGHTING.md`. Contact shadows are the
small, sharp shadows the 4096² shadow map is too coarse and too biased to
draw: the sole of a foot on the floor, an arm on the torso, a fighter
against a stage prop. They **complement** the shadow map and never replace
it.

References read (for ideas and technique; no code copied):

| Reference | License | What it shows |
|---|---|---|
| egnawake/screen-space-shadows | code **MPL-2.0**, docs CC BY-NC-SA 4.0 (C#, OpenTK, GLSL) | the simplest form: view-space march towards the light, binary hit within a thickness; 16 steps, distance 0.02, thickness 0.005. Its README states the two classic problems: artifacts where the ground meets model edges, and a thickness that is right near and wrong far. MPL-2.0 is file-level copyleft and compatible with GPL-2.0-or-later only via its secondary-licence clause; we write our own instead |
| saffron-anima, "Contact shadows" (Vulkan 1.4, Rust) | **none stated**, no LICENSE in the repo: ideas only | a thin G-buffer (view normal + view Z), a 0.02 normal offset, a 0.2 view-space march, 12 / 16 steps by quality tier, **interleaved gradient noise** jitter of the first step, a hit window 0.01 < Δz < 0.1, R8 output **multiplied into the directional shadow term only**, off on Low/Medium |
| HiveEngine, "Screen-space lighting" | engine's own not stated: ideas only | contact shadows for the sun **and up to four local lamps**, reach 0.6 m, thickness 0.25 m + 4 mm per metre from the eye, 12 steps, bias 0.02 m, fade at 60 m, a **history-free** 4×4 plane-weighted denoiser shared with its AO, and measured costs: **0.19 ms** march + 0.10-0.13 ms denoise at 1080p on an RTX 5060 Ti |
| myemural/VulkanCppExamples | MIT | no contact-shadow sample; its shadow-mapping (PCF, PCSS) and post-processing samples are the background the shadow map already uses |

---

## 1. The renderer as it is

(Fuller in `AMBIENT-OCCLUSION.md` §1 and `DYNAMIC-LIGHTING.md` §1.)

- **Draw path**: OpenGL ES 1.1 calls → `rgl_front.c` → the driver's fixed
  function (**OpenGL**, no shaders) or `rgl_ff.c` → `rgl_vk.c` /
  `rgl_d3d12.c`, one shader pair `ff.vert` / `ff.frag` / `ff.hlsl`.
- **The shadow map** (ENHANCED LIGHTING, `rt_shadows=1`): one directional
  key light fitted round the fighters (`SHADOW_ROOM` 3.2 × their radius,
  at least `SHADOW_MIN` 10 units), 4096², RGBA8 packed depth + caster id,
  5×5 PCF, each tap against the receiver's own plane, faded at the map's
  edge. A fighter's own texels are skipped (SMOOTH MODELS casts the base
  mesh and draws the smoothed one), so **a fighter never shadows himself**:
  no arm on the torso, no leg on the other leg.
- **Why the map misses contacts**: one texel covers `2r / 4096` world
  units; the receiver-plane bias and PCF need a few texels to stay free of
  acne, so the shadow starts a little away from where the caster touches
  the receiver (peter-panning): the foot looks to float by a few
  centimetres. These are exactly the cases screen-space contact shadows
  fill.
- **The scene is held** until `scene_flush` (shadow map → replay), so a
  depth pre-pass of the frame's opaque draws is free of game-side work.
  The AO plan (`AMBIENT-OCCLUSION.md` §4.1) builds that pre-pass; contact
  shadows **share it**.
- **No readable depth** exists other than the shadow map's packed RGBA8
  (the depth attachments are not sampleable). Same answer as AO: a linear
  view depth packed into RGBA8 by the pre-pass.
- **Lights**: today one directional (the shadow light) plus `LightVert`'s
  two fixed directions on the fighters; once `claude/dynamic-lighting`
  lands, point and spot lights from its `rgl_light_block`.

---

## 2. The technique

For each pixel of an opaque 3D receiver:

1. Rebuild its view position `p` from the pre-pass depth, and its normal
   `n` (from depth, or from the dynamic-lighting vertex normals once they
   exist).
2. Start at `p + n · normal_offset` (avoids self-hits on the surface).
3. March `N` steps towards the light, over `distance` in view space:
   `s_i = p' + l · distance · (i - j) / N`, with `j` per-pixel interleaved
   gradient noise in [0, 1) (no texture; stable, so no shimmer without TAA).
   - **Directional**: `l` = the light direction in view space, once a frame.
   - **Point / spot**: `l = normalize(light_pos - p)`, and the march is
     clamped to `min(distance, |light_pos - p|)` so it never passes the light.
4. Project each `s_i` to the depth target; stop at the screen's edge or
   behind the camera. Read the scene's depth `z_s` there.
5. **Hit** when the ray is behind the surface by more than `bias` and less
   than `thickness`: `bias < z_ray - z_s < thickness(z)`, with
   `thickness(z) = thickness + thickness_per_unit · z` (HiveEngine's
   growth with distance, which fixes egnawake's near/far problem).
6. A hit is not binary: weight it by how near the start it is
   (`1 - i / N`, a soft end) and by the distance fade; keep the strongest.

Output: **contact visibility**, 1 lit, 0 occluded, into the G channel of
the AO result target (R is AO), so one target, one blur and one texture
binding serve both.

### 2.1 What it catches here

| Case | Shadow map today | Contact shadows |
|---|---|---|
| Feet to floor | starts a few cm off the sole (bias, PCF) | a dark line exactly at the contact; the fighters stop floating |
| Limbs to torso (self-shadowing) | **never** (own texels skipped) | yes: screen space does not know which fighter is which, so an arm darkens the chest under it, from the key light's direction |
| Fighter to stage props, props to floor | only if both are in the map's fitted area, at its resolution | yes, anything on screen within `distance` |
| Fighter to fighter at close range (a throw) | yes, coarse | sharper at the touch |
| Point / spot lights (torch, fireball) | none until dynamic-lighting phase 5 | yes, for the brightest 1-4 lights a pixel (phase 4 below) |

### 2.2 Avoiding over-darkening

The rules, all in the application step:

- **Multiply only the direct light it belongs to**, never the whole colour.
  For the key light: `shadow_vis = min(shadow_vis, mix(1, contact, intensity))`,
  i.e. combined with the shadow map's own visibility by **min**, not by
  multiplication, so a pixel the map already shadows is **not darkened
  again** (the saffron engine multiplies; min is the safer choice for us).
- **The shadow's own strength stays the cap**: the shadow map darkens to a
  set fraction today (receivers keep their baked light); contact shadows go
  no darker than that same fraction. A contact shadow is never blacker
  than a real shadow.
- **Fighters' self-shadowing at half strength** by default
  (`contact_self=0.5`; their textures have painted light, as for AO).
- **Fade** with distance from the camera (`contact_fade`, the backdrop is
  never touched) and at the screen's edges (where rays leave the screen).
- **No contact shadow from what does not cast**: blended draws (fire,
  smoke, blood, glass), fullbright surfaces and the HUD are not in the
  pre-pass, so they neither cast nor receive.
- **AO and contact shadows do not stack in a crease**: AO is reduced where
  the contact term already darkens (`AMBIENT-OCCLUSION.md` §4.3).
- **Thickness**: too large and a thin pole shadows the whole wall behind
  it (a long smear); too small and the fighters' limbs, seen edge-on, let
  rays through. Default from the measured fighter scale (phase 0).

### 2.3 Denoising

No temporal filter exists (there is no TAA, no history), so like HiveEngine
the filter is **spatial and history-free**:

- Interleaved gradient noise jitter (stable per pixel, not per frame).
- The AO pass's **4×4 depth-aware bilateral blur**, run on R (AO) and G
  (contact) together, with a tighter depth tolerance on G so the contact
  line stays sharp at the foot.
- At `contact_scale=0.5` a depth-aware upsample; at 1.0, `contact_blur=0`
  is acceptable for a crisp, slightly dithered line.

---

## 3. Compatibility with our three backends

| Need | Vulkan | Direct3D 12 | OpenGL |
|---|---|---|---|
| Readable depth | the shared pre-pass, RGBA8 packed linear depth (as the shadow map) | same | needs the GL 3.3 backend behind `rgl_ff.c` (ENHANCED LIGHTING's planned fix) |
| Full-screen march pass | the AO full-screen pass call; a pipeline of its own | same | GL 3.3 backend |
| Light parameters | push constants for the key light; the dynamic lights' UBO when it exists | root constants; the lights' CBV | GL 3.3: a UBO (std140) |
| Result read by the scene shader | the AO target's G channel (binding 2) | `t2`, G | GL 3.3 backend |
| Loops with early exit, `textureLod` | glslang SPIR-V | fxc SM 5.1 (`[loop]`, `SampleLevel`; no gradient-dependent ops in the loop) | GLSL 330 |

Same conclusions as AO: implemented once in `rgl_ff.c` plus a pair of
shaders (GLSL and HLSL, kept in step), behind new backend calls that both
backends implement; **no Vulkan code carries over to D3D12 by itself**;
OpenGL gets it with the GL 3.3 backend, and until then the option is off
with OpenGL and `rgl_contact_active()` is 0.

---

## 4. Proposed design

### 4.1 Where it runs

In `scene_flush`, after the shared depth pre-pass:

    shadow map → depth pre-pass → AO (R) → contact march (G) → blur R+G → replay

Contact shadows can be on with AO off: the pre-pass, target and blur run
when either is on. The march is a separate pipeline writing only G (colour
mask), so AO and contact are independent switches.

### 4.2 Lights

- **Phase 2: the key light** (the shadow map's directional light), which
  is where almost all the visible contact (feet, limbs) is.
- **Phase 4: point and spot lights**, once `rgl_light_block` exists: up to
  `contact_lights` (default 1, max 4) lights per pixel, the brightest at
  that pixel (by its attenuation), each its own march; results packed one
  per channel (G key light, B and A two local lights) or HiveEngine's way
  (one ranked lamp per pixel). Each light's term multiplies only that
  light's contribution in the scene shader, inside the light loop.

### 4.3 Controls

`umk3.ini`, the settings menu and the launcher, under ENHANCED LIGHTING:

| Key | Range | Default | Meaning |
|---|---|---|---|
| `contact_shadows` | 0 off, 1 Low, 2 Medium, 3 High | 0 | on, and the preset below |
| `contact_distance` | 0.05-1.0 world units | 0.3 | how far each ray reaches |
| `contact_thickness` | 0.01-0.5 | 0.1 | how thick a depth sample is taken to be (plus growth with distance) |
| `contact_intensity` | 0-1 | 0.8 | how much of the light a contact removes (capped by the shadow's own strength) |
| `contact_steps` | 8, 12, 16, 24 | 12 | march steps |
| `contact_blur` | 0 off, 1 bilateral | 1 | denoising |
| `contact_scale` | 0.5, 1.0 | 1.0 | resolution (contacts are thin: full res by default) |
| `contact_self` | 0-1 | 0.5 | fighters' self-shadowing strength |
| `contact_lights` | 1-4 | 1 | how many lights march (phase 4) |
| `contact_fade` | world units | measured | distance at which it is gone |

Presets: **Low** 8 steps, ½ res, key light only; **Medium** 12 steps, full
res, key light; **High** 16-24 steps, full res, key light + 2 local lights.

### 4.4 What it must not touch

The decompiled code, models, animations, gameplay and the existing lighting
(`LightVert`, baked stage colours, the shadow map, relief, specular) stay as
they are. `contact_shadows=0` makes no pass; with AO also off there is no
pre-pass either, and the frame is what it is now.

---

## 5. Files involved

| File | Change |
|---|---|
| `src/runtime/render/rgl.h` | `rgl_contact` settings, `rgl_contact_active()` (backend calls shared with AO) |
| `src/runtime/render/rgl_ff.c` | the march pass in `scene_flush`, light direction in view space once a frame, the per-light list for phase 4 |
| `src/runtime/render/rgl_vk.c`, `rgl_d3d12.c` | nothing beyond the AO groundwork (full-screen pass, RGBA8 target, third texture) |
| `src/runtime/render/shaders/contact.frag`, `contact.hlsl` (new) | the march |
| `src/runtime/render/shaders/ao_blur.*` | R and G together, the tighter G tolerance |
| `src/runtime/render/shaders/ff.frag`, `ff.hlsl` | min with the shadow map's visibility, per-light term in the light loop |
| `src/runtime/render/shaders/build.py` | the new shaders |
| `src/runtime/game_main.c`, `settings_menu.c`, launcher `SecVideo.svelte`, `i18n.js`, `lib/ini.js` | the `contact_*` keys |
| `tests/rgl_test.c` | a box resting on a floor lit from the side: a dark line at the contact, nothing on the open floor, no darker than the shadow map where both apply |
| `docs/formats/LIGHTING.md`, README, PROGRESS, HANDOFF, CHANGELOG | as each phase lands |

**Dependencies**: none new at run time; glslang and fxc at build time, as
now. It depends on the AO groundwork (phases 1-2 of
`AMBIENT-OCCLUSION.md`: backend calls and depth pre-pass), and for point
and spot lights on `claude/dynamic-lighting`.

---

## 6. Performance

| Piece | Estimated cost (mid-range GPU, 1080p 3D resolution) | Note |
|---|---|---|
| Depth pre-pass | 0 if AO is on (shared); else 0.1-0.3 ms | |
| March, key light, 12 steps, full res | 0.15-0.3 ms | HiveEngine: 0.19 ms (sun + lamps, 12 steps, RTX 5060 Ti); most pixels exit early (no hit, off screen) |
| Each extra light | +0.1-0.2 ms | phase 4 |
| Bilateral blur | 0 extra with AO (same pass); else 0.1 ms | HiveEngine: 0.10-0.13 ms |
| Scene shader read | < 0.05 ms | |
| 4K | ×3-4 at full res; Low (½ res) keeps it near the 1080p cost | |

Medium, key light only: **~0.3-0.5 ms GPU**, nothing on the CPU.

---

## 7. Phased integration plan

| Phase | What | Done when |
|---|---|---|
| 0 | Shared with AO: measure the scene camera's units (fighter height, foot size) for the distance/thickness defaults | numbers in `docs/formats/LIGHTING.md` |
| 1 | Shared with AO: backend groundwork and the depth pre-pass (`AMBIENT-OCCLUSION.md` phases 1-2) | as there |
| 2 | Key-light march into G, min with the shadow map, intensity cap, distance and edge fade, `contact_debug=1` view | Mary's build: feet grounded, no darkening of the open floor, no double-dark under the shadow map |
| 3 | Fighters' self-shadowing strength, bilateral denoise on G, presets, menu and launcher controls | arms on torsos read as shadow, not dirt; no halo at silhouettes |
| 4 | Point and spot lights from `rgl_light_block` (after `claude/dynamic-lighting` is merged), `contact_lights` | a torch or fireball grounds the fighters near it |
| 5 | OpenGL with the GL 3.3 backend | `rgl_test` contact scene passes on all three renderers |

Each phase: `tests/rgl_test.c` on all three renderers, a local build for
Mary first, docs updated as it lands.

---

## 8. Risks and open questions

- **Screen-space limits**: what is off screen or hidden behind something
  else casts nothing, and a thin object seen edge-on can let rays through.
  For a side-on fighting game camera this is mild: the fighters, the floor
  and the props near them are always on screen.
- **Self-shadowing on SMOOTH MODELS**: the pre-pass sees the smoothed mesh
  (the one drawn), so there is no base/smoothed mismatch, which is what
  forced the shadow map to skip a fighter's own texels. Still, a bias too
  small will speckle the fighters' bodies; `contact_self` and the normal
  offset are the knobs.
- **Light direction vs the painted light**: the fighters' textures have
  light painted from a fixed direction; a contact shadow from the key light
  must agree with the shadow map's direction (it does: the same light), but
  may disagree with the painting on some characters. Judged in Mary's build.
- **Baked stage shadows**: stages have shadows baked into vertex colours;
  contact shadows add only where dynamic casters (fighters) touch, and the
  min-combination keeps already-dark areas from going darker.
