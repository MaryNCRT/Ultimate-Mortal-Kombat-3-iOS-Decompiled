## Ultimate Mortal Kombat 3 — PC port 0.1.3 beta (Windows)

**The first beta.** New on top of 0.1.2: **the game's own menus with the pad or the keyboard**, a launcher in the style of the console release, key and button pictures everywhere, a button setup for keys and pads, and more colours for the profile. As before, **no game executable is included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

**Coming from 0.1.1 or 0.1.2:** the launcher offers this update itself (COMPILE YOUR .IPA > DOWNLOAD AND INSTALL).

### The game's menus with the pad or the keyboard
- **D-pad or left stick** (or the arrows / movement keys) move between the options of every screen: the menus, the pages, the character select, the pause menu.
- The option you are on **lights yellow**, as it does under a resting finger; **cross / A** (or **Enter**) presses it, and it opens when you let go, as with a finger. On the roster a white frame marks the fighter.
- **Circle / B** (or **Backspace**) is BACK, and closes a page opened from the main menu. Inside a page you only move among its own options.
- In **VS (duel)** each player has a highlight (player two's is green) and both choose at once: player one with the first pad or the keyboard, player two with the second pad or their own keys.
- **Right stick** left or right: the fighter's colour (palette) on the select screen.
- **Any pad button** skips the fight intro. On the tower the stick moves between the four, NOVICE first, and only cross chooses.
- In the **moves list** L1 / R1 or left / right (Q / E, A / D) turn the pages, and select or circle close it; with the on-screen controls on, a BACK in the left corner goes to the previous page.
- On the **continue screen** circle (or Backspace) is GAME OVER, back to the main menu.
- With the on-screen controls off, **ACCEPT and BACK** show at the bottom right with your pad's buttons (or Enter and Backspace), and inside the OK and BACK buttons.

### Button setup with the on-screen controls off
- **Options > BUTTON SETUP** and the pause menu's **BUTTON SETUP** (in place of LAYOUT) open the keys and pad setup in the game's own frame: KEYBOARD and GAMEPAD on pages of their own (L1 / R1, Q / E).
- **Player two** now has, like player one, **pause**, **moves list** and **accept** keys (keypad / * 1 by default), and their moves list shows **their own fighter** with their own buttons.

### Key and button pictures
- Keyboard, Xbox, PlayStation 5, PlayStation 4 and Nintendo button pictures ([Free Input Prompts](https://juliocacko.itch.io/free-input-prompts) by Julio Cacko, CC0) in the launcher, the settings menu, the debug menu and the moves list. They follow your bindings and the device in use.
- **Button icons per player:** auto (the pad's own), Xbox, PS5, PS4 or Nintendo (`pad_icons=`, `p2pad_icons=`).
- With the on-screen controls off, **the moves list** shows the player's own keys or buttons instead of P / K / B / R / S: the player who paused in VS, both when opened with a click.

### Settings menu (Esc)
- A side panel: **GRAPHICS** (video, textures, interface), **CONTROLS** (keyboard and gamepad on pages of their own) and **PROFILE**.
- Drop-down lists for the values, picture grids for the icons and colours, **monitor** choice, 5 or 6 buttons.
- **A pad button opens it** (HOME by default: `pad_settings=`, `p2pad_settings=`).

### Launcher
- In the style of the console release: marble rings with glowing rims turning slowly, a sliding selection, a command bar with the buttons of the device in use, sections; the logo top left and wider panels.
- **Monitor** choice (`monitor=`), the profile's icon always in sight while editing it.
- **It closes when the game opens.**

### Profile
- The **full colour palette** (15 hues and greys, 7 shades each) in the launcher and the game; the light, dark or **custom** gradient with a second colour of your choice.

### Colours (palettes)
- The colours picked on the select screen are kept **for VS only**. In the other modes the computer's fighters come in either colour by chance (Motaro and Shao Kahn keep theirs).

### Other
- On 16:9 and 21:9 the moves list's NEXT sits at the screen's edge, and the last rows of HELP & ABOUT and EXTRAS come up a little.
- The player's badge is now top right on the main menu (smaller on 3:2 and 4:3), the logo a little to the left; the pages' marble panel reaches the edge on 21:9.
- Fixed from 0.1.2: `aspect=(null)` written to `umk3.ini` after some settings changes, and pad bindings written as pictures (the settings menu said there were none).

### Known problems
- Some objects besides the fighters may still not show, and some fatalities lack animation (Sonya's).
- Some texture errors remain on some stages and fighters.
- Some modes besides Arcade can close the game.
- Some specials, projectiles above all, have no sound; Mercy's sound sometimes does not play.
- After a round a fighter can be drawn with another's model (seen with Ermac against Sub-Zero).
- Some attacks seem to connect from too far.
- The right stick read through DirectInput (a PlayStation pad without Steam / DS4Windows) is a guess of its axis: tell us if it does nothing.
- Windows only.
