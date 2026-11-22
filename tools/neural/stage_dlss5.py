"""Stages what a release ships for DLSS 5 into launcher/dlss5/ (git-ignored),
which launcher/make_release.py packs and the launcher's compile copies to
graphics/dlss5 beside the game (launcher/app/lib/build.js). Port tooling.
src/runtime/neural_mod.c and docs/DLSS5-MOD.md describe the feature.

    python tools/neural/stage_dlss5.py <work folder>

What it puts there, each from its own project's official source and checked:
  - ReShade64.dll + ReShade-LICENSE.md: ReShade 6.8.0 built from source by
    build_reshade.py (BSD-3-Clause), the banner left out
  - standalone-dlssnr.addon64, nvngx.dll (its caller bridge, not NVIDIA's),
    reshade-shaders/Shaders/StandaloneBoundary.fx and licenses/: from DLSS5
    ReShade AIO 2.2.4's 64-bit release zip on GitHub, its SHA-256 the one
    the release publishes (Apache-2.0)
  - reshade-shaders/Shaders/DLSS5_AIO_Feed.fx: ours (launcher/neural/), made
    from AIO's to read the game's layers (Apache-2.0, marked modified)
  - reshade-shaders/Shaders/vort_Motion.fx and Includes/: VORT's motion estimation
    (vortigern11/vort_Shaders at a fixed commit, MIT), which AIO's guide pass
    needs for the layers' mask; motion blur and TAA stay off (its defaults)
  - reshade-shaders/Shaders/ReShade.fxh: crosire/reshade-shaders at a fixed
    commit (CC0-1.0), checked by SHA-256
  - README.txt: what the folder is, and that NVIDIA's files are the player's

Never NVIDIA's runtimes: they may not be redistributed. The player adds
them (the launcher's VIDEO > GRAPHICS > DLSS 5).
"""
import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'launcher', 'dlss5')

AIO_ZIP = ('https://github.com/kibblerz/DLSS5-Reshade-AIO/releases/download/v2.2.4/'
           'DLSS5-ReShade-AIO-v2.2.4-64-bit.zip',
           '38493d801646fce46c6caa5f0d9869c5079d09247c94368e163a7c5fb8fd38ee')
AIO_FILES = {'standalone-dlssnr.addon64': 'standalone-dlssnr.addon64',
             'nvngx.dll': 'nvngx.dll',
             'reshade-shaders/Shaders/StandaloneBoundary.fx': 'reshade-shaders/Shaders/StandaloneBoundary.fx',
             'licenses/DLSS5-ReShade-AIO-LICENSE.txt': 'licenses/DLSS5-ReShade-AIO-LICENSE.txt',
             'licenses/DLSS5-ReShade-AIO-NOTICE.txt': 'licenses/DLSS5-ReShade-AIO-NOTICE.txt',
             'licenses/THIRD_PARTY_NOTICES.md': 'licenses/THIRD_PARTY_NOTICES.md'}
VORT_ZIP = ('https://github.com/vortigern11/vort_Shaders/archive/'
            'b410b9f0c0fbb83c8cb42164aaf1655fab386f4a.zip', None)   # SHA-256 below
VORT_SHA = '231ba34a75556f9943e359559a89b0d0cc2caa322d9dcdee5630061bf9fe13b6'
VORT_PREFIX = 'vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/'
FXH = ('https://raw.githubusercontent.com/crosire/reshade-shaders/'
       'fd0022170615ce0d8162d219bff07232fa6dd84f/Shaders/ReShade.fxh',
       '6dabfbbaf968c3871905d2ea17f96572ff7b1cec01310b5d0e5252b66b30174f')

