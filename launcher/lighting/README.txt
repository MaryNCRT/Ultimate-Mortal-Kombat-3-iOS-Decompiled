ENHANCED LIGHTING -- a stage's lighting file
============================================

lighting/<the stage's scene>.ini, read when the stage loads (ENHANCED
LIGHTING on, Vulkan or DirectX 12). The scene names:

  BALCONY_LEVEL_SCENE  BELLTOWER_LEVEL_SCENE  BRIDGE_LEVEL_SCENE
  CAVE_LEVEL_SCENE (KAHN'S KAVE)  GRAVEYARD_LEVEL_SCENE
  JADESDESERT_LEVEL_SCENE  LAIR_LEVEL_SCENE  NOOBSDORFEN_LEVEL_SCENE
  PIT_LEVEL_SCENE  ROOFTOP_LEVEL_SCENE  SCISLACBUSOREZ_LEVEL_SCENE
  SCISLAC_LEVEL_SCENE  SOULCHAMBER_LEVEL_SCENE  STREET_LEVEL_SCENE
  SUBWAY_LEVEL_SCENE  TEMPLE_LEVEL_SCENE  WATERFRONT_LEVEL_SCENE

Every key is optional; ";" starts a comment.

  sky_strength=0.45      the measured sky's tint, times this (0 none)
  sky_color=255,40,20    the sky's colour, in place of the measured one
  fighter_lift=0.15      the fighters' own light (0 none), in place of
                         the guess made on a dark stage
  glow_power=1.0         the glows' lights (fire, projectiles), times this
  relief=1.6             the stage's relief and specular, times this
  emissive=Object015     meshes that are light themselves (lava, fire,
                         neon, a lit window), by name, commas between:
                         drawn at their own brightness (no shadow, no
                         occlusion, no tint) and each a light of its
                         texture's colour. The game's own fullbright
                         meshes are so already.
  no_relief=Plane006     meshes drawn flat (no relief, no specular), by
                         name, commas between: a sky whose texture shows
                         lines under the relief

  [light]                a light of the stage's own (8 at most)
  type=point             point | spot | directional
  mesh=Object015         at the centre of that mesh of the stage, as it is
                         drawn (a moving one is followed); pos is then an
                         offset from it
  pos=0,0,0              else a place in the world. As a stage loads the
                         game's log lists its meshes and where they are:
                         "lighting: <stage> mesh <name> at x,y,z"
  color=255,120,40       0-255
  intensity=2.0
  radius=8               how far it reaches (a fighter is about 3 tall)
  dir=0,0,-1             where a spot or a directional light points
                         (z is up)
  cone=20,35             a spot's inner and outer angle, in degrees
  shadows=1              a point light that casts the fighters' shadows
                         ahead of the glows (two at most at once)
