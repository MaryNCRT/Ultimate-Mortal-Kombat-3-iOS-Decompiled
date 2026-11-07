# Changelog

Release history for the native PC port of *Ultimate Mortal Kombat 3* (iOS 1.2.59).

---

Newest first. Each release says what was added or fixed and whether the
tester (Mary) has played it yet. The technical detail of every change -- the
binary addresses, the checks run -- is in [PROGRESS.md](PROGRESS.md).

## 0.4.3 beta (2026-11-07)

- **The replay player in the ONLINE screen's look** (Mary): a stone panel
  under a gold bar, the steel plates with bone-white pictures, PLAY / PAUSE
  the red plate, the fight played in blood with a gold knob, the game's
  font; the top corners, the replay list, the loading screen and the
  hidden player's hint too. The mouse's boxes are where they were.
- **STAGE SELECT's list shows every stage, nothing to scroll**: up to five
  rows, the pictures sized to the rows filled (the game's 16 in four rows
  of four), each name on a dark band across its picture's foot.
- **Esc's settings menu**: the black box beside the categories is gone --
  0.4.2 framed the icon in the engine's pixels instead of the menu's space;
  under the categories now the player's banner as the game shows it (the
  gradient on steel, the fighter out of it, the name and id).

## 0.4.2 beta (2026-11-07)

- **STAGE SELECT in the ONLINE screen's look and life** (Mary): the chosen
  stage large behind everything, dimmed; a stone panel in gold and bronze
  rising in; the heading's steel plate; the picture in a gold frame with
  gold chevrons, sliding in from the side it was turned to; in the list,
  the pictures lit under the mouse and the chosen one pulsing red; the
  buttons the ONLINE plates, SELECT the red one. The ONLINE pieces are lent
  by FrontEnd.c (`PortSkin*`, `OnButtonLook` split from `OnButton`).
  `UMK3_STAGE_PREVIEW=<tick>[:list]` opens it for a screenshot.
- The launcher's player badge is the game's ONLINE top badge -- a small
  Player Banner, the fighter out of its end, the name and EDIT PROFILE --
  docked in the command bar's left end, the gradient to its slanted tip.
- PROFILE: IDENTITY and the background's editor side by side; the banner's
  steel grain laid in overlay, so the gradient keeps its colour; the ONLINE
  banner's grain only in shade, with the launcher's slow moving light.

- **The launcher renewed** (Mary's brief): UMK3 Xbox 360's menu kept in
  its places -- the rings, the menu down the curve, the description box, the
  command bar -- and re-cast in dark blued steel, slate and smoke, with gold
  for the details and deep red for what is chosen: the same family as the
  ONLINE screen. Stone and brushed steel are SVG noise and the corner
  brackets, separators and sigils SVG shapes (`ui/gen_materials.py` writes
  `theme/materials.css`; no image files, no dependency). Every page is a
  stone panel in a steel frame with a gold bar on top; buttons are iron
  plates, the important one the red plate; headings heavy capitals with a
  sigil; MODS' seven pages fit one row.
- **PROFILE rebuilt**: the **Player Banner** on top -- the player's own
  gradient on brushed steel, a slow sheen, the fighter standing out of it
  with no box, the name, the account's state, the id and the favourite --
  stays in sight while the page scrolls. Under it IDENTITY (the name; the
  fighter, chosen in a window over a darkened screen with every fighter's
  icon and name; the outfit) and LOOK (the gradient editor, unchanged in
  how it works).
- **The icon and the favourite fighter are one**, everywhere: profiles.ini
  keeps writing its ninth field, now always the icon (launcher
  `lib/profiles.js`, game `runtime/profiles.c`); the ONLINE editor's
  favourite row shows the fighter and opens the gallery, which picks both.
- **ACCOUNT** keeps every function and every call; the page is grouped in
  boxes -- the session and e-mail, the recovery (gold), deleting (red) -- and
  a recovery code shown stands in a gold-framed plate. **FRIENDS**, **REPLAYS**
  and the **MOD MANAGER** (its how-to now a strip above the list) are plates
  of black steel; the editors' pink and lavender are gold, steel and red.
- **The game**: Esc's settings menu has no PROFILE page any more (the
  launcher edits the profile); the player's icon under the categories stays,
  in a bronze frame with a gold lip. The ONLINE screen's banners are drawn
  as the launcher's: the gradient whole on steel, a gold bar on top, the red
  line under, dark steel uprights.

## 0.4.1 beta (2026-11-07)

- **The ONLINE screen's new skin** (Mary's references: MK Deception's and
  MK Armageddon's ONLINE KOMBAT, MK9's Gearworks, MK11's menus, MK3's
  stages): black panels in a frame of antique gold with flared corners,
  dark header pills edged in gold, buttons edged in gold (the main one
  oxblood), a gold-framed title bar with end caps, gold, bone and silver
  words, a grey-green mist. Nothing moved: every position and size is the
  same.
- **A stage behind each section**, darkened: HOME the Graveyard, PROFILE
  the Soul Chamber, LOBBIES the Subway, QUICK MATCH the Bell Tower, PARTY
  the Temple; crossfaded when the section changes.
- **AUDIO > MENU CLICK SOUND**: the menus' press sound can be muted
  (umk3.ini `menu_clicks=`), asked by players.
- **The ONLINE screen's materials** (Mary's "final visual polish" brief):
  the panels are dark stone, a little see-through, with an inner shadow,
  dark bronze uprights, a beam of aged gold and gold corner brackets; the
  metals are textured (brushed, tarnished, scratched, bevelled); headings
  are black steel plates with a red edge; buttons are black steel framed in
  bronze, gold under the mouse, deep red with gold chevrons when chosen with
  the pad, sunk when pressed; the chosen section is deep red with a gold
  bar; every word has a shadow; each section has a faint air of its own
  over its stage. Positions and sizes unchanged.
- **Player Banners are metal**: the player's own gradient stays, a
  brushed metal sheen over it (light, shade, scratches), in a bronze mount.
  The editor's windows are stone, not flat navy; the gold has more relief
  (a lit lip, a cast shadow); a button chosen with the pad is a lit metal
  plate; separators are engraved, with a sigil and a stud; secondary words
  are brighter. COMING SOON lists the new fight modes: TAG TEAM, SIMUL,
  KING OF THE HILL.
- **Player Banners, worn steel** (Mary's reference, a weathered steel tag):
  a 512 x 128 steel plate (brushed, grimed, pitted, scratched) under the
  player's gradient, its grain through the colour, a finer sheen, a bevelled
  rim, ends in shade. The text boxes (MY PROFILE's name) are wells in black
  steel framed in bronze, gold while typing; the last navy leftovers are gone.

## 0.4.0 beta (2026-11-06) — the new ONLINE screen

Published 2026-11-06 as the GitHub pre-release `v0.4.0-beta`. Tried by
Mary in her test folder while it was made (the ONLINE screen, MY PROFILE,
the gradient window); not yet played online between two machines.

- **MY PROFILE's background, PERSONAL**: a window over the editor, the
  rest darkened -- the banner previewed on top, a dial (dragged, or by 5
  degrees), FROM and TO with the chosen one's hex, presets in UMK3's
  colours, the CSS (typed, copied, cleared), SAVE and CANCEL. A click on
  FROM or TO opens a small colour window: saturation and brightness, the
  hue, R G B typed, DONE and BACK. The editor's controls are smaller and
  no longer overlap.

### The ONLINE screen

- **MY PROFILE's gradient editor, detailed** (Mary): the same panel, with
  the launcher picker's abilities -- FROM and TO each with the palette's
  arrows, a swatch and its hex typed (#RRGGBB, applied as it is typed), the
  direction 0 to 359 degrees (arrows of 5, the number typed, a dial),
  ten presets of UMK3's colours (Midnight, Ice Storm, Blood, Spectral,
  Venom, Inferno, Ancient Gold, Silver, Crimson Night, Electric Blue; no
  pink), and ADVANCED: the gradient as CSS, copied, typed and applied when
  valid (one colour, or linear-gradient with deg or to top / right /
  bottom / left and two colours), taken again from the controls, cleared.
  DEFAULT now puts back only the background. The banner above is the
  preview; nothing is kept until SAVE. The launcher's editor is untouched.
- The OPTIONS entries are centred again with EDIT PROFILE among them, and
  the main menu's banner is smaller (Mary).

- **PROFILE** takes FRIENDS' and INVITATIONS' place (Mary): the sections are
  HOME, PROFILE, LOBBIES, QUICK MATCH and PARTY. PROFILE has MY PROFILE --
  name, avatar, outfit, favourite fighter (a gallery of the 24) and the
  gradient (light, dark or custom, two colours, the direction), with the
  banner previewed as it changes and SAVE, CANCEL and DEFAULTS; a new name
  is checked by the account's server and refused when another player has
  it -- FRIENDS (challenge, join or decline a fight, remove a friend, add
  one by player ID), REQUESTS RECEIVED (accept, decline) and REQUESTS SENT
  (cancel). A count on PROFILE says what waits, red while some of it is
  new; a new request or invite shows a notice once.
- **Players online**: HOME and the top of the screen say how many players
  have the ONLINE screen open (games from this version on).
- **The main menu's badge** is a small player banner; a click opens MY
  PROFILE, and OPTIONS has EDIT PROFILE.
- **Esc, AUDIO and VIDEO SETTINGS stopped working after visiting ONLINE**
  (Mary): leaving the screen left the network session open. Fixed. The
  debug menu (F2) is now off on the ONLINE screen too, like the settings
  menu.
- HOME's statistics say WINS and LOSSES (Mary).

- **The ONLINE screen's new look** (Mary's visual brief, 2026-11-06): panels
  with depth (a diagonal gradient from navy to black that lets the cemetery
  through, a light along the top edge, a darker bottom, a soft shadow),
  buttons in gradients whose states are told apart (normal, under the
  mouse, chosen with the keys or the pad in red, pressed, off), tabs with
  separators, a glow over the red bar and a count of the invites waiting.
- **Player Banners**: every player is a horizontal banner in their own
  profile gradient -- exactly the one they chose, over the whole banner --
  with their fighter (the VERSUS art, transparent) standing out of its left
  end. HOME's banner carries the name, the state, the player ID with COPY,
  and the game's own statistics (human wins and losses, best win streak,
  fatalities, flawless, favourite character). FRIENDS: a list of banners
  and the chosen friend's banner, big, with INVITE TO FIGHT.
  INVITATIONS: received (ACCEPT, DECLINE) and sent. LOBBIES, QUICK MATCH
  and PARTY show what exists (you, as the party's leader) with their
  actions off until their stages come.
- **Statistics were lost at every start** (a decomp bug): the save was
  loaded into a second copy of the stats nobody read, so the STATS screen
  and now the profile showed only the session's numbers. Fixed: they load
  into the one block the binary has. Each fighter's play count (the
  favourite character) only starts counting from this build.

