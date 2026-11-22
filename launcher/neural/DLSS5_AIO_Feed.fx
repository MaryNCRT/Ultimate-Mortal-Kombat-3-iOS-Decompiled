/*
    DLSS5 ReShade AIO guide capture for the standalone OnPresent addon.

    Originally from DLSS5 ReShade AIO (https://github.com/kibblerz/DLSS5-Reshade-AIO),
    Copyright 2026 kibblerz, licensed under the Apache License, Version 2.0
    (graphics\dlss5\licenses\DLSS5-ReShade-AIO-LICENSE.txt).

    MODIFIED by UMK3 PC (2026-11-22; src/runtime/neural_mod.c, docs/DLSS5-MOD.md):
    the neural pass is told what to touch by the game itself. UMK3 PC draws,
    besides the colour, a layer code into the presented picture's alpha
    (src/runtime/render/rgl_ff.c, rgl_layers):

        0     the HUD, the menus and everything 2D  -> left as the game drew it
        0.5   a fighter                              -> UMK3_NR_FIGHTER of the pass
        1     the stage                              -> UMK3_NR_STAGE of the pass

    DLSS5_AIO_NRMask (one applies the neural pass, zero bypasses it) is made
    from that, and DLSS5_AIO_Mask (DLSS: prefer the current frame) is one on
    the HUD, so its letters keep no history. The texture names, formats and
    the technique are the original's: the addon finds them by name.
*/

#ifndef UMK3_NR_STAGE
    #define UMK3_NR_STAGE 1.0
#endif
#ifndef UMK3_NR_FIGHTER
    #define UMK3_NR_FIGHTER 1.0
#endif

texture2D MotVectTexVort
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RG16F;
};
sampler sMotVectTexVort
{
    Texture = MotVectTexVort;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
};

texture DLSS5_AIO_GameDepth : DEPTH;
sampler sDLSS5_AIO_GameDepth
{
    Texture = DLSS5_AIO_GameDepth;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
};

/* UMK3 PC: the presented picture, for its alpha (the game's layer code) */
texture UMK3_Picture : COLOR;
sampler sUMK3_Picture
{
    Texture = UMK3_Picture;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
};

texture DLSS5_AIO_MV
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RG16F;
};

texture DLSS5_AIO_Depth
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = R32F;
};

texture DLSS5_AIO_Mask
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = R8;
};

texture DLSS5_AIO_NRMask
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = R8;
};

uniform float DLSS5_AIO_NRMaskStrength
<
    hidden = true;
> = 1.0;

void DLSS5_AIO_FullscreenVS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
    texcoord = float2((id << 1) & 2, id & 2);
    position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

void DLSS5_AIO_CaptureGuides(float4 position : SV_Position, float2 texcoord : TEXCOORD,
    out float2 motion : SV_Target0, out float depth : SV_Target1, out float mask : SV_Target2,
    out float nr_mask : SV_Target3)
{
    // VORT publishes previous_uv = current_uv + motion. DLSS uses the same
    // direction but expects pixels rather than normalized UV units.
    float2 motion_uv = tex2Dlod(sMotVectTexVort, float4(texcoord, 0.0, 0.0)).xy;
    motion = motion_uv * float2(BUFFER_WIDTH, BUFFER_HEIGHT);
    // UMK3 PC: the depth the neural pass gets is the addon's own fallback,
    // flat and far (1.0, not reversed) -- what it is given without these
    // guides, where Mary saw the effect. ReShade's depth here is the wrong
    // buffer while the game antialiases (it cannot read a multisampled one),
    // and with it the 3D-guided pass all but vanished.
    float game_depth = tex2Dlod(sDLSS5_AIO_GameDepth, float4(texcoord, 0.0, 0.0)).x;
    depth = 1.0;

    float2 previous_uv = texcoord + motion_uv;
    float outside = any(previous_uv < 0.0) || any(previous_uv > 1.0) ? 1.0 : 0.0;
    float extreme_flow = smoothstep(48.0, 160.0, length(motion));
    float2 pixel = 1.0 / float2(BUFFER_WIDTH, BUFFER_HEIGHT);
    // UMK3 PC: no depth edges either (the same untrustworthy buffer)
    float depth_edge = 0.0 * game_depth * pixel.x;
    mask = saturate(max(outside, max(extreme_flow, depth_edge)));

    // UMK3 PC: the game's layer code. Edges of a fighter over the stage come
    // out of the multisample resolve as values between the two, and fade
    // from one strength to the other.
    float layer = tex2Dlod(sUMK3_Picture, float4(texcoord, 0.0, 0.0)).a;
    float hud = 1.0 - smoothstep(0.10, 0.30, layer);
    float fighter = saturate(1.0 - abs(layer - 0.5) * 4.0);
    float stage = smoothstep(0.70, 0.90, layer);
    float strength = saturate(fighter * UMK3_NR_FIGHTER + stage * UMK3_NR_STAGE);

    // DLSS SR: one prefers the current frame -- the HUD keeps no history.
    // Feature 18's ControlMask: one applies NR and zero bypasses it.
    mask = max(mask, hud);
    nr_mask = strength * (1.0 - hud);
}

technique DLSS5_AIO_Feed
<
    ui_label = "DLSS5 ReShade AIO guides (motion + game depth; UMK3 PC layers)";
    ui_tooltip = "Rendered manually at Present by the DLSS5 ReShade AIO addon. UMK3 PC: the game's layers (HUD, fighters, stage) drive the neural pass.";
>
{
    pass
    {
        VertexShader = DLSS5_AIO_FullscreenVS;
        PixelShader = DLSS5_AIO_CaptureGuides;
        RenderTarget0 = DLSS5_AIO_MV;
        RenderTarget1 = DLSS5_AIO_Depth;
        RenderTarget2 = DLSS5_AIO_Mask;
        RenderTarget3 = DLSS5_AIO_NRMask;
    }
}
