# Checklist after 0.1.0

Mary's reports from 2026-10-15, after testing alpha 0.1.0. Each item is ticked
only once Mary has checked it in game. What is known so far is under each one.

## Aspect ratio and wide screens

- [x] **Tower select: taps land on the wrong tower** (16:9, probably every
  aspect ratio). `FE_Task_Tower` tests four fixed columns
  (`FE_WidthScale * {74,150,230,308}..{150,230,308,386}`) with no wide offset,
  and the 3D towers do not follow the 2D layout off 3:2. Fixed with
  `Tower_X` (port): the towers spread from the screen centre by the height
  (h / 320 per unit) on every aspect ratio, measured on captures; the columns
  and the torch use it. Identical to the binary on 3:2. Checked by Mary.
- [x] **Red rectangle beside ATRÁS** (wide screens): the press highlight
  (`DrawHighlight`) placed each bar with `FE_X` of its own edge, so near the
  right edge the left bars moved half the extra width and the right bar all
  of it. Every bar now moves by the box's centre, as `DrawButtonNew`
  (port). Checked by Mary.
- [x] **Achievements: the frame ends mid-screen** on wide screens while the
  CUMPLIDO / SIN CUMPLIR markers (x 460) go to the right edge. The frame now
  spans the whole width (port). Checked by Mary.
- [x] **Settings and button config: the box texts are a few units left of
  their boxes** (REINICIAR TODOS LOS DATOS, VOL. MÚSICA, VOL. EFECTOS...). The
  binary draws them at x 152/153 and 320/322 while the boxes are centred at
  156 and 325; they now use the box centres (port). Checked by Mary.
- [x] **Character select: ATRÁS stays inside** on wide screens (the select
  screens keep everything centred for the grid). BACK and its label now go
  to the right edge (`PortFERightEdge`, port). Checked by Mary.
- [x] **The main menu's submenu texts are not centred in their panel** on
  wide screens (OPTIONS, PLAY, ABOUT, EXTRAS): the panel reaches the right
  edge, so its visible part is centred a quarter of the extra width further
  right. `DrawOptionAsButton` adds `PortOptionShift` (port) on those four
  screens. Checked by Mary.
- [x] **COLOR box on the select screen**: fixed beside the screen edge
  (player one left, player two right) on every aspect ratio, inside the
  screen; it no longer follows the 2D layout off 3:2 (port). Checked by Mary.
- [x] **Usage sharing (COMPARTIR INFO): the text in the box is off-centre**
  on wide screens: it is placed at `240 * FE_WidthScale` without the centring
  offset. Fixed with `FE_XC` there and at five more sites with the same
  pattern: the name entry screen (title and name), the multiplayer select
  title (two places) and ENTER KODE's title. Checked by Mary.
- [ ] **The iPad build's layout, on 4:3 always and with the launcher's
  interface size IPAD** (Mary: the smaller iPad interface goes with that
  option, 4:3 always has the iPad's menus; on 16:9 the widescreen fixes
  stay). `PortIPad` is set by ui_size=ipad or 4:3; `PortIPadTall` by 4:3,
  for the changes that need the iPad's extra height or narrower view. Found by diffing every function and data symbol of the two
  binaries (`WORK/claude-tests/sidediff2.py`, pool islands skipped and
  registers normalised). Done, from the iPad disassembly:
  - pause and moves-list corners 40 wide, not 80 (`TogglePauseMenu`,
    `UpdateInGamePauseMenu`);
  - PLAY: five options 56 apart from y 40, the fifth "2 PLAYER" / DUELO
    (GameMode 6, two players on one device); in DUELO player two has the
    second key set (arrows, numpad), player one the first and the pad;
  - the multiplayer select title at y 26;
  - Continue and the 1P screen: texts 36 points lower, BACK at y 322, boxes
    `(int)(102 * limeScreenHeight / 640)` = 122 tall (4:3; checked by Mary);
  - fighters pulled together beyond 0x100, not 0x130 (`repell_func`, 4:3);
  - the menu vortex: two rings, VORTEX1 and VORTEX2;
  - the stage's glass window: 12 slices 55 apart, not 32 at 40;
  - data: the select screen camera x 2.3 (fighters inside the screen,
    checked by Mary) and the tower's pulled-back camera y -2.739.
  After 0.1.0b (Mary, 16:9 with the iPad interface): the select camera and
  the tower camera moved the fighters onto the grid and the tower labels
  off their towers, so both are the iPad's on 4:3 only; DUELO shows with
  every interface size (released in 0.1.1).
  Identical in both and nothing to do: `drawCharacterSelection`,
  `RenderFECharacters`, `FE_Task_Play`'s code apart from the rows, and every
  other menu screen. Still to read: `DrawTower3D` and `FE_Task_About_Help`.
