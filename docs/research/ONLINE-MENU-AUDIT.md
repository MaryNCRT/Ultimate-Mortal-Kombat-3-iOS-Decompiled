# ONLINE menu redesign — Stage 1 audit

Date: 2026-11-06. Scope: everything the PC port has for online play today,
read from the source in this repository, before any of it is replaced.
Nothing was changed in the game or the launcher for this report.

The redesign brief (Mary) asks for HOME, FRIENDS, INVITATIONS, LOBBIES,
QUICK MATCH and PARTY sections; profiles with real stats; a friends system
with categories, requests, block and invite; an invitations centre; a lobby
browser with filters and a full-screen CREATE LOBBY page with real rules
(Best of 1/3/5, Combo Slide, Host Advantage, time); host controls; a lobby
room with READY, ping and synced rules; matchmaking; notifications;
keyboard, mouse and pad; and rules that are authoritative and checked before
the fight. The look keeps the current night cemetery, moon and blue light,
with translucent panels and MK red accents.

---

## 1. Architecture found

Online play is four layers. Only the bottom one (the game's packet code) is
from the binary; everything else is the port's.

```
 Launcher (Electron, launcher/app)                 Accounts server
   lib/account.js  -- HTTPS JSON -->               Cloudflare Worker + D1
   main.js pollFriends (15 s)                      (private repo; not audited)
      | friends.txt / online_invite.txt / online_join.txt (files beside the exe)
      v
 Game, ONLINE screen (FrontEnd.c FE_Task_Online, task 51; online_ui.c badges;
                      gamecode_stubs.c port_online_*)
      v
 mp_net.c  -- session, reliable channel, find-by-code, LAN lobby
      |-- UDP 27560-27575, broadcast (LAN, Radmin/Hamachi/ZeroTier)
      |-- mp_eos.c: EOS Connect (device id), EOS Lobby as a directory, EOS P2P
      v
 limeMPGameSpecific.c (binary's packets 1..14: character, level, kode, sync…)
 rollback.c (port packets 20..24: input, hello, hash, palette, stage)
      v
 Fight engine (GameCode.c Task_GameInit / Task_GameMain)
```

### 1.1 Identity: the profile id and the account

- **Profile id** = the `XXXX-XXXX-XXXX` code in `profiles.ini`
  (`runtime/profiles.c`). It is the only identity the game itself knows: the
  handshake (`mp_net.c` `info_put`) carries code, name (100 bytes UTF-8),
  icon, two colours, dark, angle, outfit.
- **EOS** (`mp_eos.c`): anonymous **device-id** login (`EOS_ECT_DEVICEID_ACCESS_TOKEN`,
  display name fixed "UMK3 Player"), product keys from `eos.ini` beside the
  exe, SDK DLL loaded at run time (Saints Reborn's design). No Epic account,
  no EOS friends, no EOS presence. The EOS user id is never tied to the
  profile code anywhere except inside a lobby bucket name.
- **Account** (`launcher/app/lib/account.js`): optional login to Mary's
  Cloudflare Worker (`/v1/register`, `/login`, `/me`, `/progress`, `/scores`,
  `/profile`, `/users`, `/friends`, `/invite`, `/recovery`). Guests play with
  the profile code alone. The account claims the profile code (`claim()`), so
  friends are keyed by profile code.

### 1.2 Finding a player ("FIND BY CODE")

`mpnet_find(code)` asks two ways at once:

- **LAN**: a `JOIN` datagram broadcast to every adapter's broadcast address
  and localhost, ports 27560-27575.
- **Internet**: `mpeos_lookup` searches EOS lobbies in the bucket
  `"umk3:" + code`. While a player's ONLINE screen is open their game owns a
  2-member public lobby in that bucket (`mpeos_advertise`). Nobody ever joins
  it; it is only a directory entry. Then mp_net's datagrams go over EOS P2P
  (NAT traversal, Epic relays).

Whoever is found **hosts** (`connect_to(..., server=1)`). If both search for
each other at once, the lower code hosts. A host already connected answers
`BUSY`. Timeouts: 10 s LAN, 30 s if EOS found the owner.

### 1.3 The session and its reliable channel (`mp_net.c`)

Replaces the iPhone's GKSession. Header `'U' '3' kind`; kinds `JOIN, ACCEPT,
BUSY, DATA_U, DATA_R, ACK, BYE, KEEP, HELLO`. Reliable packets: sequence
numbers, a 256-packet window, resend every 120 ms, in-order delivery. Keep-
alive every 1 s; **link lost after 15 s of silence**, or on `BYE` →
`disconnected()` + `endMP()` (the binary's own path). `mpnet_session_destroy`
sends BYE twice.

### 1.4 The game-side packets

- **Binary's** (limeMPGameSpecific.c, 1..14): CHARACTER, MENU, FE MENU,
  SYNC, SPRITELIST, JOYSTICK, INGAME EVENT, **KODE (3 digits + slot)**, LEVEL,
  FPS, HEARTBEAT, TIMER, PLAY, GENERIC. The binary's versus screen already
  sends the VS kode to the other side.
- **Port's** (rollback.h): `RB_PKT_INPUT 20`, `RB_PKT_HELLO 21`,
  `RB_PKT_HASH 22`, `PORT_PKT_PALETTE 23`, `PORT_PKT_STAGE 24`.

### 1.5 Rollback (`rollback.c`)

Both machines run the fight; only inputs (11 bits a frame) travel.
Per-side input delay starting at 2, adapted once a second toward ~3 frames
of rollback (max 6; `UMK3_RB_DELAY` fixes it). Snapshots from
`savestate.c` (`st_rb_*`, 15 slots). A side ahead by ≥3 frames on average, or
12 frames past its last remote input, **holds**. The fight only ends on a
frame with both real inputs. A hash every 60 frames; a mismatch is logged
`DESYNC` but nothing else happens. Both sides seed the RNG from
(fighter1, fighter2, level).

HELLO carries `RB_VERSION` (1), the button count (`Settings[4]`), and the
setup (c1, c2, level, side). A version mismatch and `THE SETUPS DIFFER`
are **only printed to the log**; the fight starts anyway, except that the
guest takes the host's level (`rb_host_level`).

`UMK3_NETCODE=classic` turns rollback off and runs the binary's
host-authoritative netcode (host steps the engine, streams sprite lists).

### 1.6 The ONLINE screen (FrontEnd.c `FE_Task_Online`, task 51)

Drawn by the game's own front end over the Arcade select background (the
night cemetery with moon, which the brief wants kept). Left: the player's
badge, YOUR CODE, COPY. Right: the other player's code box (typed or Ctrl+V)
and FIND MATCH (`BUSCAR PARTIDA`), or a FRIENDS page with up to four rows
(`PortOnlineFriends`, invites first, INVITE / JOIN). Strings are the port's,
in 8 languages (`PortOnlineText`). Status line: INTERNET OFF / CONNECTING /
READY / COULD NOT CONNECT. Once connected the flow is the binary's: GameMode
1, the multiplayer select screen (0x2a), versus, fight, summary (REMATCH).

### 1.7 Friends and invites

All on the server, reached only by the **launcher**:
`GET/POST/DELETE /v1/friends` (request, accept = mutual POST, remove),
`GET /v1/users?q=` (search), `POST/DELETE /v1/invite` (invites live three
minutes, friends only). `main.js pollFriends` asks every 15 s, shows a
Windows notification for new requests/invites, and writes `friends.txt`
(`friend|CODE|name|look`, `invite|CODE|name|look`) for the running game.
The game writes `online_invite.txt` ("invite CODE" / "join CODE") which the
launcher sends on. The launcher's PROFILE > FRIENDS page
(`ui/src/lib/sections/ProfFriends.svelte`) has the full list.

### 1.8 Other pieces

- **MULTIPLAYER (LAN only)**: `mpnet_set_lan_only(1)`, HELLO broadcast once a
  second, a list of up to 16 games; same JOIN afterwards.
- **Mods online**: `mods_online(1)` puts back every data value a mod changed;
  only models/textures stay (`mods.c`).
- **Settings menu online**: refused while `GameMode == 1` or a session exists.
- **Leaderboards**: launcher `boards()` / `flushScores()`, `leaderboards.txt`.

---

## 2. Status: works / partial / missing

| Area | Status | Notes |
|---|---|---|
| EOS login (device id) | **Works** | Anonymous; display name is a constant. |
| Find by code (LAN + EOS) | **Works** | Only while the other ONLINE screen is open. |
| P2P transport, reliable channel | **Works** | |
| Rollback | **Works on Windows** | Checked with two games on one PC; not yet between two machines (README known problems). `savestate.c` snapshots are `#ifdef _WIN32`: on Linux `st_rb_begin` fails and the log says `NO SNAPSHOTS`. |
| Disconnect | **Works, crude** | 15 s silence; no reconnect, no "opponent left" result, no rage-quit record. |
| Version check | **Partial** | `RB_VERSION` compared and logged only; no game/build version in JOIN; nothing blocks. |
| Rules sync | **Partial** | Host's level wins; VS kode sent by the binary's packet; setup mismatch only logged. No rules object. |
| Accounts | **Works** (launcher) | Server not in this repo. |
| Friends: list, request, accept, remove, search | **Works** (launcher), **partial** in game (4 rows) | |
| Friends: categories, block | **Missing** | No server field or endpoint. |
| Invites | **Partial** | Received (3 min) and taken/declined. No *sent* list, no *expired* list, no pending state shown in game. |
| Presence (online / in menu / in match) | **Missing** | EOS presence off (`bPresenceEnabled = FALSE`); server has no presence. |
| Lobbies (browse, filters, create, join, room, READY, kick, transfer) | **Missing** | The EOS "lobby" is only a 2-member directory entry nobody joins. |
| Party | **Missing** | |
| Quick Match / matchmaking | **Missing** | There is no matchmaking at all today. |
| Notifications in game | **Missing** | Only Windows toasts from the launcher. |
| Player stats online | **Partial** | `statsdata` (Stats[] array: flawless, fight time, health kept, rounds) and achievements are local and synced via `/v1/progress`; no online W/L record. |
| Pad / mouse / keyboard on the ONLINE screen | **Partial** | Keyboard and mouse; README known problem: a pad's X glyph shown over START. |

---

## 3. Problems with the current UI

From Mary's two screenshots (`postimg.cc/5HyCPrPY`, `postimg.cc/XBfGG6vP`)
and the code:

1. **One screen does everything**: identity, code entry and friends share
   one page; FRIENDS replaces the code box instead of being a section.
2. **The code-entry model** requires both players to have the ONLINE screen
   open at the same moment; there is no lobby, no queue.
3. **Small, flat elements** floating on the background: no panels, nothing
   grouping the badge, the code and the actions; the status line
   (`INTERNET: LISTO`) is tiny at the bottom.
4. **Four friend rows**, no paging, no state (online, in match), no
   categories.
5. **No feedback** for sent invites or for what the other player is doing.
6. **Mixed-case text through the game's font** — the font is ASCII, names
   with other characters show `?` (`mayhem_name`).
7. **Pad navigation** is partial (the glyph bug above) and the layout was
   built for the mouse.
8. **Strings hand-written in `PortOnlineText`**, 8 languages in a C array:
   each new screen multiplies this.

---

## 4. Reusable parts

- `mp_net.c` transport, reliable channel, LAN discovery, BUSY, link loss —
  keep as is; it is the layer under any lobby.
- `mp_eos.c` dynamic loading, login, P2P — keep. The lobby code
  (`lobby_step`, `look_now`) is the seed of real lobbies: today it creates
  2-member lobbies with no attributes.
- `rollback.c` — keep; HELLO is the natural place to exchange and check a
  rules block.
- `limeMPGameSpecific.c` — binary code; untouched. Its KODE packet already
  carries a VS kode.
- Launcher `account.js` and `pollFriends` → the friends/invites data for the
  new screens; `friends.txt` / `online_invite.txt` as the game↔launcher
  channel (or replaced by a richer file, see §6).
- `online_ui.c` badges and `profileIcon` look fields.
- `PortOnlineBoxPx` mapping for the mouse; `ui_nav_frame` conventions.

---

## 5. Rules: what the engine really has

### 5.1 Rounds and series (Best of 1/3/5)

`GameCode.c` `Task_GameInit` state 0 (armv7 decomp, line ~6280):

```
GameTime = 99; Round = 0; RoundWins[0] = RoundWins[1] = 0;
WinsNeeded = 2   (versus and everything but training 2/0, survival 1, karnage 0)
```

`WinsNeeded` (`0x0014e234`) is the series: a round ends in
`RoundEndedAgainst` (`RoundWins[winner]++`; `== WinsNeeded` → `updateMPWins`,
match over). So **Best of 1 / 3 / 5 = WinsNeeded 1 / 2 / 3**, set right
after the binary sets it in an online fight. Both machines must set the same
value before the first frame (it is part of the simulated state; a mismatch
desyncs). Check the round banners: `Round` 0..3 select hardcoded
`"ROUND 1"`..`"ROUND 4"` — **Best of 5 can reach round 5 (and more with
draws), past the binary's four banners**. That needs a port fix (a fifth
string) before Best of 5 ships.

The kode `0x13` (see 5.3) already does "one round, both at match point".

### 5.2 Time

`GameTime = 99.0f` at init and at every round start (line ~6545);
`ClockTens`/`ClockSingles` drive the HUD; `GameTime <= 0` ends the round by
health. A time rule (e.g. 30/60/99) is a port change to those two writes,
**not an option the binary has**. "Infinite" would need the countdown
skipped — also port-only. Feasible, but it is new behaviour, so it must be
synced and flagged as the port's.

### 5.3 Rules the binary already has: the VS kodes

The kode table at `0x00176c04` (20 entries × 32 bytes, read from the binary
for this report; text from `english_text.dat`):

| value | GameText | kode |
|---:|---:|---|
| 0 | 148 | BLOCKING DISABLED |
| 1 | 149 | DARK KOMBAT |
| 2 | 150 | INFINITE RUN |
| 3–12 | 151–160 | PLAY IN/ON a fixed stage (10 stages) |
| 13 | 161 | PLAYER 1 HALF POWER |
| 14 | 162 | PLAYER 1 QUARTER POWER |
| 15 | 163 | PLAYER 2 HALF POWER |
| 16 | 164 | PLAYER 2 QUARTER POWER |
| 17 | 165 | POWER BARS INVISIBLE |
| 18 | 167 | THROWING DISABLED |
| 19 | 168 | "UNIKORIV REFERRI: SANS POWER" (one round, both at 1 health) |

(GameText 166, SILENT KOMBAT, is in the text but has no table entry.)

`theKode` holds **one** value; the binary's versus screen picks it and sends
it (KODE packet). These are the honest "other rules" for CREATE LOBBY:
blocking off, throws off, dark kombat, infinite run, power handicaps, bars
invisible, sudden death. They are already in the simulation, so rollback
handles them as long as both sides have the same `theKode`.

### 5.4 Combo Slide = the port's KOMBO SLIDE (issue #114)

It is **already in the port**, offline only (released in 0.2.0, Mary tried
it). The mechanic is the binary's: inside a chain kombo the opponent is
pushed away by each hit and by the chain's reaction, so a long kombo
"slides" him out. Two places:

- `t_ccp3` (`decomp/gamecode/logic/mkreact.c`): the chain reaction; with
  the binary's rule, when the punch count `field1c` is not below `field34`
  it goes to `t_separate_us` (the push apart). `PortNoKomboSlide` makes it
  take the plain exit instead.
- `away_x_vel` (`logic/other.c`): a hit's push away; from the second hit
  in a chain (`field4c >= 2`) the port drops it when `PortNoKomboSlide`.

Option: `port_kombo_slide` / `kombo_slide=` in umk3.ini, the settings
menu's GAME and the launcher's PLAY. `game_main.c` sets
`PortNoKomboSlide = !port_kombo_slide && !rb_active() && !rp_playing() &&
GameMode != 1` every tick — **forced to the original's rule in every
versus/online fight**, because it changes the simulation and the two
machines must agree.

So "Combo Slide ON/OFF" for a lobby is real and cheap: make it a synced
rule (part of the rules block, §9 stage 5) and let `game_main.c` take the
lobby's value instead of forcing ON when `rb_active()`. Both sides must
have the same value before frame 0, or the hash desyncs. It is the port's
rule, not the binary's: the UI should say "OFF (port option)".

Not to be confused with `ComboSlider1/2` (`0x0014e28c` / `0x0014e294`),
the HUD combo counter's slide-in, which is drawing only.

### 5.5 Other settings

`Settings[10]`: 0 blood, 1 vibration, 2 music volume, 3 sound volume,
4 buttons (5/6), 7 notifications. Only **buttons** is already exchanged
(HELLO). **Blood** is a local preference; it gates blood events, so before
offering it as a lobby rule it must be checked whether it changes simulated
state (if it does, both sides must match or the hash desyncs).

Difficulty does not apply to versus.

---

## 6. Host Advantage — what it really is in this netcode

With **rollback** (the default), the two sides are nearly symmetric:

- each side's input delay is its own, from its own round trip;
- holds are symmetric (whoever is ahead waits);
- both run the same engine on the same inputs.

What the host still has:

1. **Player 1 / left side** (`g_me = isParent() ? 0 : 1`).
2. **Stage authority**: the guest takes the host's level
   (`rb_host_level`); the host's STAGE SELECT is the one that counts, and
   the host's CANCEL keeps the game's own stage.
3. **Tie-breaks**: the lower code hosts when both search.
4. In the binary's flow, the host drives the menus (`isParent()` checks in
   the select screen; `MP:KODE NOT ENTERED` — the parent's requested level
   overrides).

