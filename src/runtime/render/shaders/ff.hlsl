// The fixed function, for Direct3D 12 (rgl_d3d12.c): the same as ff.vert and
// ff.frag. Direct3D's y runs up as OpenGL's; its depth runs from 0 to w.
// ENHANCED LIGHTING's relief (bump) as ff.frag's.
struct VSIn { float4 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; float3 n : TEXCOORD2; float3 p : TEXCOORD3; };
struct PSIn { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD0; float4 sh : TEXCOORD1; float3 n : TEXCOORD2; float3 p : TEXCOORD3; };
cbuffer PC : register(b0) { int mode; int afunc; float aref; int shadow; int lit; float bump; float layer; };
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
    // the level between two mips, not rounded: where a quad's two triangles
    // stretch the texture differently (the Waterfront's water) the rounded
    // level jumped at their diagonal and the relief with it -- the seam
    // Mary saw, with or without DLSS 5
    float lf = max(tex.CalculateLevelOfDetail(smp, uv), 0.0);
    float lod = floor(lf), fr = lf - lod;
    float2 g0 = slope(uv, lod) + 0.5 * slope(uv, lod + 2.0);
    float2 g1 = slope(uv, lod + 1.0) + 0.5 * slope(uv, lod + 3.0);
    float2 g = lerp(g0, g1, fr) * k;
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
    float4 lb_aoproj; float4 lb_aoparm; float4 lb_aovp; float4 lb_aoparm2; float4 lb_aodbg; int4 lb_count; Light lb_l[16];
    float4 lb_refl; float4 lb_rvp;      // RTX REFLECTIONS (rgl_ff.c refl_block)
    float4x4 lb_vp; float4 lb_tess; };  // RTX DISPLACEMENT: clip from world; on, scale, mid, mip

// DYNAMIC LIGHTS' shadows (rgl_ff.c point_shadows): a point light's six
// cube faces, each a tile of patlas -- three across, two rows a light --
// seen with a field of view of tan 1.05 and a near plane of 0.05, the
// texel the shadow map's (depth in RGB, the caster in A). The face is the
// one the point lies most along; its depth there back to a distance, and
// nine taps 1.5 texels apart kept inside the tile. A fighter does not
// shadow himself (self).
Texture2D patlas : register(t2);

#ifdef UMK3_RT
// RTX (graphics phase 3; built with dxc, shader model 6.5, rgl_d3d12.c): the
// scene's triangles in an acceleration structure (rgl_ff.c rt_commit), one
// instance a caster -- 0 the stage, a fighter from 1 -- traced inline from
// the pixel: the sun's shadows in place of the map's, every ENHANCED
// LIGHTING light's shadows, the ambient light's occlusion, and the lights'
// colour bounced once off what is near. A fighter's own body is passed
// over (its pose there is a frame old and its coarse mesh, as the map's).
RaytracingAccelerationStructure rt_scene : register(t4);
Texture2D rmap : register(t5);          // REFLECTIONS: the scene mirrored under the floor
SamplerState rsamp : register(s2);      // linear, clamped

// RTX HEIGHT and RELIEF: only near the fighters -- the far backdrops (a
// skyline, a sky) are painted pictures, and relief on them looked wrong
// (Mary); full to RT_NEAR world units from the eye, none past RT_FAR
#define RT_NEAR 25.0
#define RT_FAR 45.0
float rt_near(float3 P)
{
    return 1.0 - smoothstep(RT_NEAR, RT_FAR, length(P - lb_eye.xyz));
}

// RTX HEIGHT: parallax occlusion mapping on the stage's textures, the
// texture's brightness its height map (the game has none: made per pixel,
// Mary's "per-pixel"): the view ray marched down through it in RT_POM
// layers and the texture read where it meets the height -- Tatarchuk,
// "Practical Parallax Occlusion Mapping", ShaderX5 (2006). The uv's slopes
// across the surface from the screen derivatives, no tangents (the
// cotangent frame, Schueler, ShaderX5); the relief's depth RT_POM_DEPTH in
// world units, bricks' and flagstones' joints sunk
#define RT_POM 12
#define RT_POM_DEPTH 0.035
float rt_depth_at(float2 uv, float2 dx, float2 dy)
{
    float3 t = tex.SampleGrad(smp, uv, dx, dy).rgb;
    return 1.0 - saturate(dot(t, float3(0.299, 0.587, 0.114)) * 1.6);
}
float2 rt_parallax(float2 uv, float3 P, float3 fn)
{
    if (dot(fn, fn) < 1e-20)
        return uv;
    float3 N = normalize(fn), V = normalize(lb_eye.xyz - P);
    if (dot(N, V) < 0.0)
        N = -N;
    float3 dpx = ddx(P), dpy = ddy(P);
    float2 dux = ddx(uv), duy = ddy(uv);
    float3 c1 = cross(dpy, N), c2 = cross(N, dpx);
    float det = dot(dpx, c1);
    if (abs(det) < 1e-14)
        return uv;
    float3 gu = (c1 * dux.x + c2 * duy.x) / det;     // the uv's gradient on the surface
    float3 gv = (c1 * dux.y + c2 * duy.y) / det;
    float nv = max(dot(N, V), 0.25);
    // seen edge-on the layers showed as stripes (Mary: the road's lines): more
    // layers the flatter the view, and the relief faded out where flattest,
    // and far away (rt_near)
    float depth = RT_POM_DEPTH * smoothstep(0.12, 0.45, dot(N, V)) * rt_near(P);
    if (depth <= 0.0)
        return uv;
    int layers = (int)lerp(32.0, 8.0, saturate(dot(N, V)));
    float2 full = -float2(dot(V, gu), dot(V, gv)) / nv * depth;   // the deepest point's shift
    float2 st = full / layers, cuv = uv, prev = uv;
    float layer = 1.0 / layers, cur = 0.0, h = rt_depth_at(uv, dux, duy), hp = h;
    [loop] for (int k = 0; k < layers && cur < h; k++) {
        prev = cuv;
        hp = h;
        cuv += st;
        cur += layer;
        h = rt_depth_at(cuv, dux, duy);
    }
    float after = h - cur, before = hp - (cur - layer);
    float w = abs(after - before) > 1e-5 ? after / (after - before) : 0.0;
    return lerp(cuv, prev, saturate(w));
}

