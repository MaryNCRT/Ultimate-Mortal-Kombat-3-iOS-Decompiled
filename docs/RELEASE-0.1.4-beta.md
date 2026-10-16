## Ultimate Mortal Kombat 3 — PC port 0.1.4 beta (Windows)

New on top of 0.1.3: **a 64-bit build**, the **crash on winning in Arcade** fixed, classic Sub-Zero no longer vanishes in his elbow combo, and a **touch layout editor** without limits. As before, **no game executable is included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

**Coming from 0.1.1, 0.1.2 or 0.1.3:** the launcher offers this update itself (COMPILE YOUR .IPA > DOWNLOAD AND INSTALL).

### 64 bits
- The launcher's new **WINDOWS** field picks **32 BITS** (`umk3-game.exe`) or **64 BITS** (`umk3-game64.exe`). COMPILE builds the one chosen; both can be kept side by side (`arch=` in `umk3.ini`).
- With both compiled, **VERSION TO OPEN** chooses which one PLAY opens.

### Fixes
- **Winning in Arcade closed the game**, whatever the fighter, and so did leaving some stages (stage 14). The fight's scene handles start empty, as in the original game, so the end of the fight no longer frees what was never loaded.
- **Classic Sub-Zero vanished** for a few frames in his elbow combo. His frame list lacks four of the ninjas' combo frames (RPCOMBO6 to 9), as it does in the original game, but his model carries those frames; they are drawn now. The one frame nobody has (RPCOMBO10) keeps the pose before it.

### Touch controls and screens
- **Touch layout editor without limits:** move, resize and change the opacity of every button and the stick; RESET puts them back.
- The pause menu reaches the screen's right edge on 16:9 and 21:9 (it stopped half way on 21:9).
- With the on-screen controls off, the **Karnage and Survival summaries** show only BACK (circle or Backspace).

### Launcher
- **UMK3 LAUNCHER**, with its own icon and the logo inside; the game has its icon too.
- A wider window.

### Known problems
- Classic Sub-Zero's elbow combo still does not look quite right; being worked on.
- Some objects besides the fighters may still not show, and some fatalities lack animation (Sonya's).
- Some texture errors remain on some stages and fighters.
- Some modes besides Arcade can close the game.