- **ONLINE has sections**: HOME, FRIENDS, INVITATIONS, LOBBIES, QUICK
  MATCH and PARTY along the top, over the same night cemetery and moon,
  in translucent panels with the MK red along their top; a red bar slides
  to the section and each one fades in. HOME has the profile, your code and
  COPY, FIND BY CODE and the state of the connection; FRIENDS and
  INVITATIONS have what the launcher's account gives (up to six rows).
  LOBBIES, QUICK MATCH and PARTY say they are coming (the next stages).
  The mouse lights the button under it; the keyboard and the pad move
  between every button. The code box takes the keys only once it is
  clicked (Esc gives them back); Ctrl+V pastes anywhere on HOME. Checked by
  Claude in the test folder (with Mary's permission to open the game):
  clicks, the sections, typing a code, the keyboard; not yet played online
  between two machines.
### Updates always reach the player

- **An update from the launcher now always gives the new game.** Mary:
  players who updated from the launcher kept the old game. The update now
  closes the game and the launcher by force (whatever still runs from the
  folder is stopped, not waited for), throws away the compiled objects and
  marks the folder; until the game has been compiled again, PLAY, a replay
  and a friend's JOIN compile first, also when the launcher is opened by
  hand. Checked on the test folder: with the mark there, the launcher
  compiled on opening and the mark went. The forced close in apply.ps1
  was checked by generating the script (it parses), not by a real update.
### The intros' ambient occlusion

Built in Mary's test folder; she played it on Direct3D 12 and confirmed the fix.

- **AMBIENT OCCLUSION in the intros, again**: the stage's occlusion was
  still painted over the fighters in some intros (Mary: Motaro on the
  balcony). In those intros the fighters are drawn blended, and the
  occlusion's depth pass skips blended draws, so the fighters' pixels read
  the occlusion of the wall behind them (the diagnostic log said
  `fighters' draws 1 (blended 1, ...)`). The fighters are now a layer of
  their own: the depth pass marks them in the alpha channel, the occlusion
  pass and the blur only mix samples of the same layer, and a fighter's
  pixel only takes the fighters' occlusion -- none at all when the fighter
  is not in the depth (`rgl_ff.c` `ao_passes`, `shaders/ff.frag`,
  `shaders/ff.hlsl`). `tests/rgl_test.c`: OpenGL passes; Vulkan and
  Direct3D 12 fail the same 5 shadow checks as before this change (checked
  against a build of main).

## 0.3.1b beta (2026-11-06) — frame time and LOW LATENCY

Published 2026-11-06 as the GitHub pre-release `v0.3.1-beta.b`: the fix
below and the two entries after it (no plain 0.3.1 was published). Played
by Mary in her test folder (Direct3D 12).

- **AMBIENT OCCLUSION in the intros**: the occlusion of the stage was
  painted over the fighter (Mary: Ermac in the graveyard). The intros clear
  the depth before drawing the fighters, so they stand in front of the
  stage whatever is nearer; the occlusion's depth pass skipped that clear,
  so where a piece of the stage was nearer than the fighter, the depth was
  the stage's and so was the occlusion. The pass now keeps the clear
  (`rgl_ff.c`, `ao_passes`; the log says `ambient occlusion: a depth clear
  between the scene's draws, kept`, seen in Mary's run). Also guarded, not
  seen: a fighter with a projection or viewport of his own goes into the
  depth from his own draws, each placed by its viewport.

## Since 0.3.0: LOW LATENCY, Sindel's smooth idle, the moves list's mod portrait (2026-11-06)

Played by Mary in her test folder (Direct3D 12): Sindel's idle smooth, the
moves list's portrait from her texture pack with no FPS drop.

- **LOW LATENCY** (on by default; `low_latency=` in `umk3.ini`, the VIDEO
  page, the launcher): at most one frame waits between a tick and the
  screen. Direct3D 12 uses a waitable swap chain at frame latency 1 and,
  with VSYNC, waits for the display after each present; Vulkan waits for
  each present with `VK_KHR_present_wait` where the driver has it (the log
  says `vulkan: present wait on`); OpenGL on SDL calls `glFinish` after the
  swap, as Windows already did. The keys were already read right before
  each tick; what queued was the present. Off, frames may queue as before.
  Ticks, rollback and replays are unchanged. The plan and the measurements:
  [research/INPUT-LATENCY.md](research/INPUT-LATENCY.md).
- **Sindel's idle was not smoothed** (SMOOTH ANIMATIONS). The game's
  `IsLiaProblem` refuses blends on five of her frames, two of them her
  stance (LIASTANCE1 and 5, frames 257 and 261), so the idle stepped on
  four of its nine changes. SMOOTH ANIMATIONS no longer asks it; in the
  binary it only refuses blends in a fight, where nothing is blended.
- **The moves list's portrait** is the select screen's, texture pack
  included: it now looks in the mods first. A mod's HD portrait (1080x1080)
  took 40-100 ms to decode when the list first opened, an FPS drop; both
  fighters' are now loaded while the fight loads.

## After 0.3.0: frames back under 16.7 ms (2026-11-06)

Mary saw stutters in SMOOTH ANIMATIONS (Sindel) and drops in fps: fights
ran 17-19 ms a frame, and every frame past 16.7 ms runs two ticks, which
the frame blending shows. Two costs of ENHANCED LIGHTING cut
(`rgl_ff.c`): smooth normals are no longer summed every frame for a mesh
past 20,000 triangles (SMOOTH MODELS' level 2: Sindel 60,000) -- the
shaders take each face's from the derivatives, the same on so dense a
mesh; and the point lights' shadows send each cube face only the
triangles it sees, not the fighters six times a light.

## 0.3.0 beta (2026-11-06) — ENHANCED LIGHTING

Published 2026-11-06 as the GitHub pre-release `v0.3.0-beta`. Played by
Mary in her test folder through every step below (Direct3D 12); Vulkan
built and run by the same code, OpenGL without any of it.

**ENHANCED LIGHTING** (Vulkan and DirectX 12; `rt_shadows=1`, the GRAPHICS
page and the launcher), everything in the entries below since 0.2.1:
real-time shadows; relief and specular on every texture, the game's and a
pack's; mipmaps; every glow a coloured light, the two strongest casting
the fighters' shadows; each stage lit by its sky; per-stage lighting files
(`lighting/<scene>.ini`); emissive materials; AMBIENT OCCLUSION (on/off).
Also: PVRTC's block grid smoothed (every renderer); every fighter's
KOMBOS in the moves list; the launcher no longer drops the game's own
settings.

Lighting files ship for Kahn's Kave, The Rooftop, The Bridge, The Bell
Tower, Jade's Desert, The Graveyard, The Balcony and The Temple; the rest
are lit as measured.

## Since the last entry: AO over everything, emissive materials, The Temple and The Balcony (2026-11-06)

- **AMBIENT OCCLUSION** fixed and made visible. Mary's debug views (the
  debug menu's AMBIENT OCCLUSION VIEW: the occlusion after and before the
  blur, the depth, the normals, the final contribution) showed the depth
  and normals right and the occlusion all but empty: the radius was given
  in world units, but the occlusion works in the camera's, which the
  game's view scales about 5x -- it was a fifth of what it meant. Now
  converted by the view's own scale (`ao_radius` 0.6 world units); HBAO
  takes at least four directions (two left most pixels blind); the
  result is squared; it is applied last, over the dynamic lights and the
  fighters' light, a fighter as much as the stage. One on/off option in
  the menu and the launcher (`ao=` still picks Crytek/Alchemy by hand).
- **Emissive materials**: the game's fullbright meshes and a stage file's
  `emissive=` ones are drawn self-lit (no shadow, occlusion or tint) and
  each lights its surroundings in its texture's colour (`rgl_ff.c`, the
  `lime_port_mesh_fullbright` hook). Kahn's Kave's lava and fires.
- **Specular maps** were already there, made from each texture as the
  relief is.
- **The Temple**: `relief=1.6` (a new key), its four braziers warm on the
  fighters (the middle two with shadows), the openings behind its arches a
  cool green. **The Balcony**: its torches stronger.
- **The startup stall** (the GPU seconds on the first frame: the driver
  finishing the shader): the HLSL loops are no longer unrolled, the DXBC
  38% smaller. Shorter; may not be gone.
- Jade's Desert (warm daylight) and The Graveyard (night, moonlight)
  lighting files; The Bell Tower's windows are the night sky.

## AMBIENT OCCLUSION; The Bridge's and The Bell Tower's lighting (2026-11-06, after 0.2.1)

Part of ENHANCED LIGHTING (Vulkan, Direct3D 12). Mary played it: no lag
once made cheaper.

- **AMBIENT OCCLUSION** (`umk3.ini ao=0..3`, the settings menu's AMBIENT
  OCCLUSION row: off, Crytek SSAO, HBAO, Alchemy AO; HBAO by default),
  after `research/AMBIENT-OCCLUSION.md`, written from the papers. Each
  flush (`rgl_ff.c` `ao_passes`): the scene's distance into a target at
  half the 3D resolution -- the stage's held opaque draws, and the
  fighters from their base meshes (the triangles held for the key light's
  map; sending the smoothed ones again cost ~3 ms a frame, Mary saw the
  drop) -- then a full-screen occlusion pass and a 5x5 blur that keeps to
  its own depth. Every lit draw darkens its own colour by it (never the
  dynamic lights'), a fighter by 70%, less where the map's shadow already
  is, never below 0.40. Keys: `ao_radius` (1.0), `ao_intensity` (1.8),
  `ao_bias`, `ao_samples` (8), `ao_blur`, `ao_scale` (0.5), `ao_floor`.
  The backends got a fourth texture, the screen map (`screen_map`: Vulkan
  binding 4, D3D12 t3); the light block carries the projection, the
  viewport and the settings.
- **The Bridge**: its sky (`skyplane`) and far skyline (`4`) without
  relief: on the painted clouds it looked lumpy.
- **The Bell Tower**: light the colour of its picture, not its sky (warm
  amber), the night sky's cool blue through its three windows, and a warm
  light on the wooden floor from above.
- **The startup freeze** came back (3.6 s in one present before the main
  menu, all in running the frame's commands, no pipeline made). The D3D12
  diagnostic now splits it into the texture uploads since the last
  present (count, lists, MB, time run and waited for, time releasing).
  Not fixed yet.

## Each stage's sky, lighting files, smoother PVRTC (2026-11-06, after 0.2.1)

Part of ENHANCED LIGHTING unless said. Mary played each step: The Rooftop
reddish, Kahn's Kave toned down, the select screen back to normal, the
sky's grid gone.

- **Each stage's light from its own sky** (Mary: The Rooftop's red sky).
  The stage's far draws give its sky's colour, each weighed by its size
  times its saturation squared, so the dominant hue wins over a grey
  skyline (`rgl_ff.c` `sky_note`, `sky_build`). It tints everything the
  lights reach (stage and fighters alike), the hue at white's brightness
  mixed in at 0.80 from above and 85% of that from below; a grey sky adds
  nothing. Only while a stage's `_LEVEL_SCENE` is loaded
  (`lime_port_meshset_event`): the select screen's fighters came back red.
- **A dark stage lights its fighters slightly** (Mary: 0.1-0.2): up to
  0.15 of their texture's colour as the stage's mean brightness falls
  from 0.08 to 0.04 (linear).
- **Lighting files per stage**: `lighting/<scene>.ini` beside the game
  (shipped in `launcher/lighting/`, copied at extraction and in the
  release; `README.txt` has the keys): `sky_strength`, `sky_color`,
  `fighter_lift`, `glow_power`, `no_relief=<meshes>`, and up to 8
  `[light]`s (point, spot, directional) placed in the world or anchored to
  a stage mesh by name -- RenderMesh.c names each mesh as it is drawn
  (`lime_port_mesh_name`), and the log lists them with their places as a
  stage loads. `shadows=1` gives a file's point light the fighters'
  shadows ahead of the glows. Kahn's Kave: sky at 0.45, fighters' light
  0.15, a warm light at the lava, a spot on the throne. The Rooftop: sky
  at 1.5, its backdrop and swirl without relief.
- **The fighters react more to the glows' colours** as they near them:
  their response 90% (was 60%), and the falloff softer (1 + 4x^2).
- **PVRTC's block grid smoothed** (all renderers, with or without
  ENHANCED LIGHTING): Mary saw a grid of lines in The Rooftop's sky; it is
  `ROOFTOP_BACKDROP.pvr`'s own 4x4 compression blocks. At decode,
  `lime/pvr.c` `pvr_deblock` blurs (binomial 5x5) where a 5x5 window's
  colour range is under 10/255, fading out by 26: gradients only, never an
  edge.
- **The startup freeze** Mary saw (a 2.5-5 s present before the main
  menu, three runs) did not come back; `rgl_d3d12.c` prints a
  `d3d12 slow present:` breakdown and any pipeline compiled in play,
  should it.

## Every fighter's kombos in the moves list (2026-11-06, after 0.2.1)

Mary played it: works.

- **The KOMBOS column showed "-" for 11 fighters** (Kano, Sindel, Sektor,
  Kung Lao, Kabal, Shang Tsung, Smoke, Mileena, Reptile, classic Sub-Zero
  and Smoke). Their kombos now come from the fight engine's kombo trees,
  as Cyrax's do. Each row is the buttons in order (B+HK for the
  roundhouse); a row repeated in a tree, such as Mileena's, is shown once.