// RTX AURA: each light lights the air a little between the eye and what is
// seen, the in-scattering of a point light in thin, even haze, integrated
// in closed form along the view ray (the 1/r^2 falloff's integral is an
// arctangent -- Sun, Ramamoorthi, Narasimhan, Nayar, "A Practical Analytic
// Single Scattering Model for Real Time Rendering", 2005, simplified to no
// extinction). The Pit's pool becomes a green glow over it (Mary), the
// torches and lamps halos
#define RT_AURA 0.014
float3 rt_aura(float3 P)
{
    float3 E = lb_eye.xyz, D = P - E;
    float len = length(D);
    float3 sum = float3(0.0, 0.0, 0.0);
    int n = min(lb_count.x, 16);
    if (len < 1e-3)
        return sum;
    D /= len;
    [loop] for (int i = 0; i < n; i++) {
        Light L = lb_l[i];
        if (int(L.dir.w + 0.5) == 2)
            continue;                   // the sun is not a haze's glow
        float3 toL = L.pos.xyz - E;
        float t0 = dot(toL, D);                         // the ray's nearest point to the light
        float h2 = max(dot(toL, toL) - t0 * t0, 1e-4);  // and its distance, squared
        float h = sqrt(h2);
        if (h > L.pos.w)                                // the ray passes beyond its reach
            continue;
        float a = (atan((len - t0) / h) - atan(-t0 / h)) / h;
        float fade = saturate(1.0 - h / L.pos.w);
        if (int(L.dir.w + 0.5) == 1) {  // a lamp's spot: the haze lit in its beam only
            float3 X = E + D * clamp(t0, 0.0, len);
            fade *= smoothstep(L.cone.y, L.cone.x, dot(normalize(X - L.pos.xyz), normalize(L.dir.xyz)));
        }
        sum += L.col.rgb * L.col.w * a * fade * fade;
    }
    // capped: a strong warm light (the Temple's torches) made a ball of fire
    sum *= RT_AURA;
    return sum / (1.0 + dot(sum, float3(0.3, 0.6, 0.1)) * 2.0);
}

// RTX MATERIALS: a specular map and a roughness map read from each texture
// as it is drawn (the game has none): the specular from how bright and how
// grey the colour is (stone and metal catch light, saturated cloth less),
// the roughness from its grain (fwidth of the brightness: busy texels are
// rough) -- and the stage's floor wet, as Mary's reference captures: puddles
// in a slow noise over the world, the dark cracks holding water, mirror
// smooth there. wet: 0 dry .. 1 a puddle
float rt_hash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float rt_vnoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(rt_hash(i), rt_hash(i + float2(1, 0)), f.x),
                lerp(rt_hash(i + float2(0, 1)), rt_hash(i + float2(1, 1)), f.x), f.y);
}
// RTX RELIEF: a bump from the texture's brightness as a height, with no
// tangents -- Mikkelsen, "Bump Mapping Unparametrized Surfaces on the GPU"
// (2010): the height's screen derivatives against the position's give the
// surface gradient. The floor's stone the most (Mary: more relief), walls
// some, the fighters none
float3 rt_bump(float3 N, float3 P, float h)
{
    float3 dpx = ddx(P), dpy = ddy(P);
    float hx = ddx(h), hy = ddy(h);
    float3 r1 = cross(dpy, N), r2 = cross(N, dpx);
    float det = dot(dpx, r1);
    if (abs(det) < 1e-12)
        return N;
    float3 g = sign(det) * (hx * r1 + hy * r2);
    return normalize(abs(det) * N - g);
}

void rt_material(float3 al, float3 P, float3 N, bool fighter, out float spec, out float rough, out float wet)
{
    float lum = dot(al, float3(0.299, 0.587, 0.114));
    float sat = max(al.r, max(al.g, al.b)) - min(al.r, min(al.g, al.b));
    float grain = saturate(fwidth(lum) * 12.0);
    spec = lerp(0.02, 0.08, saturate(lum * 2.5)) * (1.0 - 0.6 * saturate(sat * 2.0));
    rough = fighter ? lerp(0.35, 0.6, saturate(sat * 2.0)) : saturate(0.55 + grain * 0.35 - lum * 0.2);
    wet = 0.0;
    if (!fighter) {
        bool floor_ = lb_refl.z > 0.5 && N.z > 0.75 && abs(P.z - lb_refl.x) < 0.25;
        if (floor_) {
            // reflections only, not wet (Mary): no puddles, the stone as dry
            wet = 1.0;
        } else
            wet = 0.0;
        // the stone keeps its own roughness and specular: no water sheen
    }
}

