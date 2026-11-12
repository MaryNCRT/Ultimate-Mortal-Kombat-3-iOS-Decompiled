# Steam integration: the shortcut and SteamGridDB artwork

The launcher's **STEAM** section (the menu's line before QUIT) puts the
launcher into the player's Steam library as a non-Steam game named
**Ultimate Mortal Kombat 3 IOs Decomp**. It also downloads that game's
artwork from SteamGridDB into the game folder and copies the chosen pictures
into Steam.

Status (2026-11-12): **built and tested against a fake Steam folder and a
fake SteamGridDB** (`launcher/app/test/steam.test.js`, 10 tests). It was also
tested in the launcher's page through the harness, against a copy of the
project machine's own `shortcuts.vdf`. **Not yet tested** against the
real SteamGridDB, because that needs the player's own API key. Nor has it
been tested on the real Steam client, which has to be closed for the
shortcut to be added. Nothing here is "done" until Mary has seen the game and its artwork in
her Steam library.

## Files

| File | What |
|---|---|
| `launcher/app/lib/vdf.js` | Valve's binary VDF (`shortcuts.vdf`) and text VDF (`loginusers.vdf`). Parse and write keep every node and its type in order, so an unchanged file writes back byte for byte. |
| `launcher/app/lib/steam.js` | Finds the Steam folders and accounts, checks whether Steam is running, computes the shortcut's app id, adds and removes the shortcut, and copies artwork into `config/grid`. |
| `launcher/app/lib/artwork.js` | The SteamGridDB API v2 client and the local library in `data/steam-artwork/`. |
| `launcher/app/lib/steamsync.js` | Glue behind the IPC (`steam:*`, `art:*` in `main.js`). |
| `launcher/app/ui/src/lib/sections/SecSteam.svelte` | The page: STEAM / STEAMGRIDDB / DOWNLOADED. |

## What Steam reads (checked, not assumed)

Checked on the project machine against a non-Steam shortcut that Steam was
showing with custom artwork. Its `shortcuts.vdf` and `config/grid/` were
read only:

- `Steam/userdata/<account id>/config/shortcuts.vdf`: the non-Steam games.
  Each entry holds `appid` (int32), `AppName`, `Exe` (quoted), `StartDir`
  (quoted), `icon`, `ShortcutPath`, `LaunchOptions`, `IsHidden`,
  `AllowDesktopConfig`, `AllowOverlay`, `OpenVR`, `Devkit`, `DevkitGameID`,
  `DevkitOverrideAppID`, `LastPlayTime`, `FlatpakAppID`, `sortas`, `tags`.
- The app id is `crc32(Exe + AppName) | 0x80000000`, with `Exe` including its
  quotes. This was checked against that shortcut: 2476573896.
- `Steam/userdata/<account id>/config/grid/` holds the artwork, named by
  that app id:

  | Slot | File | SteamGridDB |
  |---|---|---|
  | Portrait capsule | `<appid>p.png/.jpg` | grid 600×900 (342×482, 660×930) |
  | Wide capsule | `<appid>.png/.jpg` | grid 920×430 / 460×215 |
  | Hero | `<appid>_hero.png/.jpg` | hero |
  | Logo | `<appid>_logo.png` | logo |
  | Icon | `<appid>_icon.png/.ico`, with the shortcut's `icon` field pointing at it | icon |

  A grid goes to the portrait or the wide slot according to its shape.
- Only PNG and JPEG are requested from SteamGridDB (plus ICO for icons), and
  animated images are excluded (`types=static`). Every original can go into
  Steam as downloaded, with no conversion.

## Safety

- `shortcuts.vdf` is only written while Steam is closed. Steam keeps the
  list in memory and writes it on exit, which would undo the change. To
  check whether Steam is running, the launcher uses `tasklist` on Windows
  and the process names in `/proc` on Linux; a Flatpak Steam shows up there
  too. If that check fails, Steam counts as running.
