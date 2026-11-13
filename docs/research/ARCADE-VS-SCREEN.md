# The arcade's VS screen, as MAME plays it

— Source: observation in MAME, `umk3`, 2026-11-13 (black box: frames and the
sound output only; nothing of the ROM is in the repository —
`LABORATORIO MAME/` is git-ignored).

How it was reached: `LABORATORIO MAME/vs.lua` -- four coins, 1P START,
2P START on the select screen, both pick (Kitana vs Jade), then a frame
every 0.1 s and the sound written with `-wavwrite` (48 kHz mono).

## What it shows

- Both picks confirmed: the select screen stays ~1.5 s, fades to black
  (~0.3 s), black ~0.3 s.
- The VS screen fades in over ~0.4 s: grey brushed metal, the dragon
  spotlights top left and right, "BATTLE 1" at the top, the two fighters'
  portraits large and cut by the screen's sides, the "VS" logo in the
  middle lighting up in fire, the six kode boxes at the bottom.
- It holds ~3.3 s, cuts to black (~0.2 s), and the fight's "ROUND 1"
  follows. From the picks to the fight: ~5.7 s.

## What it sounds like

- No voice: the announcer says neither the names nor "versus" (a voice
  stands far above the music in the output; nothing does here).
- The select screen's music goes on under the VS screen and fades out over
  its last ~0.9 s, then ~0.4 s of silence, then the stage's music.
- One short sound as the VS logo lights (~0.7 s after the screen is up),
  barely above the music.

## The iOS game

Its VS screens (`FE_Task_VS_Screen`, `FE_Task_Multiplayer_Versus_Screen`)
play no sound at all (their calls, read from UMK3.armv7, hold no sound
function) and have neither "BATTLE n" nor the logo's fire: the iOS art is
VS.PNG, still.