// REFLECTIONS (rgl_ff.c refl_pass): a wet floor. The scene drawn mirrored
// under the floor's plane, at half size, read where this pixel is, rippled
// by the floor's own texture and drawn out in streaks along the view, as a
// wet stone floor shows lights; Schlick's Fresnel, and a floor at least
// half a mirror (Mary's reference captures: the lights and the fighters in
// the floor). lin: this pixel's light, linear; returns it, wet
float3 rt_wet(float3 lin, float3 P, float3 N, float2 px, float3 al, float rough, float wetness)
{
    if (lb_refl.y <= 0.0 || N.z < 0.75 || abs(P.z - lb_refl.x) > 0.25)
        return lin;
    float2 uv = (px - lb_rvp.xy) / lb_rvp.zw;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return lin;
    float3 V = normalize(lb_eye.xyz - P);
    float fres = 0.04 + 0.96 * pow(1.0 - saturate(dot(V, N)), 5.0);
    float lum = dot(al, float3(0.299, 0.587, 0.114));
    // the stone's grain ripples it; its cracks (dark) hold less water
    float2 rip = float2(0.0, 0.0);      // no ripple: not wet (Mary)
    float4 r = float4(0.0, 0.0, 0.0, 0.0);
    [unroll] for (int k = -3; k <= 3; k++) {    // streaks along the view, longer when rough
        float2 q = uv + rip * (0.3 + rough) + float2(0.0, k * 0.0012);
        r += rmap.SampleLevel(rsamp, saturate(q), 0.0) * (4.0 - abs(k));
    }
    r /= 16.0;
    // a polished floor's faint reflection, more at grazing angles (Mary: less, not wet)
    float wet = lb_refl.y * wetness * lerp(0.05, 0.16, fres);
    float3 rl = pow(max(r.rgb, float3(0.0, 0.0, 0.0)), 2.2) * 0.8;
    if (lb_refl.w > 0.5)                // UMK3_REFL_VIEW: the reflection alone, magenta where it reads nothing
        return rl;
    // no bright spots: the reflection at most a little brighter than the
    // stone under it -- a lamp's blob in the floor read as water (Mary)
    rl = min(rl, lin * 1.4 + 0.01);
    return lin * (1.0 - wet) + max(lin, rl) * wet;
}

float3 rt_sun() { return asfloat(lb_count.yzw); }   // toward the sun; 0: no RTX
bool rt_on() { float3 L = rt_sun(); return dot(L, L) > 0.5; }

// the nearest hit's instance (-1 none) and its distance
int rt_trace(float3 O, float3 D, float tmax, int self, out float t)
{
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    RayDesc r;
    r.Origin = O;
    r.Direction = D;
    r.TMin = 0.005;
    r.TMax = tmax;
    q.TraceRayInline(rt_scene, RAY_FLAG_NONE, 0xFF, r);
    while (q.Proceed()) {
        int id = (int)q.CandidateInstanceID();
        if (self == 0 || id != self)
            q.CommitNonOpaqueTriangleHit();
    }
    t = tmax;
    if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
        return -1;
    t = q.CommittedRayT();
    return (int)q.CommittedInstanceID();
}

// anything between: 0 or 1, the first hit ends the search
float rt_clear(float3 O, float3 D, float tmax, int self)
{
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    RayDesc r;
    r.Origin = O;
    r.Direction = D;
    r.TMin = 0.005;
    r.TMax = tmax;
    q.TraceRayInline(rt_scene, RAY_FLAG_NONE, 0xFF, r);
    while (q.Proceed()) {
        int id = (int)q.CandidateInstanceID();
        if (self == 0 || id != self)
            q.CommitNonOpaqueTriangleHit();
    }
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
}

float rt_noise(float2 px) { return frac(52.9829189 * frac(dot(px, float2(0.06711056, 0.00583715)))); }

// a frame of reference about a direction
void rt_basis(float3 N, out float3 T, out float3 B)
{
    T = normalize(abs(N.z) < 0.9 ? cross(N, float3(0.0, 0.0, 1.0)) : cross(N, float3(1.0, 0.0, 0.0)));
    B = cross(N, T);
}

// the sun: RT_RAYS rays in a small cone around it, a soft edge as the map's
// nine texels give (a Vogel disc turned for each pixel by interleaved
// gradient noise -- Jimenez, Call of Duty: Advanced Warfare, 2014); the
// shade as the map's, 0.45 under a fighter, 0.65 under the stage
#define RT_RAYS 1
#define RT_CONE 0.03
float rt_light(float3 P, float3 N, int self, float2 px)
{
    float3 L = rt_sun(), T, B, O;
    float turn, sum = 0.0, t;
    int k;
    if (!rt_on())
        return -1.0;
    if (dot(N, L) < 0.0)
        N = -N;
    rt_basis(L, T, B);
    O = P + N * 0.05;
    turn = 6.2831853 * rt_noise(px);
    [loop] for (k = 0; k < RT_RAYS; k++) {
        float a = k * 2.3999632 + turn, rr = sqrt((k + 0.5) / RT_RAYS) * RT_CONE;
        int id = rt_trace(O, normalize(L + (T * cos(a) + B * sin(a)) * rr), 1000.0, self, t);
        sum += id < 0 ? 1.0 : id != 0 ? 0.45 : 0.65;
    }
    return sum / RT_RAYS;
}