None of these is a frame advantage. **The real frame advantage only exists
with `UMK3_NETCODE=classic`**: the host steps the engine with its own input
immediately and the guest sees the full round trip on every press. That is
the binary's design (GKSession over Bluetooth).

So "Host Advantage OFF" can honestly mean:
- force rollback (refuse classic) — already the default;
- **randomise or alternate the side** (who is P1) — possible only if the
  host/P1 coupling (`g_me = isParent()`) is split; that is the port's code,
  not the binary's, so it can be done;
- let the stage be chosen by both (random of the two picks, or the loser of
  the last game) instead of the host.

"Host Advantage ON" = today's behaviour. It cannot mean "the host gets fewer
frames of delay": per-side delay is computed from RTT, and giving the host
less would only make the guest roll back more. Do not offer that.

---

## 7. Backend changes needed

The server is in Mary's private repository; this list is what the client
side would need from it.

| Feature | Need |
|---|---|
| Presence | `PUT /v1/presence` (state: menu / lobby / match, lobby id) with a TTL; returned in `/v1/friends`. Or EOS Presence (needs Epic Account Services — not just device id — so probably the Worker). |
| Friends categories | a `category` (or `favourite`) column on the friend row. |
| Block | `POST/DELETE /v1/block`; blocked users filtered from search, requests, invites. |
| Invites centre | `GET /v1/invites?dir=sent`, a status column (pending / accepted / declined / expired), keep expired rows for some time instead of deleting after 3 min. Invite to a **lobby id**, not just to "a fight". |
| Lobbies | Two options: **(a) EOS Lobbies** (already loaded, supports attributes, search by attributes, max members, kick, promote, host migration off) — the natural choice, no server work; **(b)** Worker-side lobbies. Recommend (a), with lobby attributes = rules + version. |
| Quick Match | **EOS Lobby search** on a "quickmatch" bucket with version/region attributes: join the first open one, else create one and wait. True skill-based matchmaking would need a server queue; nothing in the game records a rating today. |
| Party | EOS Lobby with a separate bucket ("party"), up to N members, presence-only; or a Worker table. |
| Online stats | `POST /v1/match` (result posted by both sides, accepted when they agree) for W/L, rage-quits. Without it, profiles can only show the local `statsdata`. |
| Version | the build version in every lobby attribute and in the JOIN handshake. |

