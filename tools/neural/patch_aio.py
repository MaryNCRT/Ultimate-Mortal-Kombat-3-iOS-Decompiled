import sys
p = sys.argv[1]
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'

def rep(a, b, all=False):
    global s
    a2 = a.replace('\n', nl); b2 = b.replace('\n', nl)
    if b2 in s:
        return
    assert s.count(a2) == 1 or (all and s.count(a2) > 1), a
    s = s.replace(a2, b2) if all else s.replace(a2, b2, 1)

# 1. the switch, read from ReShade.ini [Standalone.DLSSNR] Umk3UiProtect
rep('static bool g_nr_rejection_mask_enabled = false;\n',
    'static bool g_nr_rejection_mask_enabled = false;\n'
    '// UMK3 PC: NVIDIA\'s UI correction -- the input colour\'s alpha marks the\n'
    '// game\'s interface (the game writes it), and those pixels are restored from\n'
    '// the input after NR, while NVIDIA\'s automatic mask stays in charge elsewhere.\n'
    'static bool g_umk3_ui_protect = false;\n')
rep('        read_setting("NrRejectionMask", "0", value, sizeof(value)); g_nr_rejection_mask_enabled = strcmp(value, "0") != 0;\n',
    '        read_setting("NrRejectionMask", "0", value, sizeof(value)); g_nr_rejection_mask_enabled = strcmp(value, "0") != 0;\n'
    '        read_setting("Umk3UiProtect", "0", value, sizeof(value)); g_umk3_ui_protect = strcmp(value, "0") != 0;\n'
    '        Log("UMK3 PC: UI protection (DLSSNR.UICorrection from the input alpha) %s", g_umk3_ui_protect ? "enabled" : "disabled");\n')
# 2. at creation
rep('    g_ngx_params->Set("DLSSNR.UICorrection", 0u);\n',
    '    g_ngx_params->Set("DLSSNR.UICorrection", g_umk3_ui_protect ? 1u : 0u);   // UMK3 PC\n', all=True)
# 3. at every evaluation: the input as the UI (its alpha the mask) and as the backbuffer (the pixels restored)
rep('    for (const char *name : {"Color", "DLSSNR.Color"}) g_ngx_params->Set(name, color);\n',
    '    for (const char *name : {"Color", "DLSSNR.Color"}) g_ngx_params->Set(name, color);\n'
    '    if (g_umk3_ui_protect)              // UMK3 PC: the interface restored from the input\n'
    '    {\n'
    '        g_ngx_params->Set("DLSSNR.UI", color);\n'
    '        g_ngx_params->Set("DLSSNR.Backbuffer", color);\n'
    '        g_ngx_params->Set("DLSSNR.UICorrection", 1u);\n'
    '    }\n')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched')

# 4. the final composition (the proxy compositor, after NR and DLSS/DLAA):
#    where the game's alpha says interface, the original pixel goes on top --
#    the HUD, texts and menus a layer over everything, never processed
rep('"Texture2D<float3> Neural:register(t0); Texture2D<float3> Original:register(t1);',
    '"Texture2D<float3> Neural:register(t0); Texture2D<float4> Original:register(t1);')
rep('float3 original=Original.Load(int3(op,0));',
    'float4 originalA=Original.Load(int3(op,0)); float3 original=originalA.rgb;')
rep('else if(Mode==1)result=neural;',
    'else if(Mode==1)result=(NoticePadding.x!=0u&&originalA.a>0.5)?original:neural;')
rep('    constants.mode = g_composite_reshade_output && composite_post ?\n',
    '    constants.notice_padding[0] = g_umk3_ui_protect ? 1u : 0u;   // UMK3 PC: the interface on top\n'
    '    constants.mode = g_composite_reshade_output && composite_post ?\n')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched (compositor)')

# 5. the game's scene cuts reset the history: UMK3 PC's exe exports a counter,
#    umk3_dlss5_cuts (render/rgl_ff.c), bumped on a new round, a load, a
#    camera jump; when it changes, NR and DLAA start again from this frame
rep('    const bool pending_history_reset = resume_staged_nvof ?\n',
    '    {   // UMK3 PC: the game\'s scene cuts (umk3_dlss5_cuts, exported by the exe)\n'
    '        static volatile long *umk3_cuts = nullptr;\n'
    '        static bool umk3_looked = false;\n'
    '        static long umk3_seen = 0;\n'
    '        if (!umk3_looked)\n'
    '        {\n'
    '            umk3_looked = true;\n'
    '            umk3_cuts = reinterpret_cast<volatile long *>(GetProcAddress(GetModuleHandleW(nullptr), "umk3_dlss5_cuts"));\n'
    '            if (umk3_cuts) umk3_seen = *umk3_cuts;\n'
    '            Log("UMK3 PC: scene cuts from the game %s", umk3_cuts ? "found" : "not found");\n'
    '        }\n'
    '        if (umk3_cuts && *umk3_cuts != umk3_seen)\n'
    '        {\n'
    '            umk3_seen = *umk3_cuts;\n'
    '            g_need_history_reset = true;\n'
    '        }\n'
    '    }\n'
    '    const bool pending_history_reset = resume_staged_nvof ?\n')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched (cuts)')

# 6. the start sooner: the AIO waits 15 s of a stable picture before it makes
#    NVIDIA's model (a 4-6 s Present). UMK3 PC opens its borderless window at
#    the monitor's size from the first frame, so 1.5 s (and the 30 stable
#    frames it still asks for) is enough -- the model is made while the game
#    starts, not in the menus or as the first fight begins (Mary)
rep('static constexpr ULONGLONG kInitialNativeSettleMs = 15000;\n',
    'static constexpr ULONGLONG kInitialNativeSettleMs = 1500;   // UMK3 PC: was 15000\n')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched (settle)')

# 7. after a scene cut, the picture as the game drew it while NR and DLAA
#    build their history again, then the neural result faded in: restarted,
#    they showed the fighters ghosted and the light easing for a second or
#    two as every fight began (Mary). 60 frames of the original, 30 of fade
#    (the compositor's spare constant, NoticePadding.y: 0 the original,
#    1000 the neural result); the interface stays the original throughout
rep('static bool g_umk3_ui_protect = false;\n',
    'static bool g_umk3_ui_protect = false;\n'
    'static unsigned g_umk3_since_cut = 100000u;   // UMK3 PC: frames since the game\'s last scene cut\n')
rep('else if(Mode==1)result=(NoticePadding.x!=0u&&originalA.a>0.5)?original:neural;',
    'else if(Mode==1)result=(NoticePadding.x!=0u&&originalA.a>0.5)?original:lerp(original,neural,NoticePadding.y/1000.0);')
rep('    constants.notice_padding[0] = g_umk3_ui_protect ? 1u : 0u;   // UMK3 PC: the interface on top\n',
    '    constants.notice_padding[0] = g_umk3_ui_protect ? 1u : 0u;   // UMK3 PC: the interface on top\n'
    '    {   // UMK3 PC: the original while the history refills after a cut, then faded in\n'
    '        const unsigned f = g_umk3_since_cut < 100000u ? g_umk3_since_cut++ : g_umk3_since_cut;\n'
    '        constants.notice_padding[1] = f < 60u ? 0u : f < 90u ? (f - 60u) * 1000u / 30u : 1000u;\n'
    '    }\n')
rep('            umk3_seen = *umk3_cuts;\n            g_need_history_reset = true;\n',
    '            umk3_seen = *umk3_cuts;\n            g_need_history_reset = true;\n'
    '            g_umk3_since_cut = 0;\n')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched (cut fade)')