// the lights' light where a ray stopped, as dynamic() gives it, shadows aside
float3 rt_lights_at(float3 P, float3 N)
{
    float3 sum = float3(0.0, 0.0, 0.0);
    int n = min(lb_count.x, 16);
    [loop] for (int i = 0; i < n; i++) {
        Light L = lb_l[i];
        int type = int(L.dir.w + 0.5);
        float3 Ld;
        float att = 1.0;
        if (type == 2)
            Ld = -normalize(L.dir.xyz);
        else {
            float3 d = L.pos.xyz - P;
            float dist = length(d), x = dist / max(L.pos.w, 1e-4);
            if (x >= 1.0)
                continue;
            Ld = d / max(dist, 1e-4);
            float w = saturate(1.0 - x * x * x * x);
            att = w * w / (1.0 + 4.0 * x * x);
            if (type == 1)
                att *= smoothstep(L.cone.y, L.cone.x, dot(-Ld, normalize(L.dir.xyz)));
        }
        sum += L.col.rgb * L.col.w * att * max(dot(N, Ld), 0.25);
    }
    return sum;
}

// the ambient light: RT_GI rays over the hemisphere (cosine weighted), what
// is near within RT_AO_FAR occluding it, and what they meet giving back the
// lights' colour on it -- the stage's albedo taken as RT_ALBEDO, a fighter's
// a little less. occ: the ambient left, 0..1
#define RT_GI 2
#define RT_AO_FAR 0.9
#define RT_GI_FAR 2.5
#define RT_ALBEDO 0.55
float3 rt_ambient(float3 P, float3 N, int self, float2 px, out float occ)
{
    float3 T, B, O = P + N * 0.02, gi = float3(0.0, 0.0, 0.0);
    float turn = 6.2831853 * rt_noise(px + 17.0), vis = 0.0, t;
    int k;
    rt_basis(N, T, B);
    [loop] for (k = 0; k < RT_GI; k++) {
        float u = (k + rt_noise(px + k * 5.0)) / RT_GI, a = k * 2.3999632 + turn;
        float rr = sqrt(u), z = sqrt(1.0 - u);
        float3 D = normalize(T * (cos(a) * rr) + B * (sin(a) * rr) + N * z);
        int id = rt_trace(O, D, RT_GI_FAR, self, t);
        if (id < 0 || t > RT_AO_FAR)
            vis += id < 0 ? 1.0 : saturate((t - RT_AO_FAR) / RT_AO_FAR + 0.5);
        if (id >= 0) {
            float3 H = O + D * t;
            gi += rt_lights_at(H, -D) * (id != 0 ? RT_ALBEDO * 0.8 : RT_ALBEDO) / (1.0 + t * t);
        }
    }
    occ = vis / RT_GI;
    return gi / RT_GI;
}
#endif

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

// RTX MATERIALS (rt_material): this pixel's roughness and specular, read
// by dynamic() for a GGX highlight; < 0 keeps the Blinn-Phong one
static float g_rough = -1.0;
static float g_spec = 0.0;
static float g_shine = 1.0;             // 0: no highlight (the floor: Mary, "se ve mojado")

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
#ifdef UMK3_RT
            if (rt_on()) { if (att > 0.06) {
                // RTX: every light's shadows, traced -- to a sphere, not a
                // point (a tenth of its reach), so the edge is soft and its
                // own torch bowl or pool does not cut it sharp (Mary: the
                // flame's cut on the wall); stopped short of the sphere
                float r = L.pos.w * 0.1, vis = 1.0;
                // inside the light's own lamp (a globe, a torch bowl): never its own shadow -- the
                // globe's facets shadowed each other (Mary: the polygons showed)
                if (dist > r * 2.5) { vis = 0.0;
                float3 T, B, O = P + Ld * 0.06;
                float turn = 6.2831853 * rt_noise(P.xy * 731.0 + P.z * 113.0);
                rt_basis(Ld, T, B);
                [loop] for (int k = 0; k < 1; k++) {
                    float a = k * 2.3999632 + turn, rr = sqrt(rt_noise(P.yz * 517.0)) * r;
                    float3 Q = L.pos.xyz + (T * cos(a) + B * sin(a)) * rr - O;
                    float lq = length(Q);
                    vis += rt_clear(O, Q / max(lq, 1e-4), max(lq - r, 0.01), self);
                }
                }
                att *= vis;
            } }
            else
