# Project Status & Progress

**Last updated:** 2026-10-31 (release 0.1.23 beta: FRIENDS in the game's ONLINE menu, published as the pre-release `v0.1.23-beta`) · Release history: [CHANGELOG.md](CHANGELOG.md) · How to pick up the work: [HANDOFF.md](HANDOFF.md) · How the game is built: [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md)

This file has two parts. **Current state** says where everything stands
today, for someone who knows nothing of the project. **Detailed log** is
the record written as the work was done, newest first: every finding with
its binary addresses and the checks that were run. A later entry can
correct an earlier one; the current state is always the summary to trust.

---

## Current state

### The decompilation

| Area | Functions | Transcribed (`src/decomp/`) | Checked by |
|---|---:|---:|---|
| **LIME 3D engine** (`src/decomp/lime/`) | 109 | 109 (100%) | differential tests against the recompiled oracle (`tests/test_*_diff.c`) |
| **Front end & scenes** (`src/decomp/gamecode/`) | 291 | 291 (100%) | running natively (51 front-end screens, HUD, audio, blood) |
| **Arcade fight engine** (`src/decomp/gamecode/logic/`) | 2,172 | 2,172 (100%) | file-by-file differential tests (`tools/decomp/difftest/`), 229 data tables by `ctest` |
| **Total** | **2,572** | **2,572 (100%)** | `tools/decomp/progress.py` |

Every function was written from the armv7 disassembly of the player's own
copy of the game (`OUTPUT/armv7/UMK3.armv7`), never from other sources
([AGENTS.md](../AGENTS.md)). Findings made since are fixes to individual
transcriptions, each one checked against the binary before it went in --
the most recent: `repell_func` (2026-10-30, below).

### The PC port (`src/runtime/`)

Playable from start to end on Windows and Linux. What the port provides
around the decompiled game:

- **Renderers:** OpenGL, Vulkan, Direct3D 12 (`src/runtime/render/`; Vulkan and
  Direct3D 12 through one fixed-function layer, `rgl_ff.c`), HD textures, MSAA,
  real-time shadows, VSYNC, pipeline caches.
- **Screens:** 3:2 (the iPhone's), 4:3 (the iPad build's layout), 16:9,
  21:9; the port's own menus (settings, debug, STAGE SELECT, online,
  replays) fit each.
- **Sound:** a Win32 mixer and an SDL one, MP3 music, the game's 497 WAVs.
- **Input:** keyboard and pads, remappable for fights and menus.
- **Online:** LAN and internet through Epic Online Services, rollback
  netcode, input delay from the ping.
- **Replays:** every local fight recorded and played back with a player.
- **Smoothing:** SMOOTH ANIMATIONS and SMOOTH MODELS (Blender's code,
  `src/thirdparty/blender`, which makes the project GPL).
- **Mods:** `mods\<mod>\` overlays on the game's files, `data.ini` over the
  fight tables; online uses only models and textures from mods.
- **Debug menu** (`F2`): fighters (bosses included), stages, rounds,
  hitboxes, save states, logs.

### The launcher (`launcher/`)

Electron + SvelteKit (`launcher/app/ui`, built with `npm run ui`). It
compiles the game from the player's `.ipa` with a bundled `zig cc`, keeps
`umk3.ini`, updates itself from GitHub releases, and has the MODS section:
MOD MANAGER, MOD HUB (mods from the companion repository
MaryNCRT/UMK3-IOS-Mod-Hub, installed and updated), CHARACTER VIEWER (with REPLACE / EDIT MODEL: retargeting,
weight paint, Blender-style bones, shape keys, corrective animation
layers), STAGE VIEWER (the engine's stage playback, Blender-style element
editing with keyframes and shape keys, texture replacement), CHARACTER
EDITOR, TEXTURES and EXPORT (FBX, GLB, USD, OBJ).

### What the tester has and has not played

Mary plays every build in a test folder (`WORK\release-0.1.12\UMK3-PC-0.1.12-beta`
on her machine) before it is released. **Played and confirmed:** everything
up to 0.1.10b (the Linux package, the Vulkan and Direct3D 12 renderers),
REAL-TIME SHADOWS' note on its own row in 0.1.13. **Not yet played as a whole:** 0.1.11 to
0.1.18; she has tried pieces of them as she reported problems (STAGE
SELECT's music and CANCEL, the 4:3 menus, the stage editor, the MOD HUB,
the kombo branch and the KOMBOS page, Noob Saibot on the roster, EXPORT ALL and the settings categories, which she confirmed work), each
marked in the log below.

### Open problems

| Problem | State |
|---|---|
| A fight intro can drop a frame when smoothing two fighters takes more than a tick | **closed**: Mary no longer sees it (0.1.18) |
| Friendships and animalities can repeat a death scream | **closed**: Mary no longer sees it (0.1.18); the `death scream:` log line still names the handler |
| A fighter cannot be duplicated into a new slot | **closed** from the list by Mary (0.1.18); not built |
| Losing to Motaro from the debug menu softlocks | **closed**: Mary no longer sees it (0.1.18) |
| The stage editor's SCULPT showed "500 Internal Error" once | **closed**: Mary no longer sees it (0.1.18); the guards stay |
| Slight slide in a kombo (issue #114) | **the original's rule**, checked instruction by instruction (`t_cc_*_punch` → `t_ccp3` → `t_separate_us`); nothing to fix |
| After CANCEL in STAGE SELECT the select screen came back silent once | fixed by never stopping the tune on the way; the log says `stage select: the tune ...` if it happens again |

### Where to look next

The [HANDOFF](HANDOFF.md) lists how to build, test, verify and release.
New work comes from Mary's reports (GitHub issues and her logs); each is
traced to the binary's own mechanism before anything is changed.

---

## Detailed log (newest first)

## FRIENDS in the game's ONLINE menu (2026-10-31, released in 0.1.23)

Mary: the invites belong in the game's ONLINE menu, with the friends' names
and icons, and JOIN there. Played: works.

- **Launcher** (main.js): `writeGameFriends` writes `friends.txt` beside
  umk3.ini with every `pollFriends` answer ("invite|CODE|name|look",
  "friend|CODE|name|look"; removed for a guest); `gameAsked` reads the
  game's `online_invite.txt` every two seconds while it runs ("invite CODE"
  -> `POST /v1/invite`, "join CODE" -> `DELETE /v1/invite`).
- **Server** (private repo 3fdcda8): the friends' and invites' rows carry
  `look`, the synced profile's fields after the name. `deploy.cmd`: the
  tests, then `wrangler deploy`.
- **Game:** gamecode_stubs.c `port_online_friend` / `_look` / `_tell`;
  FrontEnd.c `PortOnlineFriends` (four rows, invites first; INVITE writes
  "invite", JOIN writes "join" and finds the code as FIND MATCH);
  online_ui.c draws each row's badge (slots 1..4; `ui_badge_at` now has 8).

## 0.1.22: invites, friend requests, the late press's sounds, mods' bones online (2026-10-31, released in 0.1.22)

Mary: a friend's name was not found; she wanted notices of requests,
REMOVE EMAIL and the recovery code in a window; Nightwolf "mute, only air"
when he hits; the guest's screen black after an online fight.

- **Not found:** the friend's account had no `pcode` / `pname` (his
  launcher predates 0.1.21, `claim()` never ran). The Worker now takes both
  from every `PUT /v1/progress` (`adoptProfile`), and migrates the database
  itself on the first request after a deploy (`migrate()`: columns,
  tables, then the profile of every account that lacks one). Private repo,
  commit a536d7c; no migration is run by hand any more.
- **Recovery code:** kept sealed (`accounts.recovery_seal`), `GET/POST
  /v1/recovery`; the launcher shows it in its own window (main.js,
  `recoveryWindow`). Accounts older than this have none: a new one for the
  password.
- **Requests:** `pollFriends` (main.js) every minute; `account:requests`
  sets `app.frIn` (the red dot, MainMenu and SubNav `dot`).
- **"Nightwolf mute":** an online fight, Nightwolf the other player's. His
  attack voice (voice group 2, sounds 119/120, Dfatt1/2) was asked for 32
  times and played 0: the frame that brought his late press only ever ran
  inside a rollback, and `limePlaySound` skipped every sound while
  `rb_quiet`. `rb_sound_ok` (rollback.c) keeps the sounds each frame played;
  a frame run again plays the new ones. His character sound 1 (`st_indian`
  row 1) is "" in the binary too (0x17a7cc), left as it is.
- **Desyncs:** Mary's 2026-10-30 fight: 78 desyncs; the day before, none.
  Her Sonya mod has a `.bones` (KEEP PROPORTIONS). `mods_online` already kept
  `.bones` out, but the menu's Sonya was loaded with it and the fight reused
  her. `port_skin_bones_online` (skin_mats.c, from `mods_online(1)`) puts the
  game's offsets back and drops the root height. The guest's black screen
  is likely this (the two machines ended different fights); not confirmed
  online yet.
- **The guest's STAGE SELECT:** the guest waited in `Task_GameInit`
  (`rb_host_level`) with nothing drawn. The host sends the stage it shows
  (`PORT_PKT_STAGE` 24, six frames apart, debug_menu.c `st_tell_guest`);
  the guest draws the same window without buttons (`stage_menu_watch_draw`,
  game_main.c) and waits on while packets come (two seconds without one
  ends it).
- **Invites:** the Worker's `invites` table (`POST/DELETE /v1/invite`,
  `invites` in `GET /v1/friends`, three minutes, friends only; private repo
  3c44d13). main.js `startGame`: `--screen 51` for both, `UMK3_ONLINE_FIND`
  for the one who joins; a game already running reads `online_join.txt`
  (gamecode_stubs.c `port_online_join_code`, FrontEnd.c FE_Task_Online).
  `pollFriends` every 15 s.
- **FPS:** fights froze 250 to 900 ms as SMOOTH MODELS' mesh arrived:
  `adopt` ran `umk_subsurf_set_sharp` (a full CCG sync) on the game's
  thread. It runs in `build` now. Stages' first-frame events still pause
  (STREET up to 1.3 s), not looked into.
- **Loading:** the first menu took 6.5 s; a mod's model was baked again at
  every load (`port_bake_lighting`, kept now by skin file, its time logged).
  The launcher's `init` asks for glyphs, logo and hero at once.
- **Not tried online with two players.** Published at Mary's request.

## 0.1.21: friends, live leaderboards, no help from the debug menu (2026-10-31, released in 0.1.21)

Mary asked for friends (found by profile name or by a code tied to the
account), unique profile names, the scores sent as soon as a run ends
(a friend's run had not shown until he closed the game), new profile names
shown on the boards, a run with the debug menu's help left off the boards,
and ACCOUNT and FRIENDS moved into PROFILE. She played it and confirmed.

- **Server** (private repo, commit bbb83a4; migration `0002_friends`,
  applied to the live database). `accounts.pcode` / `pname`, unique
  (`pname` under NOCASE). `PUT /v1/profile {code, name}`: the code is stored
  once and returned after; a taken one is `code_taken`; a taken name leaves
  the stored one and answers `taken`. `GET /v1/users?q=` (the code exactly,
  or a name prefix of 2+ characters, 10 rows). `GET/POST/DELETE
  /v1/friends`: a row is "src asked dst", both rows are a friendship. The
  boards show `COALESCE(pname, scores.name)`.
- **Launcher** (`lib/account.js`). `claim()` after every pull and after the
  game: sends the profile's id and name; takes the stored code back into
  profiles.ini; a `code_taken` (two accounts on one PC) gets a new id; a
  taken name goes back to the stored one. `rename()` from PROFILE's name
  field (on change). `friends`, `findPlayers`, `addFriend`, `removeFriend`.
  `live()` every 10 s while the game runs (`main.js`, `stepAside`):
  `flushScores` then `boards`, written to `leaderboards.txt.tmp` and renamed.
  UI: `SecProfile.svelte` with `PROF_TABS` (PROFILE, ACCOUNT, FRIENDS;
  `ProfAccount.svelte`, `ProfFriends.svelte`); the conflict at PLAY opens
  PROFILE > ACCOUNT.
- **Game** (Port). `gamecode_stubs.c`: `mayhem_load` reads
  `leaderboards.txt` again when its time or size changed; a post with
  `PortDbgTainted` set is dropped (and logged). `game_main.c`:
  `PortDbgTainted` set by any debug request but SAVE STATE, by `fight_key`,
  and in a fight while INFINITE HEALTH, FREEZE CLOCK or another speed is on
  (`dbg_taint_watch`, each frame); cleared when `GameMode` changes and when a
  run starts (a Karnage fight; Survival's first, `SurvivalStage == 0`).
- **Checked:** Worker tests (profile, search, friends, board names);
  launcher tests (20 pass); Mary in game and launcher.

## 0.1.20: the game's leaderboards on the account server (2026-10-31, released in 0.1.20)

Mary asked for the leaderboards back, now that accounts exist; then for the
profile name instead of the account's, the frame fixed on wide screens and
the gamepad handled as ACHIEVEMENTS does. She played it and confirmed.

- **Game** (`gamecode_stubs.c`, Port, not in the binary). The front end's
  calls into EA's Mayhem stay where they were; their answers now come from
  two files beside umk3.ini that the launcher carries to and from the
  server. `leaderboards.txt` (`user=<profile name>`, `board=<key>`, then
  `<name>|<score>` rows, best first, 100 at most) makes
  `EASDK_ConnectedToNetwork` 1; without it (a GUEST) it is 0 as before, and
  `FE_Task_Select_Leaderboard` shows its own no-internet modal.
  `EASOC_MayhemPostStatWithData` (called by the Karnage and Survival
  summaries with `shaokahn_med` / `survival_easy`) appends
  `<key> <score>` to `scores.txt` and places the score in the board in
  memory. `EASOC_MayhemGetLeaderBoard` calls
  `FE_DrawLeaderBoardEntriesCallback` once a row (ranks 1..8 on the page)
  and returns the page's row count -- 7 for a full last page, since every
  period but 5 enables Next for any full page. `game_main.c` hands the
  folder over with `mayhem_set_dir(g_ini_path)`.
- **Front end** (`FrontEnd.c`, Port). `FE_Task_LeaderboardsSK` and
  `FE_Task_Leaderboards`: the `FEBits3` frame is `FE_W(468) + PortFEExtraX`
  wide, as in `FE_Task_Achievements` (the callback puts the score 60 in from
  the screen's right edge, which fell outside a 468-wide frame on 16:9); with
  `PortHideControls` no buttons, `PortMovesTurn` for the pages,
  `PortPagesExit` to leave and `PortPagesLine` for the hints.
- **Launcher** (`lib/account.js`, `main.js`). `boards()` before PLAY
  (`/v1/scores/<key>?limit=100`, both boards; a guest's file deleted, an
  offline one kept); `flushScores()` when the game exits, before the
  progress push, the best per board posted with the profile's name; what
  fails stays in `scores.txt`. Log-out and deleting the account delete
  `leaderboards.txt`. Both files are git-ignored.
- **Server** (private repo, commit 856237c): `scores.name` (migration
  `0001_scores_name`, applied to the live database), the profile name the
  boards show, the username when there is none.
- **Checked:** Worker tests; launcher tests (20 pass); boards / flushScores
  against the live server with a throwaway account (posted, best kept,
  deleted with the account); Mary in game: boards, profile name, frame,
  gamepad.

## 0.1.19: MULTIPLAYER over LAN, accounts and cloud progress (2026-10-30, released in 0.1.19)

Mary asked for accounts (username, password, optional email, a recovery
code, hashed and encrypted secrets, progress synced across devices, the way
back to leaderboards), a GUEST mode without leaderboards, a warning against
personal emails and reused passwords, and PLAY > MULTIPLAYER working again
as ONLINE does but over the LAN alone, Radmin VPN included. She played all
of it in her test folder and confirmed.

- **MULTIPLAYER** (Port, not in the binary). `FE_Task_Multiplayer`
  (0x0001782c) is the iPhone's Bluetooth lobby; its peer list came from
  `getNumberOfPeers` / `getPeerNumber` / `connectToPeer`
  (limeMPGameSpecific.c, 0x000afbcc / 0x000afac4 / 0x000afbfc), which
  returned nothing. The PLAY menu now calls `mpnet_set_lan_only(1)` before
  `startMP` for MULTIPLAYER and `0` for ONLINE. In LAN-only mode
  `mpnet_session_create` (mp_net.c) skips EOS; each game broadcasts a
  `K_HELLO` with its profile every second and lists who it hears (dropped
  after 3.5 s of silence); `connectToPeer` asks that game by its code with
  the usual `K_JOIN`, also sent straight to the address it was heard from.
  From there it is the online fight (GameMode 1, rollback). Every broadcast
  (HELLO and JOIN, ONLINE's too) now goes to 255.255.255.255 **and** to each
  adapter's own broadcast address (`SIO_GET_INTERFACE_LIST` on Windows,
  `getifaddrs` on Linux): Windows sends the limited broadcast down one
  adapter only, which missed Radmin VPN's 26.0.0.0/8.
- **Accounts** (launcher only; the game is unchanged). `lib/account.js`
  talks to a Cloudflare Worker + D1 database at
  `https://umk3-accounts.umk3-accounts.workers.dev`, whose code is in
  Mary's private repository (MaryNCRT-Universal-Game-Services,
  `umk3-accounts/`; she asked for the server to stay out of this public
  one). Server side: PBKDF2-SHA256 (100,000 rounds, 16-byte salt) sealed with
  AES-GCM under a Worker secret for passwords and recovery codes; AES-GCM
  for emails; SHA-256 of session tokens; a 15-minute lock after ten wrong
  passwords; limits per IP; every account-bound route takes the account
  from the session, never from the request. Client side: `account.ini`
  (git-ignored, never packed) keeps the user, the session token sealed by
  Electron's safeStorage, and the last synced version and hash; no
  password. Synced: the `profile=` line of profiles.ini and the save files
  `savedata`, `achievementsdata`, `statsdata`, `survival`, `towerdata`,
  `unclaimedtreasure*`. `pull` before PLAY, `push` when the game exits;
  conflicts (both changed) are settled on the ACCOUNT page, the replaced
  side copied to `save/backup/<date>/`.
- **Checked:** the Worker's own tests (every route, against node:sqlite
  standing in for D1); the launcher client against that server on one
  machine with two simulated PCs (upload, download with the profile id,
  conflict, resolution, guest refused by the leaderboards, no password in
  account.ini); the deployed server with a throwaway account (made, email
  sealed and read back, progress uploaded, deleted). Mary: account, sync
  and MULTIPLAYER in her test folder.
