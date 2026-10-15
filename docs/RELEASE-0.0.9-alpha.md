## Ultimate Mortal Kombat 3 — PC port 0.0.9 alpha (Windows)

**Gamepad support**, a palette picker on the select screen, no input lag, and a set of fixes and debug tools. As before, **no game data and no game executable are included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

### Gamepads (port only)
- **Xbox** pads through XInput; **PlayStation 4 and 5**, **Switch Pro** and other USB pads through WinMM. Windows' own search runs on a thread of its own, so a slow device never holds the game up, and a pad plugged in later is picked up.
- Launcher **CONTROLS** has three tabs: **KEYBOARD**, **GAMEPAD** and **DEBUG**. *GAMEPAD to use* lists the pads Windows has attached, by name. You can pick:
  - keyboard only;
  - the first pad connected;
  - the Xbox pad;
  - one particular pad.
  
  The keyboard keeps working alongside the pad.
- The buttons go by position:
  - left = high punch, bottom = low punch;
  - top = high kick, right = low kick;
  - with 5 buttons, both left buttons punch and both right buttons kick;
  - R1 blocks, L1 and L2 run, R2 is the special button;
  - OPTIONS/START pauses and SHARE/BACK opens the moves list.
- **Debug menu from the pad:** hold **L3 + R3** to open it; the combination can be changed in *DEBUG*. Inside the menu:
  - d-pad to move;
  - cross/A to pick;
  - L1/R1 to turn pages;
  - circle/B to close.
- The game's menus are still used with the mouse.

### New
- **Select screen colour picker:** a "< COLOR 1 >" box under each fighter switches palette 1 or 2. Player two's box appears in 2-player versus. The 3D model, the VS portrait and the fight follow it, and the choice is saved. An Arcade ladder continued later keeps the palette it began with.
- **No input lag:**
  - a press changes the move on the same tick;
  - held keys stay held;
  - hold-and-release moves fire (Mileena's sai, Liu Kang's bicycle kick).
- **Crash report:** a crash writes `logs\crash-*.log` and shows the reason and the function in a message box.
- **Debug menu, TOOLS page:** hitboxes, fight speed (10%–400%), and save states that restore the exact moment without restarting (4 slots, in memory).
- A fight started from the debug menu no longer leaves its fighters, stage, mode or palettes for the other modes.
- The launcher's buttons are smaller.

### Fixed
- **Loading screen Kombat Kode:** the six icons are one row again, with the kode's name under them (`drawKodeTip`, armv7 0xa0530: the 0x74 offset is on X, not Y).
- The dizzy loser sliding toward the winner during FINISH HIM is no longer seen.

Everything in [0.0.8](RELEASE-0.0.8-alpha.md) and earlier is included.

### Known problems (still open)
- Some objects other than the fighters may still not be drawn; some fatalities miss animations (Sonya's).
- Texture errors remain on some stages and fighters; modes other than Arcade can crash.
- Pausing sometimes shows the fight shrunk into a corner.
- Some special attacks have no sound, projectiles especially.
- The game's menus are not driven by the pad yet.
- The launcher is Windows only (Linux/macOS: build from source with CMake). Only the iPhone 1.2.59 `.ipa` works.

If the game crashes, send the `logs\` file from beside `umk3-game.exe` in a GitHub issue.