#endif
            if (L.cone.z > 0.5)         // a light with shadows (point_shadows)
                att *= point_shadow(int(L.cone.z + 0.5) - 1, P - L.pos.xyz, L.pos.w, self);
        }
        float ndl = max(dot(N, Ld), 0.0);
        float3 c = L.col.rgb * L.col.w * att;
        dif += c * ndl;
        float3 H = normalize(Ld + V);
        if (g_rough >= 0.0) {
            // GGX (Walter et al. 2007) with Schlick's Fresnel and the
            // Smith-Schlick visibility, as UE4's (Karis, SIGGRAPH 2013)
            float a = max(g_rough * g_rough, 0.002), a2 = a * a;
            float nh = max(dot(N, H), 0.0), nv = max(dot(N, V), 1e-3);
            float dd = nh * nh * (a2 - 1.0) + 1.0;
            float D = a2 / (3.14159 * dd * dd);
            float k = a * 0.5;
            float vis = 0.25 / ((nv * (1.0 - k) + k) * (ndl * (1.0 - k) + k));
            float F = g_spec + (1.0 - g_spec) * pow(1.0 - saturate(dot(V, H)), 5.0);
            spc += c * min(D * vis * F, 4.0) * ndl * 0.25 * g_shine;  // tamed twice: it shone too much (Mary)
        } else
            spc += c * pow(max(dot(N, H), 0.0), sh) * (sh + 8.0) / 25.13 * ndl;
    }
    if (g_rough >= 0.0)
        return albedo * dif * (fighter ? 0.9 : 1.0) * (1.0 - g_spec) + spc;
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
// a sample on the other layer: the centre a fighter's, the sample the
// stage's (A of the depth: a fighter's 1) -- a fighter is a layer of his
// own on top, darkened by himself and never by the stage (Mary)
bool off_layer(float2 uv, float fl)
{
    return fl > 0.5 && scr.SampleLevel(spt, uv, 0).a < 0.5;
}

float4 ao_pass(float2 fc)
{
    float sw_, sh_, sl_;
    scr.GetDimensions(0, sw_, sh_, sl_);
    float2 sz = float2(sw_, sh_);
    float2 uv = fc / sz;
    float3 P = view_at(uv);
    float fl = scr.SampleLevel(spt, uv, 0).a;
    float3 N = cross(ddx(P), ddy(P));
    float far = lb_aoproj.z;
    float2 e = frac(min(P.z / far, 0.999) * float2(1.0, 255.0));
    e.x -= e.y / 255.0;
    if (P.z > far * 0.98 || dot(N, N) < 1e-20)
        return float4(1.0, e, fl);
    N = normalize(N);
    if (dot(N, -P) < 0.0)
        N = -N;
    float R = lb_aoproj.w;
    float rpx = R * lb_aoproj.x * 0.5 * sz.x / P.z;
    if (rpx < 1.0)
        return float4(1.0, e, fl);
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
            float2 qu = uv + float2(cos(a), sin(a)) * rpx * sqrt(t) / sz;
            if (off_layer(qu, fl))
                continue;
            float3 Q = view_at(qu);
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
                float2 qu = uv + dir * rpx * (float(s) - noise * 0.5) / 4.0 / sz;
                if (off_layer(qu, fl))
                    continue;
                float3 v = view_at(qu) - P;
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
    return float4(ao, e, fl);
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
        return c0;
    float s = 0.0, w = 0.0;
    [loop] for (int y = -2; y <= 2; y++)
        [loop] for (int x = -2; x <= 2; x++) {
            float4 c = scr.SampleLevel(spt, uv + float2(float(x), float(y)) / sz, 0);
            float z = c.g + c.b / 255.0;
            float wt = max(0.0, 1.0 - abs(z - z0) / (0.03 * z0 + 1e-5))
                     * (abs(c.a - c0.a) < 0.5 ? 1.0 : 0.0);   // each layer its own
            s += c.r * wt;
            w += wt;
        }
    return float4(s / max(w, 1e-4), c0.g, c0.b, c0.a);
}

// the scene's read: the blurred occlusion at this pixel, bilinear by hand;
// a fighter's only from his own layer's texels (none: unoccluded)
float ao_at(float2 fc, bool fighter)
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
    float4 a00 = scr.SampleLevel(spt, b, 0);
    float4 a10 = scr.SampleLevel(spt, b + float2(1.0 / sz.x, 0), 0.0);
    float4 a01 = scr.SampleLevel(spt, b + float2(0.0, 1.0 / sz.y), 0);
    float4 a11 = scr.SampleLevel(spt, b + 1.0 / sz, 0);
    if (!fighter)
        return lerp(lerp(a00.r, a10.r, f.x), lerp(a01.r, a11.r, f.x), f.y);
    float4 wt = float4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y)
              * step(0.5, float4(a00.a, a10.a, a01.a, a11.a));
    float ws = wt.x + wt.y + wt.z + wt.w;
    return ws > 1e-4 ? dot(wt, float4(a00.r, a10.r, a01.r, a11.r)) / ws : 1.0;
}


