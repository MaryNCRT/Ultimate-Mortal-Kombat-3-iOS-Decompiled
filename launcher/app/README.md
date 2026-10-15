# UMK3-Launcher (Electron)

Replaces the old plain-Win32 `launcher.c` with the same settings, build and
Play loop, drawn as an arcade-cabinet shell.  Everything is self-contained in
this folder's sources; Node.js is only needed to build and package it, not to
run it.

## Layout

```
launcher/app/
  main.js            Electron main: window, root detection, IPC (compile/play/config)
  preload.js         contextBridge API (window.umk3.*); renderer has no Node
  lib/ini.js         umk3.ini reader/writer, byte-for-byte the launcher.c format
  lib/root.js        finds the game folder (launcher\build_game.ps1 mark)
  lib/build.js       runs launcher\build_game.ps1, streams log + block progress
  renderer/          the arcade UI (HTML/CSS/JS, no framemarks/jQuery)
  test/              node --test unit tests for the config format
  build/icon.ico     launcher icon
```

## Run in development

```sh
cd launcher/app
npm install
npm start                 # uses the repo root as the game folder
```

The game folder is found by walking up from the working directory and from
the executable until `launcher\build_game.ps1` or `umk3-game.exe` is found;
`UMK3_ROOT` overrides it.  A portable exe extracts to a temp folder, so when
Windows has no recollection of the folder the player is asked once and the
choice is remembered in `%APPDATA%\umk3-launcher\root.json`.

## Tests

```sh
npm test                  # node --test test/*.js
```

The tests pin the `umk3.ini` format that `runtime/game_main.c:read_config`
expects (width/height/fullscreen/widescreen/language/debug_keys,
`key_<name>=<VK>` lines) and the launcher defaults from `launcher.c`.

> The game-side of `widescreen` is a DRAFT: `game_main.c` parses the key and
> `draw_gl.c` keeps the 2D in the centred 3:2 rectangle, but
> `LIMEDS_Set3dMode` is not widened yet, so a rebuilt game with
> `widescreen=1` stretches the 3D. See the top of `docs/PROGRESS.md`.

## Package it

```sh
cd launcher/app
npm run dist              # -> dist/win-unpacked/ (the app, unpacked)
```

`make_release.py <dist/win-unpacked> <out>` puts that folder in the release
as `launcher\bin\` and compiles `launcher/stub/umk3_launcher_stub.c` into the
`UMK3-Launcher.exe` at the top of the game folder. The stub sets `UMK3_ROOT`
and the working folder to the game folder, starts `launcher\bin\UMK3-Launcher.exe`
and exits.

**Why not the single portable .exe any more:** it unpacked ~270 MB to `%TEMP%`
at every start, 9.3 s before the window showed (measured 2026-10-15); unpacked,
the window shows in 0.7 s (about 5 s the very first time, cold). `npm run
portable` still builds the old single file.

One launch blocker to know about:
- `signAndEditExecutable` is off (winCodeSign needs admin rights to unpack);
  the window icon is supplied at runtime by the app itself.

## Design notes (the iOS-style brief, since 2026-10-09)

- Frameless midnight-blue/near-black window with a nebula background, now
  **resizable** (min 560x620) with a maximize/restore titlebar button; the
  layout is flex so it scales to any size.
- iOS-style dark menu: frosted grouped list (`backdrop-filter` blur, 14 px
  radius, hairline separators), the active row tinted iOS-blue with a glowing
  blue dot, caption MENU/OPCIONES etc, system font, footer "UMK3 IOS".
- All panels are frosted cards (no metal rivets); no Xbox button chips (status
  bar only).
- Buttons flip instantly to iOS-blue/gold on hover/focus, `transition: none`.
- Block-style progress bar (24 segments), step titles, streamed build log.
- Short synthesized beeps on hover/select (WebAudio), no audio assets.
- Settings are saved to `umk3.ini` on every change; **PANTALLA ANCHA (16:9)**
  writes `widescreen=1`. The 16:9 resolutions (1280x720 … 3840x2160) sit after
  the native 3:2 ones in the resolution list.