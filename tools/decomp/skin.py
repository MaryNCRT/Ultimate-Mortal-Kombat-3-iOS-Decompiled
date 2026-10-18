"""
Parser and validator for the .skin format.

Derived from the disassembly of LIME_LoadSkin1 (0x000604c0 in the armv7 slice),
whose limeMalloc tags conveniently name every buffer it allocates:
"skin_indexes", "skin_mweights", "skin_normals", "skin_uvs".

The check is the same one used for .meshset: if the layout is right, walking
the file has to land exactly on its last byte. There is no header field giving
the size, so an exact landing across every file is strong evidence.

Usage:
  python skin.py validate <res dir>
  python skin.py dump     <file.skin>
"""

import os
import struct
import sys

# 16-bit weights are scaled by this on load (vmul against the literal at
# 0x00060634, which is 0x37800000 = 1/65536).
WEIGHT_SCALE = 1.0 / 65536.0

MATRIX43_SIZE = 48      # SKINMATRIX43: 4x3 floats
BONE_MATRICES = 2       # two per entry, stored back to back


class Skin(object):
    __slots__ = ("num_matrices", "num_verts", "indexes", "weights",
                 "matrices_a", "matrices_b", "vert_data", "vert_extra")


def parse(data):
    """
    Parse a whole .skin file. Returns (list of Skin, end_offset).

    LIME_LoadSkin reads a leading int32 giving the number of blocks, then calls
    LoadSkin1 once per block, chaining them through SKININFO+0x00. The count is
    1 or 2 in practice -- the code only ever looks for == 2.
    """
    (count,) = struct.unpack_from("<i", data, 0)
    if 1 <= count <= 8:
        off = 4
        blocks = []
        try:
            for _ in range(count):
                s, off = parse_block(data, off)
                blocks.append(s)
            if off == len(data):
                return blocks, off
        except Exception:                                       # noqa: BLE001
            pass

    # ROBO1_STANDARD and ROBO2_STANDARD have no leading count: their first
    # int32 is already the matrix count. They are the same two files that use
    # the odd unindexed variant of .meshset, so they came out of a different
    # export path. Fall back to a single block starting at offset 0.
    s, off = parse_block(data, 0)
    return [s], off


def parse_block(data, off):
    """Parse one skin block, the unit LoadSkin1 consumes."""
    num_matrices, num_verts = struct.unpack_from("<2i", data, off)
    if num_matrices < 0 or num_verts < 0:
        raise ValueError("negative counts: %d, %d" % (num_matrices, num_verts))

    s = Skin()
    s.num_matrices = num_matrices
    s.num_verts = num_verts
    off += 8

    # skin_indexes: one uint32 per matrix, copied verbatim
    s.indexes = list(struct.unpack_from("<%dI" % num_matrices, data, off))
    off += num_matrices * 4

    # skin_mweights: 4 uint16 per matrix, each scaled by 1/65536 on load
    raw = struct.unpack_from("<%dH" % (num_matrices * 4), data, off)
    s.weights = [v * WEIGHT_SCALE for v in raw]
    off += num_matrices * 8

    # Two 4x3 matrices per entry. The first goes to SKININFO+0x14, the
    # second to +0x28.
    s.matrices_a = []
    s.matrices_b = []
    for _ in range(num_matrices):
        s.matrices_a.append(struct.unpack_from("<12f", data, off))
        s.matrices_b.append(struct.unpack_from("<12f", data, off + MATRIX43_SIZE))
        off += MATRIX43_SIZE * BONE_MATRICES

    # 24 bytes per vertex, copied verbatim (tagged "skin_uvs" by the loader,
    # though 24 bytes is more than a UV pair needs -- see the note in the docs)
    s.vert_data = data[off:off + num_verts * 24]
    off += num_verts * 24

    # 6 bytes per vertex, copied verbatim
    s.vert_extra = data[off:off + num_verts * 6]
    off += num_verts * 6

    return s, off



def cmd_validate(resdir):
    files = []
    for base, _d, names in os.walk(resdir):
        for n in names:
            if n.lower().endswith(".skin"):
                files.append(os.path.join(base, n))
    files.sort()

    ok = bad = 0
    tot_m = tot_v = 0
    for p in files:
        data = open(p, "rb").read()
        if not data:
            continue
        try:
            blocks, end = parse(data)
        except Exception as e:                                  # noqa: BLE001
            print("  %-34s ERROR: %s" % (os.path.basename(p), e))
            bad += 1
            continue
        if end == len(data):
            ok += 1
            tot_m += sum(b.num_matrices for b in blocks)
            tot_v += sum(b.num_verts for b in blocks)
        else:
            bad += 1
            print("  %-34s ends at %d, file is %d (delta %+d)"
                  % (os.path.basename(p), end, len(data), end - len(data)))

    print("\n.skin files:        %d" % len(files))
    print("  exact:            %d" % ok)
    print("  mismatched:       %d" % bad)
    print("skinning matrices:  %d" % tot_m)
    print("vertices:           %d" % tot_v)
    return bad