// CONTACT SHADOWS: as ff.frag's contact_at
float contact_at(float2 fc, bool fighter)
{
    float3 L = lb_aodbg.yzw;
    if (fighter || dot(L, L) < 0.5 || lb_aoparm2.w < 0.5)
        return 1.0;
    float2 uv = (fc - lb_aovp.xy) / lb_aovp.zw;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return 1.0;
    float far = lb_aoproj.z;
    float sw_, sh_, sl_;
    scr.GetDimensions(0, sw_, sh_, sl_);
    float2 sz = float2(sw_, sh_);
    float4 c0 = scr.SampleLevel(spt, uv, 0);
    float d = (c0.g + c0.b / 255.0) * far;
    if (d > far * 0.98 || d < 1e-3)
        return 1.0;
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float3 P = float3(ndc.x * d / lb_aoproj.x, ndc.y * d / lb_aoproj.y, d);
    float R = lb_aoproj.w;
    float len = R * 0.5, thick = R * 0.35 + 0.004 * d;
    P += L * (0.05 * R + 0.004 * d);
    [loop] for (int i = 0; i < 12; i++) {
        float t = (float(i) + 0.5) / 12.0;
        float3 S = P + L * (len * t);
        if (S.z < 1e-2)
            break;
        float2 su = float2(S.x * lb_aoproj.x / S.z * 0.5 + 0.5, 0.5 - S.y * lb_aoproj.y / S.z * 0.5);
        if (su.x < 0.0 || su.y < 0.0 || su.x > 1.0 || su.y > 1.0)
            break;
        if (length((su - uv) * sz) < 2.0)
            continue;
        float4 c = scr.SampleLevel(spt, su, 0);
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
    return ao_at(fc, false).xxx;                 // 1 after the blur, 2 before it
}

// PHOTO MODE's effects: as ff.frag's post_fx (the settings in lb_l[0], [1])
float3 post_c(float2 uv)
{
    return scr.SampleLevel(spt, saturate(uv), 0).rgb;
}

float post_d(float2 uv)
{
    return unpack_depth(smap.SampleLevel(spt, saturate(uv), 0)) * lb_aoproj.z;
}

// GRAPHICS QUALITY's tonemap: as ff.frag's (l1.pos w)
float3 tonemap(float3 c, float s)
{
    float3 lin = pow(max(c, 0.0), 2.2);
    float L = dot(lin, float3(0.2126, 0.7152, 0.0722));
    if (L <= 0.6)
        return c;
    float e = min((L - 0.6) / 0.8, 1.0);
    float L2 = 0.6 + 0.4 * e * (2.0 - e);
    float3 t = pow(min(lin * (L2 / L), 1.0), 1.0 / 2.2);
    return lerp(c, t, s);
}

float4 post_fx(float2 fc)
{
    float4 P0 = lb_l[0].pos, P1 = lb_l[0].col, P2 = lb_l[0].dir, P3 = lb_l[0].cone, P4 = lb_l[1].pos;
    float2 uv = (fc - lb_aovp.xy) / lb_aovp.zw;
    float2 px = 1.0 / lb_aovp.zw;
    const float3 Y = float3(0.299, 0.587, 0.114);
    float2 dc = (uv - 0.5) * P3.x * 0.012;
    float3 c = float3(post_c(uv + dc).r, post_c(uv).g, post_c(uv - dc).b);
    int i;
    if (P4.x > 0.5 && lb_aoparm2.w > 0.5) {
        float df = (post_d(float2(0.5, 0.5)) + post_d(float2(0.47, 0.5)) + post_d(float2(0.53, 0.5))
                  + post_d(float2(0.5, 0.46)) + post_d(float2(0.5, 0.54))) * 0.2 * P4.y;
        float coc = saturate(abs(post_d(uv) - df) / max(df, 1e-4) * P4.z) * 10.0;
        if (coc > 0.5) {
            float3 acc = c;
            for (i = 0; i < 24; i++) {
                float a = (float)i * 2.39996, r = sqrt(((float)i + 0.5) / 24.0) * coc;
                acc += post_c(uv + float2(cos(a), sin(a)) * r * px);
            }
            c = acc / 25.0;
        }
    }
    if (P3.y > 0.0) {
        // a soft glow: 32 taps over a disc (a Vogel spiral turned for each
        // pixel), weighted by a Gaussian, the bright part taken with a soft
        // knee -- 16 bare taps at a hard 0.6 drew a stepped outline round a
        // dark fighter on a bright stage (Mary: the lava's)
        float3 b = float3(0.0, 0.0, 0.0);
        float wsum = 0.0;
        float turn = 6.2831853 * frac(52.9829189 * frac(dot(fc, float2(0.06711056, 0.00583715))));
        for (i = 0; i < 32; i++) {
            float a = (float)i * 2.39996 + turn, t = sqrt(((float)i + 0.5) / 32.0), r = t * 28.0;
            float w = exp(-t * t * 3.0);
            float3 s = post_c(uv + float2(cos(a), sin(a)) * r * px);
            float k = smoothstep(0.5, 0.95, dot(s, Y));
            b += s * k * w;
            wsum += w;
        }
        c += b / wsum * (P3.y * 2.2);
    }
    if (P1.w > 0.0) {
        float3 n = post_c(uv + float2(px.x, 0.0)) + post_c(uv - float2(px.x, 0.0))
                 + post_c(uv + float2(0.0, px.y)) + post_c(uv - float2(0.0, px.y));
        c += (c * 4.0 - n) * (P1.w * 0.25);
    }
    c *= exp2(P0.x);
    if (P4.w > 0.0) c = tonemap(c, P4.w);
    c *= float3(1.0 + 0.25 * P1.x, 1.0 - 0.15 * P1.y, 1.0 - 0.25 * P1.x);
    float l = dot(c, Y);
    c += P2.y * 0.35 * (1.0 - smoothstep(0.0, 0.5, l));
    c += P2.x * 0.35 * smoothstep(0.5, 1.0, l);
    c += P0.y;
    c = (c - 0.5) * P0.z + 0.5;
    l = dot(c, Y);
    c = lerp(float3(l, l, l), c, P0.w);
    int fl = (int)(P3.z + 0.5);
    if (fl == 1) c = float3(l, l, l);
    else if (fl == 2) c = float3(l, l, l) * float3(1.07, 0.86, 0.62) + float3(0.06, 0.03, 0.0);
    else if (fl == 3) c *= float3(0.85, 0.97, 1.15);
    else if (fl == 4) c *= float3(1.15, 1.0, 0.82);
    else if (fl == 5) c = lerp(c, float3(c.r * 1.2, c.g * 0.55, c.b * 0.5), 0.85);
    else if (fl == 6) c = lerp(c, float3(c.r * 0.9 + 0.05, c.g * 0.75, c.b * 1.2 + 0.05), 0.8);
    else if (fl == 7) { float k = smoothstep(0.12, 0.88, l); c = float3(k, k, k); }
    else if (fl == 8) c = lerp(c, float3(l, l, l), 0.35) * float3(1.05, 1.0, 0.88) + 0.04;
    c = pow(max(c, 0.0), 1.0 / max(P1.z, 0.1));
    {
        float2 q = (uv - 0.5) * float2(1.0, lb_aovp.w / lb_aovp.z) * 1.6;
        c *= lerp(1.0, smoothstep(1.05, 0.25, length(q)), P2.z);
    }
    c += (frac(sin(dot(fc + P3.w * 61.0, float2(12.9898, 78.233))) * 43758.5453) - 0.5) * P2.w * 0.2;
    return float4(saturate(c), 1.0);
}

float4 ps(PSIn i) : SV_Target
{
    if ((shadow & 15) == 6)             // PHOTO MODE's effects
        return post_fx(i.pos.xy);
    if ((shadow & 15) == 7) {           // DLSS 5's interface mask made whole (rgl_ff_layer_fix):
        // any interface touch in the 3x3 about the pixel marks it whole, so
        // the soft edges of letters are kept from the neural pass too
        float2 px = 1.0 / lb_aovp.zw, uv = (i.pos.xy - lb_aovp.xy) * px;
        float a = 0.0;
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++)
                a = max(a, scr.SampleLevel(spt, uv + float2(x, y) * px, 0).a);
        return float4(0.0, 0.0, 0.0, a > 0.03 ? 1.0 : 0.0);
    }
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
    float2 uvp = i.uv;
#ifdef UMK3_RT
    if (sm == 1 && lit != 0 && (shadow >> 4) == 0 && mode != 0 && rt_on() && lb_tess.x < 0.5)
        uvp = rt_parallax(i.uv, i.p, fn);   // RTX HEIGHT: the stage's relief (not with real displacement)
#endif
    if (mode == 1) {
        tc = tex.SampleGrad(smp, uvp, ddx(i.uv), ddy(i.uv));
        c *= tc;
    } else if (mode == 2) {
        tc = tex.SampleGrad(smp, uvp, ddx(i.uv), ddy(i.uv));
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
        return pack_depth(i.pos.w / lb_aoproj.z, (shadow >> 4) != 0 ? 255.0 : 0.0);
    if (sm == 1)
    {
        float lt = -1.0;
#ifdef UMK3_RT
        {
            float3 Nr = dot(i.n, i.n) > 0.25 ? normalize(i.n) : (dot(fn, fn) > 1e-20 ? normalize(fn) : float3(0.0, 0.0, 1.0));
            lt = rt_light(i.p, Nr, shadow >> 4, i.pos.xy);
        }
#endif
        if (lt < 0.0)
            lt = light_at(p, qx, qy, shadow >> 4);
        bool fighter = (shadow >> 4) != 0;
        lt = min(lt, contact_at(i.pos.xy, fighter));   // CONTACT SHADOWS
        // in linear light (gamma 2.2), as the walkthrough's: the relief and
        // the highlight are light added and taken away, and only there do
        // they add up as light does; the map's shadow, a shade as it was
        // tuned (lt in gamma), then the dynamic lights over it all
        float3 lin = pow(max(c.rgb, float3(0.0, 0.0, 0.0)), 2.2);
        if (bump > 0.0) {
            float sp = specular(lin, fighter);
#ifdef UMK3_RT
            {   // RTX: the floor without its relief's highlight (not wet)
                float3 fz = dot(fn, fn) > 1e-20 ? normalize(fn) : float3(0.0, 0.0, 0.0);
                if (rt_on() && lb_refl.z > 0.5 && abs(fz.z) > 0.75 && abs(i.p.z - lb_refl.x) < 0.25)
                    sp = 0.0;
            }
#endif
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
#ifdef UMK3_RT
                float g_wet = 0.0;
                if (rt_on()) {          // RTX MATERIALS for the lights' highlights and the floor
                    float ms, mr;
                    rt_material(al, i.p, N, fighter, ms, mr, g_wet);
                    g_spec = ms;
                    g_rough = mr;
                    if (!fighter) {     // the stone's relief: the floor deep, walls some
                        bool fl = lb_refl.z > 0.5 && N.z > 0.75 && abs(i.p.z - lb_refl.x) < 0.25;
                        if (fl)
                            g_shine = 0.0;
                        float hgt = pow(saturate(dot(al, float3(0.299, 0.587, 0.114)) * 2.0), 0.6);
                        N = rt_bump(N, i.p, hgt * (fl ? 0.06 : 0.025) * rt_near(i.p));
                    }

                }
#endif
                lin += dynamic(i.p, N, al, sm2, fighter, shadow >> 4);
                // AMBIENT OCCLUSION over everything, last (as ff.frag)
                float ao = max(lerp(1.0, ao_at(i.pos.xy, fighter), fighter ? lb_aoparm2.y : 1.0), lb_aoparm.z);
                int refl_on = 0;
#ifdef UMK3_RT
                if (rt_on()) {          // RTX: traced in its place, and the lights' colour bounced
                    float occ;
                    float3 gi = rt_ambient(i.p, N, shadow >> 4, i.pos.xy, occ);
                    ao = max(lerp(1.0, occ, fighter ? 0.6 : 0.85), lb_aoparm.z);
                    lin += al * gi;
                    if (!fighter)       // the wet floor's reflection, after the occlusion
                        refl_on = 1;
                }
#endif
                lin *= ao;
#ifdef UMK3_RT
                if (refl_on)
                    lin = rt_wet(lin, i.p, N, i.pos.xy, al, g_rough, g_wet);
#endif
                if (lb_aodbg.x > 0.5 && lb_aoparm2.w > 0.5)    // a debug view: that, alone
                    return float4(ao_debug_view(i.pos.xy, ao), 1.0);
            }
        }
#ifdef UMK3_RT
        if (rt_on() && c.a > 0.98)      // the haze's glow over what is solid (once: not again on a lamp's light shaft)
            lin += rt_aura(i.p);
#endif
        c.rgb = pow(lin, 1.0 / 2.2);
    }
    if (layer >= 3.0)                   // DLSS 5's layers: an opaque draw covers its pixels whole
        c.a = 1.0;
    return c;
}

