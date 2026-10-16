## Ultimate Mortal Kombat 3 — PC port 0.1.5 alpha, the online update (Windows)

New on top of 0.1.4: **ONLINE**, two players on two games, found by their player code. Also fixed: the versus screens' blinking spotlights and the broken FATALITY sign in the 64-bit build. As before, **no game executable is included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

**Coming from 0.1.1 to 0.1.4:** the launcher offers this update itself (COMPILE YOUR .IPA > DOWNLOAD AND INSTALL).

### Online
- **PLAY > ONLINE** (the new seventh option; the seven are spaced closer so they fit).
- **PLAY > ONLINE** is translated in every language the game has (EN LÍNEA, EN LIGNE, 온라인, 在线...), and so is the whole ONLINE screen in the Latin ones.
- The ONLINE screen shows your profile (icon and name) and **YOUR CODE**: the id of your profile, the same one chosen at start-up. **COPY** puts it on the clipboard; give it to the other player.
- Type or paste (Ctrl+V) the other player's code in the box, then **FIND MATCH** (or Enter). Their game has to be on its ONLINE screen; theirs hosts the fight.
- Then it is the game's own two-player match, as the iPhone game played it over Bluetooth: fighter select (the other player's icon and name top right), the versus screen with both names, the fight with each player's icon on their side (player 1, the host, on the left), and at the end **REMATCH**, another fighter, or out.
- **For now it works between two games on the same PC or on the same local network.** Internet play through Epic Online Services (no ports to open) comes in a later update.
- The debug menu and the debug keys are off in an online fight.
- The game no longer pauses when its window loses the focus (online the other game waits for it).

### Fixes
- The **spotlights behind the dragon** on the versus screens blinked in the 64-bit build, and the **FATALITY sign** broke there: every frame on the second sheet of those animations drew nothing.

### For players testing online on one PC
Two games in one folder share `profiles.ini`, so they would have the same code. Start the second one with `UMK3_PROFILES=profiles-2.ini` (another profile file in the same folder) and, if you like, `UMK3_WINDOW=720x480` for a small window.

### Known problems
- Online only reaches games on the same PC or local network (see above).
- The waiting before an online fight starts can feel long.
- Classic Sub-Zero's elbow combo still does not look quite right.
