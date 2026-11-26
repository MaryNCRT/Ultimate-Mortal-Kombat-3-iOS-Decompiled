// The fixed function's fragment stage, for Vulkan (rgl_vk.c): the texture
// environment (1 MODULATE, 2 REPLACE) and the alpha test (the GL compare
// enum, 0 off), as OpenGL 1.1 does them; and REAL-TIME SHADOWS (rgl_ff.c),
// as ff.hlsl: shadow (its low four bits) 1 darkens where the map has
// something nearer the light -- the bits above are the receiver's own caster
// -- and 2 writes the map's texel. ENHANCED LIGHTING's relief (bump above 0,
// rgl_ff.c tex_bump) only with 1.
#version 450
layout(location = 0) in vec4 v_col;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec4 v_sh;
layout(location = 3) in vec3 v_n;
layout(location = 4) in vec3 v_p;
layout(location = 0) out vec4 o;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D smap;
layout(push_constant) uniform PC { int mode; int afunc; float aref; int shadow; int lit; float bump; } pc;

// the map's texel: the depth nearest the light, 24 bits in RGB, and in A
// which caster put it there (0 the stage, a fighter from 1; rgl_ff.c)
vec4 pack_depth(float d, float id)
{
    vec3 e = fract(min(d, 0.999) * vec3(1.0, 255.0, 65025.0));
    e.xy -= e.yz * (1.0 / 255.0);
    return vec4(e, id / 255.0);
}

float unpack_depth(vec4 e)
{
    return dot(e.rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0));
}