## Cyrax's kombos in the moves list (2026-11-06, after 0.2.1)

Mary played it: works.

- **The moves list's KOMBOS column showed "-" for Cyrax.** The binary's
  moves tables (`MovesListTab`) have no kombo section for him. His kombos
  are in the fight engine's own kombo trees, and the column now reads them
  from there: HP, HP, LP; HP, HP, HK, HP, HK, HK; HK, HK, B+HK (P and K with
  5 buttons). Kano, Sindel, Sektor, Kung Lao, Kabal, Shang Tsung, Smoke,
  Mileena, Reptile and the two classic fighters still show "-": some of
  their trees hold entry kinds not decoded yet.

## DYNAMIC LIGHTS' shadows, phase 5 (2026-11-06, after 0.2.1)

Mary played it: everything looks right, though the change is subtle.

- The two strongest point lights (power times reach) cast the fighters'
  shadows all round, as a cube map: six views a little over 90 degrees
  (tan 1.05, so a filter does not run off a face) into 512-texel tiles of
  one target, three across and two rows a light (`rgl_ff.c`
  `point_shadows`). Only the fighters cast, on the stage and the other
  fighter; a fighter skips his own texels. The texel is the key light's
  (packed depth + caster id), so the same shader mode writes it.
- The shaders (`point_shadow`) pick the face the point lies most along,
  turn its depth back into a distance, and take nine taps kept inside the
  tile.
- Backends: a third texture (`shadow_map2`: Vulkan binding 3, D3D12 t2).
- The log says `dynamic lights: point shadows on` the first time.

## DYNAMIC LIGHTS, phases 1-2 (2026-11-06, after 0.2.1)

Part of ENHANCED LIGHTING (Vulkan and Direct3D 12). Mary played it: "se ve
bien".

- **Every glow is a light.** Each additive draw of the scene (a fireball, a
  glow, sparks, a teleport) becomes a point light of its own colour where it
  is: the texture's mean colour (`tex_avg`, measured at upload) times the
  draw's vertex colour. Draws close together are merged, up to 16
  (`rgl_ff.c` `emit_note`, `lights_build`). Radius 6x the draw's size,
  2.5-12 units; power 2.5. The log prints a `dynamic lights:` line every
  600 flushes with the count, the first light's place, radius and colour.