- [ ] **The red press highlight looks too big** (Mary's capture of the
  1P screen): `DrawRedHighlight` bars are 11 units thick by the binary.
- [ ] **OPTIONS on the main menu (16:9):** when the submenu opens, the main
  buttons slide out of place and the submenu texts (SETTINGS, BUTTON CONFIG,
  SOCIAL) are not centred in their panel. The slabs are fixed. The texts
  went through `FE_X`, which outside task 0 moves x 90-390 by half the extra
  width; `DrawMainMenu` now places them with `MainMenu_X` (port), on the left
  edge with their slabs on every screen. Checked by Mary. Still open: the
  submenu texts centred in their panel.
- [ ] **Back buttons and the like stick to the right edge** on wide screens
  (ATRÁS, SALIR...), using the whole screen. Left-side buttons (SIG.) to the
  left edge.
- [ ] **The on-screen buttons stay in the 3:2 spot on wide screens** (original
  UI size); the stick moves to the edge but the buttons do not. They come from
  the fixed `ButtonsPos4/5/6` tables (x 320-448 of 480).
- [ ] **The on-screen buttons are broken with the iPad UI size**: drawn in the
  middle of the stage. Same cause: `ButtonsPos` is in 480x320 units and is
  drawn unscaled on the 960x640 surface. The iPad build draws them with the
  same code (`DrawControls` identical), so the fix is the port's: scale and
  anchor them like the stick.
- [ ] **Loading screen: the tip and the Kombat Kode row are not centred**
  on wide screens (shifted left by half the extra width): `Task_LoadingScreen_DRAWSCREEN`
  and `drawKodeTip` place them with `FE_WidthScale` themselves.
- [ ] **The debug menu (F2) is stretched** on wide screens. It draws with a
  fixed 480x320 ortho (`runtime/debug_menu.c`); it must keep its shape.

## In-game settings

- [ ] **ESC opens a settings menu** in game, apart from the debug menu, with
  every launcher option (aspect ratio, HUD size, graphics, controls...) by
  category, applied live.

## Controls

- [x] **The pad's START button does nothing** (on 16:9 / 21:9; the P key
  too). START and P tap the pause corner at x 470, but `TogglePauseMenu`
  tests `x > limeScreenWidth - 80`, and a wide surface is wider than 480.
  The tap now goes 10 units from the real right edge, and RESUME is found
  with `FE_X` (`runtime/game_main.c`). Checked by Mary.
- [x] **Bind the pad freely in the launcher** (every action to any button),
  as the keyboard can be. CONTROLS > GAMEPAD now has two buttons per action
  (either presses it), for the 6- and 5-button layouts, pause and moves
  included, and RESTABLECER. The launcher writes `pad_<action>=` /
  `pad5_<p|b|k|r>=` (PB_* names joined by "+"); `runtime/game_main.c` reads
  them over its by-position defaults. Checked by Mary.

- [ ] **Player two's controls for DUELO** (0.1.1): CONTROLS > PLAYER 2 in
  the launcher, keys and pad like player one's; the two never share a pad.
  Two pads at once not tested yet.
- [ ] **JUGADORES frame centred** on wide screens (0.1.1, `FE_XC`).

## Fight

- [ ] **Models mixed after a round ends** (Ermac against Sub-Zero, round 2,
  Mary's capture): on player two's side Sub-Zero is drawn together with a
  second, red Ermac-coloured body, and a loose head lies on the floor behind
  them. Cause to be found.

## Sound

- [ ] **Reptile's specials have sound errors.**
- [ ] **Reptile's fatalities: the audio does not sound as it should** (sound
  missing).
- [ ] **Check every other fighter's fatalities and specials for missing
  sounds.**

## Debug tools

- [ ] **Strange hitboxes: attacks connect where they should not.** Check the
  collision code against the binary (also how TOOLS > HITBOXES draws them).
  Read against the iPhone disassembly and identical: `strike_check_regs`
  (0x59280), `strike_check_ptr`, `strike_check_box`, `intersect` (signed,
  `>=`), the four `*_mpart_ob`, `mk3_getbbox` and `FrameID_GetBBox`; the
  `FrameInfo` and `FrameInfo2` tables match the binary byte for byte. A
  scripted Scorpion-Reptile fight with the overlay shows body boxes that
  follow the frames. Next: a concrete move from Mary that hits from too far,
  then `get_char_stk` and the strike tables it reads.
- [ ] **Debug menu with tabs at the top, one per section** (FIGHT, SCREENS,
  TOOLS...), tidier and easier to use; not stretched (see above).

