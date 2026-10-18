/*
 * lime/common — the engine's own global state.
 *
 * Every object here has a real symbol in the retail binary. They are gathered
 * into one translation unit because the original spread them across the .cpp
 * files that used them, and a decompilation that mirrors that would need each
 * file to both declare and define them — which is how you end up with two
 * definitions and a link error nobody can place.
 *
 * **These are engine data, not runtime data.** Anything the platform provides
 * (limeLoadFile, limeMalloc, the GL entry points) is deliberately NOT here;
 * those belong to runtime/ and are only declared in lime.h.
 *
 * Sizes and initial values are stated only where the disassembly gives them.
 * Where it does not, the object is defined at its natural size and left zeroed,
 * which is what __DATA,__common means anyway.
 */

#include "lime.h"


/* ---------------------------------------------------------------- events
 *
 * A fixed pool: 192 slots of 248 bytes, 47,616 bytes total. Confirmed three
 * ways — the allocator, FindEventOffsets stepping 0xf8, and LIME_UpdateEvents
 * ending at a sentinel of base + 0xb900 + 8, which is exactly 0xBA00 minus one
 * slot. Never grown, never reallocated.
 */
EVENT            SceneEvents[EVENT_SLOTS];

/* The scratch track LIME_PlayFBXAtPos overwrites on every call. Static in the
 * original too, which is what makes that function non-reentrant. */
SCENEEVENTTRACK  g_fbxScratchTrack;
limeMATRIX44     g_fbxScratchMatrix;

/* The master offsets: 80-byte rows, a 64-byte name then x, y, z (and a
 * spare word) -- LIME_LoadMasterEventOffsets 0xa45d4, AddNewID 0xa452c. */
char            *MasterEventOffsets;     /* 0x00177818 */
int              NumMasterEventOffsets;  /* 0x00177814 */
/* 0x00177830, a pointer to the literal at 0x0017781c. */
const char      *MasterOffsetsFilename = "MASTER.eventoffsets";

/* The scene cache. AddScene and LIME_GetSceneFromFilename both walk it through
 * SCENEINFO+0x90, so it is a singly linked list with this as its head. */
struct SCENEINFO *g_sceneList;


/* ------------------------------------------------------ transparent meshes
 *
 * 255 entries of 48 bytes. `_NumTranspMeshes` is the counter's name in the
 * symbol table, and ClearTranspMeshList resets it rather than touching the
 * array — the slots are overwritten in place on the next frame.
 *
 * Overflowing this hangs the game. See docs/GAME-BUGS.md.
 */
TRANSPMESH       g_transpMeshList[TRANSPMESH_MAX];
int              g_transpMeshCount;


/* ------------------------------------------------------------ debug overlay
 *
 * The window array LIME_InitDebugWindow walks and ClearDebugWindow indexes,
 * with -1 meaning "no window". Sliders occupy slots 10 through 15.
 */
/* The array itself, 0x3e windows at 0x00392024 in __common. The `ldr r1, [r3]`
 * in ClearDebugWindow reads the non-lazy pointer slot, not a variable; see
 * lime.h. */
DEBUGWINDOW      DebugWindows[DEBUG_WINDOWS];
int              DS_DebugWindowOn;

/* RenderDebugCube's lazily loaded scene, and the flag that gates it. */
int              g_debugEnabled;
struct SCENEINFO *g_debugCubeScene;


/* ---------------------------------------------------------------- lighting
 *
 * Two directional lights, monochrome, no ambient term. Held as bare float[3]
 * because NormaliseLDirs indexes them with [0], [1], [2].
 *
 * The initial values are not recovered — the game sets them per scene — so they
 * start zeroed. NormaliseLDirs divides by the length, and a zero-length vector
 * would produce a division by zero here where the original never sees one, so a
 * caller must set them before the first normalise.
 */
float            g_lightDir0[3];
float            g_lightDir1[3];
float            g_lightPower0, g_lightPower1;
float            g_lightExp0,   g_lightExp1;

/* Scales the lit value into a 0..255 grey byte. The literal sits in a pool this
 * pass did not resolve; 255.0f is the value that makes the surrounding clamp
 * (`s < 0 ? 0 : (unsigned char)s`) cover the full range, and it is marked here
 * as the assumption it is rather than presented as recovered. */
const float      LIGHT_SCALE = 255.0f;


/* --------------------------------------------------------------- skinning
 *
 * The binary's own names and extents (__DATA,__common, sized by the distance
 * to the next symbol). CreateMatrixPaletteForGeneratingMesh (0x60278) unpacks
 * two frames into DecompAnimFrames0/1 and Root_Trans0/1, blends them into the
 * 0 side, points MatrixSource0 at DecompAnimFrames0, zeroes MatrixSourceCount
 * and aims MatrixDst2 at MatrixPalette2; CreateMatrixPaletteRecurse2 (0x60048)
 * then consumes the cursors one bone at a time. DrawSkinnedMesh2 (0x608d8)
 * reads the palette and writes SkinnedVerts and RenderIndexes.
 */
SKINMATRIX43     MatrixPalette2[150];       /* 0x002c3f48, 7,200 bytes */
unsigned char    SkinnedVerts[360000];      /* 0x002c5b68, 24 bytes a vertex */
uint16_t         RenderIndexes[10000];      /* 0x0036a820, 20,000 bytes */
BONEANIMFRAME    DecompAnimFrames0[150];    /* 0x0036f640, 3,000 bytes */
BONEANIMFRAME    DecompAnimFrames1[150];    /* 0x003701f8, 3,000 bytes */
limeVECTOR3      Root_Trans0;               /* 0x00370db0 */
limeVECTOR3      Root_Trans1;               /* 0x00370dbc */
BONEANIMFRAME   *MatrixSource0;             /* 0x00370dc8 */
BONEANIMFRAME   *MatrixSource1;             /* 0x00370dcc */
long             MatrixSourceCount;         /* 0x00370dd0 */
SKINMATRIX43    *MatrixDst2;                /* 0x00370dd4 */

/* LIME_RenderMeshSingleIndexed (0x5e358): the colour scratch it hands
 * glColorPointer, and the int16 position scale it divides by. */
unsigned char    TempRGBS[32000];           /* 0x00298174 */
float            VertScale = 54.61333465576172f; /* 0x00171844, 0x425a740e */


/* ----------------------------------------------------------- full-bright
 *
 * The table IsTextureFullBright searches, and the flag that makes the parse
 * happen once. 4100 bytes in __DATA,__common — the distance to the next symbol
 * — which is exactly 4 + 64 * 64.
 */
FULLBRIGHTINFO   TheFullBrightInfo;
int              FullBrightLoaded;

/* CreateFadedLookupTable's [512][256] byte table and its one-time flag. */
int              HaveFadeTable;             /* 0x001715d0 */
uint8_t          ScaleTable[0x200 * 256];   /* 0x0029fe74, 0x20000 bytes */



/* Filled by CreateFadedRGBS and handed to glColorPointer in the same breath:
 * `_TempRGBS` (0x00298174, 32,000 bytes up to _ScaleTable -- 8,000 vertices
 * of RGBA, defined above), literal 0x5e7cc in LIME_RenderMeshSingle. The pointer used to be
 * left NULL, and the first lit mesh drawn wrote through it. It stays a
 * pointer so the differential tests can hand in their own buffer. */
uint8_t     *g_vertexColourScratch = TempRGBS;
/* `_StaticMeshAmbient` (0x002bfe74), CreateFadedRGBS's offset: the arena's
 * ambient light times 255, written by the gamecode at level load. */
float        StaticMeshAmbient[3];
