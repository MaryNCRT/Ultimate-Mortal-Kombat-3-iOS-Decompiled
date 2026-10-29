// The fixed function, for Direct3D 12 (rgl_d3d12.c): the same as ff.vert and
// ff.frag. Direct3D's y runs up as OpenGL's; its depth runs from 0 to w.
struct VSIn { float4 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; };
struct PSIn { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; };
cbuffer PC : register(b0) { int mode; int afunc; float aref; int shadow; };
Texture2D tex : register(t0);
Texture2D smap : register(t1);
SamplerState smp : register(s0);
SamplerState spt : register(s1);

PSIn vs(VSIn i)
{
    PSIn o;
    o.pos = float4(i.pos.x, i.pos.y, (i.pos.z + i.pos.w) * 0.5, i.pos.w);
    o.col = i.col;
    o.uv = i.uv;
    o.sh = i.sh;
    return o;
}

// REAL-TIME SHADOWS (rgl_ff.c), as ff.frag: the map's texel is the depth
// nearest the light, 24 bits in RGB, and in A which caster put it there (0
// the stage, a fighter from 1); cleared white, which reads past 1 --
// nothing there.
float4 pack_depth(float d, float id)
{
    float3 e = frac(min(d, 0.999) * float3(1.0, 255.0, 65025.0));
    e.xy -= e.yz * (1.0 / 255.0);
    return float4(e, id / 255.0);
}

float unpack_depth(float4 e)
{
    return dot(e.rgb, float3(1.0, 1.0 / 255.0, 1.0 / 65025.0));
}

// how much light reaches the point: 1 lit, down to 0.45 under a fighter
// and 0.65 under the stage (a shade each, the fighters' the darker), 25
// texels two apart averaged (blurred); each tap against the receiver's
// plane carried to it, a fighter's own texels skipped, faded out towards
// the map's edge
float light_at(float3 p, float3 dx, float3 dy, int self)
{
    float2 uv = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5), px, g = float2(0.0, 0.0);
    float edge = min(min(uv.x, uv.y), min(1.0 - uv.x, 1.0 - uv.y));
    float ref, w, h, det, bias, nf = 0.0, ns = 0.0;
    int x, y;

    if (edge <= 0.0 || p.z < -1.0 || p.z > 1.0)
        return 1.0;
    ref = p.z * 0.5 + 0.5;
    smap.GetDimensions(w, h);
    px = float2(1.0 / w, 1.0 / h);
    det = dx.x * dy.y - dx.y * dy.x;
    if (abs(det) > 1e-14)
        g = float2(dy.y * dx.z - dx.y * dy.z, dx.x * dy.z - dy.x * dx.z) / det;
    g = clamp(g, -0.05, 0.05);
    bias = 0.0004 + dot(abs(g), px);
    [unroll] for (y = -2; y <= 2; y++)
        [unroll] for (x = -2; x <= 2; x++) {
            float2 o = float2(x, y) * px * 2.0;
            float4 e = smap.SampleLevel(spt, uv + o, 0);
            float d = unpack_depth(e);
            int id = (int)(e.a * 255.0 + 0.5);
            if (d < 0.999 && (id == 0 || id != self) && d < ref + dot(g, o) - bias) {
                if (id == 0) ns += 1.0; else nf += 1.0;
            }
        }
    return 1.0 - (0.55 * nf + 0.35 * ns) / 25.0 * saturate(edge * 12.0);
}

float4 ps(PSIn i) : SV_Target
{
    float4 c = i.col;
    int sm = shadow & 15;
    // the shadow map's coordinates and their slopes, before any discard
    float3 p = i.sh.w > 0.0 ? i.sh.xyz / i.sh.w : float3(2.0, 2.0, 2.0);
    float3 q = float3(p.x * 0.5, -p.y * 0.5, p.z * 0.5);
    float3 qx = ddx(q), qy = ddy(q);
    if (mode == 1)
        c *= tex.Sample(smp, i.uv);
    else if (mode == 2)
        c = tex.Sample(smp, i.uv);
    if (afunc != 0) {
        bool ok = true;
        if (afunc == 0x200) ok = false;
        else if (afunc == 0x201) ok = c.a < aref;
        else if (afunc == 0x202) ok = c.a == aref;
        else if (afunc == 0x203) ok = c.a <= aref;
        else if (afunc == 0x204) ok = c.a > aref;
        else if (afunc == 0x205) ok = c.a != aref;
        else if (afunc == 0x206) ok = c.a >= aref;
        if (!ok)
            discard;
    }
    if (sm == 2)
        return pack_depth(i.pos.z, i.sh.x);
    if (sm == 1)
        c.rgb *= light_at(p, qx, qy, shadow >> 4);
    return c;
}
