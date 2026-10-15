## Ultimate Mortal Kombat 3 — PC port 0.1.0b alpha (Windows)

Fixes on top of 0.1.0: **the iPad build's menus on 4:3**, a local two-player mode, gamepad binding in the launcher, a tidier debug menu, and the screens that were off-centre on wide screens. As before, **no game data and no game executable are included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

**Coming from 0.1.0:** the launcher offers this update itself (COMPILE YOUR .IPA > DOWNLOAD AND INSTALL).

### The iPad layout (4:3, or INTERFACE SIZE > IPAD)
Read from the iPad build's own code (UMK3 for iPad 1.2.56):
- **On 4:3:**
  - the menus are always the iPad's;
  - Continue and the 1P screen have their texts 36 points lower, BACK lower and taller boxes;
  - the fighters are pulled together sooner.
- **On 4:3, and with INTERFACE SIZE > IPAD on any aspect ratio:**
  - the smaller pause and moves-list corners;
  - the select screen's camera keeps the fighters inside the screen, and the tower has its own camera;
  - the menu vortex has two rings;
  - the stage's glass window has the iPad's slices;
  - the PLAY menu gets a fifth option, DUELO.
- **PLAY > DUELO:** two players on one PC. Player one uses the first key set and the gamepad, and player two uses the second key set (arrows, numpad).

### Wide screens (16:9, 21:9)
- **Tower select:** taps land on the tower you touch, and the towers spread from the centre.
- Fixed elements that were not centred:
  - texts in the settings and button-config boxes;
  - the main menu's submenus;
  - usage sharing, name entry, the multiplayer and ENTER KODE titles;
  - the achievement pop-up.
- The achievements frame spans the screen, ATRÁS on the select screen goes to the right edge, and the COLOR box sits by each player's edge.
- The red highlight beside ATRÁS stays on its button.

### Controls
- **The pad's START button (and P) pauses** on every aspect ratio.
- **CONTROLS > GAMEPAD binds every action freely:** two buttons per action, for the 6- and 5-button layouts, pause and the moves list included. RESTABLECER returns to the defaults.

### Debug menu (F2)
- It keeps its shape on any screen, and it has **a tab per section at the top**: FIGHT, SCREENS, TOOLS... Switch tabs with TAB or a click.

### Known problems
- **Reptile's fatalities and specials are missing sounds**; other fighters may be too.
- After a round a fighter can be drawn with another's model (seen with Ermac against Sub-Zero).
- Some attacks seem to connect from too far.
- The red press highlight looks too thick.
- A few objects besides the fighters may still not show, and some fatalities lack animation (Sonya's).
- Some texture errors remain on some stages and fighters.
- Some modes besides Arcade can close the game.
- The game's menus are used with the mouse, not the pad.
- Windows only.
