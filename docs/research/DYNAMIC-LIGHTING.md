# Real-time dynamic lighting: compatibility report and plan

**Status (2026-11-06):** phases 1 and 2 are in (normals and world
positions to the GPU; point/spot/directional lights in the shaders, with
emissive draws as point lights). Played by Mary. The rest is still plan.
Phase 1 found that the fighters' light is baked per animation frame, so
their normals are made from their triangles in `rgl_ff.c`, not taken from
the skinning. Asked for by Mary on 2026-11-06, after ENHANCED LIGHTING (real-time
shadows, texture relief, specular, mipmaps) was built for testing.

References read (all for ideas and technique, no code copied):

| Reference | License | What it covers that we want |
|---|---|---|
| myemural/VulkanCppExamples | MIT | directional/point/spot lights, multiple lights, specular and emissive maps, deferred/tiled/clustered shading, spot and omnidirectional shadow maps, PCF and hardware PCF |
| russkev/opengl | MIT | directional/point/spot lights, many lights with a shadow map each (own resolution, bias, far clip), Blinn shader with specular/roughness maps, a self-illuminated shader |
| Sibras/OpenGL4-Tutorials | **GPL-3.0** | point and spot lights, normal and parallax mapping, Blinn-Phong and GGX, spot and omnidirectional shadows, PCF and PCSS, deferred rendering. Ideas only: GPL-3 code cannot be copied into this GPL-2.0-or-later tree without relicensing |
| stackgl glsl-lighting-walkthrough | MIT | per-pixel Phong, specular map, point-light attenuation (Madams), gamma |

---

## 1. The renderer as it is

**Draw path.** The decompiled code draws with OpenGL ES 1.1 fixed-function
calls. `src/runtime/render/rgl_front.c` defines every `glX`:

- **OpenGL**: straight to the driver's fixed function. There are no shaders
  and no per-pixel anything; it cannot light per pixel as it stands.
- **Vulkan / Direct3D 12**: `rgl_ff.c` keeps the fixed-function state,
  transforms vertices on the CPU to clip space, and hands each draw to
  `rgl_vk.c` / `rgl_d3d12.c` with a small state block. One shader pair does
  everything: `shaders/ff.vert` + `ff.frag` (SPIR-V) and `ff.hlsl` (DXBC),
  compiled into `rgl_shaders_*.h` by `shaders/build.py`.

**The vertex** (`rgl_vtx`, `rgl.h`): clip position, RGBA8 colour, UV, and
`s[4]` (its place in the shadow map's light). **No normal and no world
position** reach the GPU.

**Per draw** (`rgl_frag`, push constants / root constants, 20 bytes):
texture mode, alpha test, shadow mode + caster id, relief strength. One
texture and the shadow map are bound; there is no uniform buffer.

**Coordinate system.** World Z is up and the scene camera looks along +Y
(`rgl_ff.c`, REAL-TIME SHADOWS notes). Clip space is OpenGL's (z from -w
to w); each backend converts it. The scene camera is set by
`LIMEDS_SetCameraOrientation`, and `rgl_shadow_camera` captures its inverse
view, so world positions can be recovered for every 3D draw. The HUD has
its own camera and must stay unlit.

**Lighting today** (`docs/formats/LIGHTING.md`):

- **Fighters**: `LightVert` on the CPU, two directional lights with a power
  falloff, no ambient, **monochrome** (R=G=B). The skinned normals exist on
  the CPU (`RenderSkinned.c`) but are not sent to GL.
- **Stages**: lighting is **baked** into vertex colours from `.lighting`
  files. The meshset files **do carry a per-vertex normal** (12 bytes at
  `+0x0E` of each 26-byte vertex) that the loader discards
  (`MESHSET-FORMAT.md` §4). That is the key fact for stage per-pixel
  lighting: real smooth normals are on disk.
- **Materials**: there is no material system. What a texture has is its
  blend mode, texture environment, alpha test, and the **fullbright list**
  (`IsTextureFullBright`, `RenderMesh.c`; 40 retail entries: the ICE
  fighters, blood, vomit, acid, the snowman). That list is the game's own
  notion of "self-lit", the natural seed for emissive materials.

