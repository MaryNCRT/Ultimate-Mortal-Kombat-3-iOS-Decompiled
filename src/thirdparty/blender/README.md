# src/thirdparty/blender

Blender's **Weld** and **Subdivision Surface** (Catmull-Clark) modifiers, used
by SMOOTH MODELS: the game (`src/runtime/smooth_models.c`) and the launcher's
model viewer (`launcher/app/ui/src/lib/mods/subsurf.js`, the same C as
WebAssembly). GPL-2.0-or-later, as Blender is; this is why the project as a
whole is under the GPL from 0.1.10 (see `LICENSE`).

| File | From | Changes |
|---|---|---|
| `CCGSubSurf.c`, `CCGSubSurf_legacy.c`, `CCGSubSurf_util.c`, `CCGSubSurf_intern.h`, `CCGSubSurf_inline.h` | Blender 2.79b, `source/blender/blenkernel/intern/` | none |
| `CCGSubSurf.h` | Blender 2.79b, as above | `CCG_OMP_LIMIT` 20000 instead of 1000000, so a fighter's mesh is subdivided on several cores |
| `omp_lite.c` | written here | the OpenMP runtime calls (`__kmpc_*`) that `-fopenmp` emits for CCGSubSurf's pragmas; zig cc ships none. A pool of at most four threads (`UMK3_THREADS` overrides). Empty without `-fopenmp` |
| `shim/*.h` | written here | stand-ins for the Blender headers those files include (`MEM_guardedalloc.h`, `BLI_*.h`, `BKE_ccg.h`, `BKE_subsurf.h`, `GL/glew.h`), only what they use |
| `umk3_subsurf.c`, `umk3_subsurf.h` | ported from Blender, see the file's header | the weld from 4.2.0 (`blenlib/intern/kdtree_impl.h`, `geometry/intern/mesh_merge_by_distance.cc`); the subsurf glue from 2.79b (`subsurf_ccg.c`, `mesh_mapping.c`, `math_geom.c`), for triangle arrays |

Why 2.79b for Catmull-Clark: later Blender evaluates the modifier with
OpenSubdiv, a separate C++ library. 2.79b is the last version whose own C code
(CCGSubSurf) does it.

Check: `tests/subsurf_test.c` (the build line is in its header).
WebAssembly for the viewer: `node launcher/app/scripts/build-subsurf.js`.