- **Not done (then):** the game's own leaderboards; done on 2026-10-31,
  section above.

## 0.1.18: Noob Saibot on the roster, EXPORT ALL, settings by category (2026-10-30, released in 0.1.18)

Mary asked for five things; all played in her test folder and confirmed.

- **Noob Saibot.** `SetupLockedCharacters` (FrontEnd.c) already sets
  `CharacterAvailable[23] = 2`; `CS_Layout` / `CS_Layout2` (0x00101630,
  0x001016a0) just have no cell for 23. `drawCharacterSelection` now writes
  23 into the empty cell [3][6] when `port_noob_saibot` (umk3.ini
  `noob_saibot=`, read in game_main.c, written by the launcher's ini.js and
  the Esc menu) is on and `GameMode` is neither 1 (online: both grids must
  match) nor 2 (the one-character mode), and -1 otherwise. Port, not in the
  binary.
- **Online and character mods.** No code change: `mods_online(1)`
  (runtime/mods.c, from `rb_online_begin`, classic netcode included) puts
  every data.ini word back and serves only models and textures. The
  CHARACTER EDITOR's note (`edOnline`) said the opposite and now says this.
- **Esc menu categories** (settings_menu.c): `PAGE_VIDEO`, `PAGE_GFX`,
  `PAGE_TEX`, `PAGE_GAME` instead of one GRAPHICS page; the game's own video
  panel (`g_framed`) still shows them all on one page. GAME holds the
  interface rows and NOOB SAIBOT ON THE ROSTER.
- **Resolutions** sorted by width then height in `k_res` (settings_menu.c),
  `RES` (lib/ini.js, app.svelte.js); the ini stores width and height, so
  saved settings keep their size.
- **TEXTURES: EXPORT ALL** (ModTextures.svelte `exportMany`): the shown
  fighter's or stage's textures, or the whole group's, 16 at a time through
  `exportSave`.
- Also looked at, nothing changed: Mary's replay
  `20261030-185205_sonya_vs_smoke.umkr`, where human Smoke (CPU) only hit
  Sonya at FINISH HER and raised his arms. `t_drone_execute_fatality`
  (0x000707cc) offers mercy only with `H[0] > 0 && H[1] > 0` (both won a
  round; this fight was 2-0), else picks fatality 1 or 2 on a coin toss;
  `_ochar_fatalities2[22]` is `t_non_violent_finish`, a plain hit. The
  original's behaviour.

## 0.1.17: kombos that share a start, CHARACTER EDITOR KOMBOS (2026-10-30, released in 0.1.17)

Mary: Mileena's HK, HK, D, F + LK and HK, HK, U + LK, U + HK start alike and
the game always gives the first.

- **Traced to the binary.** `combo_scan_a11` (0x326d8) walks the 0x18-byte
  entries of the next list and stops at the first whose button (word 2's
  low byte, a switch id) was pressed; the stick is not read.
  `ct_mileena_knee3` (0x1659dc) holds two entries, both switch 4 (LK): the
  first goes on to `ct_mileena_dummy1`, the second to `ct_mileena_knee4`,
  which no input reaches. A scan of the 114 `ct_*` lists finds the same in
  `ct_tusk_knee3`, `ct_sonya_roundh`, `ct_swat_knee2`, `ct_kano_post_elbow2`
  and `ct_sonya_elbow3`. The original behaves so; nothing was mistranscribed.
- **Port rule** (`mkcombo.c`, marked in the code): on a match, with up held
  (the stick word `joystick_in_a0_px` reads, bit 1), the next entry of the
  list with the same button is taken. No proc field is touched; without up
  the scan is the binary's.
- **Switch ids**, from `_swtab` (0x16f10c) and checked against known kombos
  (Liu Kang HP, HP, BL, LK, LK, HK, LK; Jax HP, HP, BL, LP, HP): HP 0, LP 1,
  BL 2, HK 3, LK 4; 6 to 9 the stick.
- **CHARACTER EDITOR → KOMBOS** (`ModEditor.svelte`): from
  `ochar_elbow_combos` (HP starter) and `ochar_knee_combos` (HK starter),
  every path through the lists, a list running on into the next table in
  address order as the binary's does (`ct_swat_knee2` continues in
  `ct_swat_elbow2`). Each hit's button (word 2, low byte) and strike (word
  4, bits 8+, an index into `combo_strike_table`) go to `data.ini`. The
  strike's damage is shown, not edited: the `stk_combo*` rows are shared by
  every fighter. Checked against all 24 fighters with a script over
  `build/logic_tables.c`.
- **Played by Mary** in her test folder: both work.

## 0.1.16: MOD HUB (2026-10-30, released in 0.1.16)

