// The fixed function's fragment stage, for Vulkan (rgl_vk.c): the texture
// environment (1 MODULATE, 2 REPLACE) and the alpha test (the GL compare
// enum, 0 off), as OpenGL 1.1 does them; and REAL-TIME SHADOWS (rgl_ff.c),
// as ff.hlsl: shadow (its low four bits) 1 darkens where the map has
// something nearer the light -- the bits above are the receiver's own caster
// -- and 2 writes the map's texel.
#version 450
layout(location = 0) in vec4 v_col;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec4 v_sh;
layout(location = 0) out vec4 o;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D smap;
layout(push_constant) uniform PC { int mode; int afunc; float aref; int shadow; } pc;

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

void main()
{
    vec4 c = v_col;
    int sm = pc.shadow & 15;
    /* the shadow map's coordinates and their slopes, taken before any
     * discard; y as the map's v runs, down */
    vec3 p = v_sh.w > 0.0 ? v_sh.xyz / v_sh.w : vec3(2.0);
    vec3 q = vec3(p.x * 0.5, -p.y * 0.5, p.z * 0.5);
    vec3 qx = dFdx(q), qy = dFdy(q);
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
        c.rgb *= light_at(p, qx, qy, pc.shadow >> 4);
    o = c;
}
