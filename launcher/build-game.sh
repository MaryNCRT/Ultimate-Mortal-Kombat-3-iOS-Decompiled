#!/bin/sh
# Builds the game from your own UMK3 1.2.59 .ipa without opening the
# launcher's window -- the same steps as its COMPILE YOUR .IPA button. Run it
# from anywhere; the game is built into this folder (it must be writable).
#
#     ./build-game.sh ~/Downloads/UMK3.ipa
#
# The launcher's Electron runs launcher/app/cli.js as plain Node: nothing else
# to install. See README-linux.md.
here=$(cd "$(dirname "$0")" && pwd)
ELECTRON_RUN_AS_NODE=1 exec "$here/UMK3-Launcher" "$here/resources/app.asar/cli.js" --root "$here" "$@"