**ENHANCED LIGHTING, as built now** (for reference): a 4096² directional
shadow map (RGBA8 packed depth + caster id, 5×5 PCF), texture-space relief
and specular derived from the texture itself, gamma-correct, and forced
trilinear + 16× anisotropic mipmaps on 3D textures. Vulkan and D3D12 only.

---

## 2. Compatibility, requirement by requirement

| # | Requirement | Vulkan / D3D12 | OpenGL | What it needs |
|---|---|---|---|---|
| 1 | Point lights (pos, colour, intensity, radius) | yes | needs §4 phase 6 | world position + normal per vertex, a light buffer |
| 2 | Spot lights (cone, attenuation) | yes | phase 6 | as 1, plus direction and inner/outer angle |
| 3 | Directional lights | yes; the shadow light is one already | phase 6 | as 1 |
| 4 | Multiple dynamic lights | yes, forward with a per-draw light list | phase 6 | a uniform/constant buffer per frame (new backend call) |
| 5 | Per-pixel lighting | partial today (relief); full with normals | phase 6 | normals: fighters from the CPU skinning, stages from the meshset (loader keeps them) |
| 6 | Specular maps | auto today; per texture from packs | phase 6 | a second texture binding (`_s` / `_n` from a pack) |
| 7 | Dynamic shadow mapping | done (directional) | phase 6 | spot lights reuse the same 2D map code |
| 8 | Omnidirectional point shadows | yes, at a cost | phase 6 | cube (or 6-layer array) depth targets: new backend target type |
| 9 | PCF | done (5×5, software) | phase 6 | hardware PCF wants a real depth texture + comparison sampler instead of RGBA packing |
| 10 | Emissive materials | yes | phase 6 | material flags; emissive lights from them (§3) |

**OpenGL** is the one real gap: the fixed-function driver path cannot do
any of this. The modular answer is a third backend, `rgl_gl.c`, behind
`rgl_ff.c` exactly like Vulkan and D3D12 (OpenGL 3.3/4.x core, the same
GLSL with push constants replaced by a uniform block). OpenGL then gets
every feature, and the legacy path stays as the fallback when the GL 3.3
backend cannot start. Without it, the feature remains "Vulkan and DirectX
12 only", as REAL-TIME SHADOWS is today.

**Gameplay is untouched** by all of it: everything happens after the game
has issued its draws. Nothing reads or writes game state; rollback,
online and replays are unaffected. The only decomp-side change is a
port-marked one in the meshset loader (keep the normal it already reads
past), plus passing the fighters' normals that `RenderSkinned.c` already
computes.

---

## 3. Emissive materials that light the stage (Mary's idea)

Emissive surfaces should give off light of their own colour, so a stage
gets more variety than one white key light:

- **Which surfaces**: the fullbright list (the game's own "self-lit"), the
  additive-blended draws (glows, fireballs, sparks, Sub-Zero's ice ball,
  teleports, portals), and anything a stage's config marks emissive.
- **Their colour**: each texture's average colour, measured once at upload
  (as `tex_bump` measures relief today), times the draw's vertex colour and
  alpha.
- **Their light**: every frame, the emissive triangles held for the scene
  (world positions are already kept for the shadow map) are grouped into a
  few clusters; each cluster becomes a point light at its centre, coloured
  as above, its intensity from its area on screen and its alpha, its radius
  from its size. Capped (8, say), merged when close.
- **Result**: a projectile lights the floor and the fighters in its colour
  as it travels, a lava or neon texture tints what is near it, and it all
  follows what the game draws without a line of per-move code.

The emissive surface itself is drawn unlit (no relief, no shadow, no
darkening), as fullbright is today.

**Each stage's light from its own sky** (Mary's red-sky example on The
Rooftop). Not one tint for every stage: the light's colour comes from
**that stage's** sky and its emissive elements. The Rooftop's red sky
gives its stone and fighters a reddish fill from above; a blue night sky
gives a cool one; a stage with no strong sky colour stays neutral. As a
hemisphere ambient: the sky's colour on surfaces facing up, a darker
ground colour on those facing down. Worked out automatically per stage
from the average colour of its sky/backdrop textures (the draws behind
everything, far from the camera), weighted by how saturated they are, so
a grey sky adds nothing; the emissive lights of §3 add their own colours
near them. A per-stage file can override or turn it off where the guess
is wrong.

