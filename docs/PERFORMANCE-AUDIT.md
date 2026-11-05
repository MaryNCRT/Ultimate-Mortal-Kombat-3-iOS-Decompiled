# Performance audit: what the PC build inherited from iOS (2026-11-05)

Mary's two briefs ("OpenGL Native Performance and Hardware Optimization",
"Native Hardware Resource Utilization") ask for an audit before any change.
This is it: what was measured, every limit found, how each is classed, and
the order to change them in. Nothing here changes the decompiled game logic.

## 1. What was measured

Scripted fights on Mary's PC (RTX 5070, Direct3D 12, 1920x1080, smooth models
level 2, real-time shadows, 5 mods): `--fight scorpion sub-zero 2` with
`UMK3_DBG_KEY` / `UMK3_DBG_FIN` for the fatality. The slow-frame line
(rgl_front.c) now splits each slow frame:

```
slow frame: 65 ms, 3 the map and the scene's draws, 1 presenting,
            61 in 1 game ticks, 120816 smoothed triangles (57 ms), 0 sounds, 0 loading
```

| Where the time went | Measured |
|---|---|
| SMOOTH MODELS, a fight (60,000-120,000 smoothed triangles) | 6-11 ms a frame |
| SMOOTH MODELS, the first frame of a new pose (hits, fatalities) | 36-57 ms in one frame |
| The rest of a game tick (logic and draws) | 2-5 ms |
| Presenting (the GPU) | 2-3 ms |
| Sounds, file loads during a fight | ~0 ms |
| Booting to the menus ("loading: menus") | 6.3-7.4 s (Mary's logs), 7.6-17.7 s (scripted runs) |
| Loading a fight | 0.8-1.9 s; one file-loading step took 1.5 s |

**The lag in hits and fatalities is SMOOTH MODELS.** A frame has 16.7 ms;
two fighters' smoothing alone takes up to 11 of them, and a new pose takes
three frames' worth. A late frame used to run its catch-up ticks with the
smoothing too, so it fed itself (3 to 15 ticks a frame): fixed already, only
the tick that is shown smooths (game_main.c `port_tick_unseen`).

Caveat: the scripted runs' window sat behind another app, so their whole
frame times (45-75 ms) are not the game's; each part's time is.

## 2. Inherited limits, classed

### Must be replaced with a PC-native implementation

| What | Where | Why |
|---|---|---|
| SMOOTH MODELS subdivides every pose on the CPU, twice (the fit pass) | `smooth_models.c` `pose_restore_or_eval`, `thirdparty/blender/umk3_subsurf.c` `umk_subsurf_eval` (`sync_positions_partial` x2) | The subdivision is linear in the control points for a fixed mesh, so it can be a precomputed stencil table (OpenSubdiv's method): built once per mesh on the build thread, then one sparse multiply per pose. Same surface, a fraction of the cost. The pose cache and the 250,000-triangle budget (`SMOOTH_FRAME_TRIS`) then stop mattering. |
| Vulkan and Direct3D 12 wait for the GPU at the end of every frame | `rgl_vk.c` (`vkWaitForFences` after each submit), `rgl_d3d12.c` `wait_gpu` | One frame in flight: the CPU and the GPU take turns. Two frames in flight, with per-frame vertex buffers and descriptor pools, let them overlap. |
| Texture uploads submit and wait at once | `rgl_vk.c` upload flush (`g_ufence`), `rgl_d3d12.c` upload path | A load screen stalls on each one. Batched uploads, waited once per frame. |
| OpenGL draws with OpenGL 1.1 client arrays | `rgl_front.c` (straight to the driver), `draw_gl.c` | Every draw copies its vertices from CPU memory again; no VBOs, VAOs, persistent mapping or fences. Either a GL 3.3+/4.4 backend behind `rgl_ff.c` (as Vulkan and D3D12 have, so all three share one path), or a VBO ring buffer in the GL path. |
| Vertices transformed on the CPU for Vulkan and D3D12 | `rgl_ff.c` `from_arrays`, `update_mvp` | Fixed-function emulation: positions go to the GPU already in clip space. Fine today; with the stencil tables, the next CPU cost. A vertex shader taking the matrix is the PC way. |
| Loading on the game's thread | `lime_platform.c` `limeLoadFile`, `draw_gl.c` `load_texture`, `lime/pvr.c` | Decoding is already parallel (`lime_par_for`); reading files is not. Read ahead on a thread while the loading screen draws. |

### Must be removed

| What | Where | Why |
|---|---|---|
| Catch-up ticks smoothing models nobody sees | `game_main.c` tick loop | Done this session (`port_tick_unseen`). |
| Unbuffered session log | `game_main.c` `open_session_log` (`setvbuf(stdout, NULL, _IONBF, 0)`) | **Done 2026-11-05**: measured ~4 ms a line, several a hit -- the hits' frame drops. A 64 KB buffer, flushed once a frame and in `on_crash`. |

### Must be preserved for compatibility

| What | Where | Why |
|---|---|---|
| Game memory below 4 GB, 32-bit pointers in game structures | `savestate.h` `P32` (`__ptr32`), `lime_platform.c` `low_reserve` (up to 2 GB) | The decompiled structures keep the binary's offsets; save states and rollback copy that region. Its size is already a PC size (2 GB reserved, committed as used; a fight uses ~400 MB). |
| The 60 Hz fixed logic tick | `game_main.c` (`acc`, `1.0 / 60.0`) | The combat simulation, replays and rollback depend on it. |
| Fixed-width types and table sizes taken from the binary | `decomp/` | Verified against the oracle. |
| PVRTC decoding of the original textures | `lime/pvr.c` | The player's own .ipa is the asset source. |

### Already PC-sized (no iOS limit left)

- Texture quality MAX: the full sheets, lossless PNG twins, mipmaps (`draw_gl.c`, `PortTextureQuality`).
- 3D resolution independent of the window, antialiasing up to what the card allows, shadow map 4096 (`rgl_ff.c` `SHADOW_SIZE`).
- The pipeline cache, compiled before a fight (`rgl_pipes_note`, COMPILE SHADERS).

### Requires further investigation

- The boot to the menus: 6-7 s, two frames of 2.6-3.8 s in Task_LoadGeneralData and the front end's first load. Which files, and how much is reading against decoding.
- A fight's first frame: 0.7-7 s in the scripted runs (the stage's first-frame pauses, known: STREET up to 1.3 s).
- Fixed tables that could fill with mods: `SMOOTH_CACHE 32`, `MATS_MAX 64`, `CUSTOM_MAX 16`, `BONES_MAX 8` (skin_mats.c), the sound slots (`LIME_SOUNDS`, lime_menu.c).
- GPU memory budgets: Vulkan allocates each buffer itself (no VMA); D3D12 has no budget query (`QueryVideoMemoryInfo`).

## 3. Roadmap

Each step on its own, measured before and after with the same scripted
fight, the existing tests run (difftest, `tests/rgl_test.c` on the three
renderers), and Mary's eyes on the build.

1. **Done 2026-11-05.** **SMOOTH MODELS stencil tables** (`umk3_subsurf.c`, `smooth_models.c`).
   Goal: under 1 ms a frame for two fighters, no spike on a new pose. Test:
   the stencil result against `umk_subsurf_eval` on every fighter, to float
   rounding.
2. **Two frames in flight** on Vulkan and Direct3D 12 (`rgl_vk.c`,
   `rgl_d3d12.c`). Risk: resources freed while the GPU still reads them.
   Test: rgl_test on both, a fight and a fatality with the validation layers.
3. **Batched texture uploads** (same files). Test: boot time, the menus'
   pictures.
4. **The diagnostics line and overlay**: frame time, FPS, game ticks,
   smoothing, draws, texture and buffer memory, GPU memory where the API
   reports it (DXGI budget, `VK_EXT_memory_budget`, `GL_NVX`/`GL_ATI`
   memory info when present), in the log and on the debug menu.
5. **OpenGL as a modern backend** behind `rgl_ff.c` (GL 3.3 core minimum;
   `ARB_buffer_storage` persistent ring buffer and fences on 4.4; KHR_debug
   on 4.3), keeping today's path as the fallback. The largest step.
6. **Loading on a thread**: files read ahead while the loading screen draws;
   then the boot's two long frames.
7. **Vertex transform on the GPU** for the three backends, once the
   profile says it is the next cost.

## 4. Input latency (Mary's "Ultra Low Input Latency" brief)

What is there already (`game_main.c` `pace_frame`, `pace_work_start/end`):
on a 60 Hz screen the loop waits until just before the refresh -- the
period less the longest recent frame work (`g_pace_work`, 4-12 ms) less a
margin -- and only then reads the keys, ticks and draws. On 120/180/240 Hz,
Vulkan and D3D12 present each frame for 2/3/4 refreshes
(`rgl_swap_interval`). `UMK3_LOG_PACE=1` logs it; `UMK3_FRAME_DELAY=0`
turns it off. The simulation is a fixed 60 Hz tick, apart from presenting.

| Finding | Where | Class |
|---|---|---|
| SMOOTH MODELS' spikes push `g_pace_work` up to its 12 ms cap, so the keys are read up to 12 ms earlier than needed for the rest of the session | `pace_frame` | Fixed by roadmap step 1; then the cap can come down |
| One frame in flight on Vulkan and D3D12 (the wait after each submit) | `rgl_vk.c`, `rgl_d3d12.c` | **Preserve for latency** -- two in flight (roadmap step 2) adds up to a frame of lag. Keep one, or two with a frame-latency wait: DXGI `SetMaximumFrameLatency(1)` + the waitable object, Vulkan `VK_KHR_present_wait` |
| D3D12 swap chain without `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` | `rgl_d3d12.c` (FLIP_DISCARD) | Replace: with VSYNC off, borderless and windowed cannot tear and still wait for the refresh |
| Vulkan picks MAILBOX, else IMMEDIATE, with VSYNC off | `rgl_vk.c` | Keep |
| OpenGL swap interval 0 or 1 only; no adaptive (-1, `EXT_swap_control_tear`) | `win32_gl.c` `wglSwapIntervalEXT`, `sdl_gl.c` `SDL_GL_SetSwapInterval` | Add adaptive where the extension is there |
| OpenGL has no fences: the driver may queue 2-3 frames ahead | `rgl_front.c` | Add a `glFenceSync` a frame, waited before the next frame's input |
| Keys: window messages (`WM_KEYDOWN`) and `XInputGetState` read in the frame | `win32_gl.c` | Keep; check where a pad is read against the late key read |
| NVIDIA Reflex | -- | Investigate: through Streamline, D3D12 and Vulkan only, optional, never claimed on OpenGL |

Metrics to add with roadmap step 4: the key read to the tick that used it,
CPU frame time, GPU frame time (timestamp queries), frames in flight, the
present's timing (DXGI frame statistics, `VK_GOOGLE_display_timing` where
there), and input to photon only where the API can say when the frame
reached the screen.
