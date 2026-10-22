# Headers for the Linux build

The launcher compiles the game on the player's machine. A Linux desktop has
OpenGL and SDL2 installed as libraries (`libGL.so.1`, `libSDL2-2.0.so.0`) but
usually not their development headers, which come in separate packages
(`libgl-dev`, `libsdl2-dev`). Without them the build stopped at
`fatal error: 'GL/gl.h' file not found`. These copies are searched after the
system's own (`-idirafter`, `launcher/app/lib/compile.js`), so an installed
`-dev` package still wins.

| Files | From | License |
|---|---|---|
| `GL/gl.h` | Mesa 24.0.9, `include/GL/gl.h` | MIT (in the file) |
| `GL/glext.h` | Khronos OpenGL-Registry, `api/GL/glext.h` | MIT (in the file) |
| `KHR/khrplatform.h` | Khronos EGL-Registry, `api/KHR/khrplatform.h` | MIT (in the file) |
| `SDL2/*.h` | SDL 2.30.9 release, `include/` (no test or other-platform configs) | zlib, `SDL2/LICENSE.txt` |

The SDL headers fall back to `SDL_config_minimal.h` on Linux, which is enough
for a program that calls SDL: the library itself is the system's.
