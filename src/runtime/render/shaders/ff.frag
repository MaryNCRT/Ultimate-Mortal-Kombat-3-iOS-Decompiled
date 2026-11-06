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
layout(location = 0) out vec4 o;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D smap;
layout(push_constant) uniform PC { int mode; int afunc; float aref; int shadow; float bump; } pc;

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
    float lod = max(floor(textureQueryLod(tex, uv).x), 0.0);
    vec2 g = (slope(uv, lod) + 0.5 * slope(uv, lod + 2.0)) * k;
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

void main()
{
    vec4 c = v_col;
    int sm = pc.shadow & 15;
    /* the shadow map's coordinates and their slopes, taken before any
     * discard; y as the map's v runs, down */
    vec3 p = v_sh.w > 0.0 ? v_sh.xyz / v_sh.w : vec3(2.0);
    vec3 q = vec3(p.x * 0.5, -p.y * 0.5, p.z * 0.5);
    vec3 qx = dFdx(q), qy = dFdy(q);
    float rl = sm == 1 && pc.bump > 0.0 ? relief(v_uv, pc.bump) : 1.0;
    if (pc.mode == 1)
        c *= texture(tex, v_uv);
    else if (pc.mode == 2)
        c = texture(tex, v_uv);
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
    else if (sm == 1)
    {
        float lt = light_at(p, qx, qy, pc.shadow >> 4);
        if (pc.bump > 0.0) {
            // in linear light (gamma 2.2), as the walkthrough's: the relief
            // and the highlight are light added and taken away, and only
            // there do they add up as light does; the shadow, a shade as
            // it was tuned, after
            vec3 lin = pow(max(c.rgb, vec3(0.0)), vec3(2.2));
            float sp = specular(lin, (pc.shadow >> 4) != 0);
            lin = lin * rl + sp * smoothstep(0.75, 1.0, lt);
            c.rgb = pow(lin, vec3(1.0 / 2.2));
        }
        c.rgb *= lt;
    }
    o = c;
}
