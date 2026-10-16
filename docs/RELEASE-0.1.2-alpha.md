## Ultimate Mortal Kombat 3 — PC port 0.1.2 alpha (Windows)

New on top of 0.1.1: **a settings menu inside the game (Esc)**, **a player profile**, the continue screens drawn for wide screens, and sound and control fixes. As before, **no game executable is included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

**Coming from 0.1.0b or 0.1.1:** the launcher offers this update itself (COMPILE YOUR .IPA > DOWNLOAD AND INSTALL).

### Settings menu (Esc)
- **Esc** opens it over the game, which waits underneath; Esc again, circle / B or BACK TO THE GAME closes it. Arrows, Enter, Q / E (L1 / R1) for the tabs, or the mouse.
- **GRAPHICS:** window, borderless or fullscreen and the resolution (APPLY DISPLAY), 3D resolution, antialiasing, texture quality, aspect ratio, interface size and on-screen controls, **all without restarting the game**. Texture quality reloads every texture at once.
- **KEYBOARD and GAMEPAD:** player 1 and player 2, 6 or 5 buttons, every key and pad button (Enter, then press it; Esc cancels), pad choice, reset.
- **PROFILE:** see below.
- **QUIT THE GAME** on every tab.
- Every change is saved to `umk3.ini` at once, with the launcher's names: the launcher shows it when it comes back to the front, and the next start keeps it.

### Player profile
- One per installation: a **name** of your choosing (any characters, up to 24), a **player ID** given once at random (XXXX-XXXX-XXXX) that never changes, and an **icon**: a fighter's bio and endings art over a glossy gradient of the colour you choose, **light** (white into the colour) or **dark** (the colour into black).
- Edited in the launcher (**PROFILE**) and in the game (Esc > **PROFILE**); RESET DATA puts the name, icon and colour back and keeps the ID. Saved in `profiles.ini`. It is there for the leaderboards to come.
- Your badge (icon and name) shows in the corner of the main menu and of the launcher.

### Wide screens
- **The continue screens** (Arcade's continue / new game and the continue screen) on **16:9 and 21:9** use new artwork made for those shapes (Mary's), with the texts and the red boxes on its frames. 3:2 and 4:3 keep the game's own.
- **The on-screen controls** follow a change of aspect ratio or interface size, in the game or in the launcher: they no longer stay where the previous shape put them, and the stick keeps its size.

### Sound
- A sound that plays again while it is still playing **starts over**, as on the iPhone (`-[Sound play]`), instead of doubling over itself: the screams that repeated in some fatalities.

### Other
- The red press highlight is half as thick.
- Debug mode: a finisher forced from the debug menu first stops the loser, who could keep running into it.

### Known problems
- Some objects besides the fighters may still not show, and some fatalities lack animation (Sonya's).
- Some texture errors remain on some stages and fighters.
- Some modes besides Arcade can close the game.
- Some specials, projectiles above all, have no sound.
- After a round a fighter can be drawn with another's model (seen with Ermac against Sub-Zero).
- Some attacks seem to connect from too far.
- The game's menus are used with the mouse, not the pad (the settings menu does take the pad).
- Windows only.
