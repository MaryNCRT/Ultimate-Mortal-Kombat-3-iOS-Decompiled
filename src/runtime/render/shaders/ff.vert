// The fixed function's vertex stage, for Vulkan (rgl_vk.c). The position
// arrives in OpenGL's clip space (rgl_ff.c transformed it); Vulkan's y runs
// down and its depth from 0 to w.
#version 450
layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 col;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec4 v_col;
layout(location = 1) out vec2 v_uv;
void main()
{
    v_col = col;
    v_uv = uv;
    gl_Position = vec4(pos.x, -pos.y, (pos.z + pos.w) * 0.5, pos.w);
}
