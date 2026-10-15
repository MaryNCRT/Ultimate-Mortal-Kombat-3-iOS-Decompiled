# Open items after 0.0.6c: what the binary says, and where to start

> **History (2026-10-15):** most items here were fixed in 0.0.6c-0.1.0 (see
> PROGRESS.md). The open list is the README's *Known problems*; the
> measurements below are kept because they are still true.

Written on 2026-10-09, at the end of the session that shipped 0.0.6c, so that
nothing is left half-understood. Each item records what was **measured** (a
run, a trace, a disassembly), what is **not known yet**, and the **first step**
for whoever picks it up. Every address is armv7.

Tools used throughout: the F2 menu's FINISHER row, `UMK3_DBG_SPECIAL="tick:n"`
(player one does special move n through `DoASpecial`), `UMK3_DBG_FIN`,
`UMK3_DBG_KEY`, and temporary `printf` traces in a built game folder (never
committed).

---

## 1. Objects other than the fighters (Scorpion's spear, attack props)

**Measured.** `UMK3_DBG_SPECIAL="1000:0" --fight scorpion reptile 13` throws
the spear and it works (Reptile is hit and pulled in), but nothing is drawn.
A trace in `RenderLevelPlayers` (GameCode.c) shows the spear as projectile
slot 4, character 18, frames 0x1a7e then 0x1a80. Its frame lookup through the
owner's animation record gives -1, and `HavePreloadedCharacter` returns NULL.
**That is by design.** The spear ids are the ones `RenderLevelPlayers` turns
into `DrawSpear` / `SpearWhichTexture`, and the spear is a pair of face-me
sprites drawn by `RenderExtras` (0x20fa8), not a model. `DrawSpear[0]` is 1
for the 0x1a7e frames.

Sub-Zero's ice projectile, by contrast, finds its frames (239, 240, ...), so
projectiles with models are looked up correctly.

**Not known.** Why `RenderExtras` draws nothing visible while `DrawSpear` is
set: the mirror/direction test, `SpearStartPos`/`SpearEndPos`, `FaceMeMatrix`,
or `limeDrawFaceMeSpriteWH` itself.

**First step.** Re-read `RenderExtras` and `limeDrawFaceMeSpriteWH` against
armv7: the argument list is "transcribed positionally" in the C, and the
names are not established. Print `SpearStartPos`/`SpearEndPos` while
`DrawSpear` is set. Then check which other props Mary means (a list per
character) and whether each is a model projectile or a sprite.

## 2. Typed finishers, and the FATALITY banner and voice

**Measured.**
- `DoASpecial` (moves.c, 0x51830) is where a typed finisher lands. `which` 0xd..0x13 are pit, mercy, fatality 1, fatality 2, animality, babality and friendship.
- Every finisher goes through `mercy_xfer` (0x54ac4), which does nothing unless all of these hold: G+0x45c == 3, G+0x450 == 0, the winner is not airborne, and the loser's thread is in `t_dizzy_sleep`.
- The loser reaches `t_dizzy_sleep` only after its current action ends, about 20 engine passes after FINISH HIM in a debug win. A finisher entered before that is dropped silently.
- Forced once the loser is dizzy, the fatality runs: `t_drone_do_fatality1` -> `t_fatality_align` -> `t_do_fatality_1` -> `ochar_fatalities1[ch]`.

**The banner and the announcer.**
- Blood.c, event 22, sets `FatalityMessage = 1`. That is `create_fx` 0x16, which `create_fx_param` (0x58b64) also turns into two `tsound_func` calls: the voice.
- `death_blow_complete` (0x336e8) consumes G+0x450 and sends code 0x37 or 0x3b (`send_code_a3`). The emitter of fx 0x16 was not found yet.

**Not known.**
- Why typed inputs do not reach `DoASpecial`. Candidates: the input happens before the loser is dizzy (above); the joystick sequence matcher for finishers (joy.c, the `secret_move_search` family in moves.c) never matches; or the distance predicates (`q_fatal_dist`) fail.
- Who emits fx 0x16 after a fatality.