EOS limits to keep in mind: a lobby's member count is ≤ 64; lobby attribute
values are strings/ints/bools; anonymous (device id) users **can** create and
search lobbies and use P2P, but **not** EOS Friends/Presence (those need Epic
Account Services) — so friends stay on the Worker, as now.

---

## 8. Game ↔ launcher channel

Today: `friends.txt`, `online_invite.txt`, `online_join.txt`, polled every
2 s / 15 s. A full social UI inside the game needs more (requests, sent
invites, block, presence, notifications). Two ways:

1. **Keep the files**, extend the format (`request|`, `sent|`, `blocked|`,
   `presence` field; and the game's commands `accept CODE`, `block CODE`…).
   Simple, already proven, latency up to 15 s.
2. **The game talks to the Worker itself** (WinHTTP / libcurl with the
   account token). Faster, but moves the token into the game and adds an
   HTTPS stack to the C runtime.

Recommend 1 for now, with the poll at 5 s while the ONLINE screen is open.

---

## 9. Staged plan

Each stage: build into the test folder, Mary tests, document, fix, then the
next stage. Nothing working is removed until its replacement is approved.

1. **Audit** — this report. *Waiting for Mary's answers (§10).*
2. **HOME and navigation** — a new `FE_Task_Online` shell: section bar
   (HOME, FRIENDS, INVITATIONS, LOBBIES, QUICK MATCH, PARTY), translucent
   panels over the existing cemetery background, MK red accents, one
   focus model for keyboard / mouse / pad (fixes the X-over-START glyph).
   HOME = profile badge, code, COPY, status, and today's FIND BY CODE kept as
   a card so nothing is lost.
3. **Friends, profiles, invites** — friends list with paging, categories,
   requests, block (needs §7 server work), invite; profile card with only
   real stats (statsdata, achievements count, and W/L once the server has
   it); invitations centre (received / sent / pending / expired).
4. **Lobbies** — EOS lobbies with attributes; browser with filters;
   full-screen CREATE LOBBY with a rules preview.
5. **Rules and sync** — one `struct online_rules` (best-of, time, kode,
   combo-slide if confirmed, side/stage policy, build version, rules
   version), sent in HELLO and in the lobby attributes; **mismatch blocks
   the start** with a message instead of a log line. Best of N via
   `WinsNeeded`; fifth round banner.
6. **Lobby room** — members, READY, ping (from `g_rtt` / keepalive), synced
   rules, host controls: kick, invite, transfer, close, start.
7. **Party and Quick Match** — party as an EOS lobby of its own; Quick Match
   via an EOS bucket with version attributes.
8. **Engine integration** — rules applied in `Task_GameInit` on both sides,
   side/stage policy (Host Advantage), disconnect result, stats posting.
9. **Tests** — two machines; desync hash must stay at 0 for every rule.

---

## 10. Open questions for Mary

1. **Combo Slide**: confirm it is the KOMBO SLIDE option of issue #114
   (§5.4), made a lobby rule; today it is forced ON online.
2. **Host Advantage OFF**: is "random side + stage not chosen by the host"
   what you want?
3. **Time**: 99 is the binary's only value; OK to add 30/60/infinite as
   port rules?
4. **Backend**: the server work in §7 (presence, block, categories,
   invite states, match results) is in your private repo — will you do it,
   or should the client go ahead with only what exists (EOS lobbies need no
   server)?
5. **Reference images**: `gamereactor.eu/media/42/_4294203b.jpg` and
   `postimg.cc/1f0w4wn3` return **404**; please send them again. The
   Gamersyde image (an Xbox-style Community / Friends panel with tabs) and
   the Skullheart one (an "ONLINE LOBBY" grid of player cards with
   record and connection) both opened.
