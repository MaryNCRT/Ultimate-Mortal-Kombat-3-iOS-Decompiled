// The fixed function, for Direct3D 12 (rgl_d3d12.c): the same as ff.vert and
// ff.frag. Direct3D's y runs up as OpenGL's; its depth runs from 0 to w.
// ENHANCED LIGHTING's relief (bump) as ff.frag's.
struct VSIn { float4 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; };
struct PSIn { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; };
cbuffer PC : register(b0) { int mode; int afunc; float aref; int shadow; float bump; };
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

// ENHANCED LIGHTING: the texture as its own normal map. Its brightness is a
// height and the Sobel slopes about the texel tilt the normal (plus the
// slopes two mip levels down, at half, for the large shapes), at the mip
// level drawn, so a far wall's relief does not shimmer.
//
// All of it is in the texture's own space, lit from a light fixed there
// (from the top left, as an artist draws relief), not from the map's: the
// stage's meshes carry no normals, and one made from a triangle's flat face
// shaded each face on its own -- a seam at every edge between two (Mary
// saw them in a backdrop). What depends on the texture alone runs on
// across an edge as the texture does.
static const float3 RL = float3(-0.40, -0.45, 0.80);  // the light, (u, v, out of the surface)

float lum(float2 uv, float lod)
{
    return dot(tex.SampleLevel(smp, uv, lod).rgb, float3(0.299, 0.587, 0.114));
}

float2 slope(float2 uv, float lod)
{
    float w, hh, lv;
    tex.GetDimensions(0, w, hh, lv);
    float2 d = exp2(lod) / float2(w, hh);
    float tl = lum(uv + float2(-d.x, -d.y), lod), t = lum(uv + float2(0.0, -d.y), lod);
    float tr = lum(uv + float2(d.x, -d.y), lod), l = lum(uv + float2(-d.x, 0.0), lod);
    float r = lum(uv + float2(d.x, 0.0), lod), bl = lum(uv + float2(-d.x, d.y), lod);
    float b = lum(uv + float2(0.0, d.y), lod), br = lum(uv + float2(d.x, d.y), lod);
    return float2((tr + 2.0 * r + br) - (tl + 2.0 * l + bl),
                (bl + 2.0 * b + br) - (tl + 2.0 * t + tr)) * 0.125;
}

// the bumped normal (texture space), set by relief() for specular()
static float3 nb;

// what scales the colour: how much more or less the bumped normal faces the
// light than the flat surface does, so the game's own light stays as it
// was on the whole
float relief(float2 uv, float k)
{
    float lod = max(floor(tex.CalculateLevelOfDetail(smp, uv)), 0.0);
    float2 g = (slope(uv, lod) + 0.5 * slope(uv, lod + 2.0)) * k;
    float3 l = normalize(RL);
    nb = normalize(float3(-g, 1.0));
    return clamp(1.0 + 1.5 * (dot(nb, l) - l.z), 0.4, 1.6);
}

// ENHANCED LIGHTING's specular (Blinn-Phong, as the stackgl lighting
// walkthrough's Phong shading), on the relief: a glint where a bump turns
// towards the light and the viewer, beyond what the flat surface has (so a
// shiny texture is not made brighter all over). Its specular map is the
// texture's own, as the normal map: bright and grey is shiny (stone, metal,
// water), dark or strongly coloured is not (cloth, skin). The stage's the
// full amount, a fighter's a touch.
float specular(float3 col, bool fighter)
{
    float3 h = normalize(normalize(RL) + float3(0.0, 0.0, 1.0));
    float l = dot(col, float3(0.299, 0.587, 0.114));
    float sat = max(col.r, max(col.g, col.b)) - min(col.r, min(col.g, col.b));
    float map = smoothstep(0.25, 0.85, l) * (1.0 - 0.8 * saturate(sat * 2.0));
    float sh = fighter ? 32.0 : 20.0;
    // Blinn-Phong's normalisation, so a tight highlight is not a dimmer one
    float n = (sh + 8.0) / 25.13;
    float spec = (pow(max(dot(nb, h), 0.0), sh) - pow(h.z, sh)) * n;
    return max(spec, 0.0) * map * (fighter ? 0.04 : 0.22);
}

float4 ps(PSIn i) : SV_Target
{
    float4 c = i.col;
    int sm = shadow & 15;
    // the shadow map's coordinates and their slopes, before any discard
    float3 p = i.sh.w > 0.0 ? i.sh.xyz / i.sh.w : float3(2.0, 2.0, 2.0);
    float3 q = float3(p.x * 0.5, -p.y * 0.5, p.z * 0.5);
    float3 qx = ddx(q), qy = ddy(q);
    float rl = sm == 1 && bump > 0.0 ? relief(i.uv, bump) : 1.0;
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
    {
        float lt = light_at(p, qx, qy, shadow >> 4);
        if (bump > 0.0) {
            float3 lin = pow(max(c.rgb, 0.0), 2.2);
            float sp = specular(lin, (shadow >> 4) != 0);
            lin = lin * rl + sp * smoothstep(0.75, 1.0, lt);
            c.rgb = pow(lin, 1.0 / 2.2);
        }
        c.rgb *= lt;
    }
    return c;
}
