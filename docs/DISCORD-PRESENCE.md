# Discord Rich Presence

While the game runs, the player's Discord profile shows what they are doing:
"Playing UMK3 iOS Decomp", a first line (the mode) and a second (the
fighters), the time since the game started, and the large image. The large
image is the same HD icon that the launcher's STEAM section puts in Steam
(SteamGridDB icon 76847, 512×512). In an online lobby the profile also
carries a party (members / maximum) and a **Join** button. A friend who
presses it, or who accepts an invite sent from Discord, enters that EOS
lobby in their own game.

Status (2026-11-12): built into the test folder.
`tests/discord_presence_test.c` passes, and its `--live` mode connected to
the project machine's Discord and showed an activity. **Mary saw it on her
profile** while playing the test-folder build (2026-11-12, 19:20); that
session's log has `discord: connected`. A Join between two accounts has
not been tried yet, and neither has Linux.

## How (and why not the Social SDK)

- The Discord Social SDK is downloaded from the developer portal only after
  the game is registered there through a form. Mary did not want to
  register a game that is not hers, nor make players link a Discord account.
  So the game talks to the **Discord desktop client already running on
  the PC**, over its local IPC: a named pipe `\\.\pipe\discord-ipc-N` on
  Windows, a Unix socket `discord-ipc-N` on Linux. This is the
  unauthenticated path Discord documents for Rich Presence. It needs only
  the application id, with no sign-in, no linking and no library to ship.
  The deprecated `discord-rpc` library is not used: the protocol is written
  in `src/runtime/discord_presence.c`, about 400 lines.
- Protocol: frames of `int32 opcode, int32 length, JSON`. Opcode 0 is the
  handshake `{"v":1,"client_id":...}`. Opcode 1 carries commands
  (`SET_ACTIVITY`, `SUBSCRIBE ACTIVITY_JOIN`) and their answers; the first
  answer is the `READY` dispatch. Opcode 2 closes; 3 and 4 are ping and
  pong. Checked against the project machine's Discord on 2026-11-12:
  `READY`, then an accepted `SET_ACTIVITY` with an https large image (Discord
  proxies it as `mp:external/...`), a party and a join secret, and an
  accepted `ACTIVITY_JOIN` subscription.
- It never blocks a frame. Not connected: one try every 8 s. Connected: the
  activity is sent when it changes, at most once every 4 s, since Discord
  allows 5 updates per 20 s. If Discord is missing or closes, nothing
  happens.

## The application

`DISCORD_APP_ID` in `src/runtime/discord_presence.h`:
`1570587748946018304`, Mary's application, named "UMK3 iOS Decomp" in the
developer portal. Discord shows that name in "Playing ...". To change it,
rename the application in the portal; the code does not change.

## States (`src/runtime/presence_game.c`)

| The game | First line | Second line |
|---|---|---|
| `CurrentTask 3`, `FE_CurrentTask 0` | Main menu | |
| `FE_CurrentTask 0x1b` | Arcade / Local versus / Training | Choosing a fighter |
| `FE_CurrentTask 51` | Online menu | |
| `FE_CurrentTask 0x2a` / `0x2b` | Online | Choosing a fighter / A vs B |
| in an EOS lobby (`mplobby_state() == MPLOBBY_IN`) | In an online lobby | the lobby's name, party n/max, Join |
| `CurrentTask 6`, `GameMode` 0 / 3 / 4 / 5 | Arcade / Karnage / Survival / Shao Kahn's Lost Treasures | A vs B |
| `GameMode 6` (`PortPractice`) | Local versus (Training) | A vs B (the fighter) |
| online (`isMPConnected()` or `GameMode 1`) | Online match | own fighter vs the other |

The texts follow the game's language (`Language` ES / EN). The fighters come
from `CharacterNames`, in title case.

## Joining from Discord

The lobby's join secret is `umk3lobby:<EOS lobby id>`. When Discord sends
`ACTIVITY_JOIN`, the id waits in `presence_game.c`. From the main menu the
game opens the ONLINE screen on LOBBIES (`FrontEnd.c
PortOnlineOpenLobbies`). Once EOS is ready and the game is in no lobby, the
LOBBIES tick takes the id (`presence_take_lobby`) and joins it, as the
list's JOIN does (`mplobby_join`). A full lobby offers no Join. Joining
needs the friend's game to be already open: registering the game with
Discord so that it can be launched from Discord is not done.

## Turning it off

The launcher's PLAY page: **SHOW ON DISCORD WHAT YOU PLAY**. It sets
`umk3.ini discord=0/1` (`launcher/app/lib/ini.js`), read by
`game_main.c read_config`. `UMK3_DISCORD=0` also turns it off.

## Testing

```bash
zig cc -Isrc/runtime tests/discord_presence_test.c src/runtime/discord_presence.c -o dp_test
./dp_test           # the JSON and the join parsing
./dp_test --live    # 20 s of a test activity on the Discord running here
```
