# Ultimate Mortal Kombat 3 PC port on Linux

The release is a launcher, not the game: it builds the game on your machine
from **your own** `.ipa` of *Ultimate Mortal Kombat 3* for iPhone, and then
starts it. Nothing of the game is in the download.

## 1. What you need

- **The .ipa:** *Ultimate Mortal Kombat 3* **version 1.2.59** for iPhone
  (the file is about 300 MB, `UMK3.ipa` or similar), from a copy you own.
  Other versions are refused: the build checks the binary inside it.
- **x86-64 Linux** with a desktop (X11 or Wayland).
- **SDL2 and a graphics driver**, which almost every desktop already has.
  The game ships the SDL2 and OpenGL *headers* it compiles against, so the
  `-devel` / `-dev` packages are **not** needed. If SDL2 itself is missing:

  | Distribution | Command |
  |---|---|
  | Debian, Ubuntu, Mint, Pop!_OS | `sudo apt install libsdl2-2.0-0` |
  | Fedora, Nobara | `sudo dnf install SDL2` |
  | Arch, Manjaro, SteamOS | `sudo pacman -S sdl2` |
  | openSUSE | `sudo zypper install libSDL2-2_0-0` |

  OpenGL is loaded when the game starts (`libGL.so.1`, from Mesa or your
  GPU's driver); with the renderer set to **Vulkan** the game needs
  `libvulkan.so.1` instead (`vulkan-loader` / `libvulkan1`).
- The compiler (Zig) is downloaded by the launcher the first time, into
  `~/.local/share/umk3-launcher/toolchain` (about 60 MB). Nothing is installed
  system-wide.

## 2. Unpack it somewhere you can write to

The game is built **into the launcher's own folder**, and keeps its saves,
settings (`umk3.ini`), logs and mods there. So unpack it in your home, not in
`/opt` or anything owned by root:

```sh
tar -xzf UMK3-PC-<version>-linux-x64.tar.gz -C ~/Games
cd ~/Games/UMK3-PC-<version>-linux
```

## 3. Build and play

**With the launcher's window:**

```sh
./UMK3-Launcher
```

COMPILE YOUR .IPA, pick the file, wait for the five steps, then PLAY.
`./install-shortcut.sh` adds the launcher to your application menu.

**From a terminal (no window, scripts, CI):**

```sh
./build-game.sh ~/Downloads/UMK3.ipa
```

It runs exactly the launcher's build, prints each step, and exits 0 when the
game is ready. Then start the game itself:

```sh
./umk3-game
```

It must be started from its folder or by its full path: it reads `res/`
beside the executable (`res/` is what the build extracts from your `.ipa`).

## 4. Files

| | |
|---|---|
| `UMK3-Launcher` | the launcher (Electron) |
| `build-game.sh` | the build without the window |
| `umk3-game` | the game, once built |
| `res/` | the game's data, extracted from your `.ipa` |
| `umk3.ini` | settings: the launcher's and the in-game menu's (Esc) |
| `logs/` | the build's and the game's logs: attach them to an Issue |
| `mods/` | mods (the launcher's MODS section) |
| `eos.ini` | internet play; see `eos.ini.example` |

## 5. Renderer

GRAPHICS > RENDERER in the launcher, or Esc > GRAPHICS > RENDERER in the
game: **OpenGL** (the default) or **Vulkan**. Changed in the game, it takes
effect at the next start. If Vulkan cannot start, the game draws with
OpenGL and says so in its log.

## 6. Internet play

ONLINE over the internet uses Epic Online Services, 64-bit only. The
release brings the project's `eos.ini` when it has one; to use your own
product, see `eos.ini.example`. Without it ONLINE works on the local network.

## 7. When something fails

Open an Issue with the launcher's output (run it from a terminal) and the
`logs/` folder. A build that stops says at which of the five steps.