---

## 4. Plan, in phases

Each phase ships on its own, behind ENHANCED LIGHTING, tested by Mary
before the next.

**Order (Mary, 2026-11-06): point lights, spot lights and their shadow
mapping first.** That is phases 1, 2 and 5 below: phase 1 is what point and
spot lights need to light anything per pixel (a normal and a world
position), phase 2 the lights, phase 5 their shadows (spot first, it
reuses the current map code; then point cubes). The sky colour per stage,
emissive lights (§3), materials and configuration follow (phases 3-4), then
OpenGL (phase 6).

**Phase 1: normals and world position to the GPU.** `rgl_vtx` gains a world
position and a normal (or a second vertex stream; +24 bytes a vertex).
`rgl_ff.c` takes `glNormalPointer` instead of ignoring it, transforms
normals by the model-view's inverse transpose into world space. Fighters:
`RenderSkinned.c` hands its skinned normals to GL (port-marked). Stages:
the meshset loader keeps the 12-byte normal (port-marked), `RenderMesh.c`
passes it. Fallback where a mesh has none: the current texture-space
relief. The relief then moves onto the real normal (proper tangent frame
from position/UV derivatives against a smooth normal: no facet seams).

**Phase 2: the light module.** New `src/runtime/render/rgl_lights.c/.h`:
light list (directional, point, spot: position, direction, colour,
intensity, radius, inner/outer cone), set per frame; CPU culling per held
draw against each light's sphere/cone; a per-frame light buffer (new
backend call `lights(const rgl_light *, int n)`: a Vulkan UBO, a D3D12
root CBV) and a per-draw light mask (up to 8 lights a draw). Shader loop:
Blinn-Phong with the specular map, smooth windowed attenuation (inverse
square with a radius window, as Madams/Karis), spot falloff with
`smoothstep` between the cone angles, gamma-correct. The game's own
baked/vertex lighting stays the base; dynamic light is **added** on top,
so a stage with no lights configured looks as it does now.