**First step.**
- Trace `DoASpecial` entry with `which` while typing a known fatality (Scorpion fatality 1, from the moves list) with a dizzy opponent.
- Trace `create_fx_param` for `field1c == 0x16`, and `death_blow_complete`, after a forced fatality.

## 3. After the last hit, the loser acts on during FINISH HIM

**Measured.** In a debug win the CPU loser enters `t_finish_him` -> `t_dizzy_dude` about 20 passes after the winner. Until then its AI keeps going: it attacks and flips. In `t_finish_him` (0x7dab4) the loser is chosen by comparing `H[me]` with `H[1-me]`. Mary saw the walking-towards-you case fixed in her own test.

**First step.** Compare with a real KO (not the debug key): the real last hit puts the loser in a hit reaction, which returns through `t_finish_him` sooner. If the delay remains, check what makes the loser's thread call `t_finish_him`.

## 4. Kung Lao's hat on the character select screen

**Not investigated.** The select screen draws the fighters with
`RenderIntroCharacterPlayer` (GameCode.c), not `RenderPlayer`. The 0.0.2 fix
for the hat was in `LIME_LoadSkin` / the fighter path. **First step:** check
whether the select-screen path loads the scene meshes with the fighter's
texture suffix (`LIME_LoadMeshSetTextures(meshset, suffix)`, the `_DIFF`
flag), as the fight path does.

## 5. Survival crashes after a won fight

**Not investigated.** Survival is GameMode 4. **Places to check:**
- `Task_GameDestroy`'s mode-4 branch, which saves the streak and calls `PushFETask(0x26)` (GameCode.c, around the "mode 4 (survival)" comment);
- `FE_Task_Survival_Summary`.

The crash handler writes the address to `logs\`: subtract the image base, add 0x400000 and look it up with `llvm-nm -n`. **First step:** get one log of the crash.

## 6. Sub-Zero's ice clone: misplaced textures

**Not investigated.** The clone is a second copy of Sub-Zero's model drawn with a frozen look. The 0.0.2 notes mention `*_DIFFUSE_ICE.PNG` textures (`CYRAX_DIFFUSE_ICE`, `ERMAC_DIFFUSE_ICE` load in every fight). **First step:**
- find the clone's projectile or object in a `RenderLevelPlayers` trace, as for the spear;
- check which texture `RenderPlayer` binds for it (`w[0x528]`, the tint triple at 0x534..0x53c).

The white frozen fighters are probably the same texture path.

## 7. Noob Saibot

**What the binary has.** He is a complete character, character id 0x17:
- PlayerDefs row: `NOOBSAIBOT_STANDARD.bones/.skin/.skinanim/.scene`, frame list `noobsaibotframes.txt`, scale 0.96;
- intro frames (`NoobSaibotIntroFrames`), idles (`Player_NOOB_Idles`), versus and portrait art;
- his own stage (`NOOBSDORFEN_LEVEL_SCENE`), stage deaths, a babality, and deaths at other fighters' hands;
- "NOOB SAIBOT WINS", and `t_noob_slam`.

See [HIDDEN-CONTENT.md](HIDDEN-CONTENT.md).

**He is always selectable.** `CharacterAvailable[0x17] = 2` is set unconditionally (FrontEnd.c, the availability function), like the normal roster. He is not gated by a kode or a treasure.

**What he lacks, in the binary itself, not in the port.**
- **No special moves.** `DoASpecial` has arms for characters 0x00..0x16 only; 0x17 falls out with no handler.
- **No fatalities.** `ochar_fatalities1/2[0x17]` is `t_non_violent_finish` ([FATALITY-TABLES.md](FATALITY-TABLES.md)).
- **Not counted as "most played".** The most-played scan stops before 23.

So a faithful port plays Noob as the iPhone did: basic moves and throws, no specials or fatalities. Giving him the arcade's moves would be new content, not restoration. If wanted, it belongs behind an option, like the widescreen patch.

**Still to check in game:** that he can be picked and fights without crashing, that `t_noob_slam` works, and that the shadow-clone pair at 0x162ea0 (`delete_slave`, `noob_p`) is used anywhere.
