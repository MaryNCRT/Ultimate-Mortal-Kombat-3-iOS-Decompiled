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

## Stages

1. **Rules block** -- done on `claude/online-lobbies`, see below.
2. **EOS lobby core** (`mp_eos.c`): create with attributes (version, rules
   text, name, public/private), search with filters, join by id, members
   and member attributes (ready, name, look), leave, kick, promote,
   updates; the lobby owner hosts the `mp_net` session over P2P.
3. **LOBBIES in the ONLINE screen**: browser with filters, CREATE LOBBY
   (the rules window's rows, plus HOST ADVANTAGE), the room (members,
   READY, ping, START for the host).
4. **Rules checked in HELLO** (`rollback.c`): both blocks equal or the
   fight does not start, with a message; Host Advantage OFF draws the side
   and the stage from both hellos.
5. **QUICK MATCH**: search a quick-match bucket by version, join the first
   open lobby, else create one and wait.
6. **PARTY**: a private lobby of friends (invites by the friends' codes
   through EOS), the leader picks who fights.
7. **Tests**: two machines, every rule, the desync hash at 0.

## The rules block (stage 1)

`src/runtime/online_rules.c` (port): `online_rules` -- version, best_of
(rounds), series (fights), time, kode, kombo_slide, host_adv -- as text
`r1;b3;m1;t99;k-1;s1;h1` for lobby attributes and the hello. Two blocks: the
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
| KODE | GameCode.c Task_GameInit, before `preprocessPostloadKode` | `*theKode` |
| KOMBO SLIDE | game_main.c | `PortNoKomboSlide` from the rule |
| SERIES | GameCode.c QuitAsWin / QuitAsLose; the HUD under the clock | counted once a fight (before its fade); SERIES a - b, then who has it |

Not yet: the side and stage draw (Host Advantage OFF) -- the binary ties the
host to player 1 in the select screen, the inputs and the stats; it comes
with stage 4. The stage kodes (3-12) act before the load and are left out;
STAGE SELECT picks the stage. The one-round kode (0x13) is handled by the
binary online only.

Tests: `tests/online_rules_test.c` (ctest `online_rules`); a scripted fight
(`UMK3_TWOUP=1 UMK3_RULES=... --fight`) showed the clock from 30 and a fight
going on past two round wins.