README = """DLSS 5 for UMK3 PC (src/runtime/neural_mod.c, docs/DLSS5-MOD.md)

The launcher's compile copies this folder to graphics\\dlss5 beside the game.
GRAPHICS > DLSS 5 in the game (or VIDEO > GRAPHICS in the launcher) turns it on.

Here, shipped with the game:
  ReShade64.dll                  ReShade 6.8.0 by crosire, built for UMK3 PC
                                 (no banner) -- BSD-3-Clause, ReShade-LICENSE.md
  standalone-dlssnr.addon64      DLSS5 ReShade AIO 2.2.4 by kibblerz, and its
  nvngx.dll                      caller bridge (not NVIDIA's) -- Apache-2.0,
  reshade-shaders\\Shaders\\StandaloneBoundary.fx   licenses\\
  reshade-shaders\\Shaders\\DLSS5_AIO_Feed.fx       AIO's, modified by UMK3 PC to
                                 read the game's layers -- Apache-2.0
  reshade-shaders\\Shaders\\ReShade.fxh             crosire/reshade-shaders, CC0-1.0

Not here, and never shipped: NVIDIA's nvngx_dlss.dll and nvngx_dlssnr.dll.
Their licence does not allow redistribution: add them yourself with the
launcher's VIDEO > GRAPHICS > DLSS 5 > ADD NVIDIA FILES.
"""


def fetch(url):
    print('fetching', url)
    with urllib.request.urlopen(url, timeout=120) as r:
        return r.read()


def checked(data, sha, what):
    got = hashlib.sha256(data).hexdigest()
    if got != sha:
        sys.exit('%s: SHA-256 %s, expected %s -- not staged' % (what, got, sha))
    return data


def put(rel, data):
    p = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'wb') as f:
        f.write(data)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    work = os.path.abspath(sys.argv[1])
    os.makedirs(work, exist_ok=True)
    global OUT
    final, OUT = OUT, OUT + '.new'           # staged aside, swapped in only when complete
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)

    # ReShade, built here
    rs_out = os.path.join(work, 'reshade-out')
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'neural', 'build_reshade.py'),
                    os.path.join(work, 'reshade-work'), rs_out], check=True)
    for n in ('ReShade64.dll', 'ReShade-LICENSE.md'):
        shutil.copy2(os.path.join(rs_out, n), os.path.join(OUT, n))

    # DLSS5 ReShade AIO's release zip, checked
    z = os.path.join(work, 'DLSS5-ReShade-AIO-v2.2.4-64-bit.zip')
    data = checked(fetch(AIO_ZIP[0]), AIO_ZIP[1], 'DLSS5-ReShade-AIO-v2.2.4-64-bit.zip')
    with open(z, 'wb') as f:
        f.write(data)
    with zipfile.ZipFile(z) as zf:
        for src, dst in AIO_FILES.items():
            put(dst, zf.read(src))

    # our guide shader (the layers), and ReShade.fxh
    with open(os.path.join(ROOT, 'launcher', 'neural', 'DLSS5_AIO_Feed.fx'), 'rb') as f:
        put('reshade-shaders/Shaders/DLSS5_AIO_Feed.fx', f.read())
    put('reshade-shaders/Shaders/ReShade.fxh', checked(fetch(FXH[0]), FXH[1], 'ReShade.fxh'))

    # VORT's motion (MIT): vort_Motion.fx and its includes, at a fixed commit
    data = checked(fetch(VORT_ZIP[0]), VORT_SHA, 'vort_Shaders zip')
    z = os.path.join(work, 'vort_Shaders.zip')
    with open(z, 'wb') as f:
        f.write(data)
    with zipfile.ZipFile(z) as zf:
        for name in zf.namelist():
            rel = name[len(VORT_PREFIX):]
            if name.endswith('/') or not (rel == 'Shaders/vort_Motion.fx' or rel.startswith('Shaders/Includes/')
                                          or rel.startswith('Textures/')):
                continue
            put('reshade-shaders/' + rel, zf.read(name))
        put('licenses/vort_Shaders-LICENSE.txt', zf.read(VORT_PREFIX + 'LICENSE'))
    put('README.txt', README.replace('\n', '\r\n').encode('utf-8'))

    for d, _, files in os.walk(OUT):
        for n in files:
            p = os.path.join(d, n)
            print('  %-50s %9d' % (os.path.relpath(p, OUT), os.path.getsize(p)))
    if os.path.isdir(final):
        shutil.rmtree(final)
    os.replace(OUT, final)
    print('staged in', final)


if __name__ == '__main__':
    main()
