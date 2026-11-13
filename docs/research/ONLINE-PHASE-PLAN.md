# ONLINE: lobbies, Quick Match, party -- the plan

Date: 2026-11-12. It follows [ONLINE-MENU-AUDIT.md](ONLINE-MENU-AUDIT.md), whose
stages 1-2 (audit, HOME, PROFILE) are done.

## Decisions (Mary)

| Question | Answer |
|---|---|
| Combo Slide | the port's KOMBO SLIDE (issue #114), a lobby rule (2026-11-06) |
| Lobbies, Quick Match, party | EOS Lobbies, no server work (2026-11-06) |
| The special modes' rules | the binary's VS kodes, in the lobby's rules (2026-11-06) |
| Host Advantage OFF | the side (who is player 1) and the stage are drawn (2026-11-12) |
| Time | 30 / 60 / 99 / infinite, the port's rule; 99 the default (2026-11-12) |
| Server (presence, invites to a lobby) | not now: EOS only; friends' presence waits (2026-11-12) |
| Best of N | both: ROUNDS a fight (1/3/5) and a SERIES of separate fights (1, best of 3, best of 5) (2026-11-12) |
| Local versus | the same rules, in a window SELECT opens on its select screen (2026-11-12) |
| Rounds past two wins | a coin each, side by side (2026-11-12) |
| Kodes | several at once, ticked in a checklist the rules window opens (2026-11-12) |

## Stages

1. **Rules block** -- done on `claude/online-lobbies`, see below.
2. **EOS lobby core** -- done, see below. (`mp_eos.c`): create with attributes (version, rules
   text, name, public/private), search with filters, join by id, members
   and member attributes (ready, name, look), leave, kick, promote,
   updates; the lobby owner hosts the `mp_net` session over P2P.
3. **LOBBIES in the ONLINE screen** -- done, see below: browser with filters, CREATE LOBBY
   (the rules window's rows, plus HOST ADVANTAGE), the room (members,
   READY, ping, START for the host).
4. **Rules checked in HELLO** -- done, see below (`rollback.c`): both blocks equal or the
   fight does not start, with a message; Host Advantage OFF draws the side
   and the stage from both hellos.
5. **QUICK MATCH**: search a quick-match bucket by version, join the first
   open lobby, else create one and wait.
6. **PARTY**: a private lobby of friends (invites by the friends' codes
   through EOS), the leader picks who fights.
7. **Tests**: two machines, every rule, the desync hash at 0.

## The rules block (stage 1)