// how much light reaches the point: 1 lit, down to 0.45 under a fighter and
// 0.65 under the stage (a shade each, the fighters' the darker), 25
// texels two apart averaged for a soft, blurred edge (PCF). Each tap is compared with the receiver's
// own plane carried to it (its depth's slope across the map, from the
// screen derivatives), so a surface does not shadow itself; a fighter's own
// texels are skipped (its pose in the map is a frame old); and the shadow
// fades out towards the map's edge, which follows the fighters.
float light_at(vec3 p, vec3 dx, vec3 dy, int self)
{
    vec2 uv = vec2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    float edge = min(min(uv.x, uv.y), min(1.0 - uv.x, 1.0 - uv.y));
    if (edge <= 0.0 || p.z < -1.0 || p.z > 1.0)
        return 1.0;
    float ref = p.z * 0.5 + 0.5;
    vec2 px = 1.0 / vec2(textureSize(smap, 0));
    vec2 g = vec2(0.0);
    float det = dx.x * dy.y - dx.y * dy.x;
    if (abs(det) > 1e-14)
        g = vec2(dy.y * dx.z - dx.y * dy.z, dx.x * dy.z - dy.x * dx.z) / det;
    g = clamp(g, vec2(-0.05), vec2(0.05));
    float bias = 0.0004 + dot(abs(g), px);
    float nf = 0.0, ns = 0.0;
    for (int y = -2; y <= 2; y++)
        for (int x = -2; x <= 2; x++) {
            vec2 o = vec2(x, y) * px * 2.0;
            vec4 e = textureLod(smap, uv + o, 0.0);
            float d = unpack_depth(e);
            int id = int(e.a * 255.0 + 0.5);
            if (d < 0.999 && (id == 0 || id != self) && d < ref + dot(g, o) - bias) {
                if (id == 0) ns += 1.0; else nf += 1.0;
            }
        }
    return 1.0 - (0.55 * nf + 0.35 * ns) / 25.0 * clamp(edge * 12.0, 0.0, 1.0);
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
const vec3 RL = vec3(-0.40, -0.45, 0.80);  // the light, (u, v, out of the surface)

float lum(vec2 uv, float lod)
{
    return dot(textureLod(tex, uv, lod).rgb, vec3(0.299, 0.587, 0.114));
}

vec2 slope(vec2 uv, float lod)
{
    vec2 d = exp2(lod) / vec2(textureSize(tex, 0));
    float tl = lum(uv + vec2(-d.x, -d.y), lod), t = lum(uv + vec2(0.0, -d.y), lod);
    float tr = lum(uv + vec2(d.x, -d.y), lod), l = lum(uv + vec2(-d.x, 0.0), lod);
    float r = lum(uv + vec2(d.x, 0.0), lod), bl = lum(uv + vec2(-d.x, d.y), lod);
    float b = lum(uv + vec2(0.0, d.y), lod), br = lum(uv + vec2(d.x, d.y), lod);
    return vec2((tr + 2.0 * r + br) - (tl + 2.0 * l + bl),
                (bl + 2.0 * b + br) - (tl + 2.0 * t + tr)) * 0.125;
}

// the bumped normal (texture space), set by relief() for specular()
vec3 nb;

// what scales the colour: how much more or less the bumped normal faces the
// light than the flat surface does, so the game's own light stays as it
// was on the whole
float relief(vec2 uv, float k)
{
    // between two mips, not rounded: the Waterfront's seam (ff.hlsl relief)
    float lf = max(textureQueryLod(tex, uv).x, 0.0);
    float lod = floor(lf), fr = lf - lod;
    vec2 g0 = slope(uv, lod) + 0.5 * slope(uv, lod + 2.0);
    vec2 g1 = slope(uv, lod + 1.0) + 0.5 * slope(uv, lod + 3.0);
    vec2 g = mix(g0, g1, fr) * k;
    vec3 l = normalize(RL);
    nb = normalize(vec3(-g, 1.0));
    return clamp(1.0 + 1.5 * (dot(nb, l) - l.z), 0.4, 1.6);
}

// ENHANCED LIGHTING's specular (Blinn-Phong, as the stackgl lighting
// walkthrough's Phong shading), on the relief: a glint where a bump turns
// towards the light and the viewer, beyond what the flat surface has (so a
// shiny texture is not made brighter all over). Its specular map is the
// texture's own, as the normal map: bright and grey is shiny (stone, metal,
// water), dark or strongly coloured is not (cloth, skin). The stage's the
// full amount, a fighter's a touch.
float specular(vec3 col, bool fighter)
{
    vec3 h = normalize(normalize(RL) + vec3(0.0, 0.0, 1.0));
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    float sat = max(col.r, max(col.g, col.b)) - min(col.r, min(col.g, col.b));
    float map = smoothstep(0.25, 0.85, l) * (1.0 - 0.8 * clamp(sat * 2.0, 0.0, 1.0));
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
struct Light { vec4 pos; vec4 col; vec4 dir; vec4 cone; };
layout(std140, set = 0, binding = 2) uniform Lights {
    vec4 eye; vec4 sky; vec4 ground;
    vec4 aoproj; vec4 aoparm; vec4 aovp; vec4 aoparm2; vec4 aodbg;
    ivec4 count; Light l[16];
} lb;

// DYNAMIC LIGHTS' shadows (rgl_ff.c point_shadows): a point light's six
// cube faces, each a tile of patlas -- three across, two rows a light --
// seen with a field of view of tan 1.05 and a near plane of 0.05, the
// texel the shadow map's (depth in RGB, the caster in A). The face is the
// one the point lies most along; its depth there back to a distance, and
// nine taps 1.5 texels apart kept inside the tile. A fighter does not
// shadow himself (self).
layout(set = 0, binding = 3) uniform sampler2D patlas;

float point_shadow(int slot, vec3 D, float far, int self)
{
    vec3 ad = abs(D), F, U;
    int fc;
    if (ad.x >= ad.y && ad.x >= ad.z)
        fc = D.x > 0.0 ? 0 : 1;
    else if (ad.y >= ad.z)
        fc = D.y > 0.0 ? 2 : 3;
    else
        fc = D.z > 0.0 ? 4 : 5;
    F = fc == 0 ? vec3(1.0, 0.0, 0.0) : fc == 1 ? vec3(-1.0, 0.0, 0.0)
      : fc == 2 ? vec3(0.0, 1.0, 0.0) : fc == 3 ? vec3(0.0, -1.0, 0.0)
      : fc == 4 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 0.0, -1.0);
    U = fc < 4 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 R = cross(U, -F);
    float z = dot(D, F);
    if (z < 1e-4)
        return 1.0;
    vec2 nd = vec2(dot(D, R), dot(D, U)) / (z * 1.05);
    vec2 sz = vec2(textureSize(patlas, 0));
    float T = sz.x / 3.0;
    vec2 o = vec2(float(fc % 3), float(slot * 2 + fc / 3)) * T;
    vec2 px = o + (nd * 0.5 + 0.5) * T;
    float n = 0.05, f = max(far, 0.1);
    float bias = 0.03 + 0.02 * z;
    float occ = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 q = clamp(px + vec2(float(x), float(y)) * 1.5, o + 1.0, o + T - 1.0);
            vec4 e = textureLod(patlas, vec2(q.x / sz.x, 1.0 - q.y / sz.y), 0.0);
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

vec3 dynamic(vec3 P, vec3 N, vec3 albedo, float smap, bool fighter, int self)
{
    vec3 V = normalize(lb.eye.xyz - P);
    vec3 dif = vec3(0.0);
    vec3 spc = vec3(0.0);
    float sh = fighter ? 32.0 : 20.0;
    int n = min(lb.count.x, 16);
    for (int i = 0; i < n; i++) {
        Light L = lb.l[i];
        vec3 Ld;
        float att = 1.0;
        int type = int(L.dir.w + 0.5);
        if (type == 2)
            Ld = -normalize(L.dir.xyz);
        else {
            vec3 d = L.pos.xyz - P;
            float dist = length(d), x = dist / max(L.pos.w, 1e-4);
            if (x >= 1.0)
                continue;
            Ld = d / max(dist, 1e-4);
            float w = clamp(1.0 - x * x * x * x, 0.0, 1.0);
            att = w * w / (1.0 + 4.0 * x * x);   // softer: reaches the fighters as they near it (Mary)
            if (type == 1)
                att *= smoothstep(L.cone.y, L.cone.x, dot(-Ld, normalize(L.dir.xyz)));
            if (L.cone.z > 0.5)         // a light with shadows (point_shadows)
                att *= point_shadow(int(L.cone.z + 0.5) - 1, P - L.pos.xyz, L.pos.w, self);
        }
        float ndl = max(dot(N, Ld), 0.0);
        vec3 c = L.col.rgb * L.col.w * att;
        dif += c * ndl;
        vec3 H = normalize(Ld + V);
        spc += c * pow(max(dot(N, H), 0.0), sh) * (sh + 8.0) / 25.13 * ndl;
    }
    return albedo * dif * (fighter ? 0.9 : 1.0) + spc * smap * (fighter ? 0.15 : 0.5);
}


// AMBIENT OCCLUSION (rgl_ff.c ao_passes): the screen map is the scene's
// distance from the camera packed (mode 4 reads it), then the occlusion in
// R with the distance beside it in G, B (mode 5 reads it), then that
// blurred (the scene reads R). view_at: a texel back to the camera's space
// (x right, y up, z the distance ahead), from the projection's scale.
layout(set = 0, binding = 4) uniform sampler2D scr;

vec3 view_at(vec2 uv)
{
    float d = unpack_depth(textureLod(scr, uv, 0.0)) * lb.aoproj.z;
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    return vec3(ndc.x * d / lb.aoproj.x, ndc.y * d / lb.aoproj.y, d);
}

// the occlusion of a pixel, by the technique chosen (aoparm.w): 1 Crytek
// SSAO (a hemisphere of points about the normal, each occluded when the
// depth there is nearer than it), 2 HBAO (the horizon each way, how far it
// rises over the surface), 3 Alchemy AO (the obscurance of points on a
// disc). Written from the papers; the samples turned per pixel by
// interleaved gradient noise, the blur after takes the noise out.
// a sample on the other layer: the centre a fighter's, the sample the
// stage's (A of the depth: a fighter's 1) -- a fighter is a layer of his
// own on top, darkened by himself and never by the stage (Mary)
bool off_layer(vec2 uv, float fl)
{
    return fl > 0.5 && textureLod(scr, uv, 0.0).a < 0.5;
}

vec4 ao_pass(vec2 fc)
{
    vec2 sz = vec2(textureSize(scr, 0));
    vec2 uv = fc / sz;
    vec3 P = view_at(uv);
    float fl = textureLod(scr, uv, 0.0).a;
    vec3 N = cross(dFdx(P), dFdy(P));
    float far = lb.aoproj.z;
    vec2 e = fract(min(P.z / far, 0.999) * vec2(1.0, 255.0));
    e.x -= e.y / 255.0;
    if (P.z > far * 0.98 || dot(N, N) < 1e-20)
        return vec4(1.0, e, fl);
    N = normalize(N);
    if (dot(N, -P) < 0.0)
        N = -N;
    float R = lb.aoproj.w;
    float rpx = R * lb.aoproj.x * 0.5 * sz.x / P.z;
    if (rpx < 1.0)
        return vec4(1.0, e, fl);
    rpx = min(rpx, sz.x * 0.1);
    int n = clamp(int(lb.aoparm2.x + 0.5), 4, 32);
    float noise = fract(52.9829189 * fract(dot(fc, vec2(0.06711056, 0.00583715))));
    float bias = lb.aoparm.y, k = lb.aoparm.x;
    int tech = int(lb.aoparm.w + 0.5);
    float ao = 1.0, occ = 0.0;
    if (tech == 3) {
        for (int i = 0; i < 32; i++) {
            if (i >= n)
                break;
            float t = (float(i) + 0.5) / float(n);
            float a = float(i) * 2.39996 + noise * 6.28318;
            vec2 qu = uv + vec2(cos(a), sin(a)) * rpx * sqrt(t) / sz;
            if (off_layer(qu, fl))
                continue;
            vec3 Q = view_at(qu);
            vec3 v = Q - P;
            float vv = dot(v, v);
            if (vv < 4.0 * R * R)
                occ += max(0.0, dot(v, N) - bias * P.z) / (vv + 0.01 * R * R);
        }
        ao = 1.0 - 2.0 * k * R * occ / float(n);
    } else if (tech == 2) {
        int dirs = clamp(n / 4, 4, 8);  // never under four: two left most pixels blind (Mary saw dots)
        for (int d = 0; d < 8; d++) {
            if (d >= dirs)
                break;
            float a = (float(d) + noise) * 6.28318 / float(dirs);
            vec2 dir = vec2(cos(a), sin(a));
            float hmax = 0.0;
            for (int s = 1; s <= 4; s++) {
                vec2 qu = uv + dir * rpx * (float(s) - noise * 0.5) / 4.0 / sz;
                if (off_layer(qu, fl))
                    continue;
                vec3 v = view_at(qu) - P;
                float l = length(v);
                if (l < 1e-4 || l > R)
                    continue;
                hmax = max(hmax, (dot(v, N) / l - bias) * (1.0 - l * l / (R * R)));
            }
            occ += hmax;
        }
        ao = 1.0 - k * occ / float(dirs);
    } else {
        vec3 T = normalize(abs(N.z) < 0.9 ? cross(N, vec3(0.0, 0.0, 1.0)) : cross(N, vec3(1.0, 0.0, 0.0)));
        vec3 Bt = cross(N, T);
        for (int i = 0; i < 32; i++) {
            if (i >= n)
                break;
            float t = (float(i) + 0.5) / float(n);
            float a = float(i) * 2.39996 + noise * 6.28318;
            float rr = sqrt(1.0 - t * t);
            vec3 S = P + (T * (rr * cos(a)) + Bt * (rr * sin(a)) + N * t) * R * mix(0.1, 1.0, t * t);
            if (S.z <= 0.01)
                continue;
            vec2 su = vec2(S.x * lb.aoproj.x / S.z * 0.5 + 0.5, 0.5 - S.y * lb.aoproj.y / S.z * 0.5);
            if (off_layer(su, fl))
                continue;
            float dz = view_at(su).z;
            float range = clamp(R / max(abs(P.z - dz), 1e-4), 0.0, 1.0);
            occ += (dz < S.z - bias * P.z ? 1.0 : 0.0) * range;
        }
        ao = 1.0 - k * occ / float(n);
    }
    ao = clamp(ao, 0.0, 1.0);
    ao = ao * ao;                       // the occluded darker, the open as they were
    return vec4(ao, e, fl);
}

// the occlusion blurred 5x5, each texel weighed by how near its distance is
// to the centre's, so a fighter's dark edge does not bleed onto the wall
vec4 ao_blur(vec2 fc)
{
    vec2 sz = vec2(textureSize(scr, 0));
    vec2 uv = fc / sz;
    vec4 c0 = textureLod(scr, uv, 0.0);
    float z0 = c0.g + c0.b / 255.0;
    if (lb.aoparm2.z < 0.5)
        return c0;
    float s = 0.0, w = 0.0;
    for (int y = -2; y <= 2; y++)
        for (int x = -2; x <= 2; x++) {
            vec4 c = textureLod(scr, uv + vec2(float(x), float(y)) / sz, 0.0);
            float z = c.g + c.b / 255.0;
            float wt = max(0.0, 1.0 - abs(z - z0) / (0.03 * z0 + 1e-5))
                     * (abs(c.a - c0.a) < 0.5 ? 1.0 : 0.0);   // each layer its own
            s += c.r * wt;
            w += wt;
        }
    return vec4(s / max(w, 1e-4), c0.g, c0.b, c0.a);
}

// the scene's read: the blurred occlusion at this pixel, bilinear by hand;
// a fighter's only from his own layer's texels (none: unoccluded)
float ao_at(vec2 fc, bool fighter)
{
    if (lb.aoparm2.w < 0.5)
        return 1.0;
    vec2 uv = (fc - lb.aovp.xy) / lb.aovp.zw;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return 1.0;
    vec2 sz = vec2(textureSize(scr, 0));
    vec2 t = uv * sz - 0.5;
    vec2 f = fract(t);
    vec2 b = (floor(t) + 0.5) / sz;
    vec4 a00 = textureLod(scr, b, 0.0);
    vec4 a10 = textureLod(scr, b + vec2(1.0 / sz.x, 0.0), 0.0);
    vec4 a01 = textureLod(scr, b + vec2(0.0, 1.0 / sz.y), 0.0);
    vec4 a11 = textureLod(scr, b + 1.0 / sz, 0.0);
    if (!fighter)
        return mix(mix(a00.r, a10.r, f.x), mix(a01.r, a11.r, f.x), f.y);
    vec4 wt = vec4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y)
            * step(0.5, vec4(a00.a, a10.a, a01.a, a11.a));
    float ws = wt.x + wt.y + wt.z + wt.w;
    return ws > 1e-4 ? dot(wt, vec4(a00.r, a10.r, a01.r, a11.r)) / ws : 1.0;
}


// CONTACT SHADOWS (rgl_ff.c ao_block, docs/research/CONTACT-SHADOWS.md): the
// small sharp shadows the 4096 map is too coarse for -- a foot on the floor,
// an arm on the torso. From this pixel's place in the camera's space, a
// march of 12 steps towards the map's light (aodbg.yzw) through the
// occlusion's distances (G, B of the blurred screen map); a step behind a
// surface nearer the camera, within a thickness, is in shadow, fading with
// how far along it was. The stage receives them, the fighters do not:
// marched over a fighter's own curved surface in the half-size map they
// came out as a stipple on the arms and the chest (Mary, 2026-11-22) --
// their shading is the shadow map's and the occlusion's. No per-pixel
// jitter (it was the stipple's pattern), the map's own texels about the
// start skipped, the bias a hundredth of the distance. Multiplied into the
// map's light only, never the dynamic lights. 1 lit.
float contact_at(vec2 fc, bool fighter)
{
    vec3 L = lb.aodbg.yzw;
    if (fighter || dot(L, L) < 0.5 || lb.aoparm2.w < 0.5)
        return 1.0;
    vec2 uv = (fc - lb.aovp.xy) / lb.aovp.zw;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return 1.0;
    float far = lb.aoproj.z;
    vec2 sz = vec2(textureSize(scr, 0));
    vec4 c0 = textureLod(scr, uv, 0.0);
    float d = (c0.g + c0.b / 255.0) * far;
    if (d > far * 0.98 || d < 1e-3)
        return 1.0;
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    vec3 P = vec3(ndc.x * d / lb.aoproj.x, ndc.y * d / lb.aoproj.y, d);
    float R = lb.aoproj.w;
    float len = R * 0.5, thick = R * 0.35 + 0.004 * d;
    P += L * (0.05 * R + 0.004 * d);
    for (int i = 0; i < 12; i++) {
        float t = (float(i) + 0.5) / 12.0;
        vec3 S = P + L * (len * t);
        if (S.z < 1e-2)
            break;
        vec2 su = vec2(S.x * lb.aoproj.x / S.z * 0.5 + 0.5, 0.5 - S.y * lb.aoproj.y / S.z * 0.5);
        if (su.x < 0.0 || su.y < 0.0 || su.x > 1.0 || su.y > 1.0)
            break;
        if (length((su - uv) * sz) < 2.0)
            continue;                   // its own texels: the map is coarser than the screen
        vec4 c = textureLod(scr, su, 0.0);
        if (fighter && c.a < 0.5)
            continue;
        float dz = S.z - (c.g + c.b / 255.0) * far;
        if (dz > 0.01 * S.z + 0.1 && dz < thick)
            return 1.0 - 0.5 * (1.0 - t * t);
    }
    return 1.0;
}

// AMBIENT OCCLUSION's debug views (rgl_ff.c rgl_ao_debug): the screen map
// at this pixel as grey, or the depth's normals as colour
vec3 ao_debug_view(vec2 fc, float contrib)
{
    int m = int(lb.aodbg.x + 0.5);
    vec2 uv = (fc - lb.aovp.xy) / lb.aovp.zw;
    if (m == 5)
        return vec3(contrib);
    if (m == 3) {                       // the distance, 0-30 units, near white
        float d = unpack_depth(textureLod(scr, uv, 0.0)) * lb.aoproj.z;
        return vec3(1.0 - clamp(d / 30.0, 0.0, 1.0));
    }
    if (m == 4) {
        vec2 px = 1.0 / vec2(textureSize(scr, 0));
        vec3 P = view_at(uv), Px = view_at(uv + vec2(px.x, 0.0)), Py = view_at(uv + vec2(0.0, px.y));
        vec3 n = normalize(cross(Px - P, Py - P));
        if (dot(n, -P) < 0.0)
            n = -n;
        return n * 0.5 + 0.5;
    }
    return vec3(ao_at(fc, false));             // 1 after the blur, 2 before it
}

// PHOTO MODE's effects (runtime/photo_mode.c, rgl_ff.c rgl_ff_post): a pass
// over the picture once the scene is drawn. scr is the picture, smap the
// scene's distance packed (the depth pass, when there is one: aoparm2.w),
// aovp where the game's area is in the target (x, y from the top, w, h),
// aoproj.z the distance the pack is in. The settings in l[0] and l[1]:
//   l0.pos   exposure (EV), brightness, contrast, saturation
//   l0.col   temperature, tint, gamma, sharpness
//   l0.dir   highlights, shadows, vignette, grain
//   l0.cone  chromatic aberration, bloom, filter, time (the grain's seed)
//   l1.pos   depth of field on, focus (x the centre's distance), aperture,
//            tonemap strength (GRAPHICS QUALITY, runtime/gfx_preset.c)
vec3 post_c(vec2 uv)
{
    return textureLod(scr, clamp(uv, vec2(0.0), vec2(1.0)), 0.0).rgb;
}

float post_d(vec2 uv)
{
    return unpack_depth(textureLod(smap, clamp(uv, vec2(0.0), vec2(1.0)), 0.0)) * lb.aoproj.z;
}

// GRAPHICS QUALITY's tonemap (l1.pos w, 0..1; runtime/gfx_preset.c): a
// shoulder on the picture's brightness in linear light. Below 0.6 nothing
// changes -- the game's dark stages keep every shade (an ACES curve with a
// toe crushed them to black) -- and from there to 1.4 the brightness eases
// into 1 (a quadratic whose slope meets 1 at the knee), so what the bloom
// pushed past white rolls off instead of clipping, and white itself comes
// down to 0.9. The colour is scaled by it, its hue kept. Mixed by strength.
vec3 tonemap(vec3 c, float s)
{
    vec3 lin = pow(max(c, vec3(0.0)), vec3(2.2));
    float L = dot(lin, vec3(0.2126, 0.7152, 0.0722));
    if (L <= 0.6)
        return c;
    float e = min((L - 0.6) / 0.8, 1.0);
    float L2 = 0.6 + 0.4 * e * (2.0 - e);
    vec3 t = pow(min(lin * (L2 / L), vec3(1.0)), vec3(1.0 / 2.2));
    return mix(c, t, s);
}

vec4 post_fx(vec2 fc)
{
    vec4 P0 = lb.l[0].pos, P1 = lb.l[0].col, P2 = lb.l[0].dir, P3 = lb.l[0].cone, P4 = lb.l[1].pos;
    vec2 uv = (fc - lb.aovp.xy) / lb.aovp.zw;
    vec2 px = 1.0 / lb.aovp.zw;
    const vec3 Y = vec3(0.299, 0.587, 0.114);
    vec2 dc = (uv - 0.5) * P3.x * 0.012;
    vec3 c = vec3(post_c(uv + dc).r, post_c(uv).g, post_c(uv - dc).b);
    if (P4.x > 0.5 && lb.aoparm2.w > 0.5) {      // depth of field
        float df = (post_d(vec2(0.5)) + post_d(vec2(0.47, 0.5)) + post_d(vec2(0.53, 0.5))
                  + post_d(vec2(0.5, 0.46)) + post_d(vec2(0.5, 0.54))) * 0.2 * P4.y;
        float coc = clamp(abs(post_d(uv) - df) / max(df, 1e-4) * P4.z, 0.0, 1.0) * 10.0;
        if (coc > 0.5) {
            vec3 acc = c;
            for (int i = 0; i < 24; i++) {
                float a = float(i) * 2.39996, r = sqrt((float(i) + 0.5) / 24.0) * coc;
                acc += post_c(uv + vec2(cos(a), sin(a)) * r * px);
            }
            c = acc / 25.0;
        }
    }
    if (P3.y > 0.0) {                            // bloom: the bright parts spread
        vec3 b = vec3(0.0);
        for (int i = 0; i < 16; i++) {
            float a = float(i) * 2.39996, r = (4.0 + float(i)) * 1.2;
            b += max(post_c(uv + vec2(cos(a), sin(a)) * r * px) - 0.6, 0.0);
        }
        c += b * (P3.y * 0.25);
    }
    if (P1.w > 0.0) {                            // sharpness: an unsharp mask
        vec3 n = post_c(uv + vec2(px.x, 0.0)) + post_c(uv - vec2(px.x, 0.0))
               + post_c(uv + vec2(0.0, px.y)) + post_c(uv - vec2(0.0, px.y));
        c += (c * 4.0 - n) * (P1.w * 0.25);
    }
    c *= exp2(P0.x);
    if (P4.w > 0.0) c = tonemap(c, P4.w);
    c *= vec3(1.0 + 0.25 * P1.x, 1.0 - 0.15 * P1.y, 1.0 - 0.25 * P1.x);
    float l = dot(c, Y);
    c += P2.y * 0.35 * (1.0 - smoothstep(0.0, 0.5, l));
    c += P2.x * 0.35 * smoothstep(0.5, 1.0, l);
    c += P0.y;
    c = (c - 0.5) * P0.z + 0.5;
    l = dot(c, Y);
    c = mix(vec3(l), c, P0.w);
    int fl = int(P3.z + 0.5);
    if (fl == 1) c = vec3(l);
    else if (fl == 2) c = vec3(l) * vec3(1.07, 0.86, 0.62) + vec3(0.06, 0.03, 0.0);
    else if (fl == 3) c *= vec3(0.85, 0.97, 1.15);
    else if (fl == 4) c *= vec3(1.15, 1.0, 0.82);
    else if (fl == 5) c = mix(c, vec3(c.r * 1.2, c.g * 0.55, c.b * 0.5), 0.85);
    else if (fl == 6) c = mix(c, vec3(c.r * 0.9 + 0.05, c.g * 0.75, c.b * 1.2 + 0.05), 0.8);
    else if (fl == 7) c = vec3(smoothstep(0.12, 0.88, l));
    else if (fl == 8) c = mix(c, vec3(l), 0.35) * vec3(1.05, 1.0, 0.88) + 0.04;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / max(P1.z, 0.1)));
    {
        vec2 q = (uv - 0.5) * vec2(1.0, lb.aovp.w / lb.aovp.z) * 1.6;
        c *= mix(1.0, smoothstep(1.05, 0.25, length(q)), P2.z);
    }
    c += (fract(sin(dot(fc + P3.w * 61.0, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * P2.w * 0.2;
    return vec4(clamp(c, 0.0, 1.0), 1.0);
}

void main()
{
    if ((pc.shadow & 15) == 6) {        // PHOTO MODE's effects
        o = post_fx(gl_FragCoord.xy);
        return;
    }
    if ((pc.shadow & 15) == 4) {        // AMBIENT OCCLUSION's passes (rgl_ff.c)
        o = ao_pass(gl_FragCoord.xy);
        return;
    }
    if ((pc.shadow & 15) == 5) {
        o = ao_blur(gl_FragCoord.xy);
        return;
    }
    vec4 c = v_col;
    int sm = pc.shadow & 15;
    /* the shadow map's coordinates and their slopes, taken before any
     * discard; y as the map's v runs, down */
    vec3 p = v_sh.w > 0.0 ? v_sh.xyz / v_sh.w : vec3(2.0);
    vec3 q = vec3(p.x * 0.5, -p.y * 0.5, p.z * 0.5);
    vec3 qx = dFdx(q), qy = dFdy(q);
    float rl = sm == 1 && pc.bump > 0.0 ? relief(v_uv, pc.bump) : 1.0;
    vec3 fn = cross(dFdx(v_p), dFdy(v_p));   // the face's normal, before any discard
    vec4 tc = vec4(1.0);
    if (pc.mode == 1) {
        tc = texture(tex, v_uv);
        c *= tc;
    } else if (pc.mode == 2) {
        tc = texture(tex, v_uv);
        c = tc;
    }
    if (pc.afunc != 0) {
        bool pass = true;
        if (pc.afunc == 0x200) pass = false;
        else if (pc.afunc == 0x201) pass = c.a < pc.aref;
        else if (pc.afunc == 0x202) pass = c.a == pc.aref;
        else if (pc.afunc == 0x203) pass = c.a <= pc.aref;
        else if (pc.afunc == 0x204) pass = c.a > pc.aref;
        else if (pc.afunc == 0x205) pass = c.a != pc.aref;
        else if (pc.afunc == 0x206) pass = c.a >= pc.aref;
        if (!pass)
            discard;
    }
    if (sm == 2)
        c = pack_depth(gl_FragCoord.z, v_sh.x);
    else if (sm == 3)                   // AMBIENT OCCLUSION: the distance, packed
        c = pack_depth((1.0 / gl_FragCoord.w) / lb.aoproj.z, (pc.shadow >> 4) != 0 ? 255.0 : 0.0);
    else if (sm == 1)
    {
        float lt = light_at(p, qx, qy, pc.shadow >> 4);
        bool fighter = (pc.shadow >> 4) != 0;
        lt = min(lt, contact_at(gl_FragCoord.xy, fighter));   // CONTACT SHADOWS
        // in linear light (gamma 2.2), as the walkthrough's: the relief and
        // the highlight are light added and taken away, and only there do
        // they add up as light does; the map's shadow, a shade as it was
        // tuned (lt in gamma), then the dynamic lights over it all
        vec3 lin = pow(max(c.rgb, vec3(0.0)), vec3(2.2));
        if (pc.bump > 0.0) {
            float sp = specular(lin, fighter);
            lin = lin * rl + sp * smoothstep(0.75, 1.0, lt);
        }
        lin *= pow(lt, 2.2);
        if (pc.lit != 0) {
            vec3 N = dot(v_n, v_n) > 0.25 ? normalize(v_n) : vec3(0.0);
            if (dot(N, N) < 0.5 && dot(fn, fn) > 1e-20) {
                N = normalize(fn);
                if (dot(N, lb.eye.xyz - v_p) < 0.0)
                    N = -N;
            }
            if (dot(N, N) > 0.5) {
                // the texture's colour, the baked light only half in it
                vec3 al = pow(tc.rgb, vec3(2.2)) * (pc.mode == 2 ? vec3(1.0) : v_col.rgb * 0.5 + 0.5);
                float sm2 = smoothstep(0.25, 0.85, dot(al, vec3(0.299, 0.587, 0.114)));
                // the stage's sky (rgl_ff.c sky_build): a tint, its hue at
                // white's brightness, from above and less from below
                float up = N.z * 0.5 + 0.5;      // world Z is up
                vec3 tint = mix(lb.ground.rgb, lb.sky.rgb, up);
                float tk = mix(lb.ground.w, lb.sky.w, up);   // the fighters as much as the stage (Mary)
                lin *= mix(vec3(1.0), tint, tk);
                // a dark stage's slight light on its fighters (rgl_ff.c FIGHTER_LIFT)
                if (fighter)
                    lin += al * lb.eye.w;
                lin += dynamic(v_p, N, al, sm2, fighter, pc.shadow >> 4);
                // AMBIENT OCCLUSION over everything, last (Mary: under the
                // other effects it hardly showed): a fighter's share of
                // it, never below the floor
                float ao = max(mix(1.0, ao_at(gl_FragCoord.xy, fighter), fighter ? lb.aoparm2.y : 1.0), lb.aoparm.z);
                lin *= ao;
                if (lb.aodbg.x > 0.5 && lb.aoparm2.w > 0.5) {   // a debug view: that, alone
                    o = vec4(ao_debug_view(gl_FragCoord.xy, ao), 1.0);
                    return;
                }
            }
        }
        c.rgb = pow(lin, vec3(1.0 / 2.2));
    }
    o = c;
}
