# Input latency: where it is and the plan

Port work, not in the binary. The game ticks at a fixed 60 Hz and
simulates one tick per refresh on a 60 Hz screen; nothing here changes a
tick's inputs or results, so rollback (`rollback.c`) and replays stay
deterministic. Everything below is about how long a key press waits before
the tick that reads it, and how long that tick's picture waits before the
screen shows it.

## What the frame loop does now (`src/runtime/game_main.c`)

1. `plat_poll()` at the top of the loop: every pending window message
   (Win32 `PeekMessage`, SDL `SDL_PollEvent`). Keys are kept as state, read
   by `plat_key()`; no queue sits between the message and the tick.
2. The clock decides whether a tick is due (`acc`). If not, the loop goes
   back to 1 at once -- it spins, polling, so a press that arrives while it
   waits is seen by the next tick.
3. The tick: the pads are read right here (`plat_pad_read`, XInput /
   WinMM / SDL), then `GameCodeMain()`.
4. Draw, `render_present()`, `plat_swap()`.
5. `pace_frame()`: on a 60 Hz screen with vsync, sleep until just before
   the next refresh (the period less the longest recent frame work, less
   3 ms), then go to 1. The press reaches the screen in about the time a
   frame takes to make, not a whole refresh later. `UMK3_FRAME_DELAY=0`
   turns it off (environment only).

So the input side is already late-polled and queue-free: events are
drained, the pad read, and the tick run in one go, after the pacing sleep.
The latency left is on the presenting side.

## Presenting, per renderer

| Renderer | Now | Frames that can wait between the tick and the screen |
|---|---|---|
| OpenGL (`win32_gl.c`, `sdl_gl.c`) | `wglSwapIntervalEXT` / `SDL_GL_SetSwapInterval` from VSYNC, `glFinish()` after the swap on Win32 | none queued on Win32 (glFinish); **SDL has no glFinish** |
| Vulkan (`rgl_vk.c`) | the CPU waits on a fence after every submit; FIFO with vsync, `minImageCount` = the driver's (2..3) | **up to `images - 1` presents queued** by the presentation engine: the CPU is serialised with the GPU, not with the display |
| Direct3D 12 (`rgl_d3d12.c`) | `wait_gpu()` after every list and after `Present`; FLIP_DISCARD, `NBUF` buffers, no waitable object | **up to DXGI's default frame latency (3)**: `Present` returns as soon as the GPU is done, the flip waits in DXGI's queue |

Waiting for the GPU is not waiting for the display. On Vulkan and D3D12
the pacing sleep in `pace_frame()` assumes the swap returned at a refresh;
when the present queue holds a frame or two, the swap returns at once, the
loop runs ahead on the clock, and each tick's picture sits one or two
refreshes in the queue -- 17 to 33 ms that the pacing cannot see.

## Plan

One setting, `low_latency=` in `umk3.ini` (on by default), in the
settings menu (VIDEO) and the launcher (`launcher/app/lib/ini.js`,
`SecVideo.svelte`, `i18n.js`). Off restores today's behaviour exactly.

1. **D3D12**: create the swap chain with
   `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`,
   `IDXGISwapChain2_SetMaximumFrameLatency(1)`, and wait on
   `GetFrameLatencyWaitableObject()` once per frame **before** `plat_poll()`
   -- the frame starts when the previous one has been taken by the display,
   then reads input. The resize path (`ResizeBuffers`) must keep the flag.
2. **Vulkan**: with vsync, at most one present queued: `VK_KHR_present_id`
   + `VK_KHR_present_wait` where the driver has them (wait for the previous
   present before starting a frame); otherwise ask for the minimum image
   count the surface allows and keep FIFO. Never mailbox with vsync: at
   60 ticks per 60 Hz it adds nothing and drops frames.
3. **OpenGL on SDL**: `glFinish()` after `SDL_GL_SwapWindow`, as Win32
   already does.
4. **Pacing**: with 1-3 the swap returns at the refresh on every
   renderer, which is what `pace_frame()` already assumes, so it needed no
   change. The frame delay stays an environment switch
   (`UMK3_FRAME_DELAY=0`).
5. **Input**: nothing to move -- polling already happens after the sleep
   and immediately before the tick. The one gap is the 2 ms snap of `dt`
   to a whole tick; it stays (it keeps one tick per refresh).

What it trades: with one frame in flight a frame that takes longer than a
refresh is shown a refresh late instead of being absorbed by the queue
(the queue only hid it by being late all the time). That is why it is a
setting.

## Status (2026-11-06)

1, 2 and 3 are in (`rgl_d3d12.c`, `rgl_vk.c`, `sdl_gl.c`), behind
`rgl_low_latency`. Mary played it on Direct3D 12. Not measured with a
latency tester; Vulkan without `VK_KHR_present_wait` (the log says `vulkan:
present wait not available`) still queues as before.

## Not done here

`rgl_ff.c` and the shaders are another session's (0.3.1 frame-time work);
the slow-present diagnostic in `dx_present` stays as it is.

## References (ideas only, no code taken)

- SDL2 wiki: `SDL_PollEvent`, `SDL_GL_SetSwapInterval`.
- Microsoft: "Reduce latency with DXGI 1.3 swap chains" (waitable object,
  `SetMaximumFrameLatency`).
- Khronos: `VK_KHR_present_wait`.
- NVIDIA Reflex SDK and dxvk-low-latency: the idea of sleeping the CPU so
  the frame starts as late as possible before the display needs it, which
  `pace_frame()` already does for the input side.
