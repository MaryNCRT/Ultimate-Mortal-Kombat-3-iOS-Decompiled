/*
 * gfx_preset.c -- GRAPHICS QUALITY (gfx_preset.h). Port, not in the binary.
 *
 *   ORIGINAL   the game as it draws: no ENHANCED LIGHTING, no occlusion,
 *              no contact shadows, no bloom, no tonemap
 *   ENHANCED   ENHANCED LIGHTING (shadows, relief, specular, the dynamic
 *              lights), AMBIENT OCCLUSION (HBAO), CONTACT SHADOWS, BLOOM
 *              and TONEMAP
 *   RTX, RTX + DLSS 5   planned (docs/research/GRAPHICS-OVERHAUL.md); not
 *              selectable until they exist
 *
 * The lighting parts draw with Vulkan and Direct3D 12; with OpenGL only the
 * bloom and the tonemap do (render/rgl_post.c), and the menus say so.
 *
 * The bloom and the tonemap are PHOTO MODE's own pass (render/rgl.h
 * rgl_post_apply, the shaders' post_fx) run every frame of a fight over
 * the scene, before the HUD is drawn, so the bars, the timer and the names
 * stay as the game drew them. Nothing here reads or writes the fight's
 * state: it is drawing only, and the same with any rollback or replay.
 */
#include <string.h>

#include "gfx_preset.h"
#include "photo_mode.h"
#include "render/rgl.h"

int gfx_bloom, gfx_tonemap;

#define ENH_BLOOM 30                    /* the bloom ENHANCED sets, percent */

static const char *const k_key[GFX_PRESETS] = { "original", "enhanced", "rtx", "rtx_dlss5", "custom" };

const char *gfx_preset_key(int p)
{
    return p >= 0 && p < GFX_PRESETS ? k_key[p] : "custom";
}

int gfx_preset_parse(const char *v)
{
    int i;
    for (i = 0; i < GFX_PRESETS; i++)
        if (strcmp(v, k_key[i]) == 0)
            return i;
    return -1;
}

int gfx_preset_selectable(int p)
{
    return p == GFX_ORIGINAL || p == GFX_ENHANCED;
}

int gfx_preset(void)
{
    /* the occlusion and the contact shadows are parts of ENHANCED LIGHTING
     * and draw nothing without it (ao=2 is umk3.ini's default) */
    if (!rgl_shadows && !gfx_bloom && !gfx_tonemap)
        return GFX_ORIGINAL;
    if (rgl_shadows && rgl_ao && rgl_contact_shadows && gfx_bloom == ENH_BLOOM && gfx_tonemap)
        return GFX_ENHANCED;
    return GFX_CUSTOM;
}

void gfx_preset_set(int p)
{
    if (p == GFX_ORIGINAL) {
        rgl_shadows = 0;
        rgl_ao = 0;
        rgl_contact_shadows = 0;
        gfx_bloom = 0;
        gfx_tonemap = 0;
    } else if (p == GFX_ENHANCED) {
        rgl_shadows = 1;
        rgl_ao = 2;                     /* HBAO, the menu's ON */
        rgl_contact_shadows = 1;
        gfx_bloom = ENH_BLOOM;
        gfx_tonemap = 1;
    }
}

void gfx_scene_post(void)
{
    float prm[RGL_POST_PARAMS];

    if ((!gfx_bloom && !gfx_tonemap) || photo_on() || photo_free_on() || !rgl_post_available())
        return;
    memset(prm, 0, sizeof prm);
    prm[2] = 1.0f;                      /* contrast */
    prm[3] = 1.0f;                      /* saturation */
    prm[6] = 1.0f;                      /* gamma */
    prm[13] = (float)gfx_bloom / 100.0f;
    prm[19] = gfx_tonemap ? 1.0f : 0.0f;
    rgl_post_apply(prm);
}
