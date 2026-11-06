// The fixed function, for Direct3D 12 (rgl_d3d12.c): the same as ff.vert and
// ff.frag. Direct3D's y runs up as OpenGL's; its depth runs from 0 to w.
// ENHANCED LIGHTING's relief (bump) as ff.frag's.
struct VSIn { float4 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; float3 n : TEXCOORD2; float3 p : TEXCOORD3; };
struct PSIn { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; float3 n : TEXCOORD2; float3 p : TEXCOORD3; };
cbuffer PC : register(b0) { int mode; int afunc; float aref; int shadow; int lit; float bump; };
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
    o.n = i.n;
    o.p = i.p;
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


// DYNAMIC LIGHTS (rgl_ff.c, rgl.h rgl_light_block): point, spot and
// directional lights in world space, per pixel, added over the game's own
// light (its baked stage colours, the fighters' LightVert) rather than in
// place of it: a stage with none looks as it did. Blinn-Phong, as the
// lighting walkthrough's, in linear light; the attenuation smooth to 0 at a
// light's radius (a window over an inverse square, after Karis), a spot's
// edge a smoothstep between its two cones. The normal is the mesh's own
// where it sent one (the stage's .meshset, a fighter's skinning), else the
// face's from the screen derivatives, turned to the camera.
struct Light { float4 pos; float4 col; float4 dir; float4 cone; };
cbuffer Lights : register(b1) { float4 lb_eye; float4 lb_sky; float4 lb_ground; int4 lb_count; Light lb_l[16]; };

// DYNAMIC LIGHTS' shadows (rgl_ff.c point_shadows): a point light's six
// cube faces, each a tile of patlas -- three across, two rows a light --
// seen with a field of view of tan 1.05 and a near plane of 0.05, the
// texel the shadow map's (depth in RGB, the caster in A). The face is the
// one the point lies most along; its depth there back to a distance, and
// nine taps 1.5 texels apart kept inside the tile. A fighter does not
// shadow himself (self).
Texture2D patlas : register(t2);

float point_shadow(int slot, float3 D, float far, int self)
{
    float3 ad = abs(D), F, U;
    int fc;
    if (ad.x >= ad.y && ad.x >= ad.z)
        fc = D.x > 0.0 ? 0 : 1;
    else if (ad.y >= ad.z)
        fc = D.y > 0.0 ? 2 : 3;
    else
        fc = D.z > 0.0 ? 4 : 5;
    F = fc == 0 ? float3(1.0, 0.0, 0.0) : fc == 1 ? float3(-1.0, 0.0, 0.0)
      : fc == 2 ? float3(0.0, 1.0, 0.0) : fc == 3 ? float3(0.0, -1.0, 0.0)
      : fc == 4 ? float3(0.0, 0.0, 1.0) : float3(0.0, 0.0, -1.0);
    U = fc < 4 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    float3 R = cross(U, -F);
    float z = dot(D, F);
    if (z < 1e-4)
        return 1.0;
    float2 nd = float2(dot(D, R), dot(D, U)) / (z * 1.05);
    float sw, shh, slv;
    patlas.GetDimensions(0, sw, shh, slv);
    float2 sz = float2(sw, shh);
    float T = sz.x / 3.0;
    float2 o = float2(float(fc % 3), float(slot * 2 + fc / 3)) * T;
    float2 px = o + (nd * 0.5 + 0.5) * T;
    float n = 0.05, f = max(far, 0.1);
    float bias = 0.03 + 0.02 * z;
    float occ = 0.0;
    [unroll] for (int y = -1; y <= 1; y++)
        [unroll] for (int x = -1; x <= 1; x++) {
            float2 q = clamp(px + float2(float(x), float(y)) * 1.5, o + 1.0, o + T - 1.0);
            float4 e = patlas.SampleLevel(spt, float2(q.x / sz.x, 1.0 - q.y / sz.y), 0);
            float d = unpack_depth(e);
            int id = int(e.a * 255.0 + 0.5);
            if (d >= 0.999 || (self != 0 && id == self))
                continue;
            float zl = 2.0 * n * f / ((f + n) - (d * 2.0 - 1.0) * (f - n));
            if (zl < z - bias)
                occ += 1.0;
        }
    return 1.0 - occ / 9.0;
}

