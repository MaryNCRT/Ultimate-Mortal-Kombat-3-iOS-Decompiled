"""
Render a UMK3 mesh to a PNG. The project's first visible output.

This is a **software rasteriser**, deliberately. A GLFW window would look
better and run in real time, but it cannot be looked at from a script, and the
whole point of this tool is to *see* whether four format specifications are
right. Three separate times this project has spent hours on a problem that one
glance at an image ended -- touchHLE's off-screen gamepad coordinates, MAME's
door interlock, and a PVRTC "bug" that lived in the reference data. Rendering to
a file rather than a window is that lesson applied.

It draws on every asset format the project has solved:

    .meshset   geometry and UVs          tools/meshset.py
    .pvr       PVRTC textures            tools/pvr.py, tools/pvrtc.py
    Matrix.cpp perspective and rotation  decomp/lime/Matrix.c, verified

and uses `CreatePerspectiveMatrix`'s own convention -- `cot(fov/2)`, with
`aspect` dividing the X term only, which is the single widescreen hook.

No OpenGL, no window, no display. numpy and Pillow only.

Usage:
  python meshview.py <file.meshset> <out.png> [--size 512] [--angle 30]
                                              [--pitch 8] [--fit 0.7]
  python meshview.py <file.meshset> <out.png> --turntable 6
"""

CULL_SIGN = 1.0    # flip to -1.0 to keep the other winding; 0 disables culling
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import meshset                                  # noqa: E402

try:
    import numpy as np
except ImportError:
    raise SystemExit("numpy is required: pip install numpy")
try:
    from PIL import Image
except ImportError:
    raise SystemExit("Pillow is required: pip install Pillow")


# ---------------------------------------------------------------- textures

def load_texture(res_dir, name):
    """Decode the mesh's texture to an (h, w, 4) uint8 array, or None.

    The mesh names its texture without an extension. PVRTC is a PowerVR
    hardware format that desktop GPUs cannot sample, which is why the decode
    happens on the CPU here and would in the port too.
    """
    if not name:
        return None
    # The exporter wrote a literal ".???" placeholder extension into every mesh
    # ("KANO_DIFFUSE.???"), so the stem is what identifies the file and the
    # engine supplies the real extension. Most are PVRTC; a few ship only as
    # PNG, which is why both are tried.
    stem = os.path.splitext(name)[0]
    for ext in (".pvr", ".PVR", ".png", ".PNG"):
        for base in (stem, stem.upper()):
            path = os.path.join(res_dir, "Textures", base + ext)
            if not os.path.exists(path):
                path = os.path.join(res_dir, base + ext)
            if not os.path.exists(path):
                continue
            try:
                if ext.lower() == ".pvr":
                    import pvr
                    import pvrtc
                    tex = pvr.load(path)
                    w, h, px = pvrtc.decode(tex)
                    return np.frombuffer(bytes(px), np.uint8).reshape(h, w, 4)
                img = Image.open(path).convert("RGBA")
                return np.asarray(img, np.uint8)
            except Exception:                    # noqa: BLE001
                continue
    return None


# ---------------------------------------------------------------- transform

def perspective(fov, aspect, z_near, z_far):
    """The engine's own projection, from the verified `CreatePerspectiveMatrix`.

    `f = sin(fov) / (1 - cos(fov))`, which is `cot(fov/2)`, computed in double
    and narrowed at the end. `fov` is the full vertical field of view, and
    **aspect divides the X term only** -- the single point a widescreen patch
    would touch.
    """
    f = math.sin(fov) / (1.0 - math.cos(fov))
    m = np.zeros((4, 4), np.float64)
    m[0, 0] = f / aspect
    m[1, 1] = f
    m[2, 2] = (z_far + z_near) / (z_near - z_far)
    m[2, 3] = 2.0 * z_far * z_near / (z_near - z_far)
    m[3, 2] = -1.0
    return m


def rot_y(a):
    """Row-major, matching `RotMatrixY`: m[0]=cos, m[2]=-sin, m[8]=sin."""
    c, s = math.cos(a), math.sin(a)
    m = np.eye(4)
    m[0, 0], m[0, 2] = c, -s
    m[2, 0], m[2, 2] = s, c
    return m


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    m = np.eye(4)
    m[1, 1], m[1, 2] = c, s
    m[2, 1], m[2, 2] = -s, c
    return m


# ---------------------------------------------------------------- raster

