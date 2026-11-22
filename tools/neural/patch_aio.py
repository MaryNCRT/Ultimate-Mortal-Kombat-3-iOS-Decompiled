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
