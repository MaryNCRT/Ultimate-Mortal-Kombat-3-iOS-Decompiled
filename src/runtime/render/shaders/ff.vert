// The fixed function's vertex stage, for Vulkan (rgl_vk.c). The position
// arrives in OpenGL's clip space (rgl_ff.c transformed it); Vulkan's y runs
// down and its depth from 0 to w. sh is where the vertex is in the shadow
// map's light (REAL-TIME SHADOWS), passed on; nrm and wpos the normal and
// the place in the world (DYNAMIC LIGHTS).
#version 450
layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 col;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 sh;
layout(location = 4) in vec3 nrm;
layout(location = 5) in vec3 wpos;
layout(location = 0) out vec4 v_col;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec4 v_sh;
layout(location = 3) out vec3 v_n;
layout(location = 4) out vec3 v_p;
void main()
{
    v_col = col;
    v_uv = uv;
    v_sh = sh;
    v_n = nrm;
    v_p = wpos;
    gl_Position = vec4(pos.x, -pos.y, (pos.z + pos.w) * 0.5, pos.w);
}