def render(meshes, res_dir, size=512, yaw=0.6, pitch=0.15, bg=(24, 24, 28),
           fit=1.0):
    """Rasterise every mesh into one image. Returns (RGB array, stats)."""
    colour = np.zeros((size, size, 3), np.float64)
    colour[:] = bg
    depth = np.full((size, size), np.inf)

    # gather geometry so the camera can frame whatever it is given
    all_pos = []
    for m in meshes:
        if m.verts:
            all_pos.append(np.array([[v[0], v[1], v[2]] for v in m.verts]))
    if not all_pos:
        return colour.astype(np.uint8), {"meshes": 0, "triangles": 0}
    pts = np.vstack(all_pos)
    centre = (pts.max(0) + pts.min(0)) * 0.5
    radius = max(float(np.linalg.norm(pts - centre, axis=1).max()), 1e-6)

    view = rot_x(pitch) @ rot_y(yaw)
    dist = radius * 2.8 * fit
    proj = perspective(math.radians(50.0), 1.0, radius * 0.05, dist + radius * 3)

    light = np.array([0.4, 0.7, 0.6])
    light /= np.linalg.norm(light)

    tris = 0
    for mesh in meshes:
        if not mesh.verts or not mesh.faces:
            continue
        tex = load_texture(res_dir, mesh.texture)

        v = np.array([[p[0], p[1], p[2]] for p in mesh.verts], np.float64)
        uv = np.array([[p[3], p[4]] for p in mesh.verts], np.float64)
        v = (v - centre) @ view[:3, :3].T
        v[:, 2] -= dist                                  # camera pulls back

        clip = np.column_stack([v, np.ones(len(v))]) @ proj.T
        w = clip[:, 3].copy()
        w[np.abs(w) < 1e-9] = 1e-9
        ndc = clip[:, :3] / w[:, None]
        sx = (ndc[:, 0] * 0.5 + 0.5) * (size - 1)
        sy = (1.0 - (ndc[:, 1] * 0.5 + 0.5)) * (size - 1)

        for a, b, c in mesh.faces:
            if max(a, b, c) >= len(v):
                continue
            if w[a] <= 0 or w[b] <= 0 or w[c] <= 0:      # behind the camera
                continue
            x0, y0, x1, y1, x2, y2 = sx[a], sy[a], sx[b], sy[b], sx[c], sy[c]
            area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
            if CULL_SIGN and area * CULL_SIGN >= 0:      # back-facing
                continue
            tris += 1

            lo_x = max(int(math.floor(min(x0, x1, x2))), 0)
            hi_x = min(int(math.ceil(max(x0, x1, x2))), size - 1)
            lo_y = max(int(math.floor(min(y0, y1, y2))), 0)
            hi_y = min(int(math.ceil(max(y0, y1, y2))), size - 1)
            if lo_x > hi_x or lo_y > hi_y:
                continue

            # flat shading from the face normal, in view space
            n = np.cross(v[b] - v[a], v[c] - v[a])
            ln = np.linalg.norm(n)
            shade = 0.25 if ln < 1e-9 else 0.25 + 0.75 * abs(float(n @ light) / ln)

            px, py = np.meshgrid(np.arange(lo_x, hi_x + 1),
                                 np.arange(lo_y, hi_y + 1))
            w0 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0)
            w1 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)
            w2 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2)
            # normalise by the signed area so the test works for either
            # winding -- hard-coding one sign silently rejects every pixel of
            # a triangle wound the other way
            sgn = -1.0 if area < 0 else 1.0
            inside = (w0 * sgn >= 0) & (w1 * sgn >= 0) & (w2 * sgn >= 0)
            if not inside.any():
                continue

            l0 = w1 / area
            l1 = w2 / area
            l2 = 1.0 - l0 - l1
            z = l0 * ndc[a, 2] + l1 * ndc[b, 2] + l2 * ndc[c, 2]

            sub = depth[lo_y:hi_y + 1, lo_x:hi_x + 1]
            visible = inside & (z < sub)
            if not visible.any():
                continue
            sub[visible] = z[visible]

            if tex is not None:
                th, tw = tex.shape[:2]
                # Perspective-correct interpolation. Interpolating UVs linearly
                # in screen space is affine mapping -- the PlayStation 1 defect
                # -- and on a character's large triangles the texture visibly
                # swims, which reads as clothing sliding through itself.
                # Barycentrics have to be divided by w and renormalised.
                ia, ib, ic = 1.0 / w[a], 1.0 / w[b], 1.0 / w[c]
                den = l0 * ia + l1 * ib + l2 * ic
                den = np.where(np.abs(den) < 1e-12, 1e-12, den)
                p0 = (l0 * ia) / den
                p1 = (l1 * ib) / den
                p2 = 1.0 - p0 - p1
                u = p0 * uv[a, 0] + p1 * uv[b, 0] + p2 * uv[c, 0]
                vv = p0 * uv[a, 1] + p1 * uv[b, 1] + p2 * uv[c, 1]
                tx = np.clip((u % 1.0) * (tw - 1), 0, tw - 1).astype(np.int32)
                ty = np.clip((vv % 1.0) * (th - 1), 0, th - 1).astype(np.int32)
                rgb = tex[ty, tx, :3].astype(np.float64)
            else:
                rgb = np.full(inside.shape + (3,), 190.0)

            tile = colour[lo_y:hi_y + 1, lo_x:hi_x + 1]
            tile[visible] = np.clip(rgb[visible] * shade, 0, 255)

    return colour.astype(np.uint8), {"meshes": len(meshes), "triangles": tris}


# ---------------------------------------------------------------- cli

def _res_dir_for(path):
    d = os.path.dirname(os.path.abspath(path))
    return d


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    src, out = argv[1], argv[2]
    size = 512
    yaw = 0.6
    pitch = 0.15
    fit = 1.0
    turntable = 0
    for i, a in enumerate(argv):
        if a == "--size" and i + 1 < len(argv):
            size = int(argv[i + 1])
        elif a == "--angle" and i + 1 < len(argv):
            yaw = math.radians(float(argv[i + 1]))
        elif a == "--turntable" and i + 1 < len(argv):
            turntable = int(argv[i + 1])
        elif a == "--pitch" and i + 1 < len(argv):
            pitch = math.radians(float(argv[i + 1]))
        elif a == "--fit" and i + 1 < len(argv):
            fit = float(argv[i + 1])

    with open(src, "rb") as fh:
        meshes, _end = meshset.parse(fh.read())     # parse returns (meshes, offset)
    res_dir = _res_dir_for(src)
    print("%s: %d meshes" % (os.path.basename(src), len(meshes)))

    frames = turntable if turntable > 0 else 1
    for f in range(frames):
        angle = yaw + (2 * math.pi * f / frames if turntable else 0.0)
        img, stats = render(meshes, res_dir, size=size, yaw=angle,
                            pitch=pitch, fit=fit)
        name = out if frames == 1 else "%s_%02d%s" % (
            os.path.splitext(out)[0], f, os.path.splitext(out)[1])
        Image.fromarray(img).save(name)
        print("  %-40s %d triangles" % (name, stats["triangles"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
