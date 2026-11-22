"""Builds the DLSS5 ReShade AIO add-on that UMK3 PC ships for DLSS 5
(src/runtime/neural_mod.c, docs/DLSS5-MOD.md). Port tooling, not part of the
game's own build.

AIO (kibblerz/DLSS5-Reshade-AIO, Apache-2.0) at a fixed tag, with
tools/neural/patch_aio.py applied, which keeps the game's interface out of
DLSS 5:
  - NVIDIA's UI correction on (DLSSNR.UICorrection, with the input as
    DLSSNR.UI and DLSSNR.Backbuffer): NVIDIA's automatic mask stays in charge
    of the scene, and the pixels the game marks as interface are put back
    after the neural pass;
  - the final compositor draws those pixels from the original on top of
    the neural pass and DLAA, so the HUD, texts and menus are a layer over
    everything;
  - both only with [Standalone.DLSSNR] Umk3UiProtect=1 in ReShade.ini (the
    game sets it). The game marks the interface in the picture's alpha
    (render/rgl_ff.c rgl_layers).

    python tools/neural/build_aio.py <work folder> [<output folder>]

It needs git, Python and the Visual Studio Build Tools (cl through
vcvars). The NGX headers and import library come from NVIDIA's public DLSS
SDK repository at a fixed commit; they are used to build the add-on, never
copied into this repository, and are under NVIDIA's RTX SDKs licence (the
add-on is a separate program that ReShade loads; the game does not link it).
The Vulkan headers come from this repository (src/runtime/platform/
vulkan-include, Apache-2.0 / MIT).
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AIO_REPO, AIO_TAG = 'https://github.com/kibblerz/DLSS5-Reshade-AIO', 'v2.2.4'
DLSS_REPO, DLSS_COMMIT = 'https://github.com/NVIDIA/DLSS', '9981c6350dc58c0ce21f024fd65e31e851c31689'


def run(*a, cwd=None):
    print('>', ' '.join(a))
    subprocess.run(a, check=True, cwd=cwd)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    work = os.path.abspath(sys.argv[1])
    dest = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(work, 'out')
    os.makedirs(work, exist_ok=True)
    aio = os.path.join(work, 'aio')
    sdk = os.path.join(work, 'nvidia-dlss')

    if not os.path.isdir(aio):
        run('git', 'clone', '--branch', AIO_TAG, '--depth', '1', AIO_REPO, aio)
        run('git', 'submodule', 'update', '--init', '--depth', '1', 'external/DLSS5-Feeder', cwd=aio)
    if not os.path.isdir(sdk):
        run('git', 'clone', '--filter=blob:none', '--sparse', '--no-checkout', DLSS_REPO, sdk)
        run('git', 'sparse-checkout', 'set', 'include', 'lib/Windows_x86_64/x64', cwd=sdk)
        run('git', 'checkout', DLSS_COMMIT, cwd=sdk)

    feeder = os.path.join(aio, 'external', 'DLSS5-Feeder', 'external')
    ngx = os.path.join(feeder, 'ngx')
    os.makedirs(os.path.join(ngx, 'libs'), exist_ok=True)
    for n in os.listdir(os.path.join(sdk, 'include')):
        if n.endswith('.h'):
            shutil.copy2(os.path.join(sdk, 'include', n), ngx)
    shutil.copy2(os.path.join(sdk, 'lib', 'Windows_x86_64', 'x64', 'nvsdk_ngx_d.lib'), os.path.join(ngx, 'libs'))
    vk = os.path.join(ROOT, 'src', 'runtime', 'platform', 'vulkan-include')
    for n in ('vulkan', 'vk_video'):
        shutil.copytree(os.path.join(vk, n), os.path.join(feeder, 'vulkan', n), dirs_exist_ok=True)

    run(sys.executable, os.path.join(ROOT, 'tools', 'neural', 'patch_aio.py'),
        os.path.join(aio, 'addon', 'src', 'nr-standalone.cpp'))

    # cmd.exe will not run a batch file through a path with spaces as the
    # build expects: build through a junction with none
    link = os.path.join(tempfile.gettempdir(), 'umk3-aio-build')
    if os.path.lexists(link):
        os.rmdir(link)
    run('cmd', '/c', 'mklink', '/J', link, aio)
    try:
        log = os.path.join(link, 'addon', 'build_out.txt')
        subprocess.run([os.environ.get('ComSpec', 'cmd.exe'), '/c',
                        os.path.join(link, 'addon', 'build.bat') + ' > ' + log + ' 2>&1'])
        built = os.path.join(link, 'addon', 'build', 'standalone-dlssnr.addon64')
        if not os.path.exists(built):
            sys.exit('the add-on did not build; see ' + log)
        os.makedirs(dest, exist_ok=True)
        shutil.copy2(built, dest)
    finally:
        os.rmdir(link)
    print('standalone-dlssnr.addon64 (UMK3 PC build) in', dest)


if __name__ == '__main__':
    main()
