#!/bin/sh
# Adds the launcher to your desktop's application menu (a .desktop file in
# ~/.local/share/applications pointing at this folder). Run it again if you
# move the folder.
here=$(cd "$(dirname "$0")" && pwd)
dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$dir"
cat > "$dir/umk3-launcher.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=Ultimate Mortal Kombat 3 (PC port)
Comment=Builds and starts the PC port of UMK3 from your own .ipa
Exec="$here/UMK3-Launcher"
Path=$here
Icon=$here/umk3.png
Terminal=false
Categories=Game;
DESK
chmod +x "$dir/umk3-launcher.desktop"
echo "added: $dir/umk3-launcher.desktop"