**Phase 3: materials and emissive.** A material table per texture name
(substring match, like the fullbright list): `emissive`, `shininess`,
`specular`, `relief`, `normal_map` / `specular_map` overrides (a pack's
`name_n.png` / `name_s.png`, as DK's GTA plugin does). Emissive lights from
§3.

**Phase 4: configuration per stage and per fighter.** Files in the game's
own folder (golden rule), e.g. `lighting/<stage>.ini` and
`lighting/<fighter>.ini`: ambient sky/ground tint, key light colour and
direction, fixed lights (torches, windows, lava), material overrides; mods
can ship them (`mods.c` already layers packs). A fighter's file: rim light
colour, specular amount, emissive parts (Sub-Zero's ice, Cyrax's lights).

**Phase 5: more shadows.** Spot-light shadows reuse the 2D map path (a
second map, perspective instead of orthographic). Point-light shadows: a
cube (6 faces) at 512-1024, **fighters only** as casters, at most 2 lights
with shadows at a time (the rest unshadowed). Move the maps to real depth
textures with comparison samplers: hardware PCF, cheaper and smoother than
the 25 software taps.

**Phase 6: OpenGL.** `rgl_gl.c` backend behind `rgl_ff.c`, GL 3.3 core, the
same shaders; the old direct path stays as the fallback. Run
`tests/rgl_test.c` on all three renderers.

---

## 5. Files involved

| File | Change |
|---|---|
| `src/runtime/render/rgl.h` | `rgl_vtx` (+normal, +world pos), `rgl_light`, backend `lights()`, cube targets |
| `src/runtime/render/rgl_ff.c` | normal arrays, world-space normals, held-draw culling per light, emissive clusters, material lookup |
| `src/runtime/render/rgl_lights.c/.h` (new) | light list, config loading, per-stage/per-fighter sets |
| `src/runtime/render/rgl_vk.c`, `rgl_d3d12.c` | vertex layout, light buffer binding, second texture binding, cube/array depth targets |
| `src/runtime/render/rgl_gl.c` (new, phase 6) | OpenGL 3.3 backend |
| `src/runtime/render/shaders/ff.vert`, `ff.frag`, `ff.hlsl`, `build.py` | light loop, materials, emissive, cube shadow sampling |
| `src/decomp/lime/RenderSkinned.c` | port-marked: hand the skinned normals to GL |
| meshset loader (`decomp/lime`), `RenderMesh.c` | port-marked: keep and pass the stored normal |
| `src/runtime/settings_menu.c`, launcher `SecVideo.svelte`, `i18n.js`, `lib/ini.js` | quality settings: light count, point-shadow resolution, on/off per feature |
| `tests/rgl_test.c` | lit scenes on all three renderers |
| `docs/formats/LIGHTING.md`, README, PROGRESS, HANDOFF, CHANGELOG | as each phase lands |

**Dependencies**: none new at run time. Build time: glslang (already in
`WORK/tools/glslang`) and fxc (Windows SDK), as now. Vulkan 1.0 and D3D12
feature level 11 are enough (UBOs, cube maps, comparison samplers are
core). OpenGL 3.3 for phase 6.

---

## 6. Performance

Measured baseline (`docs/PERFORMANCE-AUDIT.md`): a fight is dominated by
SMOOTH MODELS on the CPU (6-11 ms a frame); presenting on the GPU 2-3 ms;
the shadow map and held draws a few ms.

| Feature | Estimated cost | Note |
|---|---|---|
| Normals + world position per vertex | CPU: one 3×3 transform a vertex (~5-10% of the current vertex work); upload +24 B a vertex | with SMOOTH MODELS level 2 (~120k triangles) about 1 ms CPU |
| Per-pixel lights, 8 a draw | GPU ~0.3-1 ms at 1080p, 1-3 ms at 4K (mid-range card) | the scene is small; forward shading is enough. Deferred, tiled and clustered pay off at hundreds of lights, not here |
| Emissive clustering | CPU < 0.2 ms | the triangles are already held for the shadow map |
| Spot shadow (2D map) | ~as the current map: 1-2 ms | one per shadowed spot |
| Point shadow (cube, fighters only) | 6 passes of the fighters: ~1-3 ms per light | capped at 2, 512-1024 per face |
| Hardware PCF | cheaper than the current 25 taps | |
| OpenGL backend | ~as Vulkan | |

All of it hangs off ENHANCED LIGHTING, with quality settings (light count,
point shadows on/off and size) so a weak machine keeps today's look.

---

## 6b. The fighters lit and shadowed too (Mary, 2026-11-06)

Every dynamic light and shadow reaches the fighters as it reaches the
stage, every frame, as they move:

- **Lights on fighters**: point, spot, sky and emissive lights light the
  fighters per pixel from their skinned normals (phase 1), so a fireball
  passing lights one side of a body in its colour, and walking towards a
  torch brightens the side facing it. Their response is turned down
  (textures with light painted in), as relief and specular are now, and
  set per fighter in his file.
- **Shadows on fighters**: they already receive the other fighter's and
  the stage's shadow from the key light. Spot and point shadows (phase 5)
  are received by fighters the same way, and cast by them: one fighter's
  shadow falls on the other from a torch, not only on the floor.
- **Self-shadowing**: today a fighter's own texels are skipped in the map
  (his pose there is a frame old, and SMOOTH MODELS casts the base mesh,
  so he would speckle himself). With the map drawn the same frame from the
  mesh he is drawn with, an arm can shadow his chest; to be tried behind a
  setting, since it is the most likely to show acne.

## 7. Risks and open questions

- **The meshset normal is "almost certainly" the normal** (`MESHSET-FORMAT.md`):
  confirm by decoding a few stage vertices and checking they are unit
  length and match the faces before phase 1 builds on it.
- **Baked stage lighting plus dynamic light** can double-light; the add-on
  model (base unchanged, dynamic added and capped) keeps the original as
  the floor. Each stage may need its tint tuned, which is what the
  per-stage file is for.
- **Fighters' monochrome LightVert**: coloured light on fighters is added
  over it; their textures have light painted in, so their response stays
  low by default (as relief and specular do now).
- **The HUD and 2D draws** must stay unlit: only draws under the scene
  camera (as the shadow receivers are now) are lit.