def cmd_dump(path):
    data = open(path, "rb").read()
    blocks, end = parse(data)
    print("%s: %d bytes, %d block(s), walk ends at %d %s"
          % (os.path.basename(path), len(data), len(blocks), end,
             "(exact)" if end == len(data) else "(MISMATCH)"))
    for i, s in enumerate(blocks):
        print("  -- block %d --" % i)
        print("    skinning matrices: %d" % s.num_matrices)
        print("    vertices:          %d" % s.num_verts)
        print("    first 8 indexes:   %s" % s.indexes[:8])
        print("    first 8 weights:   %s" % ["%.5f" % w for w in s.weights[:8]])
        if s.matrices_a:
            print("    matrix A[0]:       %s" % ["%.3f" % v for v in s.matrices_a[0]])
            print("    matrix B[0]:       %s" % ["%.3f" % v for v in s.matrices_b[0]])


# ------------------------------------------------------------------ .skinanim

ANIM_HEADER = 12          # float scale, int32 numFrames, int32 frameSize
ANIM_HEADER_ALT = 16      # SINDEL only: an extra leading float
BONEANIMFRAME_SIZE = 20   # 5 floats per bone per frame
FRAME_FIXED = 16          # int32 tag + limeVECTOR3 root, before the bone array


def parse_anim(data):
    """
    Parse a .skinanim header.

    The header describes itself, and that matters: deriving the frame size from
    the matching .bones file appears to work and then fails on ROBO1 and ROBO2,
    whose animations carry a different bone count than their skeletons.
    Read frameSize from the header; never compute it.

    **Two header layouts exist.** 28 of 29 files use 12 bytes:

        float scale; int32 numFrames; int32 frameSize;

    `SINDEL_STANDARD.skinanim` uses 16, with an extra leading float:

        float scale; float scale2; int32 numFrames; int32 frameSize;

    and it holds **twice** the frames its count field claims -- 844 against 422,
    with `16 + 844*1256` landing exactly on the file's last byte. Its two halves
    differ in **47 bytes out of 530,032**, so they are two near-identical takes
    rather than a duplicated buffer. Whether the doubling is deliberate or a
    build accident is not established; the reader trusts the file size over the
    field, and `validate-anim` reports the discrepancy rather than hiding it.

    Returns (scale, numFrames, frameSize, numBones, end_offset, header_size).
    """
    scale, num_frames, frame_size = struct.unpack_from("<f2i", data, 0)
    if num_frames >= 0 and frame_size >= FRAME_FIXED:
        end = ANIM_HEADER + num_frames * frame_size
        if end == len(data):
            num_bones = (frame_size - FRAME_FIXED) // BONEANIMFRAME_SIZE
            return scale, num_frames, frame_size, num_bones, end, ANIM_HEADER

    # 16-byte variant: an extra float before the counts.
    scale, _scale2, num_frames, frame_size = struct.unpack_from("<2f2i", data, 0)
    if num_frames < 0 or frame_size < FRAME_FIXED:
        raise ValueError("implausible header: frames=%d frameSize=%d"
                         % (num_frames, frame_size))
    body = len(data) - ANIM_HEADER_ALT
    if body % frame_size:
        raise ValueError("16-byte header leaves %d bytes, not a multiple of "
                         "frameSize %d" % (body, frame_size))
    actual = body // frame_size
    num_bones = (frame_size - FRAME_FIXED) // BONEANIMFRAME_SIZE
    return scale, actual, frame_size, num_bones, len(data), ANIM_HEADER_ALT


def frame_at(data, frame_size, index):
    """
    One frame: a root position followed by one BONEANIMFRAME per bone.

    UnpackAnimFrame (0x0006012c) reads three floats at +4 into a limeVECTOR3
    and then copies 20 bytes per bone starting at +0x10. The int32 at +0 is
    not touched by it.
    """
    base = ANIM_HEADER + index * frame_size
    tag = struct.unpack_from("<i", data, base)[0]
    root = struct.unpack_from("<3f", data, base + 4)
    bones = []
    off = base + FRAME_FIXED
    while off + BONEANIMFRAME_SIZE <= base + frame_size:
        bones.append(struct.unpack_from("<5f", data, off))
        off += BONEANIMFRAME_SIZE
    return tag, root, bones