`src/runtime/online_rules.c` (port): `online_rules` -- version, best_of
(rounds), series (fights), time, kodes (a bit each), kombo_slide, host_adv --
as text `r2;b3;m1;t99;k0;s1;h1` (version 1's `k-1`, one kode, still read) for lobby attributes and the hello. Two blocks: the
online one (`rules_set_active`, the lobby's) and the local versus's
(`rules_set_local`, umk3.ini `vs_rules=`); `rules_use_mode(GameMode)` at
Task_GameInit picks one (1 online, 6 local, none elsewhere).

Applied in the engine, both machines before frame 0, each line marked:

| Rule | Where | How |
|---|---|---|
| ROUNDS | GameCode.c Task_GameInit | `WinsNeeded = (best_of + 1) / 2` after the binary's |
| ROUND 5+ | GameCode.c, the round banner | the binary has "ROUND 1".."ROUND 4" only |
| Coins past two | GameCode.c DrawRoundWinCoins | 32 apart, up to five |
| TIME 30 / 60 | logic/other.c t_clock3 | a clock above the rule is a fresh 99 (round start, mercy): moved down by the difference; from the clock's own digits, so a rollback resimulates it the same |
| TIME infinite | logic/other.c t_clock4 | the frozen path training and the debug menu use |
| KODES | GameCode.c Task_GameInit, ResetFightData, DrawHUD | the lowest in `*theKode`; the others by `rules_kode_on` where the binary reads it: preprocessPostloadKode's three flags, blackout, the plates, both players' handicaps at once |
| KOMBO SLIDE | game_main.c | `PortNoKomboSlide` from the rule |
| SERIES | GameCode.c QuitAsWin / QuitAsLose; the HUD under the clock | counted once a fight (before its fade); SERIES a - b, then who has it |

The side and stage draw (Host Advantage OFF) came with stage 4, below. The stage kodes (3-12) act before the load and are left out;
STAGE SELECT picks the stage. The one-round kode (0x13) is handled by the
binary online only.

The rules window's first build drew without restoring the GL matrices
(`begin_2d` without `end_2d`): the select screen's frame rate fell (Mary saw
it), fixed. Its hint sat on player one's COLOR box; it is above it now.

Tests: `tests/online_rules_test.c` (ctest `online_rules`); a scripted fight
(`UMK3_TWOUP=1 UMK3_RULES=... --fight`) showed the clock from 30 and a fight
going on past two round wins.

## The lobby core (stage 2)

`mp_eos.c` / `mp_eos.h` (port), the `mplobby_*` calls: create, search,
join by id, leave, kick, promote, the lobby's attributes (`ver name host
rules priv key go`) and each member's (`code name look ready`), sent one
UpdateLobby at a time from `mpeos_tick`; the lobby notifications re-read
it. A private lobby is advertised but `priv=1`, found only by its `key`.
The owner calls a member (`go`), whose `mpnet_poll` connects to the owner
(`mpnet_find_eos`), and the owner hosts the mp_net session.

Tested with `tests/eos_lobby_probe.c`, two of them on one PC (the second
`UMK3_EOS_NEW_DEVICE=1`, its own `UMK3_SAVE_DIR`), public and private:
both `PROBE OK`. Build it with zig from the repository root:

    zig cc -target x86_64-windows-gnu -DUMK3_EOS -I <eos>/SDK/Include -I src/runtime
           tests/eos_lobby_probe.c src/runtime/mp_eos.c -o probe.exe

and run it beside the EOS DLL and eos.ini (`probe host [priv]`, then
`probe join [KEY]`). Not tried yet: kick, promote, host migration, the
private lobby kept out of a public search, the rules filter.

## LOBBIES in the ONLINE screen (stage 3)

`FrontEnd.c` (port), `PortOnlineLobbies` and around it, over `mplobby_*`:

- **Not in a lobby**: the open lobbies (`mplobby_search` every 5 s and
  REFRESH; ALL or MY RULES, the draft's rules text), rows with name, host,
  rules and n/m, a click joins. One box takes a lobby's name (filters the
  list as typed) or its key (JOIN BY KEY, `mplobby_search(NULL, key)`).
  Invites received sit at the top of the list (JOIN, X). CREATE LOBBY:
  name, PUBLIC / PRIVATE, 2..8 players, the rules through the rules window
  (`debug_menu.c lobby_rules_open`, the VS RULES window with HOST
  ADVANTAGE, ACCEPT hands the block back).
- **In a lobby**: the members' banners (`OnCard`, each look from the
  member's `look` attribute, `port_online_friend_look` reads it), HOST /
  READY; the key and COPY (every lobby has one; private ones are found by
  it alone); READY, LEAVE. The host: EDIT RULES (`mplobby_set_rules`),
  MAKE PUBLIC / PRIVATE (`mplobby_set_priv`), and for the member chosen
  FIGHT (`mplobby_call`, once ready), KICK, MAKE HOST.
- **Invites**: `mplobby_invite(code)` finds the player as FIND MATCH does
  (`mpeos_lookup`, their ONLINE screen open), then sends a packet of kind
  `MPLOBBY_INV_KIND` over P2P (mp_net's channel) with the lobby's id, the
  host's name and the lobby's; `mp_net.c` hands it to `mplobby_invite_got`.
  `EOS_Lobby_SendInvite` answered `EOS_AccessDenied` under this product's
  client policy, so it is not used (the received-invite notification is
  still registered).
- While in a lobby its rules are the online block (`rules_set_active`, set
  when they change); leaving clears it. BACK out of the screen leaves the
  lobby. The rules window keeps EOS ticking (`game_main.c`).

The key is the host's to choose (Mary): 4..12 letters and digits, in CREATE
LOBBY (empty: six made up) or changed in the room (`mplobby_set_key`). Two
lobbies may end up with one key; a key search joins the first found.

Test aid: `UMK3_LOBBY_TEST=host | join | invite:<code> | accept | key:<KEY>`
(the host's `UMK3_LOBBY_KEY` and `UMK3_LOBBY_PRIV=1`) drives the
section by itself (two games on one PC: the second with
`UMK3_EOS_NEW_DEVICE=1`, its own `UMK3_SAVE_DIR` and `UMK3_PROFILES`; start
the host first, so the two are different EOS users). Seen: create, join
from the list, ready, the call, the select screen and the online fight;
invite by code, accepted, the call. Frames from `UMK3_SHOTS` (the game's
own, no window capture). Not tried: kick, promote, the rules changed in a
room. No ping yet.

## Rules checked in HELLO (stage 4)

`rollback.c` `rb_online_gate`, called by Task_GameInit (GameCode.c) at
state 0 of an online fight, before STAGE SELECT and before anything loads:

- **The hello** (RB_VERSION 2, reliable): the netcode's version, the
  buttons, rollback or classic, a random number for the draw, and the
  rules block as text (`rules_online()`, the lobby's; empty for a fight not
  from a lobby). Both sides send theirs on entering the loading and wait
  for the other's (a minute). The fighters and the level went to a packet
  of their own, `RB_PKT_SETUP`, sent when the fight begins
  (`rb_online_begin`); the guest takes the host's level from it, as it took
  it from the old hello, and `rb_ready` waits for both.
- **Refused**: other rules text, another version (an old game's 24-byte
  hello counts), no hello in a minute, or the link gone. Both sides see the
  same two hellos, so both refuse; each waits 45 frames (for its hello to
  arrive over there), ends the session (`endMP`) and loads the front end
  again on the ONLINE screen (FrontEnd.c `PortOnlineRefused`: the stack
  unwound to it, LOBBIES when in a lobby, READY off, the reason in the
  notice for 8 s, ES and EN). `disconnected()` leaves a lost link during
  the gate to it (`rb_gate_busy`): its own way out (task 7) would free a
  fight never loaded.
- **HOST ADVANTAGE OFF** (rollback on both sides): a hash of the host's
  number and the guest's, in that order, so both machines get the same:
  its top bit is the side (1: the guest is player one), its low bits one of
  the binary's fourteen online stages (`mpLevelList`, getRandomLevel's
  list). Character1 / Character2 are swapped before the load, the rollback's
  `g_me` and the seed follow, and where the game asked `isParent()` for
  "this machine is player one" in a fight (the moves list, the flawless,
  the stats, the finisher stat, the play counter, the palettes, the wins)
  it asks `rb_local_p1()`. The host's STAGE SELECT does not open
  (debug_menu.c `eligible`). A series of fights counts by player, not side
  (`rules_series_swap`). The VS screen before the fight still shows the
  host on the left: the draw comes after it. With the classic netcode the
  host keeps its side and stage.
- **HOST ADVANTAGE ON**: as before -- the host is player one and its STAGE
  SELECT picks.

Test aids: `UMK3_HELLO_RULES=<text>` sends that rules text in this side's
hello instead of the lobby's (to make a mismatch); `UMK3_LOBBY_RULES=<text>`
gives `UMK3_LOBBY_TEST=host`'s lobby those rules.

Tested with two copies of the game on one PC (the second
`UMK3_EOS_NEW_DEVICE=1`, its own `UMK3_SAVE_DIR` and `UMK3_PROFILES`; the
select screen's OK tapped by `UMK3_TAPS`), 2026-11-12:

- equal rules, HOST ADVANTAGE ON: the fight started, the host player 1, the
  guest on the host's STAGE SELECT level, `desyncs 0`;
- different rules (`UMK3_HELLO_RULES` on the guest): both logged `NO FIGHT
  -- the rules differ` with both texts, both went back to LOBBIES in the
  lobby with LAS REGLAS NO COINCIDEN: NO HAY PELEA (screenshots);
- HOST ADVANTAGE OFF, four fights: both sides drew the same side and level
  every time (host P1 on 2; guest P1 on 1; guest P1 on 11; host P1 on 0),
  the swapped fights with Character1 / Character2 swapped on both,
  `desyncs 0` on all;
- the other game closed mid-fight: the one left went back to the ONLINE
  screen with CONEXION PERDIDA.

Not seen: an online fight ending by a win and coming back to the lobby
(scripted keys did not land hits; the code is the summary's same way out),
a series with drawn sides between real fights, the classic netcode, an old
game's hello, the gate's minute running out, kick / promote.

**Also in this stage (Mary, watching the test):**

- **One player's touch controls online.** `rb_apply_inputs` sets
  `P2Controls` so the engine reads both players, and DrawControls drew the
  two-players-on-one-device controls (two sticks and rows, this player's in
  the left half -- and hit-tested there). GameCode.c `DrawControls` now lays
  out and draws one player's in GameMode 1; the engine keeps P2Controls.
  Seen: one stick and one row in an online fight. It was so before stage 4.
- **Back to the lobby, not the binary's screens.** An online fight from the
  ONLINE screen ends back in it (FrontEnd.c `PortOnlineBackToLobby`, from
  the summary, FE task 0x28) instead of the summary whose QUIT led to the
  main menu; a lost link goes back to it with the notice CONEXION PERDIDA
  instead of the "connection lost" screen whose button led to the binary's
  searching lobby (FE task 3). The session ends; the host calls the next
  fight from the room.
- The fight's player names (gold plates) are PR #209's, not merged yet.
