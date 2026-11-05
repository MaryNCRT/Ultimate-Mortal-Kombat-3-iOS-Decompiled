# Sub-Zero's Ice Ball, from the button press to the frozen opponent

How the iOS binary programs Sub-Zero's freeze projectile, as read from the
decompiled sources in `src/decomp/gamecode/logic/`. Each step names the
function, its armv7 address in `OUTPUT/armv7/UMK3.armv7`, and the file it
is in. What a field *means* is stated only where the code makes it plain;
the rest is given as the code has it.

Asked by Mary, 2026-11-05.

## 1. The move: a projectile kind

The move tables send the recognised input to a one-line routine that only
picks a **projectile kind** and hands over to the shared launcher:

| Routine | armv7 | File | Kind (`field1c`) |
|---|---|---|---|
| `t_do_sz_forward_zap` | 0x00051094 | moves.c | 12 (`0xc`) -- Sub-Zero |
| `t_do_osz_zap` | 0x00051724 | moves.c | 42 (`0x2a`) -- classic Sub-Zero |

Both push `t_do_zap`.

## 2. `t_do_zap`: the table of launchers

`t_do_zap` (mkzap.c) looks the kind up in **`projectile_jumps`**
(0x00172694, 45 entries, one per projectile in the game) and runs the entry
as the fighter's next thread step. The index is not range-checked.

- 12 is `tl_do_sz_zap` (0x0007a79c)
- 42 is `tl_do_osz_zap` (0x0007af28), the same routine without the write to
  the ball's animation cursor

## 3. `tl_do_sz_zap`: the throw

- First step: `zap_init_special_act`, the throw's sound (`ochar_sound`), and
  the throwing animation (`field40 = 0x00030024`, through `t_animate_a9`).
- At token `0x856`, the moment of release: the ball is placed at the
  fighter's part (`proc->slave`) plus (0x30, 0x38) by `adjust_xy_a5`. The
  slave's animation cursor is set directly to `sz_ani_data + 0x1220`.
- Then it installs `t_osz_forward_entry`, which both Sub-Zeros share.

## 4. `t_osz_forward_entry`: point-blank first

`t_osz_forward_entry` (0x00075ae8) runs **`t_ice_collision_check`**
(0x0007bea8) up to four times, each with a different strike box packed in
`field20` / `field24`.

That check is the point-blank case. It animates one frame, then tests the
box (`local_strike_check_box`). On a hit the opponent is frozen at once:
- a sound (`field1c = 0x40003`, `hob_ochar_sound`);
- `t_sz_zap_hit` started through `create_proj_proc`;
- `t_sz_post_zap` installed.

No ball flies.

If nothing was hit, token `0x874` is the real launch:
- the slave's cursor becomes `sz_ani_data + 0x1234`;
- `field38 = t_sz_zap_proc`;
- `create_proj_proc` spawns the projectile;
- the thrower goes to `t_sz_post_zap`.

`t_sz_post_zap` is the recovery pose, animation 0x24. Character 0x15 is a
hard-coded exception there (issue #29).

## 5. `t_sz_zap_proc`: the ball in flight

`t_sz_zap_proc` (0x00075e08):
1. `init_anirate`.
2. `field1c = 0xa0000`, then `set_proj_vel`. That is the speed; read as
   16.16 fixed point it would be 10.0 a tick.
3. `field48 = 0x13`, the kind of hit it deals.
4. It flies in `tl_projectile_flight`.
5. On contact it goes to `t_sz_zap_hit`.

## 6. `t_sz_zap_hit`: the impact

`t_sz_zap_hit` (0x0007c000):
- plays the impact sound and stops the opponent (`stop_a8(GrObj)`);
- lines the ice up on them (`match_me_with_him`, `flip_multi`,
  `multi_adjust_xy` with -0x90);
- poses the burst (animation 0x24 through `borrow_char_ani`);
- runs it with `t_mframew`;
- deletes itself (`tl_delete_proj_and_die`).

## 7. The opponent: `t_r_freeze`

The reaction `t_r_freeze` (mkreact.c) runs in this order:

1. **Frozen palette.** `player_froze_pal`, with the opponent's flags in
   `field30` cleared of bit 8.
2. **Two shivers.** `t_shake_ob_up` twice, (3, 2) then (3, 8).
3. **The wait.** 0x60 ticks, 96: a little over 1.5 s at 60 ticks a second.
   This is the time to punish.
4. **Normal palette.** `player_normpal`.
5. **Airborne or not.**
   - If the opponent was in the air (`am_i_airborn`), it falls: `t_flight`
     with 1.0 / 0.5, then `t_land_on_my_back`.
   - Otherwise it goes back to its normal reactions
     (`t_local_reaction_exit`).

## Where else ice is

- `c_sky_ice`, `t_sky_ice_proc`, and projectile kinds 5 to 7 are the ice
  dropped from above: `tl_do_sky_ice_on`, `_behind` and `_front`.
- `tl_do_floor_ice`, `t_floor_ice_proc` and `t_r_floor_ice` are the floor
  ice that makes the opponent slip.
- `t_r_decoy_freeze` is the ice clone's freeze.
- Kind 43 is `tl_do_floor_ice`.