float3 dynamic(float3 P, float3 N, float3 albedo, float smap, bool fighter, int self)
{
    float3 V = normalize(lb_eye.xyz - P);
    float3 dif = float3(0.0, 0.0, 0.0);
    float3 spc = float3(0.0, 0.0, 0.0);
    float sh = fighter ? 32.0 : 20.0;
    int n = min(lb_count.x, 16);
    [loop] for (int i = 0; i < n; i++) {
        Light L = lb_l[i];
        float3 Ld;
        float att = 1.0;
        int type = int(L.dir.w + 0.5);
        if (type == 2)
            Ld = -normalize(L.dir.xyz);
        else {
            float3 d = L.pos.xyz - P;
            float dist = length(d), x = dist / max(L.pos.w, 1e-4);
            if (x >= 1.0)
                continue;
            Ld = d / max(dist, 1e-4);
            float w = saturate(1.0 - x * x * x * x);
            att = w * w / (1.0 + 16.0 * x * x);
            if (type == 1)
                att *= smoothstep(L.cone.y, L.cone.x, dot(-Ld, normalize(L.dir.xyz)));
            if (L.cone.z > 0.5)         // a light with shadows (point_shadows)
                att *= point_shadow(int(L.cone.z + 0.5) - 1, P - L.pos.xyz, L.pos.w, self);
        }
        float ndl = max(dot(N, Ld), 0.0);
        float3 c = L.col.rgb * L.col.w * att;
        dif += c * ndl;
        float3 H = normalize(Ld + V);
        spc += c * pow(max(dot(N, H), 0.0), sh) * (sh + 8.0) / 25.13 * ndl;
    }
    return albedo * dif * (fighter ? 0.6 : 1.0) + spc * smap * (fighter ? 0.15 : 0.5);
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
    float3 fn = cross(ddx(i.p), ddy(i.p));
    float4 tc = float4(1.0, 1.0, 1.0, 1.0);
    if (mode == 1) {
        tc = tex.Sample(smp, i.uv);
        c *= tc;
    } else if (mode == 2) {
        tc = tex.Sample(smp, i.uv);
        c = tc;
    }
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
        bool fighter = (shadow >> 4) != 0;
        // in linear light (gamma 2.2), as the walkthrough's: the relief and
        // the highlight are light added and taken away, and only there do
        // they add up as light does; the map's shadow, a shade as it was
        // tuned (lt in gamma), then the dynamic lights over it all
        float3 lin = pow(max(c.rgb, float3(0.0, 0.0, 0.0)), 2.2);
        if (bump > 0.0) {
            float sp = specular(lin, fighter);
            lin = lin * rl + sp * smoothstep(0.75, 1.0, lt);
        }
        lin *= pow(lt, 2.2);
        if (lit != 0) {
            float3 N = dot(i.n, i.n) > 0.25 ? normalize(i.n) : float3(0.0, 0.0, 0.0);
            if (dot(N, N) < 0.5 && dot(fn, fn) > 1e-20) {
                N = normalize(fn);
                if (dot(N, lb_eye.xyz - i.p) < 0.0)
                    N = -N;
            }
            if (dot(N, N) > 0.5) {
                // the texture's colour, the baked light only half in it
                float3 al = pow(tc.rgb, 2.2) * (mode == 2 ? float3(1.0, 1.0, 1.0) : i.col.rgb * 0.5 + 0.5);
                float sm2 = smoothstep(0.25, 0.85, dot(al, float3(0.299, 0.587, 0.114)));
                // the stage's sky (rgl_ff.c sky_build): a tint, its hue at
                // white's brightness, from above and less from below
                float up = N.z * 0.5 + 0.5;      // world Z is up
                float3 tint = lerp(lb_ground.rgb, lb_sky.rgb, up);
                float tk = lerp(lb_ground.w, lb_sky.w, up);   // the fighters as much as the stage (Mary)
                lin *= lerp(float3(1.0, 1.0, 1.0), tint, tk);
                // a dark stage's slight light on its fighters (rgl_ff.c FIGHTER_LIFT)
                if (fighter)
                    lin += al * lb_eye.w;
                lin += dynamic(i.p, N, al, sm2, fighter, shadow >> 4);
            }
        }
        c.rgb = pow(lin, 1.0 / 2.2);
    }
    return c;
}