- Before each write: a backup beside the file (`shortcuts.vdf.umk3-<time>.bak`,
  the last five kept). The new file is written to a temporary file and
  renamed over the old one, then read back and compared.
- Every other shortcut is written back byte for byte. The tests check this,
  including on the project machine's real file (read only).
- No duplicates: an entry that has our name or our executable is updated in
  place, keeping its own app id so its artwork stays. Extra copies are
  removed.
- Artwork can be copied into `grid/` while Steam is open. Steam shows the
  new pictures after a restart. Only the icon field needs Steam closed, and
  the page says when it is waiting for that.
- Removing the shortcut deletes only the grid files named with our app id.
  The downloaded originals stay in the game folder.

## SteamGridDB

- API v2, `https://www.steamgriddb.com/api/v2/{grids|heroes|logos|icons}/game/38752`.
  These are the endpoints of SteamGridDB's own npm client. 38752 is
  SteamGridDB's game id, not a Steam app id, and only that game is ever
  requested. Pages are fetched with `page=N`.
- **Automatic:** ADD TO STEAM first fills every empty slot with SteamGridDB's
  best-scored image of that kind for game 38752 (portrait grid, wide grid,
  hero, logo, icon). It downloads them and puts them in Steam with the
  shortcut, so the game arrives in the library with its artwork and the
  player chooses nothing. CHOOSE THE BEST does it again for every slot.
  Any image can still be changed by hand in STEAMGRIDDB / DOWNLOADED.
- **The project's key:** a release carries `TOOLS/steamgriddb.key` (one
  line), copied by `make_release.py` / `make_release_linux.py` from
  `UMK3_SGDB_KEY_FILE` or the project's `TOOLS/steamgriddb.key`.
  `steamgriddb.key` is git-ignored everywhere. With it, players never see a
  key box. Without it the package still works: the game is added without
  artwork. Order the launcher looks: the player's own key, then the
  project's, then `UMK3_SGDB_KEY`.
- The player can still use their own key (free, under SteamGridDB *Preferences >
  API*). It is typed into the page and saved in
  `data/steam-artwork/settings.json`, sealed by Electron's `safeStorage`
  (DPAPI / the keyring). It is never committed: `data/steam-artwork/` is
  git-ignored.
- Downloads are accepted only from `*.steamgriddb.com` over https. The file's
  own bytes must be PNG, JPEG or ICO, so an error page is never saved as an
  image. A second download of the same image uses the cached copy.

## In the game folder

```
data/steam-artwork/
  grids/ heroes/ logos/ icons/   originals, e.g. umk3-grid-123456-600x900-alternate.png
  selected/                      a copy of each chosen one: grid, wide, hero, logo, icon
  selections.json                downloaded images (url, size, sha1, author), choices, what Steam got
  settings.json                  the sealed API key, the chosen Steam account
```

To delete a downloaded image that is currently chosen, the page asks for
confirmation first.

## Privacy

The account list shows only the Steam name (PersonaName), never the account
id or SteamID. The id is used internally to find the folder.

## Linux

- Steam folders searched: `~/.steam/steam`, `~/.steam/root`,
  `~/.local/share/Steam`, the Flatpak's
  `~/.var/app/com.valvesoftware.Steam/.local/share/Steam` (and `data/Steam`),
  and the Snap's `~/snap/steam/common/.local/share/Steam`. A folder reached
  through a symlink is counted once.
- What Steam starts: `UMK3-Launcher` in the game folder, or the AppImage
  when the launcher runs as one.
- A Flatpak Steam can only start a game whose folder it can see. The page
  shows the one-time `flatpak override --user --filesystem=<folder>
  com.valvesoftware.Steam` command. **Not yet tried on a Linux machine.**

## Trying it without touching Steam

```bash
cd launcher/app
node --test test/steam.test.js
UMK3_HARNESS_STEAM=<a copy of a Steam folder> UMK3_HARNESS_STEAM_RUNNING=0 node scripts/ui-harness.js <game folder> 5179
```

Then open `http://localhost:5179/?sec=steam`.