#ifdef UMK3_RT
// RTX DISPLACEMENT, step 1 (Mary's spec, 2026-11-26): the stage's draws as
// patches of three control points, subdivided on the GPU -- the hull shader
// (here a fixed factor, RT_TESS on every edge and inside) and the domain
// shader, which puts each new vertex where the triangle has it: every
// attribute by its barycentric weights. The clip position is a linear
// function of the world position (clip = projection x view x world), so
// interpolating it is exact; the picture is the untessellated one, only
// made of more triangles. Displacement comes in step 2.
#define RT_TESS 12.0
struct HSConst { float e[3] : SV_TessFactor; float in_ : SV_InsideTessFactor; };

HSConst hs_const(InputPatch<PSIn, 3> p)
{
    HSConst o;
    o.e[0] = o.e[1] = o.e[2] = RT_TESS;
    o.in_ = RT_TESS;
    return o;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]     // the tessellator's domain winds the other way to the game's faces: ccw culled them all
[outputcontrolpoints(3)]
[patchconstantfunc("hs_const")]
PSIn hs(InputPatch<PSIn, 3> p, uint i : SV_OutputControlPointID)
{
    return p[i];
}

[domain("tri")]
PSIn ds(HSConst c, float3 b : SV_DomainLocation, const OutputPatch<PSIn, 3> p)
{
    PSIn o;
    o.pos = p[0].pos * b.x + p[1].pos * b.y + p[2].pos * b.z;
    o.col = p[0].col * b.x + p[1].col * b.y + p[2].col * b.z;
    o.uv  = p[0].uv  * b.x + p[1].uv  * b.y + p[2].uv  * b.z;
    o.sh  = p[0].sh  * b.x + p[1].sh  * b.y + p[2].sh  * b.z;
    o.n   = p[0].n   * b.x + p[1].n   * b.y + p[2].n   * b.z;
    o.p   = p[0].p   * b.x + p[1].p   * b.y + p[2].p   * b.z;
    if (lb_tess.x > 0.5) {
        // step 2: the height (the texture's brightness, as the parallax read
        // it) moves the vertex along its normal -- P + N (h - mid) scale --
        // and its clip position by the same move through the view and the
        // projection; faded out with the distance (no popping, no relief on
        // the far stage). The vertices' own normals, shared by neighbouring
        // patches, so a shared edge moves the same way on both: no cracks.
        // A mesh without normals takes its face's.
        float3 N = o.n;
        if (dot(N, N) < 0.25) {
            N = cross(p[1].p - p[0].p, p[2].p - p[0].p);
            if (dot(N, lb_eye.xyz - o.p) < 0.0)
                N = -N;
        }
        N = normalize(N);
        float h = dot(tex.SampleLevel(smp, o.uv, lb_tess.w).rgb, float3(0.299, 0.587, 0.114));
        float d = (saturate(h * 1.6) - lb_tess.z) * lb_tess.y * rt_near(o.p);
        float3 m = N * d;
        o.p += m;
        o.pos += mul(lb_vp, float4(m, 0.0));
    }
    return o;
}
#endif
