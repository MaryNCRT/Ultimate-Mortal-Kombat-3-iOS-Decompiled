## Ultimate Mortal Kombat 3 — PC port 0.1.0 alpha (Windows)

**Widescreen and the iPad's 4:3**, an iPad-size HUD, texture quality up to uncompressed, much faster loading, and a launcher that updates itself. As before, **no game data and no game executable are included**: you compile the game from **your own** `.ipa` of UMK3 **1.2.59 for iPhone** with `UMK3-Launcher.exe`.

**Coming from 0.0.9:** download this one by hand once. From 0.1.0 on, the launcher finds and installs new versions itself.

### Aspect ratio (GRAPHICS > ASPECT RATIO)
- **3:2** is the iPhone's, unchanged.
- **4:3** is the iPad build's own view: its projection (UMK3 for iPad 1.2.56), with its proportions and its letterboxed menus.
- **16:9** and **21:9** (port only):
  - **The fight fills the screen with nothing stretched or zoomed.** The vertical view is the iPhone's, and the extra width shows more of the stage.
  - **The HUD sits in the corners:** the life bars, timer, pause and moves-list buttons, and the touch controls.
  - **The menus use the whole width.** The main menu's buttons move to the left edge; the other screens (select, treasure, stats, options) stay centred, and their backgrounds fill the screen. The intro logos are centred.
- New window sizes for 4:3 (1024×768, 1600×1200, 2048×1536) and 21:9 (2560×1080, 3440×1440).

### Interface size (GRAPHICS > INTERFACE SIZE)
- **IPAD** lays the game out on twice the iPhone's surface. The fight's HUD and buttons come out smaller, as on the iPad (the binary's own `HUD_Scale` 1.75), and the menus look the same.

### Texture quality (GRAPHICS > TEXTURE QUALITY)
- **IPHONE:** what the iPhone loads.
- **IPAD:** the iPad build's fonts and buttons, the full-size sheets, and the 1024 skins on the select screen.
- **NO LIMIT:** uncompressed PNGs before compressed ones, mipmaps, the menu metal background in HD, and small HUD sheets upscaled 4×.
- The white seams on the select and loading screens and the grainy art are fixed.

### Speed
- **The launcher opens in under a second** (it was about 9 s). It now lives unpacked in `launcher\bin\`, and `UMK3-Launcher.exe` only starts it, so don't delete that folder.
- **Loading is several times faster.**
  - Textures decode on every core: the PNG decoder is about 4× faster and the PVRTC one about 7×, with byte-identical output.
  - Several loading steps run per frame.
  - The game can use up to 4 GB of memory.

### Updates (COMPILE YOUR .IPA)
- The launcher checks GitHub when it opens and on **CHECK FOR UPDATES**. It shows the newest version's notes on the same page, and a new version gets a Windows notification and a red dot on the menu.
- **DOWNLOAD AND INSTALL**:
  - downloads the release and checks its SHA-256;
  - closes the launcher and copies the new files in. `umk3.ini`, `res\` and anything you added are kept;
  - opens the launcher again, which recompiles the game from your `.ipa`.

### Known problems
- A few objects besides the fighters may still not show, and some fatalities lack animation (Sonya's).
- Some texture errors remain on some stages and fighters (parts of Kung Lao's hat are black).
- Some modes besides Arcade can close the game; the menu is incomplete and some modes are missing.
- The game's menus are used with the mouse, not the pad.
- Some specials, projectiles above all, have no sound.
- In 16:9 and 21:9 some less-visited screens may still have an element that is not centred. Report it with a screenshot.
- Windows only.