- **Per pixel, on the stage and the fighters**, added over the game's own
  light (Blinn-Phong in linear light, a smooth window to 0 at the radius,
  spot cones by smoothstep); a stage with no glows looks as before. Spot
  and directional lights are in the shader and the block but nothing places
  them yet (a stage's lighting file, phase 4).
- **Normals.** The stage's are the `.meshset`'s own: the 12 bytes the loader
  skipped after u, v are unit normals matching the faces (checked over the
  stage and fighter files). Kept outside the engine heap, keyed by the
  mesh's vertex block (`RenderMesh.c` `pn_keep`), sent with
  `glNormalPointer` only while ENHANCED LIGHTING is on
  (`lime_port_dyn_normals`), so the GL-trace tests see the original calls.
  The fighters' light is baked per frame, so their smooth normals are made
  from their triangles in `rgl_ff.c` `tris_indexed` (a mirrored fighter's
  winding flipped by the matrix's determinant).
- **Renderer**: `rgl_vtx` gained the world normal and position (68 bytes);
  `rgl_frag` gained `lit`; the light block (`rgl_light_block`, 1088 bytes)
  is a dynamic uniform buffer in a ring in Vulkan and a root CBV in the
  upload buffer in Direct3D 12 (`lights` backend call).

Not yet: shadows from these lights (phase 5), each stage's sky colour,
lighting files per stage and fighter, OpenGL
([research/DYNAMIC-LIGHTING.md](research/DYNAMIC-LIGHTING.md)).

## ENHANCED LIGHTING (2026-11-06, after 0.2.1)

REAL-TIME SHADOWS became **ENHANCED LIGHTING** (the same `rt_shadows=1`
in umk3.ini, so a setting already on stays on), Vulkan and Direct3D 12.
Mary saw it in her test folder: the stage relief, the far textures without
moire, and the backdrop with no seams. Not yet played as a whole.

- **Relief on every texture** (Mary, after NormalmapGenerator and DK's GTA
  SA normal map plugin): the shader takes each texture as its own normal
  map as it draws -- its brightness a height, its Sobel slopes the tilt,
  plus the slopes two mip levels down for the large shapes -- the game's
  textures and a pack's alike, with nothing to prepare. Each texture's mean
  slope is measured once at upload (`rgl_ff.c` `tex_bump`) so a flat iPhone
  texture and a busy HD one come out at the same depth; a flat colour gets
  none. The stage at 1.5, a fighter's skin at 0.15 (their light is painted
  in). Not on what adds light (glows, sparks).
- **Specular** (Blinn-Phong, after the stackgl lighting walkthrough): a
  glint where a bump turns to the light, its specular map the texture's own
  (bright and grey shines; dark or strongly coloured does not); and the
  relief and the glint worked in linear light (gamma 2.2).
- Both in **the texture's own space**, lit from a light fixed there: a
  first version took the normal from each triangle's flat face and shaded
  every face on its own, a seam at each edge (Mary saw them in a backdrop).
- **Mipmaps on every 3D texture** (Mary: a far building broke into moire):
  the levels are made for every texture, and the 3D scene samples them
  trilinear and 16x anisotropic though the game asked for none. Not a
  texture changed in place (its levels would be stale).
- **The launcher kept losing the game's own settings** (Mary: "the
  launcher's settings are not saved"): every change there rewrote
  umk3.ini from the keys it knows, dropping the ones only the game writes
  (`p2key_pause`, `p2key_moves`, `p2key_ok`, `replay_scale`). `lib/ini.js`
  `save` now keeps every line it does not write.
- `rgl_frag` gained `bump` (20 bytes of push / root constants); the
  shaders were rebuilt with `shaders/build.py`.

Next: real-time dynamic lighting (point and spot lights, their shadows, on
the stage and the fighters), planned in
[research/DYNAMIC-LIGHTING.md](research/DYNAMIC-LIGHTING.md).

## 0.2.1 beta (2026-11-05) — online fixes, the moves list, settings in one order

Published 2026-11-05 as the GitHub pre-release `v0.2.1-beta`. Checked in
scripted runs on Mary's PC (two games online over the LAN on one machine)
and by Mary in the menus; **not yet played online between two machines**.
Both players need 0.2.1, built fresh.

- **Settings in one order everywhere** (Mary): VIDEO, GRAPHICS, TEXTURES,
  AUDIO, INTERFACE, then CONTROLS. The launcher's GRAPHICS is SETTINGS, with
  CONTROLS as its last page. In the game, the SETTINGS screen has one
  AUDIO SETTINGS tile in place of MUSIC VOL. and EFFECTS VOL. (off / low /
  medium / high): the two bars and the sound output. VIDEO SETTINGS and
  AUDIO SETTINGS are two pages of one panel (L1 / R1, Q / E); its columns
  start at a category, with a scroll bar when they do not all fit.
- **Bars** under their label, across the row, in every menu (the debug
  menu's FIGHT SPEED ran under a long label) and in the game's own panel.
- **Pause menu** with a pad or the keys (the on-screen controls hidden):
  MOVES LIST, BUTTON SETUP, AUDIO SETTINGS and VIDEO SETTINGS, in place of
  the layout and the two volume lines.
- **INFO AND PAUSE BUTTONS** (INTERFACE, `corner_buttons=`): hidden, a
  click on their corners does nothing; the pad and the keys still open
  them.
- **The moves list:** the fighter's portrait, name and side at the top;
  NEXT / BACK go to the other fighter's list; every column fits its width
  and height, in 4:3, 16:9 and 21:9.
- **Invites:** SENT until the launcher has sent the invite, then INVITE
  again, so it can be sent once more.
- **STAGE SELECT**'s list has a scroll bar.

- **Online rollback was off one boot in two since 0.2.0** ("arena full ...
  save states off" in the log, then "NO SNAPSHOTS"): the textures read ahead
  at boot went into the save-state arena with the game's memory. They take
  the normal heap now. Six boots in a row: none full.
- **An update could leave old compiled code behind**: the launcher kept an
  object when its source was not newer, and an update's sources keep the
  release's dates. A player's build then failed ("undefined symbol:
  rb_sound_ok"), or mixed two versions' code, which makes an online fight
  drift apart. An object is now kept only when its sources have the very
  date and size they had; a failed link rebuilds everything next time.
- **Online desyncs:** every machine now compiles the same floating point
  (no fused multiply-add; Linux no longer builds for its own CPU), so a
  Windows and a Linux player compute the same fight.
- **The settings menu does not open online** (Esc, the pad's combination,
  the replay player's button), as Mary asked.
- **An invite opens ONLINE again:** the launcher starts the game with
  `--screen 51`, which the game read as no screen.
- **The moves list in three columns on one page** (Mary's reference):
  SPECIAL MOVES, FINISHING MOVES and KOMBOS -- the fighter's kombos, a part
  of the game's tables the original never showed. The basic moves follow on
  the next three pages.
- The update check asks the repository by its current name.
- The settings menu fits a 16:9 screen again: with AUDIO it has seven
  categories, and the box ran off the bottom (Mary). The categories are
  closer together from seven on.

## 0.2.0 beta (2026-11-05) — performance, audio, devices, kombos

Published 2026-11-05 as the GitHub pre-release `v0.2.0-beta`
(`UMK3-PC-0.2.0-beta.zip`, `UMK3-PC-0.2.0-beta-linux-x64.tar.gz`).
Checked in scripted runs on Mary's PC (RTX 5070, Direct3D 12); the KOMBO
SLIDE option **tried by Mary**: works.

- **KOMBO SLIDE** (issue #114), the settings menu's GAME and the launcher's
  PLAY: off, a kombo no longer pushes the opponent away (neither the chain
  rule nor the hits' push from the second hit on). The original's by
  default; offline only (online and replays keep the original's).
- **AUDIO**: a page of its own in the game's settings menu and the
  launcher's GRAPHICS: MUSIC and EFFECTS AND VOICES, 0-100 %, as bars that
  take the mouse (a click or a drag), the arrows and the pad; and SOUND
  OUTPUT. The game's settings menu has GRAPHICS CARD too. The bars replace
  the debug menu's FIGHT SPEED text bar and the sharp-edge values.

- **Hits and fatalities no longer drop frames.** The session log was
  unbuffered: every line a write to the disk (~4 ms), and a hit prints
  several. It is now written once a frame (and before a crash report).
- **SMOOTH MODELS costs a fraction:** each model's subdivision becomes a
  table of weights, made once on a thread and checked against the full
  subdivision (largest difference 3e-7); a pose is then one multiply. 6-11 ms
  a frame became 3-4, and a new pose no longer stalls (36-57 ms before). A
  fight's fighters are all smoothed from its first frames (one short wait
  as it starts), never drawn plain first.
- A late frame no longer smooths the catch-up ticks nobody sees.
- **The boot is faster:** 7.7 s to the menus became 3.2 s. The sheets a boot
  took long over are listed in `umk3-preload.txt` and decoded on threads
  from the next boot's first moment.
- The game asks Windows 11 not to slow it in the background (EcoQoS).
- **5 / 6 buttons**: a layout chosen in OPTIONS or the pause menu stays
  (each loading screen put the launcher's back) and is saved in umk3.ini;
  the settings menu's LAYOUT takes at once in a fight too. Likely the Steam
  Deck report (#171): the fight played the 5-button layout.
- **The babality's baby stands on the floor** (Jax, Sindel, Sonya... floated
  by how much taller their arcade sprite was than Sub-Zero's).
- **GRAPHICS CARD and SOUND OUTPUT** in the launcher's GRAPHICS > VIDEO:
  automatic (the most powerful card; the system's output) or one by name.
  Direct3D 12 now picks the card with the most video memory, not the first
  the system lists; OpenGL asks laptop drivers for the discrete one.
- Debug menu: MODE (the menu's, ARCADE, TRAINING) and CPU DIFFICULTY (0-9)
  for START FIGHT.
- The slow-frame line in the log says where a frame went (the game's ticks,
  smoothing, sounds, loading).

## 0.1.23 beta (2026-10-31) — FRIENDS in the game's ONLINE menu

Published 2026-10-31 as the GitHub pre-release `v0.1.23-beta`
(`UMK3-PC-0.1.23-beta.zip`, `UMK3-PC-0.1.23-beta-linux-x64.tar.gz`).
**Tried by Mary** in her test folder: works.

- The game's ONLINE screen has FRIENDS (under COPY): the friends with their
  icon and name, INVITE beside each (the launcher sends it; the screen
  already waits), and JOIN for an invite (found at once). FIND BY CODE goes
  back. Needs a logged-in account.

## 0.1.22 beta (2026-10-31) — invites, friend requests, online fixes

Published 2026-10-31 as the GitHub pre-release `v0.1.22-beta`
(`UMK3-PC-0.1.22-beta.zip`, `UMK3-PC-0.1.22-beta-linux-x64.tar.gz`).
**Tried by Mary** in her test folder; not yet online with two players (her
friend was away), published at her request.

**Online**
- INVITE beside each friend (PROFILE > FRIENDS): your game opens on ONLINE
  and waits; the friend gets a notification and JOIN, and their game opens
  on ONLINE and finds you, no code typed. A game already open finds you
  once ONLINE is opened. Friends only; an invite lasts three minutes.
- The guest sees the host's STAGE SELECT (the stage it shows, as it moves)
  instead of a black screen, and waits as long as it is open.

**Launcher**
- A friend request gets a Windows notification (a click opens PROFILE >
  FRIENDS) and a red dot on PROFILE and FRIENDS while it waits.
- A player with an older launcher is found too: the server takes the
  profile's code and name from the synced progress.
- ACCOUNT: REMOVE EMAIL; SEE RECOVERY CODE opens it in a window of its own
  (an account made before asks the password once and gets a new code).

**Game**
- Online: the other player's attack voices were never heard (the frames
  that brought the late press ran again in silence). A frame run again now
  plays the sounds it did not play the first time.
- Online: a model mod's bone lengths and height (KEEP PROPORTIONS) made the
  two machines run different fights (78 desyncs in one fight). They are
  left out of an online fight, also on a fighter the menu had loaded; the
  model itself still shows.
- SMOOTH MODELS no longer freezes the fight's first frames (0.25 to 0.9 s):
  the sharp edges are set on the build's thread.
- A mod's model has its lighting baked once a session, not at every load.
- The launcher asks for its pictures and icons all at once at start.

## 0.1.21 beta (2026-10-31) — friends, live leaderboards

Published 2026-10-31 as the GitHub pre-release `v0.1.21-beta` (`UMK3-PC-0.1.21-beta.zip`, `UMK3-PC-0.1.21-beta-linux-x64.tar.gz`). **Played by Mary** in her test folder: works.

**Launcher**
- **FRIENDS**: find a player by the profile name (not the login) or by the
  profile code, send a request, accept or decline one, remove a friend.
  Accounts only.
- **PROFILE has three pages now**: PROFILE, ACCOUNT and FRIENDS. ACCOUNT is
  no longer in the main menu.
- The profile code belongs to the account: the same on every PC.
- **No two accounts share a profile name**, whatever the case. A name
  another player has is refused, in the launcher and when it was changed
  in the game.
- **Leaderboards in real time**: while the game runs the launcher sends its
  new scores and fetches everyone's every ten seconds, and the game reads
  the boards again when they change. A new profile name shows on every
  board at once.

**Game**
- A Survival or Shao Kahn run that used the debug menu's help (infinite
  health, frozen clock, another fight speed, win / lose a round, the direct
  fight keys, loading a state, jumping to a fight, screen or boss, forcing a
  finisher) is not posted to the leaderboards. Having the debug menu on, or
  its display options, is fine.

## 0.1.20 beta (2026-10-31) — the game's leaderboards

Published 2026-10-31 as the GitHub pre-release `v0.1.20-beta` (`UMK3-PC-0.1.20-beta.zip`, `UMK3-PC-0.1.20-beta-linux-x64.tar.gz`). **Played by Mary** in her test folder: works, gamepad included.

**Game**
- **LEADERBOARDS work again**, for accounts. Survival and the Shao Kahn
  mode post their scores when the results screen is left; the board shows
  the best of every account, under each player's profile name. A GUEST gets
  the game's own "no internet" instead.
- The board's frame spans the whole width on wide screens (as ACHIEVEMENTS),
  so the scores sit inside it, and with the on-screen controls off it pages
  with L1 / R1 and closes with circle, as ACHIEVEMENTS does.

**Launcher**
- Brings the boards down when PLAY is pressed and takes the game's new
  scores up when it closes (the game has no HTTPS of its own). Offline, the
  last boards stay and the scores wait for the next time.

## 0.1.19 beta (2026-10-30) — accounts, MULTIPLAYER over LAN and Radmin VPN

Published 2026-10-30 as the GitHub pre-release `v0.1.19-beta` (`UMK3-PC-0.1.19-beta.zip`, `UMK3-PC-0.1.19-beta-linux-x64.tar.gz`). **Played by Mary** in her test folder: works.

**Game**
- **PLAY > MULTIPLAYER works again**, on the local network alone (no Epic
  Online Services): the games with that screen open are listed by profile
  name, and choosing one starts the same fight as ONLINE. It reaches
  virtual LANs too (Radmin VPN, Hamachi, ZeroTier). ONLINE is unchanged.
  Port: the original listed players over Bluetooth.

**Launcher**
- **ACCOUNT.** Username and password; email optional. Making an account
  gives a recovery code, shown once (a new one each time it is used). The
  log-in screen warns not to use a personal email or a password used
  anywhere else. Without an account the player is a GUEST: the profile code
  for online play, and no leaderboards.
- **Progress in the cloud.** The profile (and its code) and the game's
  progress (save, tower, survival, achievements, stats, treasures) come
  down when PLAY is pressed and go up when the game closes; the controls and
  settings stay on each PC. Changed on both sides: the player chooses which
  stays, and the other is copied to `save\backup`.
- The server (accounts, progress, leaderboards) is a Cloudflare Worker kept
  in a private repository; only the client (`launcher/app/lib/account.js`)
  is here. Passwords and recovery codes are stored as salted PBKDF2 hashes,
  sealed with AES-GCM; emails are sealed too; each account reads only its
  own data. Leaderboards in the game are not hooked up yet.
- **The menu's nine lines** (ACCOUNT made the ninth) on a symmetric curve,
  QUIT in line again.

## 0.1.18 beta (2026-10-30) — Noob Saibot on the roster, EXPORT ALL textures, settings by category

Published 2026-10-30 as the GitHub pre-release `v0.1.18-beta` (`UMK3-PC-0.1.18-beta.zip`, `UMK3-PC-0.1.18-beta-linux-x64.tar.gz`). **Played by Mary** in her test folder: works.

**Game**
- **Noob Saibot can be chosen** (umk3.ini `noob_saibot=1`: the launcher's
  PLAY page, or GAME in the Esc menu). The game already marks him
  available but its grid has no cell for him; he takes the empty one right
  of Smoke. Offline only: never in an online fight, whatever is set. Port,
  not in the original.
- **The Esc menu's categories:** VIDEO, GRAPHICS, TEXTURES and GAME are now
  separate (they were all under GRAPHICS); GAME holds the interface and
  what changes the game.
- **Resolutions from smallest to largest** in the Esc menu and the
  launcher (umk3.ini keeps width and height, so nothing saved changes).

**Launcher**
- **MODS → TEXTURES: EXPORT ALL** exports every texture of the fighter or
  stage shown, and **EXPORT EVERY FIGHTER / STAGE** the whole group, each
  to its place in `exports\`.
- **GRAPHICS** has a page of its own beside VIDEO, TEXTURES and INTERFACE.
- **CHARACTER EDITOR:** the note now says it as it is: character changes
  (attacks, hitboxes, kombos, data) never apply online; online only models
  and textures count (runtime/mods.c already worked that way).

**Known problems closed** (Mary no longer sees them): the dropped fight
intro frame, the repeated death scream, the Motaro softlock, SCULPT's
"500 Internal Error"; duplicating a fighter is off the list.

## 0.1.17 beta (2026-10-30) — Kombos that share a start, KOMBOS in the CHARACTER EDITOR

Published 2026-10-30 as the GitHub pre-release `v0.1.17-beta` (`UMK3-PC-0.1.17-beta.zip`, `UMK3-PC-0.1.17-beta-linux-x64.tar.gz`). Both changes were **played by Mary** in her test folder: work.

**Game**
- **Kombos that start alike can now be finished either way.** Mileena's
  HK, HK, LK, HK and HK, HK, U+LK, HK share their first hits, and the game
  always took the first: the third hit's list holds LK twice and the
  original picks the first entry for the button pressed, without looking at
  the stick. Holding **up** on that hit now takes the second. The same in
  the five other lists of the game built that way (Kabal, Sonya twice,
  Stryker, Kano). Port, not in the original.

**Launcher**
- **CHARACTER EDITOR → KOMBOS** (new page): every kombo of the fighter, hit
  by hit, as the game's tables have it; each hit's button and strike can be
  changed into the mod.

## 0.1.16 beta (2026-10-30) — MOD HUB

Published 2026-10-30 as the GitHub pre-release `v0.1.16-beta` (`UMK3-PC-0.1.16-beta.zip`, `UMK3-PC-0.1.16-beta-linux-x64.tar.gz`). The MOD HUB was **played by Mary** in her test folder: works.

**Launcher**
- **MOD HUB** (new tab in MODS): lists the mods published in the companion
  repository [MaryNCRT/UMK3-IOS-Mod-Hub](https://github.com/MaryNCRT/UMK3-IOS-Mod-Hub)
  with their pictures, downloads one into `mods\<mod>\` (checked against
  its SHA-256, turned on at the top of the MOD MANAGER) and offers UPDATE
  when a newer version is published. A mod already in `mods\` (copied by
  hand) is recognised by its folder and compared file by file with the
  published zip; UPDATE then downloads only the files that differ, not the
  whole mod. The MODS tab row wraps in a small window instead of pushing
  the page past it (Mary's report). Only the repository's owner and
  collaborators can publish there. First mods: HD NINJAS, Sonya Blade MKX,
  Rain.

## 0.1.15 beta (2026-10-30) — STAGE VIEWER with Blender-style editing, shape keys, stage textures, select-screen music, 4:3 menus

Published 2026-10-30 as the GitHub pre-release `v0.1.15-beta` (`UMK3-PC-0.1.15-beta.zip`, `UMK3-PC-0.1.15-beta-linux-x64.tar.gz`). **Not yet played by Mary** as a whole; the items she reported are marked.

**Game**
- **STAGE SELECT** keeps the select screen's music playing without a break
  (no fade, no stop on the way), and its CANCEL goes back to the select
  screen instead of starting the fight (Mary's report). Online, the host's
  CANCEL still takes the game's own stage, since the guest waits on it.
  STAGE SELECT names and pictures all 16 stages apart (the Subway, Soul
  Chamber and Lair were named Street, Balcony and Kave).
- **4:3:** the ONLINE code box no longer runs off the screen, and the
  settings / Esc menu is sized to the 4:3 picture instead of the 16:9
  window around it (Mary's reports).
- **`repell_func`**, the rule that pushes two close fighters apart, now
  follows the binary when one is above the other (a juggle); checked
  against the binary's own code on 200,000 states. The slight slide in a
  kombo (issue #114) was checked too: it is the original game's rule.

**Launcher**
- **STAGE VIEWER** (new): a stage played as the engine plays it, with the
  effects its events fire, the game's blending, element picking,
  properties and isolate; exported with its animation and effects.
  Elements are edited as in Blender: G / R / S with handles, axis locks and
  typed values, I / Alt+I keyframes, REC, linear or bezier, a dope-sheet
  timeline, and a model editor with shape keys and proportional sculpting.
  Edits are kept per stage (`stage edits\`) and exported as GLB morph
  targets, FBX blend shapes and USD BlendShapes. A **TEXTURES** tab shows
  the stage's textures and replaces them into a mod.
- **REPLACE / EDIT MODEL:** shape keys for fighters (Basis plus keys,
  values keyed per animation frame, sculpting); the T-pose shown too.
- **The timeline** under both editors is Blender-like: transport buttons,
  frame field, a tick a frame, a Summary row, the playhead's frame.
- **CHARACTER VIEWER** can put a playing stage behind the fighter.
- **TARGET MOD** lists every installed mod (it showed only the one typed).
- Editor windows are no longer covered by the launcher's bar or the
  player card; the menu's description text shrinks to fit its box.

**Known problems:** see the [README](../README.md#known-problems-0115-beta).

## 0.1.14b beta (2026-10-30) — Liu Kang's endless scream, SMOOTH WEIGHTS, imported models lit

Not yet played by Mary. 0.1.14 plus:
- **Liu Kang's death scream no longer plays forever.** The Windows mixer
  kept a sound's place as 16.16 in 32 bits, which wraps past 65,536 frames
  (4.1 s); Lkdeath, Skdiemix and Vpdeath are longer. Now 64-bit.
- **Linux sound:** a sound played again starts over instead of doubling.
- **REPLACE MODEL:** SMOOTH WEIGHTS (Blender's Smooth Vertex Weights); GLB
  bone names read from the file (dots kept); kept-proportion models bound
  to bones along their own limbs; Blender's gizmo and G / R keys; FBX read
  as Blender reads it.
- **Imported models are shaded in the fight:** their `.lighting` bake is
  made at load, byte-exact with the game's own.

## 0.1.14 beta (2026-10-30) — REPLACE MODEL like Blender, corrective animation layers

Not yet played by Mary.
- **REPLACE MODEL:** bones moved and rotated with the mouse or G / R and
  X / Y / Z, LOCATION / ROTATION fields mirrored left / right, WEIGHT PAINT,
  Ctrl+Z / Ctrl+Y, reset to a clean T or A pose, Blender's camera (axis
  gizmo, numpad views, orthographic), the file's own rig by default, EDIT
  MODEL on the fighter's current model, a model put over an earlier one
  fitted to the game's rig, the mod's own animations in the preview,
  Mixamo / AccuRig / ValveBiped rigs, KEEP PROPORTIONS, a texture per
  material.
- **Model viewer:** tabs, derived models, layers, thumbnails, floor, solid
  and weight views, CORRECTIVE ANIMATION LAYERS, GLB export, a
  `mod.meta.xml` in every mod.
- **Replays** record and load the rival actually fought; the player can be
  hidden and resized.
- **STAGE SELECT** after the fighters, with a picture for every stage;
  VSYNC option.
- **Custom models:** no black fighters with a large model, no SMOOTH
  MODELS on them, no frames lost to their material parts.

## After 0.1.13: an update stopped by a leftover update\new (2026-10-29, released in 0.1.14)

Mary's update failed with "ENOTEMPTY: directory not empty, rmdir
...\update\new\UMK3-PC-0.1.11-beta\launcher\bin\resources". `unpack`
(`launcher/app/lib/update.js`) cleared `update\new` with one `fs.rmSync`.
An earlier update's copy was still there: apply.ps1 removes it with
`-ErrorAction SilentlyContinue`, and Windows would not let a file in it go
(the usual holder is an antivirus still reading what was just unpacked).
So the update threw before it began.

Now:
- `unpack` retries the removal for two seconds (`maxRetries`).
- What still will not go is left, and the update unpacks into
  `update\new-<time>` beside it.
- Every leftover `new*` is tried again at the next update.
- apply.ps1 / apply.sh remove the folder that update used.

Checks: a test case in `test/update.test.js` (a removal that throws
ENOTEMPTY, then the next update clearing both); launcher tests 14 pass.

The fix runs in the launcher doing the update, so it helps from the next
release on: a launcher of 0.1.13 or older still stops on a leftover. Until
then, close the launcher, delete the `update` folder by hand and update
again (or download the release zip).

## Release 0.1.13 beta (2026-10-29): replays, online delay by ping, Vulkan / Direct3D 12 stutters, 4 GB

Published as the GitHub pre-release `v0.1.13-beta`, packaged as 0.1.12 was:
`UMK3-PC-0.1.13-beta.zip` (`launcher/make_release.py` over `electron-builder
--win dir`) and `UMK3-PC-0.1.13-beta-linux-x64.tar.gz` (`electron-builder
--linux dir`, then `launcher/make_release_linux.py`). It carries the five
"After 0.1.12" sections below. Before publishing, the game was built from
the 1.2.59 .ipa in the unpacked Windows package. Known problems taken off at
Mary's word: Reptile's fatalities (it was not Reptile's alone) and the
Vulkan / Direct3D 12 stutters (fixed below). **Mary has not played this
build yet.**

## After 0.1.12: REPLAYS (2026-10-29, in 0.1.13)

Not yet played by Mary. Asked for so that a fight can be watched again to
find bugs and take logs of them (Melee Unlocked's replays as the model).

**Recording** (`rollback_probe.c`, `GameCode.c`). The probe's recorder
(`UMK3_REPLAY_RECORD`, which replayed a fight identically over 3038 ticks)
now records every fight but an online one, by default, to
`replays/recent/<date>_<p1>_vs_<p2>.umkr` beside the exe; only the newest 5
stay, and one shorter than a second is not kept. umk3.ini `replays=0` turns
it off. The file is the setup -- the fighters, the level, the mode, the ten
`Settings` words and training (`PortPractice`), 18 words now -- the two
seeds, then each tick's two engine words and its result's hashes. A replay
takes the recorded settings back for its one fight, since the buttons and
the difficulty change how it plays. While recording the frame step is pinned
to 1, as online: below 60 FPS the game slows instead of skipping ticks.
Online fights are not recorded yet (their ticks go through `rollback.c`).

**Playing** (`umk3-game --replay replays/<folder>/<file>`, a path inside the
exe's folder). The game goes straight into the fight, as `--fight` does, and
`rp_frame` steps it in `GameCodeMain`'s place. A panel along the bottom
(`debug_menu.c`, drawn like the launcher: violet glass, light rules, buttons
with a rim, play lit) holds the status line, a bar to click or drag, and
buttons: to the start, back 5 s, back a frame, play / pause, on a frame, on
5 s, slower / faster (x1/4 .. x4), and the settings menu; each names itself
when the mouse is over it. Keys: Enter or P play / pause, left / right 5 s
(paused, a frame; held, they repeat), up / down speed, M the start, Esc the
settings. Going back loads the nearest keyframe (a rollback snapshot,
`st_rb_*`, every 120 ticks or more, 47 at most) and runs the ticks between
silent and undrawn, 20 ms of them a frame. Pausing draws the next frame and
puts the state back. At the last tick the player stops. Each tick's result is
compared with the recorded one: a difference shows on the panel and goes to
the log (`replay: tick N DIFFERS`, then `replay: the end ... the same as the
recording` or `DIFFERS`). Windows only for going back and pausing: the
snapshots are Windows only.

**Launcher.** A REPLAYS line on the menu (the eight lines re-centred on the
ring) opens the page (`SecReplays.svelte`, `lib/replays.js`): the keep-the-
last-5 switch, the last fights and the kept ones, each with WATCH, KEEP FOR
GOOD (recent to `replays/saved/`), EXPORT… and DELETE, and IMPORT… and OPEN
FOLDER for the kept ones. `play` passes `--replay` to the game.

**The player's top corners** (asked after the first look at the plan). A
REPLAYS button at the top left opens a list of the last fights and the kept
ones (the newest 8), the one playing marked; picking another switches to it
in the same game: this replay ends (logged as `switched to another
replay`), the game leaves the fight as the debug menu's FIGHT does and the
`--fight` path starts the new one's fighters and level, behind a LOADING
screen with its name; the sticks rest and the keys and mouse do nothing on
the way (`rp_switching`). (The first version started a second copy of the
exe instead; Mary asked for no restart.) An X at
the top right closes the game. Nothing else is drawn across the top, so the
health bars stay in sight; the replay's name moved into the status line.

Checks: Windows build without warnings in the new code, Linux compile 84/84,
launcher tests pass, the page and the menu seen in launcher captures. The
panel and playback have not been seen in the game yet.

## After 0.1.12: online lag over the internet (2026-10-29, in 0.1.13)

Not yet played by Mary. She saw lag in an online fight against another PC.
No 0.1.12 online log was on hand; the last one (0.1.11, over EOS) showed, per
600 frames, 75 to 128 holds (a frame shown again while waiting for the other
side), rollbacks always at the deepest the window allowed (7), and the other
side 6 to 9 frames behind. The input delay was fixed at 2 frames, enough on a
local network, not over the internet. Changes (`rollback.c`, `mp_net.c`):

- Inputs go on the wire when they are taken (`mpnet_flush` ticks EOS), not at
  the next frame's poll: about a frame less each way.
- The input delay follows the round trip, measured from the other side's
  acknowledgements: once a second, one frame toward the delay that keeps
  rollbacks near 3 frames, between 2 and 6. Each side's delay is its own (the
  other only receives inputs by frame), so it can move mid-fight: frames it
  skips take the same input, a frame it overlaps drops the newer one.
  `UMK3_RB_DELAY` still fixes it.
- Rollback window 8 -> 12 frames (4 more snapshots, ~25 MB).
- The log's `rollback: frame ...` line and NET STATS show the ping, the delay
  and the longest rollback in ms.

Also seen in that 0.1.11 log, not touched: the second fight (this side player
2) logged a DESYNC every second. Built into Mary's 0.1.12 test folder; not
yet tried between two PCs.

## After 0.1.12: REAL-TIME SHADOWS' note on a row of its own (2026-10-29, in 0.1.13)

Mary saw the note beside REAL-TIME SHADOWS' value ("WITH VULKAN OR DIRECT3D
12") drawn over the label: the row shrinks its text to fit, down to a floor,
and this one was still too long. The note is now a dimmed row of its own under
the option, shown, not chosen (`settings_menu.c`): ONLY WITH VULKAN OR
DIRECT3D 12 on OpenGL, EXPERIMENTAL on the others. Seen right by Mary.

## After 0.1.12: the game's memory below 4 GB, limeMalloc's heap 2 GB (2026-10-29, in 0.1.13)

Not yet played by Mary.

The 64-bit build kept the game's heaps below 2 GB, because a `long` holding an
address was feared to sign-extend. That fear was checked two ways.

**The compiler.** Clang's `-Wint-to-pointer-cast` was run over every source
of the 64-bit build. It found 320 integer-to-pointer casts, and all of them
are from `uint32_t`, which zero-extends: `P32` is `__ptr32 __uptr`, and the
game's `uintptr_t` is `uint32_t`. None are from a signed type, and
`-Wint-conversion` found no implicit ones.

**Running it.** With `UMK3_HIGH_MEM=1`, a test switch in `savestate.c`'s
`reserve_low`, every region starts at 2 GB: the arena at 0x81000000 and the
heap at 0xC1000000. With all of the game's memory above 2 GB, the game:
- booted;
- fought;
- went through FINISH HIM, the continue screen, character select and the main
  menu (8,000 ticks);
- ran a 6,000-tick fight with a finisher request, with no fault.

What changed:
- `reserve_low` now searches below 4 GB, still first fit from the bottom. The
  game's memory sits where it was; only what it never used before can go past
  2 GB.
- limeMalloc's region (`lime_platform.c`) now tries 2048 MB first (was 768) and
  gets it: `lime_heap: 2048 MB`. It is a reservation, and pages are committed
  as they are used. A fight with SMOOTH MODELS uses about 386 MB in all.
- The save-state arena stays at 1024 MB.

Linux keeps `MAP_32BIT`, so it stays below 2 GB. It falls back through the
size list to what fits. Linux compile: 84/84.

## After 0.1.12: Vulkan / Direct3D 12 stutters, compiled shaders, Sub-Zero's strap (2026-10-29, in 0.1.13)

Not yet played by Mary.

**The stutters on Vulkan and Direct3D 12 (Mary's 180 Hz screen).** Each 60 Hz
tick was presented with a swap interval of one refresh (5.6 ms at 180 Hz).
The frame's work (10-12 ms) varies by a few ms, and a frame that ended just
past a refresh was shown one refresh later. The result was 22 ms, then 11 ms,
several times a second. That is what was measured on 0.1.12: 150-330 of
~2,400 frames late. Changing vsync made no difference, because the late
refresh came from where the frame ended, not from vsync.

Now, on a 120/180/240 Hz screen, each frame stays up 2/3/4 refreshes
(`rgl_swap_interval`, set in `game_main.c`'s `pace_frame`):
- Direct3D 12 presents with that sync interval.
- Vulkan's FIFO has no interval, so `rgl_vk.c` keeps a copy of the frame and
  presents it again once for each extra refresh.
- The loop is paced as on a 60 Hz screen.

Measured on Direct3D 12 and Vulkan at 180 Hz:
- Swaps hold at 16.66 ms with 0 late frames a second through a fight.
- A few frames are still late when the frame's work goes past 16.7 ms.
  `UMK3_LOG_PACE=1` now prints the late frames a second.

OpenGL is unchanged.

**Where a fight's frame time goes (measured, D3D12, SMOOTH MODELS 2).**
- `rgl_ff` (transform, index gather, hand-off): ~3 ms.
- Catmull-Clark of the two fighters: ~7 ms.
- With 1 / 4 / 8 threads, the intro had 64 / 38 / 23 frames over 13 ms, with
  smoothing averaging 12.4 / 9.9 / 9.7 ms. The pool stays at 4.
- Open: a fight intro still has frames of 15-19 ms of CPU work. The next
  step is to make the smoothing cheaper.

**COMPILE SHADERS (Khronos' pipeline_cache sample, Microsoft's
D3D12PipelineStateCache).**
- Every pipeline a backend makes is a line in `umk3-pipelines-vk.txt` /
  `umk3-pipelines-d3d12.txt` beside the game, and the driver's compiled
  pipelines are kept in `umk3-pipelines-vk.bin` (`VkPipelineCache`) /
  `umk3-pipelines-d3d12.bin` (`ID3D12PipelineLibrary`, names carrying the
  shaders' hash).
- When the backend opens, it makes them all again, from its own seed list
  (`render/rgl_pipes_*.h`: menus and two fights, 18 / 17 pipelines) and the
  file. This takes 1-7 ms, and no pipeline is compiled in the middle of a
  fight.
- `umk3-game --compile-shaders` does only that and exits.
- The launcher's PLAY has COMPILAR SHADERS ANTES DE JUGAR
  (`compile_shaders=1`), which runs it before the game.

**Sub-Zero's strap stretched to his fist (Mary, his intro, player two, with
SMOOTH MODELS).** The weld was measured on the first pose a skin is drawn in.
In Sub-Zero's intro his hand touches the strap there, and the two were welded
for good. `DrawSkinnedMesh2` (armv7 0x608d8) unwelds the skin by UV: each
record vertex is a copy of the skin vertex its corner names (SKININFO +0x18).
The weld now joins two vertices only when, besides being close, their skin
vertices have the same four bone bytes and weights (+0x20, +0x24):
`umk_subsurf_new_keyed`. For his mesh, 3 of 670 welds were refused, and the
strap stays on his back (captures at ticks 105, 108 and 159). With SMOOTH
MODELS off the strap was always right.

Checks:
- rgl_test: 0 failed on OpenGL, Vulkan and Direct3D 12.
- Linux compile: 84/84.
- Launcher tests: pass.

## Release 0.1.12 beta (2026-10-29): fatality lag, several cores, no vertex-buffer ceiling, shadows, flawless

**Scorpion's fatality lag (Mary: lag in that fatality only).** Measured
with a per-second log of the draws (Scorpion vs Sonya, practice, finisher 2):
in the late phase ~150 draws of 180,216 vertices a frame (the copies of
Scorpion, each a smoothed body expanded to one vertex per triangle corner),
~1 GB sent to the GPU, frames of 889-968 ms (651-679 ms in the scene's
draws). Two causes:
- SMOOTH MODELS' budget of smoothed triangles read `limeRenderedPolyCount`,
  which the game's tick clears; a slow frame runs several ticks between its
  draws, so the budget never filled (logged: 0 draws over it). Now its own
  count, cleared before each present (`port_smooth_frame`, game_main.c),
  250,000 triangles: the two fighters and a third body; copies past it are
  drawn as the model is.
- Direct3D 12 and Vulkan grew their vertex buffer up to 256 MB; past that,
  every overflow waited for the GPU and made a new 256 MB buffer, 3-4 times a
  frame. Now the buffer grows to what the frame needs with no ceiling, and the
  old one is kept until the next flush instead of waiting partway.
After: one 52 ms frame in the whole fatality, the rest under the slow-frame
threshold. rgl_test 0 failed on OpenGL, Vulkan and Direct3D 12; Linux
compile 84/84. Not yet played by Mary.

**Open: random stutters on Vulkan and Direct3D 12 (Mary, RTX 4060: OpenGL
smooth).** Measured on Direct3D 12 (Liu Kang vs Sonya, practice, 40 s, a
180 Hz screen): ~150-330 of ~2,400 frames take 20-23 ms instead of 16.7,
then one ~11 ms frame (the next tick is already due); they come about every
100 ms. Ruled out: vsync (`Present(0)` changed nothing, 289 vs 318), GPU
waits (2 a frame, 0.6 ms), textures rewritten mid-frame or read back (0).
The frame work before the present is ~10-11 ms on Direct3D 12 against
~5.5 ms on OpenGL (the `pace:` log), so the CPU side of rgl_ff (transforming
and expanding every vertex) is the next place to look. Not fixed.

**Several cores.** zig cc ships no OpenMP runtime, so Blender's subdivision
ran on one core: `src/thirdparty/blender/omp_lite.c` implements the
`__kmpc_*` calls (a pool of at most 4 threads, measured fastest; `UMK3_THREADS`
overrides), `CCG_OMP_LIMIT` lowered to 20000, rgl_ff's indexed expansion in
parallel, a ring of 8 recent poses per smoothed mesh.

**Engine memory (Mary asked for more).** The game keeps pointers in 32-bit
words and longs that sign-extend, as on iOS, so its two heaps must sit below
2 GB: the save states' arena reserves 1024 MB and limeMalloc's heap 768 MB
(read at start). A 1024 or 896 MB heap was tried and does not fit beside the
exe; that space is full. Going past 2 GB would mean every pointer read in the
decompiled code zero-extended instead, a change across the whole game.
Measured use is ~386 MB for the whole process, so the heap is not what
limits the frame rate. Everything else (textures, vertex buffers, smoothed
meshes, shadows) is ordinary 64-bit memory with no limit of the engine's.

**Also since 0.1.11:** REAL-TIME SHADOWS drawn the same frame with a steady
light box; Kung Lao's hat in the fight, not only the intro; round end as the
binary's two blocks (FLAWLESS VICTORY on match point, DrawHUD 0x2902c /
0x290a6); Linux keys mapped by scancode (not tested on Linux);
`UMK3_PRACTICE=1` runs a `--fight` as TRAINING.

---

## 0.1.11-beta (2026-10-23) — updates that finish, window resize, loads, profile picker, sharp edges

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary.

- Launcher updates: every Windows update since 0.1.6 ended as "could not be
  copied" (robocopy on folders no release has) and deleted `tools\eos\`;
  now only `src\`, `launcher\` and `toolsuild\` are mirrored, the rest
  copied, the downloaded archives removed.
- Window resize and move: Direct3D 12 and Vulkan keep working through resizes
  that fail or change the buffers; the render buffer is remade once the size
  settles, not every frame.
- Online over the internet: the EOS SDK's headers and libraries are in the
  repository (`tools/eos`), and the release scripts refuse to package without
  them and `eos.ini` (0.1.10b went out without them).
- The home screen ring's picture (`image.png`) comes with the launcher.
- Load times: texture uploads batched on Direct3D 12 and Vulkan; load screens
  draw half as often; the launcher's compile is incremental (13.9 s to 1.2 s
  with nothing changed).
- Long sessions: textures found by a hash, not a list walk that grew all
  session; a memory line in the log every five minutes.
- Character editor: Motaro's page no longer fails (a strike named three times).
- PROFILE: a gradient picker (two colours, an angle, presets) and the
  fighter's second outfit as the player's picture, in the launcher and the game.
- SMOOTH MODELS: automatic sharp edges, with two sliders (which edges, how
  much) in GRAPHICS and the model viewer, and in the game's own GRAPHICS.
- SMOOTH MODELS: Kung Lao's hat is no longer pierced by his head -- the skin
  under a piece on its bones gets no correction pass (game and viewer).
- The game's Esc menu: PROFILE's background is the launcher's gradient picker,
  drawn the same, with the mouse, the keys and the pad; OUTFIT 1 / 2.
- Launcher version 0.1.11-beta.

## 0.1.10b-beta (2026-10-23) — video settings, settings in two columns

Built for Windows (64 and 32 bits) and Linux. Mary tried the two-column
menus and the new box before the last two changes below; those are untested.
Mary has now run the Linux package on a Linux machine, and played the
Vulkan and Direct3D 12 renderers: all three work.

- **VIDEO SETTINGS** on the settings screen, in the empty top-right box above
  SOUND VOLUME: it opens the settings menu's GRAPHICS page in the game's own
  panel, as BUTTON SETUP does, with the on-screen controls on or off.
- The settings in **two columns** side by side: the Esc menu, the debug menu
  (F2) and the game's own panels. The columns break only at a section's
  heading, so a section never runs into the other column; a row whose label
  and value would overlap is drawn smaller.
- CONTROLS: the keys and the pad's buttons split into FIGHT and MENU, each
  in its own column.
- Launcher: its own version `0.1.10-beta.b` reads as 0.1.10b, so it no
  longer offers the 0.1.10b release as an update to itself (`lib/update.js`).

## 0.1.10-beta (2026-10-23) — Kombos, SMOOTH MODELS, GPL

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary at release; the Linux package is untested on a Linux machine.

- Kombos (issue #114): when a player's button matched any entry of a combo
  table but the first, the port went on with the FIRST entry's next move.
  `combo_scan_a11` put the list back at its start on a match; the binary
  keeps the entry that matched, and `t_comb2` takes the next move from it
  (armv7 0x32776..0x3278e). Likely the cause of Liu Kang's 7-hit kombo
  breaking with the opponent sliding away; not yet tried in the game.
- Fixed: `t_r_combo0` called `set_no_block` on a path where the binary skips
  it (armv7 0x45da8).
- **SMOOTH MODELS** (off, level 1 or 2): the fighters drawn through Blender's
  Weld and Subdivision Surface (Catmull-Clark), in the game (GRAPHICS > VIDEO,
  `smooth_models=` in `umk3.ini`) and the model viewer, which can also play
  animations smoothed. The code is Blender's (`src/thirdparty/blender`).
- **The project is now GPL-2.0-or-later**, because it contains Blender's code.
  Releases up to 0.1.9-beta stay MIT.
- Crash reports and the session log name the version, the system, the
  renderer, the GPU and the settings in use.
- Linux: sound and music (the Linux game was silent); music needs libmpg123.
- SMOOTH ANIMATIONS also on the select screen and in the intro.
- Faster drawing of large models with Vulkan and DirectX 12.
- Fixed: SMOOTH MODELS drew dark wedges and black spots on some fighters
  (Jax, Sektor, Cyrax, Kano, Kitana, Mileena, Jade, Liu Kang).
- MODS > EXPORT: stages with their animation (FBX, USD); every export in its
  own folder: Characters, Stages, Models, and textures in their fighter's or
  stage's folder, Interface or Effects.
- MODS > TEXTURES: replace or export any texture of a fighter, a stage, the
  interface and menus, or the effects; a mod keeps each in that place's
  folder.

- **SMOOTH ANIMATIONS** (off by default): the launcher's GRAPHICS > VIDEO, or
  Esc > GRAPHICS > VIDEO in the game (applied at once), `smooth_anims=1` in
  `umk3.ini`. A fighter's frames are held for several ticks and drawn as hard
  cuts; with this on, each new frame is reached by blending from the one
  before. Drawing only: the game logic, hit boxes and events are unchanged.
  It applies to every fighter: fights, the select screen and the intro.
- Fixed: the game crashed when a fighter took Kabal's projectile (the
  reaction asked for a sound before setting its number; armv7 0x46396).
- Fixed: `tools/decomp/progress.py` looked one folder above the repository
  and counted every module as 0%.

## 0.1.9-beta (2026-10-22) — Vulkan and Direct3D 12, Linux Fixes

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary at release; the Linux package is untested on a Linux machine.

- **Vulkan and Direct3D 12 renderers** beside OpenGL (`src/runtime/render/`): every GL call goes through `rgl_front.c`; with Vulkan or Direct3D 12 a fixed-function layer (`rgl_ff.c`) keeps OpenGL 1.1's state and hands transformed vertices to `rgl_vk.c` or `rgl_d3d12.c`. Chosen in GRAPHICS > VIDEO or the in-game menu (`renderer=` in umk3.ini); a renderer that cannot start falls back to OpenGL. `tests/rgl_test.c` checks all three against the same calls (22 checks, 64 and 32 bits).
- **Move list:** the fatality page drew the special moves' buttons (section 2 starts after section 1's rows in the shared table, as the binary reads it).
- **Linux:** the game reads `umk3.ini` and `res/` beside its executable; it no longer links against libGL (loaded at run time); `text.c` includes `<stdarg.h>`; `build-game.sh`, `install-shortcut.sh`, `README-linux.md`, a CI job compiling every source with the bundled headers only.
- **Build from a terminal:** `launcher/app/cli.js`, run by `build-game.sh` / `build-game.cmd` with the launcher's own Electron.
- **eos.ini.example** for internet play.

- **Language in the in-game settings menu**, changed at once (texts and menus).
- **Mod textures reach the game's smaller copies** (`_LITE`, `_LOW`): a replaced fighter skin now shows at IPHONE texture quality.
- **Launcher:** smaller text everywhere; MODS fills the window; the model viewer has its options beside the model, a full screen button, REFRESH FOLDERS, textures by fighter, REPLACE TEXTURE from any picture (PNG, JPEG, WebP, GIF, BMP, ICO, AVIF, SVG, TGA) with the LOW and LITE copies made automatically; covers from any picture.
- **Linux:** the link no longer takes a 32-bit `libGL.so.1` from `/usr/lib` (Fedora and its kin): 64-bit directories first, each library checked for its width.
- **Viewer:** Kung Lao's hat (and anything else riding a fighter's bones), with ACCESSORIES to hide it; Reptile and Noob Saibot no longer show Scorpion's texture; texture quality NORMAL / LOW / LITE; playback speed.
- **[GOALS.md](GOALS.md):** what works and what comes next.

---

## 0.1.8-beta (2026-10-22) — Mods, Character Editor, Blender Round Trip

Built for Windows (64 and 32 bits) and Linux; not yet played by Mary at release. The Linux package is untested.

- **MODS in the launcher:** a mod manager (`mods/<mod>/`, files named like the game's replace them; priority, on/off, categories, covers), a model viewer that shows the game with or without mods, a character editor (each fighter's strikes with damage, chip and a visual hitbox editor, specials and finishers with the missing ones marked, per-fighter data, the game's texts, the fighter's textures), and export to FBX, USD (USDA/USDC/USDZ) and OBJ with a corrected T-pose.
- **From Blender back to the game:** an FBX animation replaces one of a fighter's clips, an FBX model on the same rig becomes a new `.skin`, both written into a mod.
- **The game reads mods:** `src/runtime/mods.c` lays `mods/` over `res/` and applies each mod's `data.ini` over the fight tables (`umk3_logic_names`, a new registry at the end of `logic_tables.c`).
- **Motaro and Shao Kahn** picked as FIGHTER 1/2 in the debug menu no longer crash the game (button rows, animation tables and strike rows stop before the bosses).
- **Linux:** bundled GL/KHR/SDL2 headers (`GL/gl.h` not found), soname link fallback, and a self-update that takes the Linux `.tar.gz`.

---

## 0.1.6-beta (2026-10-22) — Internet Play, Debug Menu, Linux Package

Tested by Mary on Windows (64 and 32 bits). The Linux package is built but untested.

- **ONLINE over the internet:** Epic Online Services (`src/runtime/mp_eos.c`), loaded at run time from the SDK beside the exe; 64-bit only. The release ships the SDK in `tools/eos` and the launcher copies it next to the game. LAN play works without it.
- **Rollback netcode** for online fights (`src/runtime/rollback.c`); each player picks their fighter's colour online.
- **Debug menu (F2)** reorganised like the settings menu: FIGHT, MENUS, TOOLS; new infinite health, freeze clock, FPS and online numbers (all off in an online fight).
- **Mouse wheel** in the debug and settings menus; **alerts answer the pad**.
- **iPhone-style boxes on Linux:** `src/runtime/platform/ui_alert.c` is shared by both backends (GDI on Windows, stb_truetype on Linux).
- **64-bit white textures:** the texture cache's entries come from below 2 GB (`umk3_alloc_low`); the C heap sometimes sat above 4 GB.
- **Linux package:** `UMK3-PC-0.1.6-beta-linux-x64.tar.gz`, the Electron launcher for Linux; needs SDL2 and OpenGL. Untested.
- **Release tooling:** `launcher/make_release.py` copies all of `src/` and the EOS SDK, and builds the stub with zig when llvm-mingw is missing.

### Also in 0.1.6 — Mileena's Fatalities

- **Nail fatality:** `RenderAnimatedCharacter` (0x5c2f4) now poses the skeleton on frames that draw no skinned body, as the binary does. Mileena's frames 310..334 are baked meshes, so her BIG CUP (bone 24) was riding the opponent's skeleton instead of her hand.
- **Suck-kiss fatality:** the port's "hold the last pose" for frames missing from a fighter's frame list now applies only to Classic Sub-Zero, the case it was written for. Held for everyone, it kept the victim on frame 327 after `set_inviso`, and the `MILEENA_SUK_<victim>` absorption replayed every eleven frames until the round ended.
- **Debug finisher on x64:** `opponent_dizzy` reads the engine's 32-bit words instead of 8-byte pointers (it crashed the scripted finisher test).

## 2026-10-18 — Repository Reorganization & Unified `.ipa` Build

- **Directory layout:** Separated source into `src/decomp/` (1:1 decompiled C) and `src/runtime/` (PC port layer), and tools into `tools/build/` (build-time `.ipa` & table extractors) and `tools/decomp/` (reverse engineering, Ghidra, `armrecomp`, `difftest`).
- **Single-command `.ipa` build (`build.py`):** Added `build.py` and `tools/build/unpack_ipa.py` + `CMakeLists.txt` (`-DUMK3_IPA=<path.ipa>`) to validate the 1.2.59 `.ipa`, unpack `res/`, extract the 229 data tables, and compile `umk3-game` and `umk3-fight-headless`.
- **Removed dead code:** Removed unused legacy prototype executables (`umk3-menu`, `umk3-fight`, `umk3-test`), old Win32 `launcher.c`, experimental `web/` wrapper, and one-off scaffolding scripts.

## 0.1.5-alpha (2026-10-16) — Online Update

- **PLAY > ONLINE:** Transcribed `limeMPGameSpecific.c` (48 functions, 14 packet types, 120-entry sprite-list queue) and added `src/runtime/mp_net.c` (UDP + LAN discovery) and `FE_Task_Online` (FE task 51).
- **Fixes:** Fixed `UpdateArcadeCode` guest/host `GameObjects` pointer, `Task_GameInit` online task transition (`Task_MultiplayerSync`), `dummyObjects` head pointer relink, and 64-bit `DrawAnimAsSprite` texture table width.

## 0.1.4-beta (2026-10-16) — 64-Bit Build & Arcade Crash Fix

- **64-bit build (`UMK3_LOW32`):** Added 32-bit pointer support (`__ptr32`) keeping game heaps and static data below 2 GB.
- **Fixes:** Fixed crash on winning Arcade / leaving stage 14 caused by non-NULL initial scene handles in `gamecode_globals.c`; added fallback animation frames for Classic Sub-Zero's elbow combo (`RPCOMBO6..9`).

## 0.1.3-beta (2026-10-16) — Controller Menu Navigation & Console Launcher

- **Menu navigation (`src/runtime/pad_nav.c`):** Full gamepad and keyboard navigation across all 51 front-end screens, two-player simultaneous character select, right-stick palette switching, and input prompt icons.
- **Settings & Launcher:** Added monitor selection (`monitor=`), player 2 pause/moves-list keys, and redesigned Electron launcher.

## 0.1.2-alpha (2026-10-15) — In-Game Settings & Player Profiles

- **In-game settings menu (`Esc`, `src/runtime/settings_menu.c`):** Live graphics reconfiguration (resolution, aspect ratio, texture quality) and live keyboard/gamepad rebinding without restarting.
- **Player profiles (`src/runtime/profiles.c`):** Persistent player ID, name, and customizable fighter badge (`profiles.ini`).

## 0.1.1-alpha (2026-10-15) — Player 2 Controls for Local Versus

- **Two-player controls:** Added Player 2 keyboard and gamepad bindings (`p2key_*`, `p2pad_*`) and dual-controller polling (`plat_pad2_read`).

## 0.1.0 / 0.1.0b-alpha (2026-10-15) — Widescreen, iPad Layout, HD Textures & Auto-Updater

- **Aspect ratios:** Added `3:2`, `4:3` (iPad camera projection), `16:9`, and `21:9` support.
- **iPad UI & DUELO mode:** Added `ui_size=ipad` (960×640 virtual layout) and local 2-player `DUELO` mode (`GameMode 6`).
- **Texture quality & fast loading:** Added `IPHONE`, `IPAD`, and `MAX` (4× Catmull-Rom upscale) texture modes, parallel PVRTC decoding (`src/runtime/lime/par.c`), and fast table-driven PNG inflate (`src/runtime/lime/png.c`).
- **Auto-updater:** Added GitHub release checker and in-place updater to the launcher.

## 0.0.8 / 0.0.9-alpha (2026-10-10 – 2026-10-15) — Gamepads, Save States, Hitboxes & All Finishers

- **Controls & zero input lag:** Added XInput/WinMM gamepad support, fixed hold-and-release special moves (`QueueAndJump`), and removed frame smoothing lag during fights.
- **Debug tools (`F2`):** Added hitbox viewer, fight speed control (10%–400%), and 4 in-memory save-state slots (`src/runtime/savestate.c`).
- **Finishers:** Fixed `MERCY` banner timing and verified all Fatalities, Animalities, Babalities, and Friendships.

## 0.0.1 – 0.0.7b-alpha (2026-10-08 – 2026-10-09) — Initial Playable Native Port

- **Full decompilation & integration:** Connected all 2,572 decompiled functions (`lime/`, `gamecode/`, `gamecode/logic/`) with the cooperative coroutine scheduler (`src/runtime/fight_runtime.c`) and the 229 extracted binary data tables (`tools/build/logic_tables.py`).