Mary asked for a hub inside the launcher where players download the mods
that she and her collaborators publish, installed into `mods\` and
updatable.

- **Storage:** the public repository MaryNCRT/UMK3-IOS-Mod-Hub. `index.json`
  on `main` lists each mod (id, folder, name, author, version, category,
  description, thumbnail, url, size, sha256); its files are a `.zip`
  attached to the release `<id>-v<version>` (HD NINJAS is ~920 MB, past
  git's 100 MB limit; a release file may be 2 GB). Its `publish.js` zips a
  mod folder, creates the release with `gh` and writes the entry; its README
  says how collaborators publish. Published: HD NINJAS, Sonya Blade MKX,
  Rain (from Mary's test folder).
- **Launcher:** `lib/hub.js` reads the index (an entry whose download is not
  a release of that repository is dropped), downloads with
  `lib/update.js`'s `download()` (SHA-256 checked), unpacks it itself
  (zlib, each file CRC-checked: Linux's GNU tar cannot read a zip, which
  the PR's Linux CI caught),
  replaces `mods\<folder>` and adds it to `mods.ini` on, at the top (an
  update keeps its place and on/off). What came from the hub is kept in
  `mods\hub.json` (id, folder, version); UPDATE shows when the index's
  version is newer by `compareVersions`. IPC `hub:list`, `hub:install`
  (by id, from the list fetched last); page `sections/mods/ModHub.svelte`;
  the UI harness answers both.
- **A mod already here** (Mary's report: she had all three by hand and the
  HUB offered DOWNLOAD): a mod counts as installed when `hub.json` names it
  or `mods\<folder>` exists. Its files are compared with the zip's central
  directory, read with HTTP ranges (GitHub's release storage answers 206;
  it refuses suffix ranges, so the index's `size` gives the offsets): sizes
  when the list loads, CRC-32 on UPDATE / CHECK, which downloads only the
  entries that differ (each its own range, inflated and CRC-checked) and
  deletes files the zip lacks (`mod.meta.xml` kept). A folder whose
  `mod.ini` has no version is judged by its files only.
- **Small window** (Mary's report: the HUB cut off on the right): `.app`'s
  grid had no column size, so the MODS tab row, one tab longer, made the
  column wider than the window. It is `minmax(0, 1fr)` now and the tab row
  wraps when it does not fit; at full width nothing changes.
- **Checked:** `node --test test/*.js` (new `test/hub.test.js`: index
  parsing and filtering, installed/update status, `mods.ini` registration,
  a real zip installed and then updated in place, a bad hash refused).
  **Played by Mary** in her test folder (her mods detected, update, small
  window): works.

## 0.1.14 to 0.1.15: retarget, proportion trick, own materials, stage pictures, replay fixes, STAGE VIEWER, shape keys (2026-10-29 to 2026-10-30, released in 0.1.14, 0.1.14b and 0.1.15)

Branch `claude/viewer-tabs`. Built into
`WORK\release-0.1.12\UMK3-PC-0.1.12-beta` for Mary to try; **not played
yet**. Asked by Mary after trying the first round:

- **Replays were not recorded right and did not show in the launcher.**
  Two causes, found in her logs and files:
  - Arcade's rival still changed after Task_GameInit step 42 (her
    `20261029-162545_liukang_vs_stryker.umkr` keeps Stryker, 5, in its
    setup, and the log loads LIUKANG vs KITANA, 15). `rb_fight_rival` is
    now also called at step 48 with `PLAYER2MODEL`, right before
    `LoadLevelCharacters`: the recording keeps (and is named after) the
    rival loaded; a replay puts it back there.
  - The launcher's REPLAYS page accepted only `UMK3RPL3` files; every
    recording since the rival fix is `UMK3RPL4` (`lib/replays.js`).
- **Replay HIDE is a toggle**: the click that hid the player also brought
  it back on release (it only stayed hidden while held). Now a new click
  is needed (`game_main.c`, `g_rp_hide_wait`).
- **STAGE SELECT**: smaller buttons that no longer cover the names, a
  RANDOM button, and a picture for every stage: `STAGE_<SCENE>.PNG` (a mod
  or res, for pictures added later), else the game's CS_BG sheet, else the
  stage **rendered by the launcher** (`mods/stagepics.js`, once per stage
  into `aditional textures/stages/<SCENE>.png`, when the game is ready),
  else one drawn in the game (a gradient with the name).
- **REPLACE MODEL**:
  - The bar at the foot (Enter / Esc / F11) no longer shows over the
    preview window (`app.overlay`).
  - **Every material gets its own texture** (`<skin>_M<n>`, and its
    `_ICE`): a material with no picture is its colour (FBX DiffuseColor,
    GLB baseColor, else grey); THE GAME'S is a copy of the outfit's. So the
    model's UVs are kept and no texture other models share is touched
    (Mary's Sonya mod had turned other models black; the cause was not
    found in the game's draw code, this removes any shared texture from
    the path).
  - The materials' textures are listed in REPLACE TEXTURE after the model
    is saved.
  - **Fighters drawn black with a large model, found and fixed.** Mary's
    Sonya (6,009 faces) unwelds to ~18,000 vertices; the binary's skinning
    scratch holds 15,000 vertices and 3,333 faces (`SkinnedVerts`,
    `RenderIndexes`, `RenderUVs/RGBs/Verts`), so `DrawSkinnedMesh2` wrote
    past them into the animation frames and palette. And the fighter's
    baked `STATICLIGHTING/<name>.lighting` is sliced by the skin's own
    vertex count: read past its end, colours came from the heap. Now the
    scratch holds `PORT_SKIN_VERTS` (65,536, a uint16 index's reach;
    `lime.h`), and a lighting file that does not fit the skin is replaced by
    its average light (`Players.c port_fit_lighting`, logged `flat light`).
    The importer refuses a block over 65,535 vertices once unwelded.
  - **STAGE VIEWER, stages as the engine plays them (Mary 2026-10-30; built
    into the test folder, not tried by Mary yet).** The MODEL VIEWER is now
    the CHARACTER VIEWER and stages have their own page
    (`mods/stagerun.js`, `StageViewer.svelte`). What was missing: a stage's
    `.events` fire effect scenes (Scislac's whole sky is `SCISLAC_VORTEX`,
    the Street's newspapers, the Kave's lens flares, the Pit's blades), and
    the viewer, the export and STAGE SELECT's pictures only drew the scene.
    Now, read from the code: each tick AnimateBG advances the frame
    (looping or held per LEVEL_INFO +0x20) and fires the keys on it,
    LIME_UpdateEvents runs each spawned effect (step, repeat, repeat2,
    delay, cap, group, following), LIME_RenderScene's rules draw every node
    (key above 0.03; 0.97 and over opaque, ALPHA... blended without depth
    writes, ATST... clipped at 0.9; below 0.97 additive, no culling), an
    effect tinted by its track's colour and lit from its `.lighting` plus
    the level's ambient when its `.lights` says so; the Temple's 32 window
    glow slices (RenderLevelBG). Each level's place, scale (the Street's
    0.010651, not 0.0133), loop flag, ambient and second scene come from
    LEVEL_INFO in the binary. Unlit, raw colours, on black. A click picks an
    element: its properties, ISOLATE (numpad /), FRAME (F). The export
    writes everything drawn over the stage's length, each spawn its own
    object, with the material's blend (GLB alphaMode/extras + unlit, FBX
    opacity, USD opacity / threshold). STAGE SELECT's launcher pictures are
    made again this way (`.v3`). Checked by eye in a local test page:
    Scislac, Street, Temple, Pit.
  - **Stage viewer, second pass (Mary 2026-10-30):** Jade's Desert had no
    sky and Cyrax blinked. The sky: the game's projection has no far plane
    (`CreatePerspectiveMatrix` puts 2fn/(n-f) where w is taken: depth stays
    under 0.5), so its sky ~870 units out is drawn in the game; the viewer
    and pictures cut at 400. Now 100,000. Cyrax: `DESERT_CYRAX` is a
    12-mesh flipbook, and each mesh drew untextured for the frame its
    texture took to arrive: every texture of the stage and its effects is
    loaded first and meshes are kept, not rebuilt. Also as the engine: an
    effect wrapping back draws frame 0 (not the last), depth test GL_LESS
    (coplanar pieces no longer swap). All 17 stages checked by eye in a
    local page. Scislac Busorez is not in LEVEL_INFO (the game never loads
    it); its events are the main menu's vortex sphere, textured by
    DrawVortex3D's code, so it shows black. Pictures redone (`.v4`). STAGE SELECT's
    list moves with the mouse wheel (a row a notch; `stage_menu_mouse`).
    CHARACTER VIEWER: VIEW > STAGE BEHIND puts a stage behind the fighter
    as the fight has it (ModViewer.svelte `setStage`): the StageView of
    stagerun.js playing at 30 a second, on black, the floor hidden, the
    fight's camera (GAME CAMERA button), log depth for the far skies; its
    textures marked sRGB there, as the fighter's are in that view. Not
    yet seen by Mary.
  - **The slide mid-kombo (issue #114, Mary 2026-10-30: "a slight slide";
    checked against the binary, one real difference fixed in
    `mkrepell.c`; built into the test folder, not tried yet).** The issue's
    own log names the rule: `separate us: character 13, punches 3, hits 1,
    edge 0`. That is the punch chain, and every step of it is the binary's,
    read instruction by instruction: `t_cc_hi_punch` (0x415c4) / `t_cc_lo_punch`
    (0x41604) set 6 / 4 and 5 / 3; `t_ccp3` (0x477bc) takes the smaller
    number when the one hit is the machine (`am_i_joy`: proc+0x10 bit 0) or
    a player holding away, and compares the punch count at proc+0x4c, which
    `t_r_hi_punch` / `t_r_lo_punch` bump (`inc_p_block`) and only
    `back_to_normal_px` clears -- so in a kombo, where the one hit never gets
    back to normal, the 3rd low punch (4th high) against the machine sends
    him to `t_separate_us` (0x43250): `t_stumble_back_vel` at 3.0 a tick, or
    at a wall the attacker is moved instead. The corner rule
    (`t_avoid_corner_trap`, 0x47b50, 1 hit within 103 of the level limit)
    also matches. `tools/decomp/factdiff.py` over `mkreact.c`, `mkcombo.c`
    and `other.c` against `armrecomp` transcriptions, the handler
    addresses resolved by name: no store, call or handler differs but the
    port's diagnostics and hooks. So the slide is the original's (the issue
    thread found it in videos of the original too). **One real difference,
    fixed:** `repell_func` (0xa51e4), which pushes two fighters closer than
    60 apart at 3.0 a tick: when the first vertical test passed with y1 at
    or past `Pp+0x40`, the binary goes on to the second test (0xa5246 ->
    0xa52b8) and the C skipped to the arbitration, so a fighter over the
    other (a juggle) was not pushed apart. Checked with the binary's own
    code: `recompiled.c` from `armrecomp` and the C on 200,000 random
    states, 3,278 differed before, 0 after (the test is a scratch harness:
    `tools/decomp/difftest` makes no test of a function that only reads
    globals, and its build no longer compiles mk3logic.h without
    `-include savestate.h`). The note in `mkrepell.c` doubting which global
    the second test reads is settled: `_Pp`'s slot, 0x000f3158, both times.
  - **Unbroken select-screen music into STAGE SELECT, a smaller ESC menu
    on 4:3, every mod in TARGET MOD, TEXTURES in the STAGE VIEWER (Mary
    2026-10-30; built into the test folder, not tried by Mary yet).** After
    CANCEL the select screen came back silent. The tune now never stops on
    the way: `Task_FEDestroy`'s stop and the front end's fade to the fight
    are skipped when STAGE SELECT comes next (`stage_menu_keeps_tune`), the
    window only sets the volume, and on the first front-end frame after
    CANCEL `stage_menu_fe_frame` starts `CharacterSelect.mp3` again if it is
    not playing (`plat_music_playing`, new in both audio backends); the log
    says which happened (`stage select: the tune ...`). The ESC menu on 4:3
    ran off both sides: it is drawn over the whole 16:9 window while the 4:3
    picture shows the middle, so `draw_ios` now sizes the box from the
    picture's shape (`port_aspect`, game_main.c) and draws it at 90% below
    3:2. TARGET MOD was a `<datalist>`, which lists only the names that
    start with what is in the box; `ModTarget.svelte` lists every mod under
    a button (character viewer, TEXTURES). The STAGE VIEWER has a TEXTURES
    tab: the stage's textures with pictures, the one picked large, REPLACE
    TEXTURE into a mod (with LOW / LITE / ICE copies) and the stage opened
    again with it; a texture name in PROPERTIES opens it there.
  - **STAGE SELECT: the select screen's music, CANCEL goes back (Mary
    2026-10-30; built into the test folder, not tried by Mary yet).** The
    window opens after `Task_FEDestroy`, which had stopped the tune, so
    turning the volume up played nothing: `st_music` now starts
    `CharacterSelect.mp3` again at the player's volume, as `Task_FEMain`
    does, and stops it on a pick, as `Task_FEDestroy` had. CANCEL (and
    Esc / the pad's back) went into the fight on the game's own stage; it
    now goes back to the select screen: `CurrentTask = 2` (`Task_FEInit`,
    as after a fight, with the loading picture already made), the select
    screen's pop on the way out undone (`PendingPop` / `PendingPopAll`, for
    training), and the tune plays on (the front end's load latches
    `FETuneSelection` to it). Online the host's CANCEL still takes the
    game's own stage, since the guest is waiting on it.
  - **Shape keys for fighters, a fuller timeline, 4:3 fixes, stage-select
    music (Mary 2026-10-30; built into the test folder, not tried by Mary
    yet).** REPLACE / EDIT MODEL has a SHAPE KEYS tab: Basis (the model as
    the game draws it) and named keys with a value slider, a key button and
    mute; SCULPT grabs the vertices under the brush with a smooth falloff
    into the key picked, worked out in the skin's rest space so it holds in
    any pose (`ModelImport.svelte` shapeDeltas / posedDeltas); with an
    animation chosen the values are keyed per clip frame (I, Alt+I, REC,
    linear / bezier). Kept in `<skin>.import.json` (`shapes`); the game does
    not draw shape keys. `KeyTimeline.svelte` is now Blender-like:
    transport (first, previous key, play, next key, last), a frame field,
    a tick a frame where they fit, a Summary row, the playhead with its
    frame, the wheel steps a frame. The editor windows are put on the page's
    top (`lib/portal.js`), so the launcher's bar and the player's card no
    longer show over them; the menu's description box shrinks a text that
    does not fit (`Desc.svelte`; MODS' English ran out of it). Mary saw
    "500 Internal Error" after SCULPT in the stage editor: not reproduced in
    a browser with the same data (`launcher/app/scripts/ui-harness.js`
    serves the UI with the preload's calls over HTTP); the window now
    refuses any navigation (`will-navigate`) and SCULPT's drag prevents the
    native drag, the likely cause, but this is unconfirmed. Game: the ONLINE
    code box drew in the overlay's 3:2 space and ran off a 4:3 screen; the
    menu now hands over where it drew it (`PortOnlineBoxPx`, FE_X/FE_Y/
    FE_W/FE_H) and `online_ui.c` maps it as `ui_nav_frame` does. The
    settings panel in 4:3 sized its box from the last 2D pass's width (the
    whole 16:9 window): `draw_ios` now takes the width from its own
    viewport, and shrinks the panel if it is still wider. STAGE SELECT
    plays the select screen's tune at the player's volume while it is open
    (the select screen had faded it to 0) and quiets it again for the
    loading (`st_music`, debug_menu.c). Built: `npm run ui`, the game
    (x64, test folder), the test folder's app.asar.
  - **STAGE VIEWER editing, as Blender (Mary 2026-10-30; built into the
    test folder, not tried by Mary yet).** `mods/stageedit.js`,
    `StageEditor.svelte`, `KeyTimeline.svelte`. A picked stage element (the
    scene's objects and the Temple's glow; effects are spawned and die with
    their events, so they are not editable) is moved, turned and scaled in
    place with G / R / S (X / Y / Z an axis, again its own, Shift a plane,
    typed numbers, click or Enter keeps, Esc or right button drops) or the
    MOVE / ROTATE / SCALE handles. I keys the placement on the frame shown,
    Alt+I deletes the frame's keys, REC keys every change, LINEAR / BEZIER
    (Blender's auto-clamped) sets new keys and the frame's own, Ctrl+Z /
    Ctrl+Y. The play bar's slider is now a dope-sheet timeline (drag to
    scrub, a diamond row for the placement and each shape key). The edit
    rides on top of the engine's animation about the mesh's centre, as the
    CORRECTIVE ANIMATION LAYERS ride on the fighters'; a change on a keyed
    element that is not keyed goes when the frame changes. EDIT opens the
    element in a model editor window (its mesh, texture, vertices,
    wireframe, UVs over the texture, exact T / R / S fields, the same keys
    and timeline) with SHAPE KEYS: Basis plus named keys, a value slider,
    a key button and mute each; SCULPT (Tab) grabs vertices with a smooth
    proportional falloff into the key picked (radius [ ] or the wheel while
    dragging). Kept a stage in `<game>/stage edits/<ID>.json` (IPC
    `stageedits:load/save`), reloaded with the stage. The export writes them:
    placement baked into each bone's frames, the Basis edits into the
    vertices, shape keys as GLB morph targets with a `weights` animation
    (`extras.targetNames`), FBX BlendShape / BlendShapeChannel / Shape with
    an animated DeformPercent, USD BlendShape prims bound through
    `skel:blendShapes` and SkelAnimation `blendShapeWeights`. Checked:
    `npm run ui` builds; `node launcher/app/test/stageedit.test.mjs` (curves,
    keys, shapes, the three exporters' shape data). Not checked: the files
    opened in Blender or pxr (not on this machine), and the editor by hand.
  - **STAGE SELECT named the Street twice (and the Balcony, the Kave).**
    Levels 4, 10 and 11 have a second scene and the fight starts in it
    (Task_GameInit step 51, CurrentScene = 1): the Subway, the Soul Chamber,
    the Lair. The select now names and pictures a level by that scene
    (`debug_menu.c st_scene`), so all 16 are different.
  - **EDIT MODEL shows the T-pose too** (T-POSE / ORIGINAL POSE at rest, as
    the viewer); bones can be edited in either.
  - **0.1.14b beta published** (2026-10-30, pre-release `v0.1.14b-beta`,
    zip and Linux tar.gz): the 0.1.14 packages with the new `src/`,
    CMakeLists.txt and launcher (`0.1.14-beta.b`, read by the updater as
    0.1.14b). Nothing in it played by Mary yet.
  - **Liu Kang's scream that never stops: found, Mary 2026-10-30 (built
    into the test folder, not tried yet).** Her words: it repeats for ever.
    The Windows mixer (`platform/win32_audio.c`) kept each voice's place in
    the sound as 16.16 in 32 bits, which ends at 65,536 frames (4.1 s at
    16 kHz). Three of the game's 497 sounds are longer: Liu Kang's death
    scream `Lkdeath.wav` (67,093 frames, 4.19 s), `Skdiemix.wav` and
    `Vpdeath.wav`. Past the end of the range the place wrapped to 0, never
    reached the file's end, and the voice played on without stopping. The
    place is 64-bit now. Also checked on her replay's log
    (`20261030-013216_liukang_vs_shaokahn`): every voice the game asks for
    is asked once (`group_sound` 0x580a4 and `t_do_flip` 0x30634 match the
    binary instruction for instruction; the jump yell is one a diagonal
    jump, as on the device); his hit screams `Lkwast1` / `Lkwast2` are
    twice anyone's length (2.31 s / 1.94 s). Linux's SDL mixer gave a sound
    played again a second voice on top of the first; it now starts it over
    as `-[Sound play]` does and as the Windows mixer already did.
  - **Arms and hips still stretched: the file's weights; SMOOTH WEIGHTS,
    Mary 2026-10-30 (test launcher repacked, not tried yet).** Measured on
    her Sonya against the game's own, edge by edge over every frame (p99 of
    the stretch from the bind, by the bone a vertex hangs on): the bones
    and their lengths and directions match the game's within 0-3 degrees,
    and her file's weights are kept exactly, but those weights jump: no
    finger weights, the hands holding 12% of the vertices up the forearm,
    the right upper arm 0.7% (its sleeve hung on the clavicle and the
    forearm). Upper arm L 4.57 against the game's 1.95, forearm R 4.81
    against 1.50, thigh R 3.77 against 2.14, pelvis 4.97 against 3.05.
    Tried and dropped, as measured: Blender's Preserve Volume (dual
    quaternions; no gain, the stretch is not twist), copying the fighter's
    weights (Data Transfer; worse, her coat is not Sonya's shape),
    weighting again by distance (arms better, chest and neck far worse).
    Kept: SMOOTH WEIGHTS, as Blender's Smooth Vertex Weights (factor 0.5,
    neighbours by triangle and across UV seams, painted vertices held):
    3 passes bring those to 2.87, 3.20, 2.82, 3.85 with nothing worse; 8
    empties the chest. 3 by default on a file just picked; a model opened
    again keeps what it was saved with (hers: 0, so it needs the slider).
  - **The `.lighting` bake decoded; a mod's model gets its own, Mary
    2026-10-30 (built into the test folder, not tried yet).** In a fight a
    fighter is lit from `STATICLIGHTING/<X>.lighting`, not live. A REPLACE
    MODEL skin has another vertex count, so `port_fit_lighting` gave it one
    flat grey: no shading. The file turned out to be LightVert on each
    skinned normal, every frame, one byte an unwelded vertex
    (`docs/formats/LIGHTING.md`); checked byte for byte on Sonya, Scorpion
    and Liu Kang. `port_bake_lighting` (Players.c) now bakes it at load for
    a mod's skin (`port_skin_custom`): GenerateMatrices for each frame, then
    DrawSkinnedMesh2's lit path with the shipped lights, put back after.
    Automatic for every imported model, and it follows the mod's own
    `.skinanim`.
  - **Still crushed: a GLB's bone names lost their dots, Mary 2026-10-30
    (test launcher repacked, not tried yet).** Her Sonya GLB (the
    exporter's 49 bones, through Blender) showed 13/49 bones and 1688 of
    3337 vertices weighted automatically: three.js's GLTFLoader takes '.',
    ':', '/' and brackets out of node names, so clavicle.L came in as
    clavicleL and only the 13 names without a dot matched; arms, hands and
    legs were weighted again by distance. The importer now reads each
    bone's name from the file's own node (checked on her file: clavicle.L,
    upperarm.L, ... all 49). A mod saved before this needs EDIT SAVED MODEL
    and saving again.
    - REPLACE MODEL's player got the viewer's: SMOOTH (blended between
      frames), the frame number, SPEED, PLAY from the start at the end.
    - Not changed (not asked): `<skin>.source` keeps every file picked
      beside the model; hers is 135 MB of unrelated pictures.
  - **A custom model crushed at the elbows and wrists in the moves, Mary
    2026-10-30 (test launcher repacked, not tried yet).** KEEP PROPORTIONS
    put the fighter's bones at the model's joints but kept the fighter's
    bone frames: a T-pose model on an A-pose fighter played with its arms
    45 degrees off, and each twist of a forearm or hand turned about an
    axis beside the model's limb, shearing it. Now each bone's frame is
    turned with the model's limb (the turn from the fighter's segment to
    the model's; ends and bones the file lacks take their parent's), the
    skin is bound to those frames and the offsets are written in them, as
    a retarget in Blender binds a mesh to bones along its own limbs. The
    moves' turns, the same in the world as the fighter's, then carry the
    model's limbs where the fighter's go, whatever its rest (T, A, ARM
    ANGLE). Checked by hand on an arm chain; `proportions` in
    `mods/modelin.js`.
  - **Bones moved and turned as Blender does; POSE A fixed, Mary
    2026-10-30 (test launcher repacked, not tried yet).**
    - **The gizmo** on the bone picked (Blender's Move / Rotate /
      Transform tools, three.js `TransformControls`): an arrow moves
      along one axis, a square on a plane (2D), the middle freely on the
      view (3D); a ring turns about one axis, the white one about the
      view, inside it the trackball. MOVE / ROTATE / MOVE + ROTATE (the
      default) / NONE, and GLOBAL or LOCAL (the bone's own axes).
    - **G / R as Blender's modal transform:** X / Y / Z an axis, again
      the other orientation, again none; Shift+X / Y / Z the plane
      without it; a middle-button drag picks the axis; typed numbers
      (Tab the next field, - the sign); Ctrl steps of 1 unit / 5 degrees
      (Shift 0.1 / 1); Shift alone ten times finer; R twice the
      trackball; Enter, Space or a click keeps it, Esc or the right
      button puts it back. Along an axis the point under the pointer is
      taken (the closest point of the axis to the ray), as Blender does,
      not the view's plane.
    - **CLEAN A POSE** left a model without a skeleton in T: the angle was
      measured from the file's arms, which such a model has none of. It
      now measures from the fighter's arms there (the ones it turns).
  - **The file read as Blender reads it; the gizmo under the buttons, Mary
    2026-10-30 (test launcher repacked, not tried yet).**
    - **The gizmo:** a click on an axis did nothing (the click looked for
      where the ends were drawn on a fresh list that had none); it now
      looks down that axis. It sits under the view's buttons (RIG,
      FLOOR, SOLO) instead of over them (`ViewNav` `below`).
    - **FBX as Blender's importer:** each model's RotationOrder (Maya's
      ZXY and the rest; it was always XYZ), GlobalSettings' axes (a Z-up
      file stands up), every LimbNode as a bone (end bones, bones with no
      vertices; their rest from the BindPose, else the scene), and no
      made-up normals when the file has none (worked out from the faces).
      The order and axes go through `globalM` and the clusters, so a
      take imported from the same file (REPLACE ANIMATION) moves the same
      way.
    - **GLB:** shape keys at the mesh's weights, and a part hung under a
      bone without a skin (helmet, weapon) moves with that bone.
    - Checked in node: a Z-up file's up and front land on Y and +Z; ZYX
      order turns X to +Z for (90, 0, 90). The game's skeleton is fixed,
      so bones the fighter has no match for still move with the nearest
      matched bone above them.
  - **0.1.14 beta: REPLACE MODEL like Blender, Mary 2026-10-30 (built,
    not tried yet).**
    - **Published** as v0.1.14-beta (pre-release, 2026-10-30): the 0.1.13
      zip and tar.gz with the repository's src/, CMakeLists.txt and the
      new app.asar put in (the Windows and Linux asar are the same
      files); everything else as 0.1.13 shipped it. The hand repacks had
      left out `build/icon.ico` (the launcher window's icon,
      package.json `build.files`); it is back in.
    - **Bones and weights while posed:** a clip's frame shown (or
      playing: it holds while a bone is moved, paint goes on as it
      plays) no longer jumps to the rest. A move is taken back to the
      rest through the parent's turn in that frame, a turn about its
      axis through the bone's (`editDeltas`), so the game plays what
      was set.
    - **Blender's camera** in the importer and the viewer
      (`mods/viewnav.js`): the navigation gizmo (click an axis to look
      down it, again for the other side, drag to orbit), the numpad
      (1 / 3 / 7, Ctrl the other side, 9, 4 / 6 / 8 / 2, 5) or the top
      row's digits, Auto Perspective (an axis view is orthographic;
      orbiting off it goes back), the view's name bottom left. Checked
      in node with a stubbed DOM: the views keep the distance, ortho
      in and out, 9 the opposite.
    - **EDIT MODEL** in the viewer: the fighter's skin as it is (the
      mod's model, by its parts and textures; `fighterDesc` `parts`)
      opened in REPLACE MODEL's window as an exporter GLB, no file to
      pick. With a `<skin>.source` it opens that instead.
    - **The file's own rig by default:** a model with a skeleton
      (humanoid or the exporter's names) opens with KEEP PROPORTIONS,
      its own bones. With the exporter's names that now keeps its
      joints too (`proportions` for any mapped rig).
    - **A model without a skeleton:** POSE T / POSE A / DEFAULT turn
      the fighter's arms to the model's pose before its vertices are
      weighted (`bareAdjust`, `fighterAngles`).
    - **The preview as the game plays it:** the game reads the mod's
      `<skin>.bones` the same way (offsets, root height on Y only,
      `skin_mats.c` / `RenderSkinned.c`). The window played the game's
      animations, though, where the fight plays the mod's .skinanim
      (REPLACE ANIMATION, corrective layers). It now plays the mod's
      (`gameFighter`: the game's bones and skin, the mod's .skinanim).
  - **REPLACE MODEL: undo, bone rotations, clean T / A, edit a saved
    model, a model over another, Mary 2026-10-29 (built, not tried yet).**
    UNDO / REDO (Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z, and two buttons): every
    change in the window (joints, rotations, paint strokes, angles, scale,
    fit / keep, turn, materials) is kept before it is made, a slider's run
    as one, up to 100. Each bone has Blender's LOCATION and ROTATION X / Y /
    Z fields (a slider and a number): the rotation turns its children
    about its joint, about the game's X, then Y, then Z, parents first
    (`modelin.js movedJoints` `o.turns`), mirrored to the other side (X
    the same, Y and Z the other way); R with an axis adds to that field.
    RESET: CLEAN T POSE / CLEAN A POSE drops every change and turns the
    arms level or 45 degrees down from the new model's own rig (its arms
    when keeping proportions or with the exporter's names, else the
    fighter's joints it is fitted to). **A model over an earlier one**
    (Mary: the rig and the weights were laid over the old ones): the
    window was given the viewer's fighter, which carried the mod's .skin
    and .bones, so the new model was fitted to the old one's bones; it is
    now given the game's own fighter (`store.svelte.js gameFighter`, layer
    ''). **EDIT SAVED MODEL**: SAVE now also keeps the model it was made
    from in the mod, `<skin>.source` (the file, what was beside it and the
    pictures chosen for its materials; `packSource` / `unpackSource`,
    checked round trip), and the viewer, on a mod that has one, opens it
    in the window again as `<skin>.import.json` says it was set, to save
    over it. `.source` counts as the mod's own file in mod.meta.xml.
  - **Bones and weights by hand in REPLACE MODEL, Mary 2026-10-29 (built,
    not tried yet).** Mary: move and turn them with the mouse, as Blender
    does. RIG tab, EDIT BONES: a click on a joint's dot picks it, a drag
    moves it; G moves the joint picked and R turns it (its children go
    round its joint, the joint itself stays), X / Y / Z keep to that axis
    (drawn through it in Blender's colours), a click or Enter keeps it,
    Esc or the right button puts it back; MOVE (G) / ROTATE (R) on the
    side do the same. Both end as BONE POSITIONS' offsets (the other side
    mirrored), so they are saved as before. WEIGHT PAINT: the bone picked
    is shown as Blender's weight colours (blue 0, red 1) and painted with
    a brush (ADD, SUBTRACT or Ctrl, SMOOTH; RADIUS in screen pixels, [ and ]
    change it; STRENGTH); the other bones are scaled so each vertex adds
    up to one, a vertex with no other bone gives the rest to the bone's
    parent. Painted weights are kept per vertex of the file
    (`buildSkin o.paint`, `res.src` maps each written vertex back to the
    file's) and saved in `<skin>.import.json`; CLEAR PAINT drops them.
    Editing and painting stop a playing clip and work at rest. The camera
    is no longer put back after each rebuild.
  - **Second pass on REPLACE MODEL's rig, Mary 2026-10-29 (built, not
    tried yet).** The side has two tabs, RIG and MATERIALS. RIG: FIT TO THE
    FIGHTER / KEEP PROPORTIONS, the file's pose, AUTOMATIC T / A POSE now
    **off by default** (Mary: it moved models that were fine), the arm and
    leg angle and roll sliders whenever the bones are known (a humanoid rig
    or the exporter's own names; with the names the model itself is
    turned), SCALE about 0,0,0, and **BONE POSITIONS**: a joint picked by
    clicking its dot on the view or from the list, moved in X/Y/Z, its
    other side mirrored (MIRROR L / R), each alone with its children left
    (`modelin.js movedJoints`; the offsets go into `<skin>.bones`), RESET
    BONE / RESET ALL, and RECOMPUTE WEIGHTS (every vertex to the nearest
    bones, the file's weights left). The rest shown is the one the skin is
    bound to (`buildSkin` returns it). The animation bar under the preview
    never appeared: `pf` was not reactive in Svelte 5 -- fixed, the game's
    clips play on the imported model. What the window was set to is saved
    beside the skin as `<skin>.import.json`.
  - **CORRECTIVE ANIMATION LAYERS** (the viewer's CORRECTIVE tab,
    `mods/corrective.js`): layers of turns on the game's animations, each
    on one clip or all, on/off and a weight; a bone picked, X/Y/Z degrees
    about its own axes, keyed on frames (KEY, DELETE KEY, the keys listed
    to jump to) or recorded as they are set (REC); straight in between,
    held outside. Shown live; SAVE TO THE MOD bakes them into the mod's
    `.skinanim` (each frame's bone quaternion turned, as the file stores
    it) and keeps `<fighter>.layers.json` and the animation they were put
    on, `<fighter>.skinanim.orig`, so they open again to edit and a later
    REPLACE ANIMATION goes into that original and is baked again. Checked
    here: a 90-degree key gives the 90-degree quaternion, a half-weight
    layer half of it.
  - **mod.meta.xml** in every mod (`lib/mods.js writeMeta`), rewritten when
    the mod changes: name, author, version, category, description, date and
    every file with its size and the game file it replaces (`own="1"` for
    the mod's own: mod.ini, the import settings, the layers). Checked on a
    test mod.
  - **EXPORT RIG / ANIMATIONS** in the import tab: the rig, or the rig with
    every clip, as FBX or GLB.
  - **Liu Kang's sounds** (Mary: they play wrong, "strange"): checked
    against the binary -- his voice group (6, `ochar_voice_groups`
    0x16f55c), the ten group tables (`group_choices` 0x17b2e0,
    `group_table` 0x17b308, every name) and the 26 character tables
    (`ochar_sound_tables` 0x17b278, every name) are the binary's, and his
    23 WAVs are the same format as every other (PCM 16 kHz 8-bit mono). Her
    18:02 log plays his voice from group 6 as the binary would. Not found;
    the `voice:` lines stay in the log. Needs what is heard and when.
    Also checked against the binary: `limePlaySound` (0x65344) is
    `[sound play]; [sound setGain:]` on one Finch `Sound` (one OpenAL
    source) a loaded file, `-[Sound play]` restarts it if it still sounds
    -- the port's mixers already do the same (one voice a sound) -- and
    `limeLoadSound` (0x65ad0) takes the first empty of 512 slots, as the
    port does. Mary: screams repeat that should not sound. The log now
    names every sound played (`sound: <file> (<slot>)`), not only the
    voices, so the next fight with it shows which file and when.
  - **REPLACE MODEL: the rig before it is put on, arm/leg angle and roll,
    T or A pose** (Mary, with gtastuff.com's viewer as the reference for
    the floor, the joints and the weight colours). Over the preview:
    TEXTURES / SOLID / WEIGHTS (each vertex coloured by its bones,
    `mods/viewkit.js`) / WIREFRAME, RIG (the bones as lines and the joints
    as dots, moving with the animation chosen) and FLOOR. A humanoid rig
    shows the file's pose (`T` above -20 degrees of arm rise, else `A`) and
    its leg spread beside the fighter's, and four sliders, mirrored on both
    sides: ARM ANGLE (raise/lower), ARM ROLL, LEG ANGLE (spread), LEG ROLL
    (`modelin.js adjustments`). Roll turns the model's limb about its own
    length, each vertex by its weights. KEEP PROPORTIONS turns the model's
    limbs before the bones are moved to its joints, and AUTOMATIC T / A POSE
    (on by default) first turns the file's arms to the fighter's rise, so an
    A-pose model plays the moves with its arms where the fighter's are.
    FIT TO THE FIGHTER puts the angle on the fighter's joints instead: the
    bones' offsets, written as `<skin>.bones` as the proportion trick's
    are; the bone frames keep the fighter's turn, so a large angle bends
    the elbow about the fighter's axis. The game's animations already play
    on the imported model in the preview (the clip list under the view).
  - **The model viewer**: FLOOR (a grid under the model, not on stages),
    SOLID, WEIGHTS (a fighter's) and the joints as dots with the bones.
  - **GLB export** (`export.js toGlb`): EXPORT's formats now include GLB,
    glTF 2.0 binary with the skeleton, the skin, one animation a clip and
    the PNG textures inside, in metres (a root node scales the game's
    centimetres). Checked here: the file's header, JSON and every accessor
    against the buffer, on a two-bone test; not opened in Blender yet.
  - **Liu Kang's repeated screams** (Mary, 2026-10-29): not found. Her
    17:19 log (Liu Kang vs Shao Kahn) shows one `death scream` from the
    game logic. The build now logs every voice it plays
    (`voice: kind K, voice group G, sound S` for a group sound,
    `voice: character C, sound N -> S` for a character's), so the next log
    of a fight where it happens shows which sound repeats and from where.
  - **Frames lost with a custom model.** A REPLACE MODEL skin is drawn a
    material at a time (`skin_mats.c`), each part a run of indices into the
    whole model's vertices, and the Direct3D 12 / Vulkan path
    (`render/rgl_ff.c tris_indexed`) transformed every vertex 0..max for
    each part -- or, when a part named few vertices of a big array, fell
    back to one transform per index and a malloc. Now only the vertices the
    indices name are transformed, once a draw (a stamp a vertex). And a skin
    a mod brings (`mods_find` on the skin file, `port_skin_custom`) is left
    alone by SMOOTH MODELS and what it does to the plain skin (the tuck
    under a hat, the cover pieces): such a model is already dense, as Mary
    asked; the log says `is a mod's model: no SMOOTH MODELS on it`. SMOOTH
    ANIMATIONS still apply (bones only, no cost in the vertices). Not
    measured here.
  - Every material is its own texture by default again (the materials as
    the model has them); REPLACES a game texture only when chosen.
  - **SCALE** in the importer: the model's size about its pivot (the floor
    under the fighter's root): mesh, every bone offset and the root height
    together, written in `<skin>.bones`, so the animations play on it.
  - **FBX**: parts with pivots (`RotationOffset/Pivot`, `ScalingOffset/
    Pivot`) and a geometric offset (`GeometricTranslation/Rotation/
    Scaling`) are placed right; a part with no material gets its own grey
    one; a part not skinned but hanging under a bone moves with that bone;
    diffuse, normal and specular maps are read (the preview shows all
    three; the game's fixed-function renderer draws the diffuse only); a
    material without pictures shows its colour (`DiffuseColor` times
    `DiffuseFactor`).
  - **STAGE SELECT pictures** rendered from the camera a round starts with
    (eye 1.5 over the floor, 8 back, looking at 1.25, 25 degrees); the
    older renders are made again once (`<ID>.auto2`, `.v2`).
  - **Each picture chooses the game texture it replaces** (REPLACES
    `SONYA_DIFFUSE`, an outfit, block 2's texture, or NEW TEXTURE): it is
    saved as that texture (and its `_ICE`), and the game draws it on the
    model's own UVs with no `.mats` part. By default each picture replaces
    its block's texture. Mary saw the model's pictures not arrive in the
    game; this is the direct route. The player's badge is hidden while the
    importer is open.
  - **Humanoid retarget** (`mods/retarget.js`): the file's bones are known
    by name from Mixamo, AccuRig / Character Creator (`CC_Base_*`),
    ValveBiped, 3ds Max Biped, Unreal, Unity / VRM, Blender / Rigify and
    Daz, and the file's **own weights** move the model on the fighter's
    bones (twist and helper bones with their limb, fingers with the hand,
    the rest with the nearest bone above). The placement is fitted on the
    body's joints (not the arms: T or A pose). Two ways, chosen in the
    preview: FIT TO THE FIGHTER (each part turned and stretched to its
    bone) or KEEP PROPORTIONS -- the **proportion trick**: the fighter's
    bone offsets become the model's, written as `<skin>.bones`; the game
    puts them in the bones it loads (`skin_mats.c`
    `port_skin_bones_loaded`) and scales the animations' root height
    (`lime_port_root_y` in `CreateMatrixPaletteForGeneratingMesh`), and the
    viewer reads them the same (`assets.js`).
- **Frozen copies of custom textures**: REPLACE TEXTURE (viewer, editor,
  TEXTURES) writes `<stem>_ICE` too when the game has one (a fighter's
  `_DIFFUSE_ICE`) or it is a replaced model's material, as the game's are
  made: brightness on pale blue (blue 255, red 113 + 0.55 L, green
  205 + 0.19 L, measured on four fighters). A box turns it off.
- **Sonya's second outfit picture** is `SONYA_2_VERSUS.pvr` (the others
  `<NAME>_VERSUS2.pvr`): the profile icon finds it, in the launcher and
  the game.

Checked here: the C files pass `gcc -fsyntax-only`; the launcher UI
builds; the bone roles were checked on 40 names from those rigs. Not
checked: any of it in the game or with a real Mixamo/AccuRig/Valve file.

## After 0.1.13: model viewer, REPLACE MODEL (FBX/GLB/OBJ, a texture per material), replays, VSYNC (2026-10-29, released in 0.1.14)

Branch `claude/viewer-tabs`. **Nothing here has been built into a game exe
or played by Mary yet.** The C files pass `gcc -fsyntax-only` with the
build's flags; the launcher UI builds (`npm run ui`).

- **Model viewer** (MODS): the right panel in tabs (VIEW, LAYERS, TEXTURES,
  IMPORT, EXPORT); each fighter lists its derived models (babality, frozen,
  cut in half, headless, ...) found by the meshsets' names
  (`ui/src/lib/mods/derived.js`); LAYERS hides or shows each mesh; the
  texture editor shows thumbnails, grouped, with a large preview.
- **REPLACE MODEL** takes FBX, GLB (three.js GLTFLoader, MIT) or OBJ
  (+ .mtl) and opens a preview window with the model on the fighter, its
  animations and its textures. **Each material keeps its own texture**:
  the skin's triangles are written grouped by material and
  `<FIGHTER>_STANDARD.skin.mats` lists the parts
  (`block first count alpha texture`). The game draws each part with its
  texture (`src/runtime/skin_mats.c`, hooked through function pointers in
  `RenderSkinned.c` / `RenderMesh.c` and `Players.c`): UVs as the file has
  them, GL_REPEAT past 0..1, transparent parts last (alpha test 0.1 +
  blend); frozen uses each part's `_ICE` copy, written by the launcher.
  GLB reads the UV set each material names and `KHR_texture_transform`.
  The preview and the viewer show each part on its own (eye, "1" = alone,
  click = large picture). A block with parts is not smoothed (SMOOTH
  MODELS) in the game or the viewer. Online loads `.mats` too (visual).
- **Online is vanilla**: `mods_online(1)` puts back every data.ini word a
  mod changed and only models and textures are taken from mods
  (`mods.c`, called from `rollback.c`).
- **Replay player**: HIDE (a click or Esc shows it again) and SIZE
  (75/100/125/150 %, umk3.ini `replay_scale`); the launcher steps aside
  while the game runs and comes back where it was.
- **Replays: the wrong rival.** Arcade's VS screen picks player two inside
  the fight's loading, after the setup was written, so a recording kept
  the previous rival (Mary: `liukang_vs_stryker` was a fight with Kitana;
  `scorpion_vs_nightwolf` replayed against Old Sub-Zero). Task_GameInit
  step 42 now calls `rb_fight_rival`: recording writes the real one into
  the setup and names the file after it; a replay (`UMK3RPL4`) puts it
  back. Old `UMK3RPL3` files still play but keep their old name.
- **Replays: seeking stops what still sounds** (`plat_audio_stop_all` in
  `rp_seek`).
- **Select screen, the same fighter on both sides**: TheFECharacters has
  one record per fighter, drawn for each side; the second
  `port_smooth_pose` call took the first one's frames for the game's blend
  and started over every draw (no smoothing, the smoothed pose remade).
  Now once a frame per PLAYER (`port_frame_no`, counted in `limeBegin`).
- **VSYNC** option: umk3.ini `vsync=0/1`, the game's options menu and the
  launcher's video page; OpenGL swap interval, Direct3D 12 interval 0,
  Vulkan mailbox/immediate. Applied at once.

**Death screams (open).** Mary's last replay
(`20261029-131108`, Liu Kang's decapitation) and the log of the fight it
recorded show **one** death scream per fatality from the game logic
(`group_sound`, kind 9); the repeats in the replay's log are Mary going
back over the end. So the logic does not ask twice; what is heard twice
is still unknown. Needs Mary to say which sound repeats and where
(fight or replay, which fatality).

- **STAGE SELECT** after the fighters in VS (GameMode 6), training (2)
  and online (1, the host; the guest takes the host's level and now waits
  up to a minute for it). `Task_GameInit` step 0 waits on
  `stage_menu_gate` (`debug_menu.c`), and the window takes the frame as
  the settings menu does, over `FE_SELECTFIGHTER`: the stage's picture
  large with arrows, or a list of all of them with their pictures (LIST,
  TAB, the pad's Y); CANCEL goes back to the select screen (online, the game's own stage),
  SELECT (Enter, A, START) picks. The pictures are the game's `CS_BG` ..
  `CS_BG4` (960x640 on a 1024 sheet): Subway, Bell Tower, Soul Chamber
  (a guess from the picture), The Graveyard, Temple. The other stages show
  their name only. A debug fight and a replay skip it.
- **Online select: START presses PLAY**, as in VS and Arcade.

## After 0.1.13: an update stopped by a leftover update\new (2026-10-29, released in 0.1.14)

Mary's update failed with "ENOTEMPTY: directory not empty, rmdir
...\update\new\UMK3-PC-0.1.11-beta\launcher\bin\resources". `unpack`
(`launcher/app/lib/update.js`) cleared `update\new` with one `fs.rmSync`.
An earlier update's copy was still there: apply.ps1 removes it with
`-ErrorAction SilentlyContinue`, and Windows would not let a file in it go
(the usual holder is an antivirus still reading what was just unpacked).
So the update threw before it began.

Now:
- `unpack` retries the removal for two seconds (`maxRetries`).
- What still will not go is left, and the update unpacks into
  `update\new-<time>` beside it.
- Every leftover `new*` is tried again at the next update.
- apply.ps1 / apply.sh remove the folder that update used.

Checks: a test case in `test/update.test.js` (a removal that throws
ENOTEMPTY, then the next update clearing both); launcher tests 14 pass.

The fix runs in the launcher doing the update, so it helps from the next
release on: a launcher of 0.1.13 or older still stops on a leftover. Until
then, close the launcher, delete the `update` folder by hand and update
again (or download the release zip).

## Release 0.1.13 beta (2026-10-29): replays, online delay by ping, Vulkan / Direct3D 12 stutters, 4 GB

Published as the GitHub pre-release `v0.1.13-beta`, packaged as 0.1.12 was:
`UMK3-PC-0.1.13-beta.zip` (`launcher/make_release.py` over `electron-builder
--win dir`) and `UMK3-PC-0.1.13-beta-linux-x64.tar.gz` (`electron-builder
--linux dir`, then `launcher/make_release_linux.py`). It carries the five
"After 0.1.12" sections below. Before publishing, the game was built from
the 1.2.59 .ipa in the unpacked Windows package. Known problems taken off at
Mary's word: Reptile's fatalities (it was not Reptile's alone) and the
Vulkan / Direct3D 12 stutters (fixed below). **Mary has not played this
build yet.**

## After 0.1.12: REPLAYS (2026-10-29, in 0.1.13)

Not yet played by Mary. Asked for so that a fight can be watched again to
find bugs and take logs of them (Melee Unlocked's replays as the model).

**Recording** (`rollback_probe.c`, `GameCode.c`). The probe's recorder
(`UMK3_REPLAY_RECORD`, which replayed a fight identically over 3038 ticks)
now records every fight but an online one, by default, to
`replays/recent/<date>_<p1>_vs_<p2>.umkr` beside the exe; only the newest 5
stay, and one shorter than a second is not kept. umk3.ini `replays=0` turns
it off. The file is the setup -- the fighters, the level, the mode, the ten
`Settings` words and training (`PortPractice`), 18 words now -- the two
seeds, then each tick's two engine words and its result's hashes. A replay
takes the recorded settings back for its one fight, since the buttons and
the difficulty change how it plays. While recording the frame step is pinned
to 1, as online: below 60 FPS the game slows instead of skipping ticks.
Online fights are not recorded yet (their ticks go through `rollback.c`).

**Playing** (`umk3-game --replay replays/<folder>/<file>`, a path inside the
exe's folder). The game goes straight into the fight, as `--fight` does, and
`rp_frame` steps it in `GameCodeMain`'s place. A panel along the bottom
(`debug_menu.c`, drawn like the launcher: violet glass, light rules, buttons
with a rim, play lit) holds the status line, a bar to click or drag, and
buttons: to the start, back 5 s, back a frame, play / pause, on a frame, on
5 s, slower / faster (x1/4 .. x4), and the settings menu; each names itself
when the mouse is over it. Keys: Enter or P play / pause, left / right 5 s
(paused, a frame; held, they repeat), up / down speed, M the start, Esc the
settings. Going back loads the nearest keyframe (a rollback snapshot,
`st_rb_*`, every 120 ticks or more, 47 at most) and runs the ticks between
silent and undrawn, 20 ms of them a frame. Pausing draws the next frame and
puts the state back. At the last tick the player stops. Each tick's result is
compared with the recorded one: a difference shows on the panel and goes to
the log (`replay: tick N DIFFERS`, then `replay: the end ... the same as the
recording` or `DIFFERS`). Windows only for going back and pausing: the
snapshots are Windows only.

**Launcher.** A REPLAYS line on the menu (the eight lines re-centred on the
ring) opens the page (`SecReplays.svelte`, `lib/replays.js`): the keep-the-
last-5 switch, the last fights and the kept ones, each with WATCH, KEEP FOR
GOOD (recent to `replays/saved/`), EXPORT… and DELETE, and IMPORT… and OPEN
FOLDER for the kept ones. `play` passes `--replay` to the game.

**The player's top corners** (asked after the first look at the plan). A
REPLAYS button at the top left opens a list of the last fights and the kept
ones (the newest 8), the one playing marked; picking another switches to it
in the same game: this replay ends (logged as `switched to another
replay`), the game leaves the fight as the debug menu's FIGHT does and the
`--fight` path starts the new one's fighters and level, behind a LOADING
screen with its name; the sticks rest and the keys and mouse do nothing on
the way (`rp_switching`). (The first version started a second copy of the
exe instead; Mary asked for no restart.) An X at
the top right closes the game. Nothing else is drawn across the top, so the
health bars stay in sight; the replay's name moved into the status line.

Checks: Windows build without warnings in the new code, Linux compile 84/84,
launcher tests pass, the page and the menu seen in launcher captures. The
panel and playback have not been seen in the game yet.

## After 0.1.12: online lag over the internet (2026-10-29, in 0.1.13)

Not yet played by Mary. She saw lag in an online fight against another PC.
No 0.1.12 online log was on hand; the last one (0.1.11, over EOS) showed, per
600 frames, 75 to 128 holds (a frame shown again while waiting for the other
side), rollbacks always at the deepest the window allowed (7), and the other
side 6 to 9 frames behind. The input delay was fixed at 2 frames, enough on a
local network, not over the internet. Changes (`rollback.c`, `mp_net.c`):

- Inputs go on the wire when they are taken (`mpnet_flush` ticks EOS), not at
  the next frame's poll: about a frame less each way.
- The input delay follows the round trip, measured from the other side's
  acknowledgements: once a second, one frame toward the delay that keeps
  rollbacks near 3 frames, between 2 and 6. Each side's delay is its own (the
  other only receives inputs by frame), so it can move mid-fight: frames it
  skips take the same input, a frame it overlaps drops the newer one.
  `UMK3_RB_DELAY` still fixes it.
- Rollback window 8 -> 12 frames (4 more snapshots, ~25 MB).
- The log's `rollback: frame ...` line and NET STATS show the ping, the delay
  and the longest rollback in ms.

Also seen in that 0.1.11 log, not touched: the second fight (this side player
2) logged a DESYNC every second. Built into Mary's 0.1.12 test folder; not
yet tried between two PCs.

## After 0.1.12: REAL-TIME SHADOWS' note on a row of its own (2026-10-29, in 0.1.13)

Mary saw the note beside REAL-TIME SHADOWS' value ("WITH VULKAN OR DIRECT3D
12") drawn over the label: the row shrinks its text to fit, down to a floor,
and this one was still too long. The note is now a dimmed row of its own under
the option, shown, not chosen (`settings_menu.c`): ONLY WITH VULKAN OR
DIRECT3D 12 on OpenGL, EXPERIMENTAL on the others. Seen right by Mary.

## After 0.1.12: the game's memory below 4 GB, limeMalloc's heap 2 GB (2026-10-29, in 0.1.13)

Not yet played by Mary.

The 64-bit build kept the game's heaps below 2 GB, because a `long` holding an
address was feared to sign-extend. That fear was checked two ways.

**The compiler.** Clang's `-Wint-to-pointer-cast` was run over every source
of the 64-bit build. It found 320 integer-to-pointer casts, and all of them
are from `uint32_t`, which zero-extends: `P32` is `__ptr32 __uptr`, and the
game's `uintptr_t` is `uint32_t`. None are from a signed type, and
`-Wint-conversion` found no implicit ones.

**Running it.** With `UMK3_HIGH_MEM=1`, a test switch in `savestate.c`'s
`reserve_low`, every region starts at 2 GB: the arena at 0x81000000 and the
heap at 0xC1000000. With all of the game's memory above 2 GB, the game:
- booted;
- fought;
- went through FINISH HIM, the continue screen, character select and the main
  menu (8,000 ticks);
- ran a 6,000-tick fight with a finisher request, with no fault.

What changed:
- `reserve_low` now searches below 4 GB, still first fit from the bottom. The
  game's memory sits where it was; only what it never used before can go past
  2 GB.
- limeMalloc's region (`lime_platform.c`) now tries 2048 MB first (was 768) and
  gets it: `lime_heap: 2048 MB`. It is a reservation, and pages are committed
  as they are used. A fight with SMOOTH MODELS uses about 386 MB in all.
- The save-state arena stays at 1024 MB.

Linux keeps `MAP_32BIT`, so it stays below 2 GB. It falls back through the
size list to what fits. Linux compile: 84/84.

## After 0.1.12: Vulkan / Direct3D 12 stutters, compiled shaders, Sub-Zero's strap (2026-10-29, in 0.1.13)

Not yet played by Mary.

**The stutters on Vulkan and Direct3D 12 (Mary's 180 Hz screen).** Each 60 Hz
tick was presented with a swap interval of one refresh (5.6 ms at 180 Hz).
The frame's work (10-12 ms) varies by a few ms, and a frame that ended just
past a refresh was shown one refresh later. The result was 22 ms, then 11 ms,
several times a second. That is what was measured on 0.1.12: 150-330 of
~2,400 frames late. Changing vsync made no difference, because the late
refresh came from where the frame ended, not from vsync.

Now, on a 120/180/240 Hz screen, each frame stays up 2/3/4 refreshes
(`rgl_swap_interval`, set in `game_main.c`'s `pace_frame`):
- Direct3D 12 presents with that sync interval.
- Vulkan's FIFO has no interval, so `rgl_vk.c` keeps a copy of the frame and
  presents it again once for each extra refresh.
- The loop is paced as on a 60 Hz screen.

Measured on Direct3D 12 and Vulkan at 180 Hz:
- Swaps hold at 16.66 ms with 0 late frames a second through a fight.
- A few frames are still late when the frame's work goes past 16.7 ms.
  `UMK3_LOG_PACE=1` now prints the late frames a second.

OpenGL is unchanged.

**Where a fight's frame time goes (measured, D3D12, SMOOTH MODELS 2).**
- `rgl_ff` (transform, index gather, hand-off): ~3 ms.
- Catmull-Clark of the two fighters: ~7 ms.
- With 1 / 4 / 8 threads, the intro had 64 / 38 / 23 frames over 13 ms, with
  smoothing averaging 12.4 / 9.9 / 9.7 ms. The pool stays at 4.
- Open: a fight intro still has frames of 15-19 ms of CPU work. The next
  step is to make the smoothing cheaper.

**COMPILE SHADERS (Khronos' pipeline_cache sample, Microsoft's
D3D12PipelineStateCache).**
- Every pipeline a backend makes is a line in `umk3-pipelines-vk.txt` /
  `umk3-pipelines-d3d12.txt` beside the game, and the driver's compiled
  pipelines are kept in `umk3-pipelines-vk.bin` (`VkPipelineCache`) /
  `umk3-pipelines-d3d12.bin` (`ID3D12PipelineLibrary`, names carrying the
  shaders' hash).
- When the backend opens, it makes them all again, from its own seed list
  (`render/rgl_pipes_*.h`: menus and two fights, 18 / 17 pipelines) and the
  file. This takes 1-7 ms, and no pipeline is compiled in the middle of a
  fight.
- `umk3-game --compile-shaders` does only that and exits.
- The launcher's PLAY has COMPILAR SHADERS ANTES DE JUGAR
  (`compile_shaders=1`), which runs it before the game.

**Sub-Zero's strap stretched to his fist (Mary, his intro, player two, with
SMOOTH MODELS).** The weld was measured on the first pose a skin is drawn in.
In Sub-Zero's intro his hand touches the strap there, and the two were welded
for good. `DrawSkinnedMesh2` (armv7 0x608d8) unwelds the skin by UV: each
record vertex is a copy of the skin vertex its corner names (SKININFO +0x18).
The weld now joins two vertices only when, besides being close, their skin
vertices have the same four bone bytes and weights (+0x20, +0x24):
`umk_subsurf_new_keyed`. For his mesh, 3 of 670 welds were refused, and the
strap stays on his back (captures at ticks 105, 108 and 159). With SMOOTH
MODELS off the strap was always right.

Checks:
- rgl_test: 0 failed on OpenGL, Vulkan and Direct3D 12.
- Linux compile: 84/84.
- Launcher tests: pass.

## Release 0.1.12 beta (2026-10-29): fatality lag, several cores, no vertex-buffer ceiling, shadows, flawless

**Scorpion's fatality lag (Mary: lag in that fatality only).** Measured
with a per-second log of the draws (Scorpion vs Sonya, practice, finisher 2):
in the late phase ~150 draws of 180,216 vertices a frame (the copies of
Scorpion, each a smoothed body expanded to one vertex per triangle corner),
~1 GB sent to the GPU, frames of 889-968 ms (651-679 ms in the scene's
draws). Two causes:
- SMOOTH MODELS' budget of smoothed triangles read `limeRenderedPolyCount`,
  which the game's tick clears; a slow frame runs several ticks between its
  draws, so the budget never filled (logged: 0 draws over it). Now its own
  count, cleared before each present (`port_smooth_frame`, game_main.c),
  250,000 triangles: the two fighters and a third body; copies past it are
  drawn as the model is.
- Direct3D 12 and Vulkan grew their vertex buffer up to 256 MB; past that,
  every overflow waited for the GPU and made a new 256 MB buffer, 3-4 times a
  frame. Now the buffer grows to what the frame needs with no ceiling, and the
  old one is kept until the next flush instead of waiting partway.
After: one 52 ms frame in the whole fatality, the rest under the slow-frame
threshold. rgl_test 0 failed on OpenGL, Vulkan and Direct3D 12; Linux
compile 84/84. Not yet played by Mary.

**Open: random stutters on Vulkan and Direct3D 12 (Mary, RTX 4060: OpenGL
smooth).** Measured on Direct3D 12 (Liu Kang vs Sonya, practice, 40 s, a
180 Hz screen): ~150-330 of ~2,400 frames take 20-23 ms instead of 16.7,
then one ~11 ms frame (the next tick is already due); they come about every
100 ms. Ruled out: vsync (`Present(0)` changed nothing, 289 vs 318), GPU
waits (2 a frame, 0.6 ms), textures rewritten mid-frame or read back (0).
The frame work before the present is ~10-11 ms on Direct3D 12 against
~5.5 ms on OpenGL (the `pace:` log), so the CPU side of rgl_ff (transforming
and expanding every vertex) is the next place to look. Not fixed.

**Several cores.** zig cc ships no OpenMP runtime, so Blender's subdivision
ran on one core: `src/thirdparty/blender/omp_lite.c` implements the
`__kmpc_*` calls (a pool of at most 4 threads, measured fastest; `UMK3_THREADS`
overrides), `CCG_OMP_LIMIT` lowered to 20000, rgl_ff's indexed expansion in
parallel, a ring of 8 recent poses per smoothed mesh.

**Engine memory (Mary asked for more).** The game keeps pointers in 32-bit
words and longs that sign-extend, as on iOS, so its two heaps must sit below
2 GB: the save states' arena reserves 1024 MB and limeMalloc's heap 768 MB
(read at start). A 1024 or 896 MB heap was tried and does not fit beside the
exe; that space is full. Going past 2 GB would mean every pointer read in the
decompiled code zero-extended instead, a change across the whole game.
Measured use is ~386 MB for the whole process, so the heap is not what
limits the frame rate. Everything else (textures, vertex buffers, smoothed
meshes, shadows) is ordinary 64-bit memory with no limit of the engine's.

**Also since 0.1.11:** REAL-TIME SHADOWS drawn the same frame with a steady
light box; Kung Lao's hat in the fight, not only the intro; round end as the
binary's two blocks (FLAWLESS VICTORY on match point, DrawHUD 0x2902c /
0x290a6); Linux keys mapped by scancode (not tested on Linux);
`UMK3_PRACTICE=1` runs a `--fight` as TRAINING.

## Release 0.1.11 beta (2026-10-23): updates that finish, window resize, loads, profile picker, sharp edges

Everything below, "After 0.1.10b", plus Kung Lao's hat and the Esc menu's
picker. Not played by Mary yet.

**Kung Lao's hat pierced by his head (Mary, with a picture).** Measured in
the rest pose with the viewer's own code: 4 head points through the hat
unsmoothed (the original's), 13 at level 1, 45 at level 2; sharp edges
change nothing. The cause is SMOOTH MODELS' correction pass (added for Jax
and Sektor): it makes the surface pass through the model's vertices, and
between them a convex part bulges past the faces; the hat is a scene mesh on
the head bone and is not smoothed. Tried: smoothing the hat too made it worse
(24 / 102: the cone's tip rounds down onto the head); lowering the
correction everywhere fixed it but doubled every fighter's shrink. Done: the
skin inside a piece on its bones (its box, 2 cm wider) gets no correction --
`umk_subsurf_set_fit` per vertex; the game boxes each bone-attached scene
mesh `LIME_RenderScene` draws, for the skin `RenderPlayer` names (a frame
uses the boxes of the one before); the viewer from the attachments it shows.
Measured: 0 and 0 points through the hat, the rest of the body as before.
`tests/subsurf_test.c` I covers the per-vertex share. The game was not run:
its log says `smooth models: N vertices of a skin under a piece on its
bones` the first time it engages -- if Mary's log never shows it for Kung
Lao, the box is in the wrong space.

**The Esc menu like the launcher.** PROFILE's background row opens the
launcher's gradient picker, drawn in software by `plat_ui_gradient`
(`ui_alert.c`) from the component's CSS (colours, sizes, the dial, the 18
presets eight across, the CSS and COPY / CLEAR), over the menu as the
colour list is; the mouse drags the dial and clicks the rest, the keys and
the pad move between the controls (left / right turn the dial 5 degrees, as
the launcher's arrow keys). FROM and TO open the game's colour list. OUTFIT 1
/ 2 is a row. GRAPHICS gains SHARP EDGE ANGLE and SHARP EDGE PRESERVATION
(5 degrees and 5% a press), off with SMOOTH MODELS, as in the launcher. The
launcher's fullscreen frame picture is not in the game (it picks a file).

## After 0.1.10b: updater, window resize, load times, profile picture, sharp edges (2026-10-23, released in 0.1.11)

What Mary reported on 0.1.10b, in the order she sent it, and what was done.
Nothing here has been played by Mary yet; a test build is in
`WORK/test-after-0.1.10b/` (main checkout).

**Launcher update "deletes the game, asks to download everything again".**
Reproduced with the real 0.1.10 and 0.1.10b zips in a scratch folder.
`lib/update.js` mirrored `decomp\`, `runtime\`, `tools\` and `launcher\`:
`decomp\` and `runtime\` have not been in a release since 0.1.6, robocopy
answered 16 for them, and **every Windows update since then ended as "could
not be copied"** (Mary's own `update\apply.log` shows it); the archive stayed
in `update\` (four of them, 570 MB, in her folder) and with 0.1.10b's
self-offer the launcher asked for the same download again. Mirroring `tools\`
whole also **deleted `tools\eos\`** (the EOS SDK and `eos.ini` that 0.1.10
shipped). Now only `src\`, `launcher\` and `tools\build\` are mirrored, and
only when the release has them; the rest is copied without deleting; the
unpacked copy and every archive go either way. Checked: a 0.1.10 install with
a fake game, mods, saves and `tools\eos\` updated to 0.1.10b ends as
installed, keeps all of those, and drops a source the new release lacks.
The game itself (exe, `res\`) was never touched by the script; what Mary saw
as the game deleted is not explained beyond the failed copy -- if it happens
again, `update\apply.log` and the launcher's state are what to look at.

**Losing to Motaro in the debug menu softlocks.** Not reproduced and not
changed: no log of it on this machine, and the code paths (DBG_BOSS, the
debug fight's save/restore in `game_main.c`, `QuitAsLose` -> the continue
screen) do not show it by reading. Needs Mary's `logs\umk3-*.log` of that
session and the exact steps (NEXT IS MOTARO, or a debug fight with Motaro as
fighter 2).

**Resizing and moving the window a few times crashes.** Three causes found
and fixed, none seen crashing here (the game is Mary's to run):
- Direct3D 12: after `ResizeBuffers` the back buffer index was not read again
  (drawing into one buffer, presenting another), and a failed resize went on
  with no buffers. Now the index is re-read, a failed resize keeps the old
  buffers, and drawing is skipped while there are none.
- Vulkan: a failed swapchain re-creation left a retired handle in use; now
  there is no chain until the next frame tries again. The chain was compared
  to the surface's extent, which can be a pixel off the window's, and was
  re-made every frame.
- The render buffer (`render_alloc`, multisampled and window-sized) was made
  again on every frame of a drag; now once the size has held 0.25 s.

**EOS on other PCs.** 0.1.10b was packaged in a worktree without `TOOLS/eos`
and went out with online on the local network alone. `make_release.py` and
`make_release_linux.py` now refuse to package without the SDK and `eos.ini`
(`UMK3_EOS_DIR`, else `tools/eos`, else `TOOLS/eos`; `UMK3_NO_EOS=1` on
purpose). At Mary's request the SDK is now in the repository, only what
online needs: `tools/eos/SDK/Include` (the 45 headers `mp_eos.c` includes on
Windows and Linux, checked by compiling it for both), `tools/eos/SDK/Bin`
(`EOSSDK-Win64-Shipping.dll`, `libEOSSDK-Linux-Shipping.so`) and Epic's
`ThirdPartyNotices`. The product's `eos.ini` (its client secret) is not: the
session's safety checks refused to publish it, so the release scripts take it
from `UMK3_EOS_INI` or the main checkout's `TOOLS/eos/eos.ini`. Without it a
build plays online on the local network alone.

**The "image" file.** `launcher/aditional textures/image.png`, the home
screen ring's picture, is now in the repository (Mary: "inclúyela"), so
every release and compile carries it; the launcher also looks for it in the
release's `launcher\` folder before the first compile.

**Load times.**
- D3D12 and Vulkan: every texture upload made its own command list, submitted
  it and waited for the GPU -- hundreds of waits per load. Uploads are now
  recorded in one list that runs before the next draws.
- Load screens run 100 ms of steps per frame, not 50 (the draw and the vsync
  wait took a fifth of every load).
- The launcher's compile is incremental (an object is kept when its arguments
  match and nothing in clang's `.d` is newer); the `.ipa`'s files are written
  only when their CRC changed (`res/.extracted.json`), the generated tables
  only when their text did. A compile with nothing changed: 13.9 s -> 1.2 s
  here; extraction 3.3 -> 0.5 s, sources 10 -> 0.1 s.
- The 77 s "loading: menus" in one of Mary's logs is the settings menu left
  open during a load, not a load.

**Lag and memory over a long session.** Textures are looked up by name every
frame (the menus' button pictures, hints, the move list), and the lookup was
a `strcmp` walk of every texture loaded so far -- it grew with every stage and
fighter visited. Now a hash. Textures are still never freed (by design, see
`savestate.h`): memory grows with the distinct textures a session sees, up to
the game's set. The log gets a line every five minutes, `memory: N MB, N
textures (N MB of pixels), after N min`, to see whether anything else grows.

**Character editor error with Motaro / Shao Kahn.** Motaro's strike table
names `stk_dummy` three times and the hit box drawing keyed its boxes by name
(Svelte refuses duplicate keys): his page failed. Each strike is drawn once.

**Profile: gradient picker and the second outfit.** Mary's `GradientPicker`
(two colours, an angle dial, presets) replaces the colour / shade / second
colour boxes in PROFILE; the icon can show the fighter in his second outfit
(`<NAME>_VERSUS2.pvr`, framed as the first one's PNG -- measured: the PNG is
the .pvr shrunk 2.12 times on its bottom left; Sonya has no second). The
game draws the same icon; `profiles.ini` gains `|angle|alt` (older files and
versions read as before); online carries both in the look word's high bits.

**Sharp edges for SMOOTH MODELS.** Mary's two specifications (automatic sharp
edge detection; two sliders). In `umk3_subsurf.c`: each interior edge's
dihedral angle, measured once on the positions the mesh is made with; an edge
over the threshold gets Blender's crease (`strength * levels`), and CCG's own
crease and corner rules keep it -- fractional strength is Blender's
semi-sharp crease. Open edges keep their crease; three-face and degenerate
edges are never sharp; 180 degrees or 0% is the old subdivision exactly.
Default 80 degrees, 100%: over the fighters' 74,646 interior edges 38% turn
by more than 30 (Blender's auto smooth angle would make them blocky), 9.6% by
more than 80. Sliders in GRAPHICS (under SMOOTH MODELS) and in the model
viewer (live), `umk3.ini sharp_edge_angle= sharp_edge_strength=`; the game
reads them at start. `tests/subsurf_test.c` A-H all pass (cube corners kept
exactly, smooth tube unchanged, threshold monotonic, levels 1-3, corners,
open sheet, non-manifold/degenerate, regression).

**Tests run:** `tests/rgl_test.c` on OpenGL, Vulkan and D3D12, x64 and x86:
0 failed. `tests/subsurf_test.c`: all checks passed. Launcher tests
(`extract`, `ini`, `update`): pass. Full compile of the game with the
launcher's pipeline: OK. The game was not run.

## Release 0.1.10b beta (2026-10-23): video settings, two columns

Published as the GitHub pre-release `v0.1.10b-beta` (package version
`0.1.10-beta.b`), packaged as 0.1.10. New: the settings screen's
VIDEO SETTINGS box (`FE_Task_Settings`, port-only BOXRT at 0x145, 0x59,
answer 5, `set_menu_open_video`); two columns in every settings list
(`ui_menu_split` in debug_menu.c breaks at a "\002" heading, half way when
there is none or a column would pass 16 rows; `plat_ui_menu` and
`PortControlsPanel` take that split; `ui_geo_row` maps the mouse); a row
too long for its column is drawn smaller (`al_width`, `rich_width`);
CONTROLS split into FIGHT / MENU headings. **Mary saw the first version**
(columns split half way) and asked for whole sections and the FIGHT / MENU
split; that version is built but not yet seen by her. Left/right still
change a value; they do not move between columns.

**Tried by Mary at this release:** the Linux package works on a Linux
machine (after the Linux fixes of 0.1.6-0.1.10), and the Vulkan and
Direct3D 12 renderers play. Neither is a known problem any more.

**Launcher bug in the published 0.1.10b:** `compareVersions` cut
`0.1.10-beta.b` at the `-` and read it as 0.1.10, so this launcher offers
`v0.1.10b-beta` as an update to itself (noted in the release's known
problems). Fixed in `lib/update.js` with test cases after publishing
(PR #117); GitHub's immutable releases refuse new assets, so the fix
ships with the next version.

## Release 0.1.10 beta (2026-10-23): kombos, SMOOTH MODELS, GPL

Published as the GitHub pre-release `v0.1.10-beta`, packaged as 0.1.9 was:
`UMK3-PC-0.1.10-beta.zip` (`launcher/make_release.py` over `electron-builder
--win dir`) and `UMK3-PC-0.1.10-beta-linux-x64.tar.gz` (`electron-builder
--linux dir`, then `launcher/make_release_linux.py`). It carries everything
in the three "After 0.1.9" sections below, and the kombo fix (issue #114).
Before packaging, the game was built from the 1.2.59 .ipa in Mary's test
folder (64 and 32 bits, both succeed). **Mary has not played this build
yet**: the kombo fix, the TEXTURES page, the folders and the animated stage
export are untested by her; the repeated death screams are still open.

## After 0.1.9: smooth models fixed in the game, stage animation export, folders, TEXTURES page (2026-10-23, released in 0.1.10)

**SMOOTH MODELS: dark wedges and black spots (fixed, Mary confirmed both in
the viewer and the game).** Two causes:
- UVs: Blender 2.79's subdivided UV mesh moves a UV island's rim like any
  boundary, and where the rim bends inwards it leaves the island and samples
  other parts of the texture. Now each output UV is its face's own corner UVs
  mixed with the loop weights (`umk3_subsurf.c` build_output), so it stays in
  the face's UV triangle (Blender 2.91+ "UV Smooth: None"). The UV mesh still
  decides which output vertices are shared. Checked on a flat grid with UV =
  position: interior UVs equal positions (level 1 exact, level 2 within 0.01).
- Light: the game took the skin's triangles and positions but the frame
  record's light through the record's indices; where those differ a corner
  took another vertex's light, some near black. `smooth_models.c` now builds
  the surface from the frame record alone, as `LIME_RenderMeshSingleIndexed`
  draws it: its indices, UVs and light per record vertex, positions from
  `RenderVerts`. A cache entry is keyed by the skin and an FNV-1a hash of the
  record's indices and UVs.

**Builds into the test folder take `src/` from the test folder**, not from the
repository (`node launcher/app/cli.js <ipa> --arch x64 --root <folder>`
compiles `<folder>/src`). Copy the changed sources there first, then check the
exe holds a string from the change. Three builds this session missed their
changes this way.

**Exports and mods by place (Mary).** `launcher/app/ui/src/lib/mods/owners.js`:
- Exports: `exports/Characters/<fighter>/<FORMAT>`, `exports/Stages/<stage>/<FORMAT>`,
  `exports/Models/<model>/<FORMAT>`; a texture alone goes to its fighter's or
  stage's `Textures`, or `Interface`, or `Effects`. The viewer's texture and
  rig exports and the character editor's go to the same places.
- Mods: a replaced texture goes into the mod at the same place
  (`Stages/The Pit III/PIT1.png`); a model imported from Blender into
  `Characters/<fighter>/`. The game takes a mod's files by name from any of
  its folders (`runtime/mods.c` walks them), so nothing changed in the game.
  `lib/mods.js` putFile takes a path and removes a same-named copy elsewhere
  in the mod; exportSave takes a folder path (each part a safe name, four
  deep at most).
- A texture's owner: a texture a stage's meshset uses is that stage's (so
  JADE_SKY is Jade's Desert's, not Jade's); a `*_COMPLETEMAP` is the
  interface's; a stem naming a fighter (at its start first) is that
  fighter's; a texture another meshset uses is an effect; anything else is
  the interface's. 1582 textures sort into 27 fighters (1065), 16 stages (164),
  88 effects and 265 interface.

**TEXTURES page in MODS (Mary: stage and menu textures in their own
section).** Fighters, stages (a list each), interface and menus, effects:
every texture with SEE, REPLACE (PNG) into the chosen mod and EXPORT (PNG);
the mod's own ones are marked.

**Issue #101, animated stage export.** EXPORT > STAGES > WITH ITS ANIMATION
(FBX, USDA, USDC, USDZ): each scene object that is ever shown is a bone of its
own with its mesh bound to it; each frame's key gives the bone its palette
rotation, translation and scale relative to its first shown key, and a key at
alpha 0.03 or below scales it to nothing. FBX gets `Lcl Scaling` curves and
USD `SkelAnimation` scales samples when a pose has a scale. Checked: frame 0
equals the static export exactly; on Scorpion's Lair (1001 frames, 43
objects) every shown vertex at frames 3, 400 and 999 matches the scene's own
placement within 0.0004. **Not opened in Blender or a USD viewer yet.**

**Issue #114 (Liu Kang's 7-hit kombo, the opponent slides away): a real
difference found and fixed, not yet tried in the game.** The chain rules all
match the binary: `t_r_hi_punch` / `t_r_lo_punch` bump the punch count at
proc+0x4c (`inc_p_block`), `t_cc_hi_punch` (6, or 4 for the machine or a
player holding away) and `t_cc_lo_punch` (5 / 3) with `t_ccp3` push the pair
apart through `t_separate_us` once the count reaches the threshold, and
`back_to_normal` (from `t_local_reaction_exit`) clears it. Every writer of
proc+0x4c in the binary was listed (a scan of all `str ..., #0x4c]`):
`inc_p_block`, `t_avoid_corner_trap_b`, `t_block_exit`, `back_to_normal_px`;
the two in `t_combo_air_pause` are its literal pool.

The difference is in the combo recogniser. `combo_scan_a11` walks a combo
table (entries of 0x18 bytes, `field48` moving along) looking for the button
just pressed. On a match the binary writes the list's start to 0x1c only
(0x32780, 0x3278e) and leaves `field48` on the matched entry; the port also
put `field48` back at the start. `t_comb2` then reads the next move from
`field48 + 4` / `+ 0x14`, so in the port any kombo whose button matched a
later entry went on with the first entry's continuation. Only a player's
kombos pass through the scan (`t_comb0`: the machine goes straight to
`t_comb8`). Also fixed: `t_r_combo0` skips `set_no_block` when proc+0x10 bit
0 is set (0x45da8 branches to 0x45d32).

`tools/decomp/factdiff.py` over `mkcombo.c` and the combo and punch
reactions in `mkreact.c` (transcriptions from `armrecomp/recomp.py --func`):
the only differences left are the handler addresses stored into the frame,
which the reader does not resolve to names, and the port's `printf`
diagnostics (`separate us:`, `corner trap:`), still in place until Mary
confirms.

**Death screams in friendships and animalities (open).** `t_friendship_complete`,
`t_end_friend_proc`, `t_friend_ender` and `death_blow_complete` match the
binary. The `death scream:` line now also names the handler the thread loop is
running (`port_mk3_running`, mk3.c) and the caller of `group_sound`. Needs a
log from the current build (its PDB is kept in `WORK/pdb-0928/`).

## After 0.1.9: SMOOTH MODELS, crash report context, GPL (2026-10-23, released in 0.1.10)

**SMOOTH MODELS** (Mary): an option, OFF / LEVEL 1 / LEVEL 2, that draws the
fighters through Blender's Weld and Subdivision Surface (Catmull-Clark)
modifiers, "extracted from Blender's code". Mary chose copying Blender's code
over a clean reimplementation, knowing it makes the project GPL.

- `src/thirdparty/blender/`: Blender 2.79b's CCGSubSurf files unchanged (the
  last Blender whose own C does Catmull-Clark; later ones use OpenSubdiv),
  stand-in headers for the Blender headers they include, and
  `umk3_subsurf.c`: the weld ported from 4.2.0 (`kdtree_impl.h`
  calc_duplicates_fast, `mesh_merge_by_distance.cc` "All" mode: mean of a
  merged group, faces with a repeated vertex removed, the first of faces with
  the same vertices kept) and the subsurf glue from 2.79b `subsurf_ccg.c`
  (positions and UVs as two CCG surfaces, `BKE_mesh_uv_vert_map_create` for
  the UV vertices and seams, `get_ss_weights` for per-corner data). See its
  README for which file is which.
- Game: `src/runtime/smooth_models.c`, called from `RenderAnimatedCharacter`
  (Players.c) for the body, the second skin and the shadow, in place of
  `LIME_RenderMeshSingleIndexed` when the option is on. Positions are the
  skin's (`SkinnedVerts`, from the `DrawSkinnedMesh2` just before), topology
  and UVs the skin's own, the light the frame record's per corner. The weld
  distance is Blender's default, 0.001 m = 0.1 game units (centimetres). One
  surface per skin, dropped when `LIME_FreeSkin` frees it.
- Viewer: SMOOTH MODEL (OFF / LEVEL 1 / LEVEL 2) and SMOOTH next to LOOP for
  smooth animations: the same C as WebAssembly (`mods/subsurf.js`,
  `scripts/build-subsurf.js` regenerates `subsurf_wasm.js`; the CSP allows
  'wasm-unsafe-eval'), and frames blended like the game's GetSlerpedQ
  (`rig.js` framePoseBlend).
- Option: `smooth_models=0|1|2` in `umk3.ini`, the launcher's GRAPHICS > VIDEO,
  the in-game GRAPHICS > VIDEO row (applies at once).

**Verified:** `tests/subsurf_test.c` (a triangulated cube with seam
duplicates: the weld merges all 16, the result is closed, wound outwards,
inside the cube, UVs and corner values in range; levels 1 and 2). The
WebAssembly gives the same counts. On real skins (Kabal, Kano, Scorpion,
Ermac) no NaN, level 2 evaluates in 1.4-4.4 ms; the open edges left are the
models' own openings (none within 2 units of another shell for Kano, Scorpion,
Ermac; Kabal's 41 near ones are merged by the weld). Both exes build from
Mary's .ipa; 83 of 83 sources compile for Linux. **Not seen in the game or the
viewer yet:** Mary runs it.

**Linux had no sound at all (issue #104, fixed, not heard on Linux yet).**
The Linux package builds with `UMK3_NO_SDL_MIXER` (no library beyond SDL2),
and in that mode every audio entry point of `platform/sdl_audio.c` was empty.
Now that mode mixes on an SDL2 device itself: 16 effect voices (the mixer
path's conversion and voice stealing) and one music stream, the MP3 decoded
by libmpg123 loaded with dlopen when the first song plays (Fedora / Bazzite:
mpg123-libs). Without libmpg123 the log says so and only the music is
missing. 83 of 83 sources compile for Linux. The same issue's 6-button High
Punch on Y and the moves-list button: the SDL pad goes through the same
configured masks (`port_pad_action`, `g_pad6`) as Windows; the report is from
0.1.6, before that configuration existed. Not changed; to be checked again on
0.1.9 or later.

**SMOOTH ANIMATIONS on the select screen and in the intro (fixed, not seen
yet):** there PlayerAutoSmoothAnims blends with its own windows (40, 20)
nearly all the time, and `port_smooth_pose` left any blend of the game's
alone, so the option changed nothing there. With the option on it now blends
from the frame the game's blend is mostly on.

**Open, from Mary 2026-10-23:**
- Death screams repeat in friendships and animalities (fatalities fixed).
  Checked and ruled out: a state that screams and sleeps without moving on
  (none), the sound-number order before each sound call against the binary
  (`str [rX, #0x1c]` before `bl`: three candidates, all three false or
  faithful -- tl_bomb33 stores 0 from `sl`; the script interpreter's opcode 15
  stores into obj and calls ochar_sound(ref), as the binary does). The
  `death scream:` diagnostic names the handler: a log of a friendship and an
  animality from the 2026-10-23 09:28 build (its PDB is in the test folder)
  will name it.
- Issue #114: in Liu Kang's 7-hit kombo the opponent slides away and the
  kombo cannot be finished. Not investigated past the corner rule
  (`t_avoid_corner_trap`).
- Issue #101: export the stages with their animations and textures.

**Mary's first look at SMOOTH MODELS: holes and lag (fixed, not seen yet).**
- Holes in cloth (a fighter's trousers): the weld removed faces with the same
  three vertices everywhere; Blender's does it only among faces touching a
  welded vertex (`weld_poly_loop_ctx_alloc`, `is_vert_ctx`). The game's models
  draw cloth both ways with the same corners, so the back faces went. Now as
  Blender.
- A piece over another showing what is under it (Kano's eye plate, Sektor's
  helmet): Catmull-Clark pulls an open rim inwards. Open edges now get crease
  1.0 (Blender's own crease, creaseFactor = the level).
- Closed pieces that overlap (Jax's head in his neck): Catmull-Clark shrinks
  each, opening a gap. **Not Blender:** one correction pass in
  `umk_subsurf_eval` -- each control point moved by how far its subdivided
  self fell short, then subdivided again -- so the surface passes near the
  model's own vertices (a cube's corner within 0.13 at level 1). Sharp corners
  bulge a little in exchange.
- Lag: every frame did a full CCG sync (hash maps, allocations); now one full
  sync when the mesh is made and CCG's partial sync per frame, and an
  unchanged pose is not evaluated again. Native cost per fighter per frame,
  with the correction pass: ~1.2 ms at level 1, ~4 ms at level 2. The shadow
  draws the plain mesh again. Under Vulkan / Direct3D 12 indexed triangles
  were transformed once per index (`rgl_ff.c`); each vertex is now
  transformed once and gathered (`tris_indexed`), rgl_test 0 failed on all
  three renderers, 64 and 32 bit.

**Crash reports and the session log** start with the version (`UMK3_VERSION`,
passed by the launcher that compiled the game), 32/64-bit, the system
(Windows version by RtlGetVersion, Wine if present; uname on Linux), the
renderer drawing and the one asked, the GPU (GL_RENDERER / GL_VERSION, which
names the card under Vulkan and Direct3D 12 too), the video settings,
language, smooth animations / models and how many mods are on
(`port_report_context`, game_main.c). Linux still has no crash report of its
own; the session log carries the same block.

**Licence:** GPL-2.0-or-later from here on (`LICENSE`); 0.1.9 and earlier stay
MIT.

## After 0.1.9: SMOOTH ANIMATIONS (2026-10-22, released in 0.1.10)

Mary asked for an on/off option that makes the fighters' animations look
smoother, drawing only, without changing the animations.

- **What the game does:** a fight frame is held for several 60 Hz ticks and
  drawn as a hard cut. The iOS original ran `PlayerAutoSmoothAnims` with a
  window of 2 in fights (a delayed blend); the port sets it to 0 because the
  delay felt like input lag (game_main.c).
- **The option** (`port_smooth_anims`, `smooth_anims=` in `umk3.ini`, the
  launcher's GRAPHICS > VIDEO checkbox, the in-game GRAPHICS > VIDEO row) runs
  `port_smooth_pose` (GameCode.c) from `RenderPlayer` (the fight and the
  select screen) and before both intro draws, with one state per PLAYER; the
  fight's projectile slots are skipped. The first version only covered the
  fight, and Mary saw no change in the intro. When the game's frame changes, the drawn pose blends from the
  previous frame into the new one over as many ticks as the previous one was
  held, at most 5 (up to 4 ticks, 67 ms, behind the logic frame while
  blending). It writes only `frameB` (+0x520) and `t` (+0x524); `frameA`
  stays the game's frame, so the mesh record, the second skin and the scene
  events are the game's. No blend across a non-skinned frame, a second-skin
  change, a character/model change, 16/35..41, or IsLiaProblem /
  IsOstrichProblem; a blend the game set itself is left alone.
- **Verified:** both exes build from Mary's `.ipa`; launcher tests pass
  (ini round trip includes `smooth_anims`). **Not seen in the game yet:**
  Mary runs it.

**Crash when a fighter takes Kabal's zap (fixed).** Mary's next log printed
`csound: character 297758 out of range (event param 0x48b1e38)`. 0x48b1e38 is
`G + 0x390` in the 64-bit exe with character 10 (Kabal) added as `10 << 8`:
player 0's hit queue, which `uhq_entry` leaves in 0x1c at every reaction
start. `t_r_tusk_zap` (armv7 0x46324), the reaction to Kabal's zap, stores
`field1c = 0` BEFORE `bl his_ochar_sound` (0x46396 / 0x46398); the port had
the store after the call, so the sound number was the pointer. On iOS that
read a wrong entry quietly; on the PC it crashed. `factdiff` compares stores
as a multiset, so a store moved past a call is invisible to it.

**Fatality death screams repeat, fatalities that do not finish (open).**
Every fatality, Mary says. Ruled out: the event queue (cleared at every
`mk3_update`), the scene events (visual only), the rollback probe (off unless
set), `instck` (no -3 refusals the binary lacks). Diagnostics in the build,
to find the handler instead of guessing: `death scream: character N, handler
0x..., token 0x..., level N` (other.c group_sound, kind 9) and `mk3: tick
abandoned, handler 0x... refused token 0x...` (mk3.c). The handler address
minus 0x400000 is its offset in the 64-bit exe (llvm-symbolizer with the PDB).

**Crash when two projectiles meet.** SCORPION vs KABAL, both throwing
at once: `AddNewGameEvents` -> `get_csound` read past `ochar_sound_tables`
(26 rows) -- a character sound event (MKEvent 2/3, param = char << 8 + n)
carried a character number of 26 or more. The four senders (`ochar_sound`,
`ochar_sound_n`, `ochar_sound_c`, `his_ochar_sound`, armv7 0x57b94..0x57bfc)
match the binary, so the bad number comes from what calls them. Until the
sender is found, Blood.c skips such an event and prints `csound: character N
out of range (event param 0x...)` to the log: the next report gives the value.
Mary also reports screams repeating when a fighter takes a fatality, and
fatalities whose animation does not finish: not investigated yet.

## Release 0.1.9 beta (2026-10-22): Vulkan and Direct3D 12, Linux fixes

### Renderers (src/runtime/render)

Mary chose renderers written for the game over a translation layer (Mesa's
Zink / D3D12 drivers were offered).

- `rgl_front.c` defines every GL 1.1 call the tree makes (the list
  `platform/win32_gl_cdecl.c` had, now deleted) as plain C on every target.
  With OpenGL it forwards to the driver's entry point, fetched by name
  (opengl32.dll; libGL.so.1 by dlopen, so Linux no longer links libGL).
  `platform/gl.h` strips dllimport/stdcall on all of Windows, not only
  32-bit.
- `rgl_ff.c`: OpenGL 1.1's fixed function as the game uses it -- three
  matrix stacks, enables, blend, alpha test, depth, cull, colour mask,
  viewport and scissor, current colour, texture environment, one texture
  unit with its parameters and mipmaps (box-filtered on the CPU), client
  arrays and glBegin/glEnd, pixel store, glPushAttrib, and the framebuffer
  objects game_main.c's 3D resolution and antialiasing use (answered
  through plat_gl_proc). Draws are assembled into transformed triangles
  or lines and handed to a backend with a pipeline key.
- `rgl_vk.c` (Vulkan 1.0, loaded at run time) and `rgl_d3d12.c` (Direct3D
  12, Windows): one frame in flight (OpenGL drew with glFinish after each
  swap), one command buffer in the game's order with a flush when a texture
  in use is rewritten or pixels are read, render passes / targets that
  load, clears inside the scissor, pipelines cached by state, MSAA targets
  resolved, scaled blits (vkCmdBlitImage; a textured quad on D3D12).
- Shaders: `render/shaders/ff.vert`, `ff.frag` (glslang 16.6.0 -> SPIR-V)
  and `ff.hlsl` (fxc, SM 5.1), embedded by `shaders/build.py` into
  `rgl_shaders_vk.h` / `rgl_shaders_d3d12.h`; the game's build needs
  neither tool.
- Vulkan headers 1.3.296 in `platform/vulkan-include` (Apache-2.0 / MIT).
- Chosen with `renderer=opengl|vulkan|d3d12` (game_main.c read_config),
  GRAPHICS > VIDEO in the launcher, or Esc > GRAPHICS > RENDERER (from the
  next start). A backend that does not start falls back to OpenGL.
- **Checked:** `tests/rgl_test.c` draws the same calls on a hidden window
  with each renderer and reads the pixels back: clear, an untextured quad,
  a 2x2 texture's orientation, MODULATE / REPLACE, two blend modes, the
  alpha test, depth, culling (back and front), a scissored clear, the 3D
  resolution path (a 4x multisampled buffer resolved, then scaled onto the
  window) and three presents. 22 checks, all pass for OpenGL, Vulkan and
  Direct3D 12, 64-bit and 32-bit, on an RTX 5070. Vulkan's front face had
  to be counter-clockwise (the first guess, clockwise, failed the cull
  checks). **The game itself has not been run on them: Mary's test.**

### Move list (GameCode.c DrawCharacterPage)

A tester reported the fatality page showing the previous page's entries.
Both sections of a MovesListTab entry point at the same table (Kano:
0x103958 for both, read from the binary), and page 4 reads the notation at
`(section 1's rows + row) << 6` (armv7 0x1fd74..0x1fd7e); the port started
section 2 at row 0, so the finishers' names came with the specials'
buttons. Fixed; not yet seen in the game.

### Linux

- The game reads `umk3.ini` and `res/` beside its executable
  (`/proc/self/exe`), as on Windows; before, `res` came from the current
  folder and umk3.ini was never read, so nothing the launcher set reached
  the Linux game.
- `text.c` included no `<stdarg.h>` (Windows' headers bring it); found by
  `tools/ci_compile_linux.py`, which compiles every Linux source with the
  bundled headers only (78 of 78), and runs in `.github/workflows/linux.yml`
  with the launcher's tests. It cannot link: the data tables come from the
  .ipa.
- `launcher/app/cli.js` with `build-game.sh` / `build-game.cmd`: the
  launcher's build from a terminal, run with its own Electron as Node.
  `install-shortcut.sh` writes a .desktop entry; `README-linux.md`
  (packages by distribution, what to run, writable folder, renderer,
  internet play); `eos.ini.example`; `make_release_linux.py` packs the tar
  with the executable bits.
- From a tester's notes (Mary relayed them): AppImage was not made; the
  CI cannot build a known .ipa (it is copyrighted) or run the game under
  xvfb.

## After 0.1.8: in-game language, viewer layout, texture replacement (2026-10-22, released in 0.1.9)

What Mary asked for after 0.1.8, before the Vulkan and DirectX 12 work. The
list of what works and what comes next is now [GOALS.md](GOALS.md).
**Mary has not tried any of it yet.**

### Game

- **LANGUAGE in the settings menu** (GRAPHICS > INTERFACE, settings_menu.c):
  the system's, English, Spanish, French, German, Italian, Korean, Chinese,
  taken at once. `port_language_set` (game_main.c) sets `Language` as
  start_app does (English when Info.plist has no `TOS_URL_` for it) and
  loads the text table again as Task_LoadGeneralData does
  (`LoadTextData(LANGUAGE_TEXT_xx)`); the old table stays allocated, since a
  screen may still hold one of its strings. The port's own menus follow
  `Language` already. Written to umk3.ini as `language=`, the launcher's key.
  Compiled for both builds; not run.
- **A mod's texture now serves the game's smaller copies** (mods.c
  `mods_find_texture`): X_LITE and X_LOW by X, X_LITE2 and X_LOW2 by X2. At
  IPHONE texture quality a fighter is drawn with its 256 `_DIFFUSE_LITE`
  skin (Players.c `port_load_lite`), so a mod with only `KANO_DIFFUSE.png`
  did not show in a fight: the likely reason behind Mary's "it does not
  apply". Checked standalone against a mods folder (all seven names
  resolve).

### Launcher

- **Linux link fix** (launcher/app/lib/compile.js): Mary's Linux build got
  as far as linking and stopped at `ld.lld: error: /usr/lib/libGL.so.1 is
  incompatible with elf64-x86-64`. On Fedora and its kin /usr/lib holds the
  32-bit libraries and /usr/lib64 the 64-bit ones; the library search
  started at /usr/lib. Now the 64-bit directories come first and each
  library is checked for the build's ELF class before it is passed to the
  linker. Not yet rebuilt on Linux.
- **Checked on Mary's Rain mod** (OLDSUBZERO_DIFFUSE2.png, 1024): `putTexture`
  run in Electron wrote OLDSUBZERO_DIFFUSE2_LOW.png (512) and
  OLDSUBZERO_DIFFUSE2_LITE.png (256), the game's own sizes; the viewer's
  reader (lib/mods.js `resRead`, now with the same LOW / LITE / LITE2 /
  LOW2 fallbacks as the game) returns the mod's three files with SHOW WITH
  Rain and the game's for the rest.

- Every font size is in a new unit `--t` (tokens.css), `--u` x 0.82: all
  the text is smaller, the boxes stay the same size. The hitbox editor's
  label too.
- MODS uses the whole window (App.svelte `.stage.wide`).
- **Viewer**: the list on the left, the model in the middle, its options on
  the right (animation, outfit, bones, wireframe, the target mod, textures,
  Blender, export); FULL SCREEN (the page's fullscreen, Esc leaves);
  REFRESH FOLDERS (`reloadMods`: the mod list and every cached mod file read
  again, the open item opened again; the manager's REFRESH does the same).
- **TEXTURES by fighter**: a fighter list over the texture list
  (`^(IPAD)?<ID>(_|BABY|BABILITY|PORTRAIT|X_)`); SEE ITS TEXTURES from a
  fighter.
- **REPLACE TEXTURE** in the viewer for fighters, stages, models and
  textures, and in the editor's TEXTURES (`lib/mods/image.js`
  `putTexture`): any picture the window decodes (PNG, JPEG, WebP, GIF, BMP,
  ICO, AVIF, SVG) and TGA (own decoder, checked on plain and RLE files),
  written as the texture's PNG, and with MAKE THE LOW AND LITE COPIES (on
  by default) each smaller copy the game has, at that copy's own size.
  With it off the editor lists the copies to replace one by one.
- Viewer, fighters: TEXTURE QUALITY NORMAL / LOW / LITE shows the outfit
  with the game's smaller copy (the separate TEXTURES list there repeated
  the OUTFIT one and is gone); REPLACE TEXTURE takes the outfit shown.
  SPEED 0.1x to 3x beside the frame bar. The home menu keeps its old text
  size (Mary).
- Viewer: Reptile and Noob Saibot showed Scorpion's texture. Their meshsets
  carry a mesh textured SCORPION_DIFFUSE of the same triangle count as
  their own, and the block's texture was taken from the first match; now
  the fighter's own <NAME>_DIFFUSE wins (assets.js loadFighter).
- Viewer: Kung Lao's hat. What rides a fighter's bones is in its .scene: a
  key per frame with the visibility, a placement matrix (palette) and, in
  byte +4, the bone (RenderScene.c scene_attach_to_bone; parseScene now
  reads it). The meshset meshes it names are placed by the key's matrix
  and then the bone's, frame by frame: the hat on bone 7 (head) or in a
  hand (11, 19) when a throw has it there. ACCESSORIES turns them off.
- **COVER** takes any picture too, made a PNG no wider than 640 in the
  window. Before, the file was copied as it was and a cover over 4 MB (a
  photo) was never shown.

## Release 0.1.8 beta (2026-10-22): character editor, mods in the viewer, Blender round trip, playable bosses

Published as the GitHub pre-release `v0.1.8-beta`, packaged as 0.1.7 was
(`launcher/make_release.py` over `electron-builder --win dir`; `electron-
builder --linux tar.gz` repacked with exec bits). On top of the MODS section
below (unreleased until now), what Mary asked for next:

### Motaro and Shao Kahn as players (debug menu FIGHTER 1/2)

- Mary's crash report (`running address 0x04840a04`, SHAO KAHN vs MOTARO,
  both builds, "always been there"). Reproduced with no window: the headless
  fight simulator (`fight_headless.c`) built with a crash handler, Shao Kahn
  as player 1 holding one button. The game never hands a boss to a player,
  and the per-fighter rows a player's buttons read stop at 24 or 25 words:
  - `DoSwitchJump` (other.c) read `row[25]` past the button rows and called
    the next table as code. Port guard: no button handler for c >= 24.
  - `get_char_ani*` / `get_his_char_ani*`: the bosses' animation tables hold
    38 animations, the others 73, and Motaro's second table is not a table.
    A flip read frame data as an animation (`find_last_frame` from
    `t_do_flip`). Port guard `ani_fetch`: a boss's missing animation is its
    stance.
  - `get_char_stk`: Motaro's strike row has 7 entries and Shao Kahn's 6
    (22 to 27 for the others). Port guard: none, and the two strike checks
    treat a missing strike as no hit.
- Checked: 736 headless fights, both bosses as player 1 and 2 against four
  fighters with 46 held inputs each, none crash; ordinary fights unchanged.
  **Mary has not played it yet.** The bosses only do what the game gives them.

### Mod data: `data.ini` over the fight tables

- Both table generators (`launcher/app/lib/extract/logic_tables.js`,
  `tools/build/logic_tables.py`) end `logic_tables.c` with
  `umk3_logic_names[]`: every word table by its name in the binary, its
  address and length.
- `mods_apply_data()` (runtime/mods.c), called after the asset root is set:
  each mod's `data.ini` lines `table[word] = value`, lowest priority first.
  Unit-tested standalone (priority, signed values, unknown names reported).
- A strike record (`stk_*`, 7 words, `strike_check_regs`): reach, top,
  width, height (signed 16-bit, measured from the fighter facing right:
  left = x + reach - width), reaction, damage (high byte full, low byte chip
  on block), squeeze. Shared rows: the six ninjas use `nj_strikes`, the
  three robots `robo_strikes`, the three female ninjas `fn_strikes`.
- Data mods change the fight: online, both players need the same mods.

### Launcher

- **CHARACTER EDITOR** (`sections/mods/ModEditor.svelte`, new tab): per
  fighter, the basic strikes (damage, chip, box, reaction) with a visual
  hitbox editor (`HitboxEditor.svelte`, drag to move, corner to size); the
  special moves and finishers the move list names (`seq_<fighter>_*`), with
  the missing ones marked (Noob Saibot, Motaro and Shao Kahn have no
  specials; Reptile and the hidden ninjas lack finishers); every per-fighter
  numeric table; the game's texts (a copy of `<language>_text.dat` in the
  mod: names, move list); the fighter's textures replaced by PNG. Reads
  `build/logic_tables.c` (`gameTables` in lib/mods.js).
- **Viewer**: SHOW WITH the game alone, the mods that are on, or one mod
  (`resRead(rel, layer)`, pictures told apart by their bytes); EXPORT ITS
  TEXTURES; EXPORT RIG (FBX); REPLACE ANIMATION / REPLACE MODEL from an FBX
  (`ui/src/lib/mods/fbxin.js`): a binary FBX reader, the FBX transform
  chain, and a similarity fit from the file's cluster rests to the game's
  T-pose or bind pose, so Blender's axes and units do not matter. An
  animation replaces one clip's frames in the `.skinanim` (resampled to the
  clip's length); a model is written as a new `.skin` on the same bones
  (each material to one of the fighter's blocks). Checked through Blender
  5.2 (import and export with defaults): Kano's clip back within 0.05
  units, Sindel's model (2 blocks) within 0.14; from the T-pose rest the
  model within 3.7 (the T-pose fit's own error). **The game has not loaded
  a replaced .skin yet: Mary's test.**
- Manager: COVER (cover.png/jpg, preview.* still read), SEE IN THE VIEWER.
- Not done: duplicating a fighter into a new slot (27th). Every per-fighter
  table of the game (strike rows, animation tables, the select screen's 26
  places, names, sounds, portraits) would have to grow; a session of its own.

## After 0.1.7: MODS section, game-side mod overlay, Linux build fixes (2026-10-22, released in 0.1.8)

Mary asked for a MODS option in the launcher's menu: a mod manager, a viewer
of the game's files and an exporter for Blender. All three are in, plus the
two Linux faults she reported on 0.1.6/0.1.7.

### The game reads mods (`src/runtime/mods.c`)

- `<game>/mods/<folder>/**` is indexed by lowercased file name at start-up
  (`mods_init`, called from `set_asset_root`). A mod replaces a game file by
  carrying a file of the same name, in any sub-folder.
- `mods/mods.ini` holds the order, highest priority first, as `on=<folder>`
  / `off=<folder>` lines; folders it does not name are on, after the rest.
  The first mod in the order that has a name wins.
- Hooked in `lime_platform.c` `resolve()` (every `limeLoadFile`, sound and
  music path) and in `draw_gl.c` `load_rgba`, where textures also match by
  stem (a `.png` in a mod replaces the game's `.pvr`) and a mod's `X` also
  serves `X_LOW`.
- `mod.ini` and `preview.*` at a mod's top level are the mod's own and are
  never served. Tested standalone (priority, sub-folders, _LOW, unnamed
  folders on).

### Launcher: MODS (`ui/src/lib/sections/SecMods.svelte`, `lib/mods.js`)

- **Manager**: create a mod (name, author, category: textures, audio, music,
  models), order (up/down = priority), on/off, category filter, preview
  picture, details with every file and the game file it replaces (files that
  replace nothing are flagged), add files, open the folder, delete (a second
  press). Writes `mods/mods.ini` and `mods/<mod>/mod.ini`.
- **Viewer** (three.js 0.170): fighters, stages, other models and textures
  from `res/`. Fighters are skinned on the CPU from the game's `.skin`,
  `.bones` and `.skinanim`; one clip or all, play/pause/frame, outfits,
  bones, wireframe, T-pose or the game's own pose. Textures can be copied
  into a mod to edit.
- **Exporter**: FBX (binary 7.4: skeleton, skin, one action per clip), USD
  as USDA, USDC (crate 0.8.0, written by `ui/src/lib/mods/usd.js`) or USDZ,
  and OBJ (mesh only); textures as PNG. Fighters with every animation, one,
  or none; rest pose T-pose or bind; their other outfits as PNG. Written to
  `<game>/exports/<name>_<FORMAT>/`.
- The T-pose: the rest pose is not in `.bones`, which is why exports came
  out broken. `ui/src/lib/mods/rig.js` solves the bind pose from the clips
  (least squares over rigid fits), finds hips/spine/arms/legs from the
  skeleton's shape, faces the figure along +Z by its knees, turns the head
  to the front by symmetry (Kano's head was sideways), straightens it, and
  fits a rest mesh so Blender's skinning reproduces the game's within about
  2 mm on average. Motaro (centaur) keeps the bind pose. Sindel's hair took
  the wrong texture; blocks now take their texture from the meshset mesh
  with the same triangle count.
- Checked by importing into Blender 5.2 headless (FBX, OBJ, USDA, USDC,
  USDZ: bones, actions, textures) and by opening the USDC in pxr 26.03.

### Linux

- `fatal error: 'GL/gl.h' file not found` (Mary's 0.1.7 log): the launcher
  now ships the GL, KHR and SDL2 headers in
  `src/runtime/platform/linux-include/` (Mesa/Khronos MIT, SDL zlib; see its
  README) and links `libSDL2-2.0.so.0` / `libGL.so.1` by soname when the
  `-dev` symlinks are missing. The runtime cross-compiles for
  x86_64-linux-gnu with Zig against those headers alone; **linking on a real
  Linux machine is untested.**
- "could not unpack the update" (0.1.6): the updater took the Windows zip.
  It now picks the `linux*.tar.gz` asset, unpacks it with the system `tar`,
  and installs with a shell script (`apply.sh`) that waits for the launcher
  to exit, swaps the folders, keeps `res/`, `mods/` and the ini files, and
  restarts it. Linux users on 0.1.7 still have the old updater and must
  download the next release by hand once.

## Release 0.1.7 beta (2026-10-22)

Published as the GitHub pre-release `v0.1.7-beta`, built the same way as
0.1.6 (`launcher/make_release.py` over `electron-builder --win dir` for the
Windows zip; `electron-builder --linux dir` repacked with exec bits for the
Linux tar.gz). On top of 0.1.6: the launcher rework and the 64-bit debug
crash fixes below, the SvelteKit port of the launcher's window, and the
Linux Zig download fix. Mary asked for the release before testing the debug
fixes; the port's launcher was in her test folder (`launcher/bin`) and
opens there.

### The launcher's window is a SvelteKit app (`launcher/app/ui/`)

- Svelte 5 + SvelteKit 3 (`@sveltejs/kit` 3.1, `adapter-static`, hash
  router), built into `launcher/app/renderer/` by `npm run ui`
  (`scripts/build-ui.js`), which electron-builder also runs before it packs
  (`build.beforePack`). `renderer/` is generated and git-ignored.
- `build-ui.js` makes SvelteKit's root `/_app/` paths relative (the page is
  loaded from disk) and recomputes the Content-Security-Policy hashes of
  the inline start-up script and of SvelteKit's route announcer's style.
- SvelteKit 3 notes: the config lives in `sveltekit({...})` in
  `ui/vite.config.js` (no `svelte.config.js`); `$lib` is gone, the code
  imports `#lib/...` through `ui/package.json`'s `imports`; no page options
  with the hash router.
- One theme: `ui/src/lib/theme/tokens.css` names every colour, surface,
  finish and size; `base.css` holds the shared controls. State is one
  `$state` object in `app.svelte.js`; sections and pieces are components.
- Matched to Mary's high-quality capture of the console's main menu:
  eight-block rings (36 degrees a block, 9 between) with brushed-silver rims
  and a lavender glow, 39 units between the rings' halos; a thin ring round
  the picture; the menu's curve, text size and spacing measured off it; the
  selection orb is a glass sphere with a red M turning in it; the command
  bar higher, slanted ends, fine lines; the cloud crimson top left and
  bottom right with a blue streak down the middle; the theme in lavender,
  silver and navy, the pages' glass deeper so text reads over the traces.

### Linux: Zig download stopped at once (`EXIT CODE: -1`)

- The log said only `Build failed:` -- an error with no message, which is
  what Node's `https` gives (an AggregateError) when every address of the
  server runs out of its connect time: Node 20 tries IPv6 then IPv4 for
  250 ms each. `lib/toolchain.js` now allows 5 s per address, retries over
  IPv4 alone, and reports the parts of such an error (`describeError`).
- Checked on Windows: the Linux archive downloads and its SHA-256 matches.
  The original failure could not be reproduced here (a fast link); **not
  yet run on Linux**.

## Launcher rework (2026-10-22, not yet tested by Mary)

- Menu: smaller lines and orb; PLAY moved second, under COMPILE YOUR .IPA.
- No game compiled in either build (`root:get` -> `any`): the menu shows
  COMPILE alone; the first successful compile relaunches the launcher
  (`app:relaunch`, app.relaunch + exit) so the whole menu appears.
- A section opened from the menu fills the window, after the console
  release's online pages: its name between blue arrows (Q / E, the pad's
  triggers or bumpers step through sections), the GRAPHICS and CONTROLS
  pages in a second row between arrows (left / right, the d-pad), the
  chosen page with light lines over and under it; Esc / B returns to the
  menu, which keeps its old layout.
- Backdrop shared by both: a generated cloud of red, purple and blue light
  (`nebulaURL`), dark circuit grooves with grey edges, a big ring of short
  thick marble blocks and, on the menu only, a smaller one to the right.
- Home screen laid out as the console's main menu, in the rings' own
  1600 x 900 box (`--k` in the theme's tokens.css), so it holds at any window size:
  the title top right with a dark border, a white-blue halo and light rays
  behind it; the player's picture `aditional textures/image.png` (read at
  run time by `hero:get`, never shipped: it is game art) inside the small
  ring, tinted blue and faded towards every edge; the description box under
  it; the menu down the left 70 units a line. The version moved to the top
  left corner.

## Debug crashes on 64-bit, fixed (2026-10-22, not yet tested by Mary)

Both from Mary's crash logs, symbolized with the build's .pdb:

- Opening the debug menu, or its STAGE list: `stage_name` (debug_menu.c)
  read Level_Info's name field (+0x1c, a 32-bit word) as an 8-byte pointer
  and snprintf crashed. Now `const char *P32`.
- HITBOXES on: `hb_draw` (game_main.c) read the fighters at G +0xbc /
  +0x214 the same way and `hb_body` crashed. Now `P32` too.

Both files compile for x86 and x64. Mary also reported that the stage of
a debug fight stays chosen afterwards. Not changed: her logs show no debug
fight in the session where CAVE repeated (Arcade, then Survival, then
Karnage), and Karnage and Survival keep `LevelSelect` by the iOS game's
own rule (FrontEnd.c, modes 3/4/5). Waiting for her steps.

## Reported by Mary on 0.1.6 beta, to fix after the launcher rework (2026-10-22)

Not investigated yet; each needs its mechanism traced in the binary first.

- **64-bit:** the FLAWLESS VICTORY text is not shown (fine in 32-bit).
  Fixed: Mary confirmed it on 2026-10-22.
- **Reptile's fatalities** sometimes did not work fully; in one of them he
  only raised his arms. Taken off the known problems on 2026-10-29 at
  Mary's word: it was not Reptile's alone.
- **Hitboxes** (the debug view) crash the game. Fixed, see above.
- **Linux:** compiling in the launcher fails at once:
  `1/5 Preparing compiler toolchain... Starting download of Zig 0.13.0
  compiler / Build failed: / EXIT CODE: -1` (lib/toolchain.js).

## Release 0.1.6 beta (2026-10-22)

Published as the GitHub pre-release `v0.1.6-beta`, two packages:

- `UMK3-PC-0.1.6-beta.zip` (Windows): `launcher/make_release.py` over the
  `electron-builder --win dir` output. It now copies all of `src/` (the old
  file list had lost `mp_eos.c` and `ui_alert.c` and still named the
  removed .ps1 scripts), the EOS SDK from `TOOLS/eos` (headers, the Win64
  DLL, eos.ini; git-ignored, never in the repository), and builds the stub
  with zig when llvm-mingw is missing. Checked: the release folder compiles
  umk3-game64.exe (with EOS) and umk3-game.exe from the 1.2.59 .ipa.
- `UMK3-PC-0.1.6-beta-linux-x64.tar.gz`: `electron-builder --linux tar.gz`,
  repacked so the launcher and helpers are executable (a tar made on
  Windows has no exec bits). The EOS SDK lands in `resources/tools/eos`
  (Windows merges TOOLS/ into tools/), so `findEosDir` in compile.js looks
  for both spellings. **Never run on Linux.**
- Known: the launcher's updater takes the first .zip, so a Linux launcher
  would offer the Windows zip; Linux players update by hand.
- Open question for Mary: whether the EOS SDK headers may be redistributed
  under Epic's licence, and whether eos.ini's client credentials are a
  game-client policy (they are public in the zip).

## Debug menu, mouse wheel, iPhone-style boxes on Linux (2026-10-22, tested on Windows)

Port, not in the binary. Built for Windows x64 and x86; the Linux files
pass a zig `x86_64-linux-gnu` compile with the SDL2 lint header, but nobody
has run them on Linux yet. Mary tested all of it on Windows on 2026-10-22.

- **Debug menu (F2)** is laid out like the settings menu (Esc): a side panel
  with three categories (FIGHT = fighters + round + Arcade + cheats, MENUS,
  TOOLS/OPCIONES = options + tools + save states), sections under headings,
  a list that scrolls, and a list of values that opens with Enter or a
  click. Labels are Spanish or English, following the game's language.
  Picking a SCREEN or a FINISHER from its list does it at once.
- **New debug options:**
  - INFINITE HEALTH (player 1, 2 or both) and FREEZE CLOCK. These are the
    PortDbgHealth/PortDbgClock hooks in logic/other.c's bar_reducer and
    t_clock4, beside training's. game_main.c forces them off in an online
    fight.
  - FRAMES PER SECOND and ONLINE NUMBERS (rb_stats: frame, delay, remote
    lead, rollbacks, deepest, holds, desyncs), drawn under the INFO line.
- **Mouse wheel:** plat_mouse_wheel (WM_MOUSEWHEEL, SDL_MOUSEWHEEL) moves
  the cursor in both menus and in their open lists.
- **ui_alert.c:** the iPhone OS 3 boxes now live in one file shared by both
  backends: the alerts (plat_ask), the menus, the grid, the badge, and the
  pad button pictures.
  - Windows supplies GDI's Arial and its raw keys (ui_alert.h).
  - Linux uses stb_truetype (platform/stb, public domain/MIT) with the first
    font it finds: "aditional textures/fonts/ui_bold.ttf"/"ui.ttf", then
    Liberation Sans, DejaVu, Noto, then fontconfig. Linux also gets the
    keyboard and pad pictures, and the pad's kind comes from the SDL vendor
    id.
- **Alerts with the pad:**
  - Left / right (pad or arrow keys) light a button.
  - South or START presses the lit button, or OK when none is lit.
  - East is CANCEL.

## Rollback netcode for ONLINE fights (2026-10-22, in 0.1.6 beta)

Status: **tested by Mary on one PC, two windows (LAN), 64-bit:** fights run on
both, 0 desyncs, snapshots 0.8 ms / 6.1 MB, a few holds. Not yet tested over the
internet or 64 vs 32 bits. Port, not in the binary.

Fixed while testing (2026-10-22):
- `limeMalloc` takes its blocks from the save-state arena during a rollback fight;
  its own region is not rolled back, so a re-run frame freed a block twice.
- Snapshots skip `PreloadedCharacters` (60 MB the fight only reads); the copy was
  66 MB and 11 ms a frame.
- The guest waits, before loading the fight, for the host's hello and takes its
  level. Before, each side loaded its own random level; with the classic netcode
  the host's level arrived mid-fight and the guest crashed in `RenderLevelBG`.
- Multiplayer versus screen: the kode wheels each side may press were transcribed
  wrong (armv7 0x16b98: host 0..4, guest 7..9); portraits and wheels now sit at
  the screen's edges as on `FE_Task_VS_Screen`.
- 64-bit crash reports unwind the stack (.pdata) and print exe offsets.

- `src/runtime/rollback.c`: both machines run the fight from both players' inputs;
  only inputs travel (packets 20 INPUT, 21 HELLO, 22 HASH over `mp_net`). The
  remote input is predicted as the last confirmed one; a late input that differs
  rolls the state back and re-runs the frames silent and undrawn. Input delay 2
  (`UMK3_RB_DELAY`), at most 8 frames predicted, a desync hash every 60 frames
  (`DESYNC at frame` in the log). `UMK3_NETCODE=classic` turns it off.
- `src/runtime/savestate.c`: per-frame snapshots (`st_rb_*`) of `.umkstd/.umkstb`
  plus the arena pages written during the fight (MEM_WRITE_WATCH). Files built with
  `UMK3_NO_ROLLBACK` (lime_menu, lime_platform, lime_app, wav, limeMPGameSpecific)
  keep their state out of it; `PreloadedCharacters` too.
- Determinism: `random32` uses `umk3_rand`, held in the state; the frame step is
  pinned to 1. A recorded fight replays identically on x64 over 3038 ticks; x86
  differs only in the event queue (address-derived arguments), not yet fixed.
- No pause in a rollback fight yet (a synced pause is still to do).
- Runs over the LAN transport and over EOS (0.1.6 beta).

## Mileena's fatalities (2026-10-22)

Both were reported by the tester and both are confirmed fixed in her build.

- **Nail fatality (BIG CUP in the wrong place):** checked against the binary function by function (`RenderLevelPlayers`, `RenderPlayer`, `RenderAnimatedCharacter`, the bone attach in `LIME_RenderScene`, `limeMatrixMult`). The missing piece was the `else` arm of `RenderAnimatedCharacter` at 0x5c2f4: a frame whose body is not skinned still calls `GenerateMatrices`. Without it, `MatrixPalette2` kept the other fighter's pose and the cup followed his hand.
- **Suck-kiss fatality (victim's body kept reappearing):** `RenderLevelPlayers`, the event code (`LIME_TriggerEventFromSceneH`, `LIME_UpdateEvents`, `LIME_RenderEvents`, `CountEventsMatching`, `LIME_TriggerEventsFromSceneOffsetIfFollowing`), `mk3_update`, `t_r_kiss_suck`, `t_mframew` and `fastxfer_thread` all match the binary. The divergence was a port-only block that replaced a frame-list `-1` with the last pose. The victim's frames 1399..1405 are `-1`; held at 327, an invisible fighter re-fires that frame's events every draw. The block now applies only to Classic Sub-Zero.

## What was built and verified in this session (2026-10-18)

1. **Pure Node.js Extraction Pipeline (`launcher/app/lib/extract/`)**:
   - `zip.js`: Zero-dependency PKZIP reader supporting uncompressed (0) and Deflate (8) streams using Node's standard `zlib`.
   - `macho.js`: 32-bit universal (fat) and thin Mach-O parser verifying armv7 slice, UUID (`90d6f56a18e2303f8f2053b02a42742e`), and FairPlay encryption (`cryptid == 0`).
   - `level_info.js`: Extracts the anonymous arena string pool and pointer relocations into `level_info.c` (verified 100% byte-for-byte identical to Python).
   - `seq_data.js`: Extracts playback scripts into `seq_data.c` (verified 100% byte-for-byte identical to Python).
   - `logic_tables.js`: Reads data tables and performs 4,291 pointer relocations across 1,118 symbols into `logic_tables.c` (verified 100% byte-for-byte identical to Python).
   - `low32_static.js`: Generates 64-bit low-pointer fixups (`umk3_globals_fixup`) for `gamecode_globals64.c`.
   - `ipa.js`: Orchestrates binary verification, asset unpacking (`res/`), and C table generation in ~2 seconds.

2. **Standalone Toolchain & Parallel Compiler (`launcher/app/lib/toolchain.js` & `compile.js`)**:
   - Automatic download and extraction of pinned Zig 0.13.0 with SHA-256 verification (fallback to system Zig if already present).
   - Multi-threaded compilation pool compiling 74 C source files in parallel.
   - Successful build and link of native 64-bit Linux binary (`umk3-game`) with `-DUMK3_LOW32`, `-no-pie`, and low-pointer memory mapping.
   - Verified native execution on Linux: `UMK3_SHOT=10 ./umk3-game` executed cleanly, producing valid PPM frame capture (`851x1413`).

3. **Packaging Configuration & Release**:
   - Linux: `.tar.gz` portable archive and `.AppImage` standalone executable via `electron-builder`.
   - Windows: `.zip` archive and portable executable (`UMK3-Launcher-Windows.exe`).
   - Packaged and published GitHub Release `v0.2.1` with all 4 release binaries attached.
   - Removed legacy `build_game.ps1` and `setup_res.ps1`.

4. **Full English Localization**:
   - All build stages, progress bars, compiler messages, dialog boxes, and update alerts localized to English.
   - Default UI language set to English (`EN`) while keeping the multi-language toggle for Spanish.

5. **Linux LP64 Memory Model Compatibility & Freeze Resolution**:
   - Diagnosed runtime game freeze during character mesh loading: `sizeof(long)` is 4 bytes on ARMv7 iOS and Win32/Win64 (LLP64), but 8 bytes on Linux x86_64 (LP64).
   - In `Players.c:1918`, `meshbaseFile` pointer arithmetic `base + 1` and `hdr[0]` doubled offsets, causing `meshCount` to read as 4.5 quintillion and triggering an infinite loop on `LAC: meshbase out of ram`.
   - In accordance with the project rule to never modify the 1:1 decompiled C files in `src/decomp/`, the fix was implemented in the runtime layer: `src/runtime/savestate.h` includes standard/POSIX and GL headers before defining `#define long int` for state files under Linux 64-bit builds, with `compile.js` passing `-DUMK3_NO_LONG_REMAP` for `text.c`.
   - Verified in GDB and runtime: Kano (`who=0`, `meshCount=366`) and Sonya (`who=1`) loaded successfully and the game loop proceeded into active gameplay.
   - Launcher UI dynamically detects platform (`window.umk3.platform`): displaying `LINUX` with `64 BITS (UMK3-GAME)` on Linux hosts, and targeting the native `umk3-game` executable.

---

## Verification Suites

1. **Static & Structural Checks (`bash tools/decomp/check.sh`)**
   - Compiles all `src/decomp/` and `src/runtime/` units with `-Wall -Wextra -O2`.
   - Zero errors across the entire tree.
2. **Banned Words & Clean-Room Audit (`python3 tools/decomp/check_banned_words.py`)**
   - Passes with 0 errors.
3. **Launcher Unit Tests (`node --test test/*.js` in `launcher/app/`)**
   - 15 unit tests passing covering configuration, ZIP reader, Mach-O parser, globals transformation, and launcher state.