def cmd_validate_anim(resdir):
    files = []
    for base, _d, names in os.walk(resdir):
        for n in names:
            if n.lower().endswith(".skinanim"):
                files.append(os.path.join(base, n))
    files.sort()

    ok = bad = 0
    total_frames = 0
    alt_header = []
    for p in files:
        data = open(p, "rb").read()
        try:
            _s, nf, fs, nb, end, hdr = parse_anim(data)
        except Exception as e:                                  # noqa: BLE001
            print("  %-30s ERROR: %s" % (os.path.basename(p), e))
            bad += 1
            continue
        if end == len(data):
            ok += 1
            total_frames += nf
            if hdr != ANIM_HEADER:
                claimed = struct.unpack_from("<i", data, 8)[0]
                alt_header.append((os.path.basename(p), hdr, nf, claimed))
        else:
            bad += 1
            print("  %-30s ends at %d, file is %d (frames=%d frameSize=%d bones=%d)"
                  % (os.path.basename(p), end, len(data), nf, fs, nb))

    print("")
    print(".skinanim files:  %d" % len(files))
    print("  exact:          %d" % ok)
    print("  mismatched:     %d" % bad)
    print("animation frames: %d" % total_frames)
    for name, hdr, actual, claimed in alt_header:
        print("  %s uses the %d-byte header and holds %d frames, "
              "though its count field reads %d" % (name, hdr, actual, claimed))
    return bad



# ---------------------------------------------------------------- .bones

BONE_SIZE = 25            # bytes per bone on disk, standard variant
BONE_SIZE_ALT = 24        # ROBO1 and ROBO2


def parse_bones(data):
    """
    Parse a .bones file: `int32 numBones` then one record per bone.

    **There are two variants.** Most files use a 25-byte bone; ROBO1_STANDARD
    and ROBO2_STANDARD use 24. Both divide exactly with no remainder across the
    whole corpus, so this is a second format variant rather than a parser bug --
    the same conclusion `.meshset` reached with its three variants.

    Those same two files also omit the leading block count in their `.skin`,
    ship a stub `.scene`, and have no `.events` or `.lighting`. One export
    variant, visible in four formats.

    Returns (numBones, boneSize, bones) with each bone as its raw record.
    """
    if len(data) < 4:
        raise ValueError("shorter than the 4-byte header")
    num = struct.unpack_from("<i", data, 0)[0]
    if num <= 0:
        raise ValueError("implausible numBones %d" % num)
    rest = len(data) - 4
    for size in (BONE_SIZE, BONE_SIZE_ALT):
        if rest == num * size:
            return num, size, [data[4 + i * size:4 + (i + 1) * size]
                               for i in range(num)]
    raise ValueError("numBones %d leaves %d bytes, neither %d nor %d per bone"
                     % (num, rest, BONE_SIZE, BONE_SIZE_ALT))


def cmd_validate_bones(resdir):
    files = []
    for base, _d, names in os.walk(resdir):
        for n in names:
            if n.lower().endswith(".bones"):
                files.append(os.path.join(base, n))
    files.sort()

    ok = 0
    total = 0
    variants = {}
    failures = []
    for path in files:
        data = open(path, "rb").read()
        try:
            num, size, _bones = parse_bones(data)
        except ValueError as e:
            failures.append((os.path.basename(path), str(e)))
            continue
        ok += 1
        total += num
        variants.setdefault(size, []).append(os.path.basename(path))

    print("files parsed:    %5d of %d" % (ok, len(files)))
    print("bones walked:    %5d" % total)
    for size in sorted(variants):
        names = variants[size]
        extra = "" if len(names) > 4 else "  -> %s" % ", ".join(names)
        print("  %d bytes per bone: %d files%s" % (size, len(names), extra))
    print("failures:        %5d" % len(failures))
    for name, why in failures:
        print("   %-32s %s" % (name, why))
    return len(failures)      # the file's convention: non-zero means failure


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        raise SystemExit(1)
    if sys.argv[1] == "validate":
        raise SystemExit(1 if cmd_validate(sys.argv[2]) else 0)
    elif sys.argv[1] == "validate-bones":
        raise SystemExit(1 if cmd_validate_bones(sys.argv[2]) else 0)
    elif sys.argv[1] == "validate-anim":
        raise SystemExit(1 if cmd_validate_anim(sys.argv[2]) else 0)
    elif sys.argv[1] == "dump":
        cmd_dump(sys.argv[2])
    else:
        raise SystemExit("unknown action")
