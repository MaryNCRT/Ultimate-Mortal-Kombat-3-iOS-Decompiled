// The fixed function, for Direct3D 12 (rgl_d3d12.c): the same as ff.vert and
// ff.frag. Direct3D's y runs up as OpenGL's; its depth runs from 0 to w.
struct VSIn { float4 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD; };
struct PSIn { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD; };
cbuffer PC : register(b0) { int mode; int afunc; float aref; int pad; };
Texture2D tex : register(t0);
SamplerState smp : register(s0);

PSIn vs(VSIn i)
{
    PSIn o;
    o.pos = float4(i.pos.x, i.pos.y, (i.pos.z + i.pos.w) * 0.5, i.pos.w);
    o.col = i.col;
    o.uv = i.uv;
    return o;
}

float4 ps(PSIn i) : SV_Target
{
    float4 c = i.col;
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
    return c;
}
