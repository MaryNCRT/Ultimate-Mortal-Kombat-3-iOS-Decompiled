"""Builds the ReShade that the DLSS 5 mod loads (src/runtime/neural_mod.c,
docs/DLSS5-MOD.md). Port tooling; not part of the game's own build.

ReShade (crosire/reshade, BSD-3-Clause) at a fixed tag, with full add-on
support (the solution's Release configuration, RESHADE_ADDON=2) and one change
for UMK3 PC: its banner is never drawn. ReShade runs inside the game as a
component, and its errors still go to ReShade.log. Nothing else is touched,
and the BSD notice travels with the DLL (LICENSE.md is copied beside it).

    python tools/neural/build_reshade.py <work folder> [<output folder>]

The work folder gets the clone (with its submodules); the output folder, by
default the work folder's out/, gets ReShade64.dll and ReShade-LICENSE.md.
It needs git, Python and Visual Studio 2017 or later (the Build Tools are
enough), with MSBuild found through vswhere.
"""
import os
import shutil
import subprocess
import sys

TAG = 'v6.8.0'
REPO = 'https://github.com/crosire/reshade'

# the one change: runtime_gui.cpp's splash (banner) window is never shown
OLD = '\tconst bool show_splash_window = _show_splash && '
NEW = ('\t// UMK3 PC (the DLSS 5 mod): ReShade runs inside the game as a component, so its\r\n'
       '\t// banner is never drawn; errors still go to ReShade.log.\r\n'
       '\tconst bool show_splash_window = false && _show_splash && ')


def run(*a, cwd=None):
    print('>', ' '.join(a))
    subprocess.run(a, check=True, cwd=cwd)


def msbuild():
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
                           'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    out = subprocess.run([vswhere, '-latest', '-products', '*', '-requires',
                          'Microsoft.Component.MSBuild', '-find', r'MSBuild\**\Bin\amd64\MSBuild.exe'],
                         check=True, capture_output=True, text=True).stdout.split('\n')[0].strip()
    if not out:
        sys.exit('MSBuild not found (install the Visual Studio Build Tools with C++)')
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    work = os.path.abspath(sys.argv[1])
    dest = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(work, 'out')
    src = os.path.join(work, 'reshade')
    if not os.path.isdir(src):
        run('git', 'clone', '--branch', TAG, '--depth', '1', '--recurse-submodules',
            '--shallow-submodules', REPO, src)
    gui = os.path.join(src, 'source', 'runtime_gui.cpp')
    with open(gui, encoding='utf-8', newline='') as f:
        text = f.read()
    if NEW not in text:
        if text.count(OLD) != 1:
            sys.exit('runtime_gui.cpp does not have the line this script changes: another ReShade version?')
        text = text.replace(OLD, NEW, 1)
        with open(gui, 'w', encoding='utf-8', newline='') as f:
            f.write(text)
    run(msbuild(), os.path.join(src, 'ReShade.sln'), '/t:ReShade', '/p:Configuration=Release',
        '/p:Platform=64-bit', '/m', '/v:minimal', '/nologo')
    os.makedirs(dest, exist_ok=True)
    shutil.copy2(os.path.join(src, 'bin', 'x64', 'Release', 'ReShade64.dll'), dest)
    shutil.copy2(os.path.join(src, 'LICENSE.md'), os.path.join(dest, 'ReShade-LICENSE.md'))
    print('ReShade64.dll and ReShade-LICENSE.md in', dest)


if __name__ == '__main__':
    main()
