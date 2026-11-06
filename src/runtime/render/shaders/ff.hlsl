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
    [loop] for (y = -2; y <= 2; y++)
        [loop] for (x = -2; x <= 2; x++) {
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
cbuffer Lights : register(b1) { float4 lb_eye; float4 lb_sky; float4 lb_ground;
    float4 lb_aoproj; float4 lb_aoparm; float4 lb_aovp; float4 lb_aoparm2; float4 lb_aodbg; int4 lb_count; Light lb_l[16]; };

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
    [loop] for (int y = -1; y <= 1; y++)
        [loop] for (int x = -1; x <= 1; x++) {
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
            att = w * w / (1.0 + 4.0 * x * x);   // softer: reaches the fighters as they near it (Mary)
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
    return albedo * dif * (fighter ? 0.9 : 1.0) + spc * smap * (fighter ? 0.15 : 0.5);
}


// AMBIENT OCCLUSION (rgl_ff.c ao_passes): the screen map is the scene's
// distance from the camera packed (mode 4 reads it), then the occlusion in
// R with the distance beside it in G, B (mode 5 reads it), then that
// blurred (the scene reads R). view_at: a texel back to the camera's space
// (x right, y up, z the distance ahead), from the projection's scale.
Texture2D scr : register(t3);

float3 view_at(float2 uv)
{
    float d = unpack_depth(scr.SampleLevel(spt, uv, 0)) * lb_aoproj.z;
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    return float3(ndc.x * d / lb_aoproj.x, ndc.y * d / lb_aoproj.y, d);
}

// the occlusion of a pixel, by the technique chosen (aoparm.w): 1 Crytek
// SSAO (a hemisphere of points about the normal, each occluded when the
// depth there is nearer than it), 2 HBAO (the horizon each way, how far it
// rises over the surface), 3 Alchemy AO (the obscurance of points on a
// disc). Written from the papers; the samples turned per pixel by
// interleaved gradient noise, the blur after takes the noise out.
float4 ao_pass(float2 fc)
{
    float sw_, sh_, sl_;
    scr.GetDimensions(0, sw_, sh_, sl_);
    float2 sz = float2(sw_, sh_);
    float2 uv = fc / sz;
    float3 P = view_at(uv);
    float3 N = cross(ddx(P), ddy(P));
    float far = lb_aoproj.z;
    float2 e = frac(min(P.z / far, 0.999) * float2(1.0, 255.0));
    e.x -= e.y / 255.0;
    if (P.z > far * 0.98 || dot(N, N) < 1e-20)
        return float4(1.0, e, 1.0);
    N = normalize(N);
    if (dot(N, -P) < 0.0)
        N = -N;
    float R = lb_aoproj.w;
    float rpx = R * lb_aoproj.x * 0.5 * sz.x / P.z;
    if (rpx < 1.0)
        return float4(1.0, e, 1.0);
    rpx = min(rpx, sz.x * 0.1);
    int n = clamp(int(lb_aoparm2.x + 0.5), 4, 32);
    float noise = frac(52.9829189 * frac(dot(fc, float2(0.06711056, 0.00583715))));
    float bias = lb_aoparm.y, k = lb_aoparm.x;
    int tech = int(lb_aoparm.w + 0.5);
    float ao = 1.0, occ = 0.0;
    if (tech == 3) {
        [loop] for (int i = 0; i < 32; i++) {
            if (i >= n)
                break;
            float t = (float(i) + 0.5) / float(n);
            float a = float(i) * 2.39996 + noise * 6.28318;
            float3 Q = view_at(uv + float2(cos(a), sin(a)) * rpx * sqrt(t) / sz);
            float3 v = Q - P;
            float vv = dot(v, v);
            if (vv < 4.0 * R * R)
                occ += max(0.0, dot(v, N) - bias * P.z) / (vv + 0.01 * R * R);
        }
        ao = 1.0 - 2.0 * k * R * occ / float(n);
    } else if (tech == 2) {
        int dirs = clamp(n / 4, 4, 8);  // never under four: two left most pixels blind (Mary saw dots)
        [loop] for (int d = 0; d < 8; d++) {
            if (d >= dirs)
                break;
            float a = (float(d) + noise) * 6.28318 / float(dirs);
            float2 dir = float2(cos(a), sin(a));
            float hmax = 0.0;
            [loop] for (int s = 1; s <= 4; s++) {
                float3 v = view_at(uv + dir * rpx * (float(s) - noise * 0.5) / 4.0 / sz) - P;
                float l = length(v);
                if (l < 1e-4 || l > R)
                    continue;
                hmax = max(hmax, (dot(v, N) / l - bias) * (1.0 - l * l / (R * R)));
            }
            occ += hmax;
        }
        ao = 1.0 - k * occ / float(dirs);
    } else {
        float3 T = normalize(abs(N.z) < 0.9 ? cross(N, float3(0.0, 0.0, 1.0)) : cross(N, float3(1.0, 0.0, 0.0)));
        float3 Bt = cross(N, T);
        [loop] for (int i = 0; i < 32; i++) {
            if (i >= n)
                break;
            float t = (float(i) + 0.5) / float(n);
            float a = float(i) * 2.39996 + noise * 6.28318;
            float rr = sqrt(1.0 - t * t);
            float3 S = P + (T * (rr * cos(a)) + Bt * (rr * sin(a)) + N * t) * R * lerp(0.1, 1.0, t * t);
            if (S.z <= 0.01)
                continue;
            float2 su = float2(S.x * lb_aoproj.x / S.z * 0.5 + 0.5, 0.5 - S.y * lb_aoproj.y / S.z * 0.5);
            float dz = view_at(su).z;
            float range = clamp(R / max(abs(P.z - dz), 1e-4), 0.0, 1.0);
            occ += (dz < S.z - bias * P.z ? 1.0 : 0.0) * range;
        }
        ao = 1.0 - k * occ / float(n);
    }
    ao = clamp(ao, 0.0, 1.0);
    ao = ao * ao;                       // the occluded darker, the open as they were
    return float4(ao, e, 1.0);
}

// the occlusion blurred 5x5, each texel weighed by how near its distance is
// to the centre's, so a fighter's dark edge does not bleed onto the wall
float4 ao_blur(float2 fc)
{
    float sw_, sh_, sl_;
    scr.GetDimensions(0, sw_, sh_, sl_);
    float2 sz = float2(sw_, sh_);
    float2 uv = fc / sz;
    float4 c0 = scr.SampleLevel(spt, uv, 0);
    float z0 = c0.g + c0.b / 255.0;
    if (lb_aoparm2.z < 0.5)
        return float4(c0.r, c0.g, c0.b, 1.0);
    float s = 0.0, w = 0.0;
    [loop] for (int y = -2; y <= 2; y++)
        [loop] for (int x = -2; x <= 2; x++) {
            float4 c = scr.SampleLevel(spt, uv + float2(float(x), float(y)) / sz, 0);
            float z = c.g + c.b / 255.0;
            float wt = max(0.0, 1.0 - abs(z - z0) / (0.03 * z0 + 1e-5));
            s += c.r * wt;
            w += wt;
        }
    return float4(s / max(w, 1e-4), c0.g, c0.b, 1.0);
}

// the scene's read: the blurred occlusion at this pixel, bilinear by hand
float ao_at(float2 fc)
{
    if (lb_aoparm2.w < 0.5)
        return 1.0;
    float2 uv = (fc - lb_aovp.xy) / lb_aovp.zw;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return 1.0;
    float sw_, sh_, sl_;
    scr.GetDimensions(0, sw_, sh_, sl_);
    float2 sz = float2(sw_, sh_);
    float2 t = uv * sz - 0.5;
    float2 f = frac(t);
    float2 b = (floor(t) + 0.5) / sz;
    float a00 = scr.SampleLevel(spt, b, 0).r;
    float a10 = scr.SampleLevel(spt, b + float2(1.0 / sz.x, 0), 0.0).r;
    float a01 = scr.SampleLevel(spt, b + float2(0.0, 1.0 / sz.y), 0).r;
    float a11 = scr.SampleLevel(spt, b + 1.0 / sz, 0).r;
    return lerp(lerp(a00, a10, f.x), lerp(a01, a11, f.x), f.y);
}


// AMBIENT OCCLUSION's debug views (rgl_ff.c rgl_ao_debug): the screen map
// at this pixel as grey, or the depth's normals as colour
float3 ao_debug_view(float2 fc, float contrib)
{
    int m = int(lb_aodbg.x + 0.5);
    float2 uv = (fc - lb_aovp.xy) / lb_aovp.zw;
    if (m == 5)
        return contrib.xxx;
    if (m == 3) {                       // the distance, 0-30 units, near white
        float d = unpack_depth(scr.SampleLevel(spt, uv, 0)) * lb_aoproj.z;
        return (1.0 - clamp(d / 30.0, 0.0, 1.0)).xxx;
    }
    if (m == 4) {
        float dw_, dh_, dl_;
        scr.GetDimensions(0, dw_, dh_, dl_);
        float2 px = 1.0 / float2(dw_, dh_);
        float3 P = view_at(uv), Px = view_at(uv + float2(px.x, 0.0)), Py = view_at(uv + float2(0.0, px.y));
        float3 n = normalize(cross(Px - P, Py - P));
        if (dot(n, -P) < 0.0)
            n = -n;
        return n * 0.5 + 0.5;
    }
    return ao_at(fc).xxx;                 // 1 after the blur, 2 before it
}

float4 ps(PSIn i) : SV_Target
{
    if ((shadow & 15) == 4)             // AMBIENT OCCLUSION's passes (rgl_ff.c)
        return ao_pass(i.pos.xy);
    if ((shadow & 15) == 5)
        return ao_blur(i.pos.xy);
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
    if (sm == 3)                        // AMBIENT OCCLUSION: the distance, packed
        return pack_depth(i.pos.w / lb_aoproj.z, 0.0);
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
                // AMBIENT OCCLUSION over everything, last (as ff.frag)
                float ao = max(lerp(1.0, ao_at(i.pos.xy), fighter ? lb_aoparm2.y : 1.0), lb_aoparm.z);
                lin *= ao;
                if (lb_aodbg.x > 0.5 && lb_aoparm2.w > 0.5)    // a debug view: that, alone
                    return float4(ao_debug_view(i.pos.xy, ao), 1.0);
            }
        }
        c.rgb = pow(lin, 1.0 / 2.2);
    }
    return c;
}
