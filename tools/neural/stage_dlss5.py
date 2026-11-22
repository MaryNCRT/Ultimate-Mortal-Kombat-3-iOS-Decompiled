"""Stages what a release ships for DLSS 5 into launcher/dlss5/ (git-ignored),
which launcher/make_release.py packs and the launcher's compile copies to
graphics/dlss5 beside the game (launcher/app/lib/build.js). Port tooling.
src/runtime/neural_mod.c and docs/DLSS5-MOD.md describe the feature.

    python tools/neural/stage_dlss5.py <work folder>

What it puts there, each from its own project's official source and checked:
  - ReShade64.dll + ReShade-LICENSE.md: ReShade 6.8.0 built from source by
    build_reshade.py (BSD-3-Clause), the banner left out
  - dlss5-feed.addon64 and reshade-shaders/Shaders/DLSS5_Feed.fx: from
    DLSS5-Feeder 2.0.0's release zip on GitHub, its SHA-256 the one the
    release publishes; DLSS5-Feeder-LICENSE.txt (MIT) from the same tag
  - reshade-shaders/Shaders/ReShade.fxh: crosire/reshade-shaders at a fixed
    commit (CC0-1.0), checked by SHA-256
  - README.txt: what the folder is, and that NVIDIA's files are the player's

Never NVIDIA's runtimes or a neural consumer: those may not be
redistributed. The player adds them (the launcher's VIDEO > DLSS 5).
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

FEEDER_ZIP = ('https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/download/v2.0.0/DLSS5-Feeder-2.0.0.zip',
              'fa41fe2c5d3da803688775b802dc373a922daab0e721c8154a17066fcdd62584')
FEEDER_LICENSE = 'https://raw.githubusercontent.com/jlrouzies-fr/DLSS5-Feeder/v2.0.0/LICENSE'
FEEDER_FILES = {'dlss5-feed.addon64': 'dlss5-feed.addon64',
                'reshade-shaders/Shaders/DLSS5_Feed.fx': 'reshade-shaders/Shaders/DLSS5_Feed.fx'}
FXH = ('https://raw.githubusercontent.com/crosire/reshade-shaders/'
       'fd0022170615ce0d8162d219bff07232fa6dd84f/Shaders/ReShade.fxh',
       '6dabfbbaf968c3871905d2ea17f96572ff7b1cec01310b5d0e5252b66b30174f')

README = """DLSS 5 for UMK3 PC (src/runtime/neural_mod.c, docs/DLSS5-MOD.md)

The launcher's compile copies this folder to graphics\\dlss5 beside the game.
GRAPHICS > DLSS 5 in the game (or VIDEO > DLSS 5 in the launcher) turns it on.

Here, shipped with the game:
  ReShade64.dll        ReShade 6.8.0 by crosire, built for UMK3 PC (no banner)
                       -- BSD-3-Clause, ReShade-LICENSE.md
  dlss5-feed.addon64   DLSS5-Feeder 2.0.0 by Jean-Laurent Rouzies -- MIT,
  reshade-shaders\\Shaders\\DLSS5_Feed.fx   DLSS5-Feeder-LICENSE.txt
  reshade-shaders\\Shaders\\ReShade.fxh     crosire/reshade-shaders, CC0-1.0

Not here, and never shipped: NVIDIA's nvngx_dlss.dll and nvngx_dlssnr.dll,
and a neural consumer add-on (renodx-dlss5*.addon64 or
deep-fried-chicken.addon64). Their licences do not allow redistribution:
add them yourself with the launcher's VIDEO > DLSS 5 > ADD NVIDIA FILES.
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
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)

    # ReShade, built here
    rs_out = os.path.join(work, 'reshade-out')
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'neural', 'build_reshade.py'),
                    os.path.join(work, 'reshade-work'), rs_out], check=True)
    for n in ('ReShade64.dll', 'ReShade-LICENSE.md'):
        shutil.copy2(os.path.join(rs_out, n), os.path.join(OUT, n))

    # DLSS5-Feeder's release zip, checked, two files out of it
    z = os.path.join(work, 'DLSS5-Feeder-2.0.0.zip')
    data = checked(fetch(FEEDER_ZIP[0]), FEEDER_ZIP[1], 'DLSS5-Feeder-2.0.0.zip')
    with open(z, 'wb') as f:
        f.write(data)
    with zipfile.ZipFile(z) as zf:
        for src, dst in FEEDER_FILES.items():
            put(dst, zf.read(src))
    put('DLSS5-Feeder-LICENSE.txt', fetch(FEEDER_LICENSE))

    put('reshade-shaders/Shaders/ReShade.fxh', checked(fetch(FXH[0]), FXH[1], 'ReShade.fxh'))
    put('README.txt', README.replace('\n', '\r\n').encode('utf-8'))

    for d, _, files in os.walk(OUT):
        for n in files:
            p = os.path.join(d, n)
            print('  %-50s %9d' % (os.path.relpath(p, OUT), os.path.getsize(p)))
    print('staged in', OUT)


if __name__ == '__main__':
    main()
