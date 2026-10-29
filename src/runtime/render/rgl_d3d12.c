/*
 * rgl_d3d12.c -- the Direct3D 12 backend. Port, not in the binary.
 *
 * Draws what rgl_ff.c assembles (see rgl.h), on Windows 10 and later:
 * d3d12.dll and dxgi.dll are loaded at run time, and a machine where they
 * cannot make a device falls back to OpenGL. The same model as rgl_vk.c:
 *
 *   - One frame in flight, waited for after it is presented (OpenGL drew
 *     with glFinish after every swap).
 *   - One command list in the game's order; a texture changed after a draw
 *     that used it this frame, or pixels read back, first runs what is
 *     recorded and waits.
 *   - Targets (the window's back buffer and each framebuffer object) are a
 *     colour texture and a depth buffer; clears are ClearRenderTargetView /
 *     ClearDepthStencilView inside the scissor.
 *   - Pipeline state objects are made when a state is first drawn with and
 *     kept, keyed by rgl_pipe and the target's format, samples and depth.
 *   - Clip space: Direct3D's y runs up as OpenGL's, so only z moves (the
 *     vertex shader), and counter-clockwise triangles are the front faces.
 *   - glBlitFramebuffer: a resolve when the source is multisampled, else a
 *     textured quad drawn with the source as its texture (Direct3D 12 has
 *     no scaled copy).
 *
 * C, through the COM wrappers in mingw-w64's headers (COBJMACROS, with
 * WIDL_C_INLINE_WRAPPERS for the calls that return a structure).
 */
#ifdef _WIN32

#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rgl.h"
#include "rgl_shaders_d3d12.h"
#include "../platform/platform.h"

#define NBUF 2
#define SRV_RING 16384
#define RTV_MAX 64

typedef struct {
    ID3D12Resource *res;
    D3D12_RESOURCE_STATES state;
} dx_res;

typedef struct {
    int used, w, h, levels;
    dx_res r;
    unsigned serial;
} dx_tex;

typedef struct {
    int used, w, h, samples, depth;
    dx_res color, dep;
    int rtv, dsv;                       /* slots in the heaps */
} dx_target;

typedef struct { rgl_pipe key; DXGI_FORMAT fmt; int samples, depth; ID3D12PipelineState *p; } dx_pso;
typedef struct { unsigned key; int slot; } dx_samp;

static HMODULE g_d3d, g_dxgi;
static ID3D12Device *g_dev;
static ID3D12CommandQueue *g_queue;
static IDXGISwapChain3 *g_swap;
static IDXGIFactory4 *g_factory;
static ID3D12CommandAllocator *g_alloc;
static ID3D12GraphicsCommandList *g_list;
static ID3D12Fence *g_fence;
static HANDLE g_event;
static UINT64 g_fval;
static ID3D12RootSignature *g_root;
static ID3D12DescriptorHeap *g_rtv_heap, *g_dsv_heap, *g_srv_heap, *g_samp_heap;
static UINT g_rtv_inc, g_dsv_inc, g_srv_inc, g_samp_inc;
static int g_rtv_used[RTV_MAX], g_dsv_used[RTV_MAX];
static int g_srv_next;
static char g_adapter[128];

static HWND g_hwnd;
static int g_bw, g_bh;                  /* the back buffers' size */
static dx_res g_back[NBUF];
static int g_back_rtv[NBUF];
static dx_res g_wdepth;
static int g_wdsv;
static UINT g_bi;                       /* this frame's back buffer */
static int g_list_on, g_serial = 1;
static int g_cur;                       /* the current target */
static int g_bound_target = -1;

static ID3D12Resource *g_vbuf;
static unsigned char *g_vmap;
static UINT64 g_vsize, g_voff;

static dx_tex *g_tex;
static int g_ntex;
static dx_target *g_tg;
static int g_ntg;
static dx_pso *g_psos;
static int g_npsos, g_cpsos;
static dx_samp g_samps[64];
static int g_nsamps;
static int g_white;
static int g_shadow;                    /* the target sampled as the shadow map (t1) */

#define DEPTH_FMT DXGI_FORMAT_D24_UNORM_S8_UINT
#define COLOR_FMT DXGI_FORMAT_R8G8B8A8_UNORM

static int ok(HRESULT h, const char *what)
{
    if (SUCCEEDED(h))
        return 1;
    fprintf(stderr, "d3d12: %s failed (0x%08lx)\n", what, (unsigned long)h);
    return 0;
}

/* ----------------------------------------------------------- descriptors */

static D3D12_CPU_DESCRIPTOR_HANDLE cpu(ID3D12DescriptorHeap *h, UINT inc, int i)
{
    D3D12_CPU_DESCRIPTOR_HANDLE c = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(h);
    c.ptr += (SIZE_T)i * inc;
    return c;
}

static D3D12_GPU_DESCRIPTOR_HANDLE gpu(ID3D12DescriptorHeap *h, UINT inc, int i)
{
    D3D12_GPU_DESCRIPTOR_HANDLE g = ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(h);
    g.ptr += (UINT64)i * inc;
    return g;
}

static int slot(int *used)
{
    int i;
    for (i = 0; i < RTV_MAX; i++)
        if (!used[i]) {
            used[i] = 1;
            return i;
        }
    return -1;
}

static ID3D12DescriptorHeap *heap(D3D12_DESCRIPTOR_HEAP_TYPE t, UINT n, int visible)
{
    D3D12_DESCRIPTOR_HEAP_DESC d;
    ID3D12DescriptorHeap *h = NULL;
    memset(&d, 0, sizeof d);
    d.Type = t;
    d.NumDescriptors = n;
    d.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ok(ID3D12Device_CreateDescriptorHeap(g_dev, &d, &IID_ID3D12DescriptorHeap, (void **)&h), "heap");
    return h;
}

/* ------------------------------------------------------------- resources */

static void to_state(dx_res *r, D3D12_RESOURCE_STATES s)
{
    D3D12_RESOURCE_BARRIER b;
    if (!r->res || r->state == s)
        return;
    memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r->res;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = r->state;
    b.Transition.StateAfter = s;
    ID3D12GraphicsCommandList_ResourceBarrier(g_list, 1, &b);
    r->state = s;
}

static int g_ulist_on;                  /* uploads recorded, not yet run (upload) */
static ID3D12CommandAllocator *g_ualloc;
static ID3D12GraphicsCommandList *g_ulist;
static ID3D12Resource **g_ubufs;
static int g_nubufs, g_cubufs;
static UINT64 g_ubytes;
static void upload_submit(void);

static void release(dx_res *r)
{
    if (r->res && g_ulist_on)           /* a recorded upload may write it */
        upload_submit();
    if (r->res)
        ID3D12Resource_Release(r->res);
    r->res = NULL;
}

static ID3D12Resource *buffer(UINT64 size, D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES hp;
    D3D12_RESOURCE_DESC d;
    ID3D12Resource *r = NULL;

    memset(&hp, 0, sizeof hp);
    hp.Type = type;
    memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = d.DepthOrArraySize = d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ok(ID3D12Device_CreateCommittedResource(g_dev, &hp, D3D12_HEAP_FLAG_NONE, &d,
        type == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL, &IID_ID3D12Resource, (void **)&r), "buffer");
    return r;
}

static int texture2d(dx_res *r, int w, int h, int levels, DXGI_FORMAT fmt, int samples,
                     D3D12_RESOURCE_FLAGS flags, const D3D12_CLEAR_VALUE *cv)
{
    D3D12_HEAP_PROPERTIES hp;
    D3D12_RESOURCE_DESC d;

    memset(&hp, 0, sizeof hp);
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = (UINT64)w;
    d.Height = (UINT)h;
    d.DepthOrArraySize = 1;
    d.MipLevels = (UINT16)levels;
    d.Format = fmt;
    d.SampleDesc.Count = samples > 1 ? (UINT)samples : 1;
    d.Flags = flags;
    r->state = cv && fmt == DEPTH_FMT ? D3D12_RESOURCE_STATE_DEPTH_WRITE
             : cv ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COPY_DEST;
    return ok(ID3D12Device_CreateCommittedResource(g_dev, &hp, D3D12_HEAP_FLAG_NONE, &d, r->state, cv,
              &IID_ID3D12Resource, (void **)&r->res), "texture");
}

/* ------------------------------------------------------------- the frame */

static void list_begin(void)
{
    ID3D12DescriptorHeap *hs[2];
    if (g_list_on)
        return;
    ID3D12CommandAllocator_Reset(g_alloc);
    ID3D12GraphicsCommandList_Reset(g_list, g_alloc, NULL);
    hs[0] = g_srv_heap;
    hs[1] = g_samp_heap;
    ID3D12GraphicsCommandList_SetDescriptorHeaps(g_list, 2, hs);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(g_list, g_root);
    g_list_on = 1;
    g_bound_target = -1;
}

static void wait_gpu(void)
{
    g_fval++;
    ID3D12CommandQueue_Signal(g_queue, g_fence, g_fval);
    if (ID3D12Fence_GetCompletedValue(g_fence) < g_fval) {
        ID3D12Fence_SetEventOnCompletion(g_fence, g_fval, g_event);
        WaitForSingleObject(g_event, INFINITE);
    }
}

/* what is recorded, run and waited for */
static void flush(void)
{
    ID3D12CommandList *l;
    upload_submit();                    /* the textures this list draws with */
    if (!g_list_on)
        return;
    ID3D12GraphicsCommandList_Close(g_list);
    l = (ID3D12CommandList *)g_list;
    ID3D12CommandQueue_ExecuteCommandLists(g_queue, 1, &l);
    g_list_on = 0;
    wait_gpu();
    g_srv_next = 0;
    g_voff = 0;
    g_serial++;
}

static void free_window(void)
{
    int i;
    for (i = 0; i < NBUF; i++) {
        release(&g_back[i]);
        if (g_back_rtv[i] >= 0)
            g_rtv_used[g_back_rtv[i]] = 0;
        g_back_rtv[i] = -1;
    }
    release(&g_wdepth);
}

static int make_window(void)
{
    D3D12_CLEAR_VALUE cv;
    D3D12_DEPTH_STENCIL_VIEW_DESC dv;
    int i;

    for (i = 0; i < NBUF; i++) {
        if (!ok(IDXGISwapChain3_GetBuffer(g_swap, (UINT)i, &IID_ID3D12Resource, (void **)&g_back[i].res), "GetBuffer"))
            return 0;
        g_back[i].state = D3D12_RESOURCE_STATE_PRESENT;
        g_back_rtv[i] = slot(g_rtv_used);
        ID3D12Device_CreateRenderTargetView(g_dev, g_back[i].res, NULL, cpu(g_rtv_heap, g_rtv_inc, g_back_rtv[i]));
    }
    memset(&cv, 0, sizeof cv);
    cv.Format = DEPTH_FMT;
    cv.DepthStencil.Depth = 1.0f;
    if (!texture2d(&g_wdepth, g_bw, g_bh, 1, DEPTH_FMT, 1, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, &cv))
        return 0;
    memset(&dv, 0, sizeof dv);
    dv.Format = DEPTH_FMT;
    dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    if (g_wdsv < 0)
        g_wdsv = slot(g_dsv_used);
    ID3D12Device_CreateDepthStencilView(g_dev, g_wdepth.res, &dv, cpu(g_dsv_heap, g_dsv_inc, g_wdsv));
    return 1;
}

/* The window's buffers follow its size. ResizeBuffers starts the swap chain
 * again at its first buffer, so the index is read again (it was not, and a
 * resize drew into one buffer and presented another); a resize that fails
 * keeps the buffers it had, at their old size, instead of going on with
 * none (Mary: resizing and moving the window a few times closed the game). */
static void check_size(void)
{
    int w = 0, h = 0;
    plat_size(&w, &h);
    if (w <= 0 || h <= 0 || (w == g_bw && h == g_bh))
        return;
    flush();
    wait_gpu();
    free_window();
    if (ok(IDXGISwapChain3_ResizeBuffers(g_swap, NBUF, (UINT)w, (UINT)h, COLOR_FMT, 0), "ResizeBuffers")) {
        g_bw = w;
        g_bh = h;
    }
    if (!make_window()) {
        free_window();
        fprintf(stderr, "d3d12: the window's buffers could not be made again at %dx%d\n", g_bw, g_bh);
        g_bw = g_bh = 0;                /* tried again next frame */
    }
    g_bi = IDXGISwapChain3_GetCurrentBackBufferIndex(g_swap);
    g_bound_target = -1;
}

static void frame_begin(void)
{
    if (g_list_on)
        return;
    list_begin();
}

/* -------------------------------------------------------------- binding */

static void tsize(int t, int *w, int *h)
{
    if (t > 0 && t < g_ntg && g_tg[t].used) { *w = g_tg[t].w; *h = g_tg[t].h; }
    else { *w = g_bw; *h = g_bh; }
}

static dx_res *tcolor(int t) { return t > 0 ? &g_tg[t].color : &g_back[g_bi]; }

/* the window has no buffers: a resize that could not make them again */
static int no_window(int t) { return t <= 0 && !g_back[g_bi].res; }

static void bind_target(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, dsv;
    int t = g_cur, has_d;

    frame_begin();
    to_state(tcolor(t), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (t > 0 && g_tg[t].depth)
        to_state(&g_tg[t].dep, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    if (g_bound_target == t)
        return;
    rtv = cpu(g_rtv_heap, g_rtv_inc, t > 0 ? g_tg[t].rtv : g_back_rtv[g_bi]);
    has_d = t > 0 ? g_tg[t].depth : 1;
    dsv = cpu(g_dsv_heap, g_dsv_inc, t > 0 ? g_tg[t].dsv : g_wdsv);
    ID3D12GraphicsCommandList_OMSetRenderTargets(g_list, 1, &rtv, FALSE, has_d ? &dsv : NULL);
    g_bound_target = t;
}

static D3D12_RECT gl_rect(const rgl_rect *r, int tw, int th)
{
    D3D12_RECT o;
    o.left = r->x;
    o.top = th - (r->y + r->h);
    o.right = r->x + r->w;
    o.bottom = th - r->y;
    if (o.left < 0) o.left = 0;
    if (o.top < 0) o.top = 0;
    if (o.right > tw) o.right = tw;
    if (o.bottom > th) o.bottom = th;
    return o;
}

/* ------------------------------------------------------------ open/close */

static int make_root(void)
{
    D3D12_DESCRIPTOR_RANGE rs, rsm;
    D3D12_ROOT_PARAMETER p[3];
    D3D12_STATIC_SAMPLER_DESC sp;
    D3D12_ROOT_SIGNATURE_DESC d;
    ID3DBlob *blob = NULL, *err = NULL;
    PFN_D3D12_SERIALIZE_ROOT_SIGNATURE ser =
        (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)(void *)GetProcAddress(g_d3d, "D3D12SerializeRootSignature");

    if (!ser)
        return 0;
    memset(&rs, 0, sizeof rs);
    rs.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rs.NumDescriptors = 2;              /* t0 the texture, t1 the shadow map */
    rsm = rs;
    rsm.NumDescriptors = 1;
    rsm.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[0].Constants.Num32BitValues = 4;
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[1].DescriptorTable.NumDescriptorRanges = 1;
    p[1].DescriptorTable.pDescriptorRanges = &rs;
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[2] = p[1];
    p[2].DescriptorTable.pDescriptorRanges = &rsm;
    /* s1: the shadow map's, texel by texel, clamped */
    memset(&sp, 0, sizeof sp);
    sp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sp.AddressU = sp.AddressV = sp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sp.MaxLOD = D3D12_FLOAT32_MAX;
    sp.ShaderRegister = 1;
    sp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    memset(&d, 0, sizeof d);
    d.NumParameters = 3;
    d.pParameters = p;
    d.NumStaticSamplers = 1;
    d.pStaticSamplers = &sp;
    d.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    if (!ok(ser(&d, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature"))
        return 0;
    ok(ID3D12Device_CreateRootSignature(g_dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob),
                                        &IID_ID3D12RootSignature, (void **)&g_root), "CreateRootSignature");
    ID3D10Blob_Release(blob);
    return g_root != NULL;
}

static int dx_tex_new(void);
static void dx_tex_image(int t, int w, int h, int levels, const unsigned char *const *rgba);

static int dx_open(void *window, int width, int height)
{
    PFN_D3D12_CREATE_DEVICE create;
    HRESULT (WINAPI *mkfactory)(UINT, REFIID, void **);
    IDXGIAdapter1 *ad = NULL;
    D3D12_COMMAND_QUEUE_DESC qd;
    DXGI_SWAP_CHAIN_DESC1 sd;
    IDXGISwapChain1 *sc1 = NULL;
    UINT i;

    g_hwnd = (HWND)window;
    g_d3d = LoadLibraryA("d3d12.dll");
    g_dxgi = LoadLibraryA("dxgi.dll");
    if (!g_d3d || !g_dxgi) {
        printf("d3d12: no d3d12.dll / dxgi.dll\n");
        return 0;
    }
    create = (PFN_D3D12_CREATE_DEVICE)(void *)GetProcAddress(g_d3d, "D3D12CreateDevice");
    mkfactory = (HRESULT (WINAPI *)(UINT, REFIID, void **))(void *)GetProcAddress(g_dxgi, "CreateDXGIFactory2");
    if (!create || !mkfactory || !ok(mkfactory(0, &IID_IDXGIFactory4, (void **)&g_factory), "CreateDXGIFactory2"))
        return 0;
    /* the first hardware adapter that makes a device, as the system orders them */
    for (i = 0; IDXGIFactory4_EnumAdapters1(g_factory, i, &ad) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        IDXGIAdapter1_GetDesc1(ad, &d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            && SUCCEEDED(create((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&g_dev))) {
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, g_adapter, sizeof g_adapter, NULL, NULL);
            IDXGIAdapter1_Release(ad);
            break;
        }
        IDXGIAdapter1_Release(ad);
    }
    if (!g_dev) {
        printf("d3d12: no adapter makes a Direct3D 12 device\n");
        return 0;
    }
    memset(&qd, 0, sizeof qd);
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (!ok(ID3D12Device_CreateCommandQueue(g_dev, &qd, &IID_ID3D12CommandQueue, (void **)&g_queue), "queue"))
        return 0;

    g_bw = width > 0 ? width : 1;
    g_bh = height > 0 ? height : 1;
    {
        int w = 0, h = 0;
        plat_size(&w, &h);
        if (w > 0 && h > 0) { g_bw = w; g_bh = h; }
    }
    memset(&sd, 0, sizeof sd);
    sd.Width = (UINT)g_bw;
    sd.Height = (UINT)g_bh;
    sd.Format = COLOR_FMT;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = NBUF;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (!ok(IDXGIFactory4_CreateSwapChainForHwnd(g_factory, (IUnknown *)g_queue, g_hwnd, &sd, NULL, NULL, &sc1),
            "CreateSwapChainForHwnd"))
        return 0;
    IDXGISwapChain1_QueryInterface(sc1, &IID_IDXGISwapChain3, (void **)&g_swap);
    IDXGISwapChain1_Release(sc1);
    IDXGIFactory4_MakeWindowAssociation(g_factory, g_hwnd, DXGI_MWA_NO_ALT_ENTER);

    g_rtv_heap = heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RTV_MAX, 0);
    g_dsv_heap = heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, RTV_MAX, 0);
    g_srv_heap = heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SRV_RING, 1);
    g_samp_heap = heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, 1);
    g_rtv_inc = ID3D12Device_GetDescriptorHandleIncrementSize(g_dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_dsv_inc = ID3D12Device_GetDescriptorHandleIncrementSize(g_dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    g_srv_inc = ID3D12Device_GetDescriptorHandleIncrementSize(g_dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_samp_inc = ID3D12Device_GetDescriptorHandleIncrementSize(g_dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    if (!g_rtv_heap || !g_dsv_heap || !g_srv_heap || !g_samp_heap)
        return 0;
    for (i = 0; i < NBUF; i++)
        g_back_rtv[i] = -1;
    g_wdsv = -1;
    if (!make_window())
        return 0;

    if (!ok(ID3D12Device_CreateCommandAllocator(g_dev, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                &IID_ID3D12CommandAllocator, (void **)&g_alloc), "allocator")
        || !ok(ID3D12Device_CreateCommandList(g_dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_alloc, NULL,
                                              &IID_ID3D12GraphicsCommandList, (void **)&g_list), "list")
        || !ok(ID3D12Device_CreateFence(g_dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&g_fence), "fence"))
        return 0;
    ID3D12GraphicsCommandList_Close(g_list);
    g_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!make_root())
        return 0;
    g_vsize = 4u << 20;
    g_vbuf = buffer(g_vsize, D3D12_HEAP_TYPE_UPLOAD);
    if (!g_vbuf || FAILED(ID3D12Resource_Map(g_vbuf, 0, NULL, (void **)&g_vmap)))
        return 0;
    g_bi = IDXGISwapChain3_GetCurrentBackBufferIndex(g_swap);

    rgl_d3d12.max_texture = 16384;
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q;
        int s;
        rgl_d3d12.max_samples = 1;
        for (s = 2; s <= 16; s *= 2) {
            memset(&q, 0, sizeof q);
            q.Format = COLOR_FMT;
            q.SampleCount = (UINT)s;
            if (SUCCEEDED(ID3D12Device_CheckFeatureSupport(g_dev, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &q, sizeof q))
                && q.NumQualityLevels > 0) {
                q.Format = DEPTH_FMT;
                if (SUCCEEDED(ID3D12Device_CheckFeatureSupport(g_dev, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &q, sizeof q))
                    && q.NumQualityLevels > 0)
                    rgl_d3d12.max_samples = s;
            }
        }
    }
    {
        static const unsigned char white[4] = { 255, 255, 255, 255 };
        const unsigned char *lv[1];
        lv[0] = white;
        g_white = dx_tex_new();
        dx_tex_image(g_white, 1, 1, 1, lv);
    }
    printf("d3d12: %s, window %dx%d\n", g_adapter, g_bw, g_bh);
    return 1;
}

static void dx_close(void)
{
    int i;
    g_shadow = 0;
    if (!g_dev)
        return;
    flush();
    wait_gpu();
    for (i = 0; i < g_npsos; i++)
        ID3D12PipelineState_Release(g_psos[i].p);
    for (i = 0; i < g_ntex; i++)
        if (g_tex[i].used)
            release(&g_tex[i].r);
    for (i = 0; i < g_ntg; i++)
        if (g_tg[i].used) {
            release(&g_tg[i].color);
            release(&g_tg[i].dep);
        }
    free_window();
    upload_submit();
    if (g_ulist) ID3D12GraphicsCommandList_Release(g_ulist);
    if (g_ualloc) ID3D12CommandAllocator_Release(g_ualloc);
    g_ulist = NULL;
    g_ualloc = NULL;
    free(g_ubufs);
    g_ubufs = NULL;
    g_cubufs = 0;
    if (g_vbuf) ID3D12Resource_Release(g_vbuf);
    if (g_root) ID3D12RootSignature_Release(g_root);
    if (g_list) ID3D12GraphicsCommandList_Release(g_list);
    if (g_alloc) ID3D12CommandAllocator_Release(g_alloc);
    if (g_fence) ID3D12Fence_Release(g_fence);
    if (g_rtv_heap) ID3D12DescriptorHeap_Release(g_rtv_heap);
    if (g_dsv_heap) ID3D12DescriptorHeap_Release(g_dsv_heap);
    if (g_srv_heap) ID3D12DescriptorHeap_Release(g_srv_heap);
    if (g_samp_heap) ID3D12DescriptorHeap_Release(g_samp_heap);
    if (g_swap) IDXGISwapChain3_Release(g_swap);
    if (g_queue) ID3D12CommandQueue_Release(g_queue);
    if (g_factory) IDXGIFactory4_Release(g_factory);
    ID3D12Device_Release(g_dev);
    g_dev = NULL;
    if (g_event) CloseHandle(g_event);
}

static const char *dx_device(void) { return g_adapter; }

/* ------------------------------------------------------------- textures */

static int dx_tex_new(void)
{
    int i;
    for (i = 1; i < g_ntex; i++)
        if (!g_tex[i].used)
            break;
    if (i >= g_ntex) {
        int n = g_ntex ? g_ntex * 2 : 256;
        g_tex = (dx_tex *)realloc(g_tex, (size_t)n * sizeof *g_tex);
        memset(g_tex + g_ntex, 0, (size_t)(n - g_ntex) * sizeof *g_tex);
        if (!g_ntex)
            i = 1;
        g_ntex = n;
    }
    memset(&g_tex[i], 0, sizeof g_tex[i]);
    g_tex[i].used = 1;
    return i;
}

/* Rows into a texture's levels through an upload buffer, recorded in a list
 * of their own that runs before the next draws (flush, the same queue, so in
 * order) -- or sooner, when the buffers pass 64 MB or a resource is
 * released. Each upload used to make its own allocator and list, run them
 * and wait for the GPU: one wait per texture, hundreds in a load. */
static void upload_submit(void)
{
    ID3D12CommandList *one;
    int i;
    if (!g_ulist_on)
        return;
    g_ulist_on = 0;
    ID3D12GraphicsCommandList_Close(g_ulist);
    one = (ID3D12CommandList *)g_ulist;
    ID3D12CommandQueue_ExecuteCommandLists(g_queue, 1, &one);
    wait_gpu();
    for (i = 0; i < g_nubufs; i++)
        ID3D12Resource_Release(g_ubufs[i]);
    g_nubufs = 0;
    g_ubytes = 0;
}

static int upload_begin(UINT64 bytes)
{
    if (g_ulist_on && g_ubytes + bytes > ((UINT64)64 << 20))
        upload_submit();
    if (!g_ualloc) {
        if (!ok(ID3D12Device_CreateCommandAllocator(g_dev, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    &IID_ID3D12CommandAllocator, (void **)&g_ualloc), "upload allocator"))
            return 0;
        if (!ok(ID3D12Device_CreateCommandList(g_dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_ualloc, NULL,
                                               &IID_ID3D12GraphicsCommandList, (void **)&g_ulist), "upload list")) {
            ID3D12CommandAllocator_Release(g_ualloc);
            g_ualloc = NULL;
            return 0;
        }
        ID3D12GraphicsCommandList_Close(g_ulist);
    }
    if (!g_ulist_on) {
        ID3D12CommandAllocator_Reset(g_ualloc);
        ID3D12GraphicsCommandList_Reset(g_ulist, g_ualloc, NULL);
        g_ulist_on = 1;
    }
    return 1;
}

static void upload(dx_res *r, int x, int y, int w, int h, int levels, const unsigned char *const *rgba)
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[16];
    ID3D12Resource *ub;
    ID3D12GraphicsCommandList *keep = g_list;
    unsigned char *map;
    UINT64 total = 0;
    int l, lw = w, lh = h;

    if (levels > 16)
        levels = 16;
    for (l = 0; l < levels; l++) {
        UINT pitch = ((UINT)lw * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
        total = (total + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & ~(UINT64)(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
        fp[l].Offset = total;
        fp[l].Footprint.Format = COLOR_FMT;
        fp[l].Footprint.Width = (UINT)lw;
        fp[l].Footprint.Height = (UINT)lh;
        fp[l].Footprint.Depth = 1;
        fp[l].Footprint.RowPitch = pitch;
        total += (UINT64)pitch * lh;
        lw = lw > 1 ? lw / 2 : 1;
        lh = lh > 1 ? lh / 2 : 1;
    }
    if (!upload_begin(total))
        return;
    ub = buffer(total, D3D12_HEAP_TYPE_UPLOAD);
    if (!ub || FAILED(ID3D12Resource_Map(ub, 0, NULL, (void **)&map))) {
        if (ub) ID3D12Resource_Release(ub);
        return;
    }
    for (l = 0; l < levels; l++) {
        int row;
        for (row = 0; row < (int)fp[l].Footprint.Height; row++)
            memcpy(map + fp[l].Offset + (UINT64)row * fp[l].Footprint.RowPitch,
                   rgba[l] + (size_t)row * fp[l].Footprint.Width * 4, (size_t)fp[l].Footprint.Width * 4);
    }
    ID3D12Resource_Unmap(ub, 0, NULL);
    if (g_nubufs == g_cubufs) {
        int n = g_cubufs ? g_cubufs * 2 : 256;
        ID3D12Resource **nb = (ID3D12Resource **)realloc(g_ubufs, (size_t)n * sizeof *nb);
        if (!nb) {
            ID3D12Resource_Release(ub);
            return;
        }
        g_ubufs = nb;
        g_cubufs = n;
    }
    g_ubufs[g_nubufs++] = ub;
    g_ubytes += total;
    g_list = g_ulist;
    to_state(r, D3D12_RESOURCE_STATE_COPY_DEST);
    for (l = 0; l < levels; l++) {
        D3D12_TEXTURE_COPY_LOCATION dst, src;
        memset(&dst, 0, sizeof dst);
        memset(&src, 0, sizeof src);
        dst.pResource = r->res;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = (UINT)l;
        src.pResource = ub;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = fp[l];
        ID3D12GraphicsCommandList_CopyTextureRegion(g_ulist, &dst, l ? 0 : (UINT)x, l ? 0 : (UINT)y, 0, &src, NULL);
    }
    to_state(r, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    g_list = keep;
}

static void before_write(dx_tex *t)
{
    if (t->serial == (unsigned)g_serial && g_list_on)
        flush();
}

static void dx_tex_image(int i, int w, int h, int levels, const unsigned char *const *rgba)
{
    dx_tex *t;
    if (i <= 0 || i >= g_ntex || !g_tex[i].used)
        return;
    t = &g_tex[i];
    before_write(t);
    if (t->r.res && (t->w != w || t->h != h || t->levels != levels))
        release(&t->r);
    if (!t->r.res && !texture2d(&t->r, w, h, levels, COLOR_FMT, 1, D3D12_RESOURCE_FLAG_NONE, NULL))
        return;
    t->w = w;
    t->h = h;
    t->levels = levels;
    upload(&t->r, 0, 0, w, h, levels, rgba);
}

static void dx_tex_sub(int i, int x, int y, int w, int h, const void *rgba)
{
    const unsigned char *lv[1];
    if (i <= 0 || i >= g_ntex || !g_tex[i].used || !g_tex[i].r.res)
        return;
    before_write(&g_tex[i]);
    lv[0] = (const unsigned char *)rgba;
    upload(&g_tex[i].r, x, y, w, h, 1, lv);
}

static void dx_tex_free(int i)
{
    if (i <= 0 || i >= g_ntex || !g_tex[i].used)
        return;
    before_write(&g_tex[i]);
    release(&g_tex[i].r);
    g_tex[i].used = 0;
}

/* -------------------------------------------------------------- targets */

static int dx_target_new(int w, int h, int samples, int depth)
{
    D3D12_CLEAR_VALUE cv;
    D3D12_RENDER_TARGET_VIEW_DESC rv;
    D3D12_DEPTH_STENCIL_VIEW_DESC dv;
    dx_target *g;
    int i, s = 1;

    while (s * 2 <= samples && s * 2 <= rgl_d3d12.max_samples)
        s *= 2;
    for (i = 1; i < g_ntg; i++)
        if (!g_tg[i].used)
            break;
    if (i >= g_ntg) {
        int n = g_ntg ? g_ntg * 2 : 8;
        g_tg = (dx_target *)realloc(g_tg, (size_t)n * sizeof *g_tg);
        memset(g_tg + g_ntg, 0, (size_t)(n - g_ntg) * sizeof *g_tg);
        if (!g_ntg)
            i = 1;
        g_ntg = n;
    }
    g = &g_tg[i];
    memset(g, 0, sizeof *g);
    g->w = w;
    g->h = h;
    g->samples = s;
    g->depth = depth;
    memset(&cv, 0, sizeof cv);
    cv.Format = COLOR_FMT;
    if (!texture2d(&g->color, w, h, 1, COLOR_FMT, s, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &cv))
        return 0;
    g->rtv = slot(g_rtv_used);
    memset(&rv, 0, sizeof rv);
    rv.Format = COLOR_FMT;
    rv.ViewDimension = s > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
    ID3D12Device_CreateRenderTargetView(g_dev, g->color.res, &rv, cpu(g_rtv_heap, g_rtv_inc, g->rtv));
    g->dsv = -1;
    if (depth) {
        cv.Format = DEPTH_FMT;
        cv.DepthStencil.Depth = 1.0f;
        if (!texture2d(&g->dep, w, h, 1, DEPTH_FMT, s, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, &cv))
            return 0;
        g->dsv = slot(g_dsv_used);
        memset(&dv, 0, sizeof dv);
        dv.Format = DEPTH_FMT;
        dv.ViewDimension = s > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
        ID3D12Device_CreateDepthStencilView(g_dev, g->dep.res, &dv, cpu(g_dsv_heap, g_dsv_inc, g->dsv));
    }
    g->used = 1;
    return i;
}

static void dx_target_free(int i)
{
    if (i <= 0 || i >= g_ntg || !g_tg[i].used)
        return;
    flush();
    if (g_cur == i)
        g_cur = 0;
    if (g_shadow == i)
        g_shadow = 0;
    release(&g_tg[i].color);
    release(&g_tg[i].dep);
    if (g_tg[i].rtv >= 0) g_rtv_used[g_tg[i].rtv] = 0;
    if (g_tg[i].dsv >= 0) g_dsv_used[g_tg[i].dsv] = 0;
    g_tg[i].used = 0;
    g_bound_target = -1;
}

static void dx_target_use(int t)
{
    if (t < 0 || (t > 0 && (t >= g_ntg || !g_tg[t].used)))
        t = 0;
    g_cur = t;
}

static void dx_target_size(int t, int *w, int *h)
{
    if (t == 0) {
        int pw = 0, ph = 0;
        plat_size(&pw, &ph);
        if (pw > 0 && ph > 0 && !g_list_on) { *w = pw; *h = ph; return; }
    }
    tsize(t, w, h);
}

/* ---------------------------------------------------------------- draws */

static void dx_clear(const rgl_rect *sc, int color, const float rgba[4], int depth, float z)
{
    D3D12_RECT r;
    int w, h, t = g_cur;

    if (!color && !depth)
        return;
    if (!g_list_on)
        check_size();
    if (no_window(t))
        return;
    bind_target();
    tsize(t, &w, &h);
    if (sc)
        r = gl_rect(sc, w, h);
    else {
        r.left = r.top = 0;
        r.right = w;
        r.bottom = h;
    }
    if (r.right <= r.left || r.bottom <= r.top)
        return;
    if (color)
        ID3D12GraphicsCommandList_ClearRenderTargetView(g_list,
            cpu(g_rtv_heap, g_rtv_inc, t > 0 ? g_tg[t].rtv : g_back_rtv[g_bi]), rgba, 1, &r);
    if (depth && (t == 0 || g_tg[t].depth))
        ID3D12GraphicsCommandList_ClearDepthStencilView(g_list,
            cpu(g_dsv_heap, g_dsv_inc, t > 0 ? g_tg[t].dsv : g_wdsv), D3D12_CLEAR_FLAG_DEPTH, z, 0, 1, &r);
}

static D3D12_BLEND dx_bf(int f)
{
    static const D3D12_BLEND k[] = {
        D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA,
        D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_INV_DEST_ALPHA, D3D12_BLEND_SRC_COLOR,
        D3D12_BLEND_INV_SRC_COLOR, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR };
    return f >= 0 && f < 10 ? k[f] : D3D12_BLEND_ONE;
}

/* the colour factors' alpha twins: Direct3D wants alpha factors for the
 * alpha channel */
static D3D12_BLEND dx_bfa(int f)
{
    D3D12_BLEND b = dx_bf(f);
    if (b == D3D12_BLEND_SRC_COLOR) return D3D12_BLEND_SRC_ALPHA;
    if (b == D3D12_BLEND_INV_SRC_COLOR) return D3D12_BLEND_INV_SRC_ALPHA;
    if (b == D3D12_BLEND_DEST_COLOR) return D3D12_BLEND_DEST_ALPHA;
    if (b == D3D12_BLEND_INV_DEST_COLOR) return D3D12_BLEND_INV_DEST_ALPHA;
    return b;
}

static ID3D12PipelineState *get_pso(const rgl_pipe *k, int samples, int depth)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
    D3D12_INPUT_ELEMENT_DESC il[4];
    ID3D12PipelineState *p = NULL;
    int i;

    for (i = 0; i < g_npsos; i++)
        if (g_psos[i].samples == samples && g_psos[i].depth == depth
            && memcmp(&g_psos[i].key, k, sizeof *k) == 0)
            return g_psos[i].p;
    memset(il, 0, sizeof il);
    il[0].SemanticName = "POSITION"; il[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; il[0].AlignedByteOffset = 0;
    il[1].SemanticName = "COLOR"; il[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM; il[1].AlignedByteOffset = 16;
    il[2].SemanticName = "TEXCOORD"; il[2].Format = DXGI_FORMAT_R32G32_FLOAT; il[2].AlignedByteOffset = 20;
    il[3].SemanticName = "TEXCOORD"; il[3].SemanticIndex = 1; il[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    il[3].AlignedByteOffset = 28;
    for (i = 0; i < 4; i++)
        il[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    memset(&d, 0, sizeof d);
    d.pRootSignature = g_root;
    d.VS.pShaderBytecode = k_dx_vs;
    d.VS.BytecodeLength = sizeof k_dx_vs;
    d.PS.pShaderBytecode = k_dx_ps;
    d.PS.BytecodeLength = sizeof k_dx_ps;
    d.BlendState.RenderTarget[0].BlendEnable = k->blend;
    d.BlendState.RenderTarget[0].SrcBlend = dx_bf(k->src);
    d.BlendState.RenderTarget[0].DestBlend = dx_bf(k->dst);
    d.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    d.BlendState.RenderTarget[0].SrcBlendAlpha = dx_bfa(k->src);
    d.BlendState.RenderTarget[0].DestBlendAlpha = dx_bfa(k->dst);
    d.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    d.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    d.BlendState.RenderTarget[0].RenderTargetWriteMask = k->mask;
    d.SampleMask = 0xffffffffu;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = k->cull == RGL_CULL_FRONT ? D3D12_CULL_MODE_FRONT
                               : k->cull == RGL_CULL_BACK ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
    d.RasterizerState.FrontCounterClockwise = TRUE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.RasterizerState.MultisampleEnable = samples > 1;
    d.DepthStencilState.DepthEnable = k->depth_test && depth;
    d.DepthStencilState.DepthWriteMask = k->depth_write && depth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    d.InputLayout.pInputElementDescs = il;
    d.InputLayout.NumElements = 4;
    d.PrimitiveTopologyType = k->topo == RGL_LINES ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = COLOR_FMT;
    d.DSVFormat = depth ? DEPTH_FMT : DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = (UINT)samples;
    if (!ok(ID3D12Device_CreateGraphicsPipelineState(g_dev, &d, &IID_ID3D12PipelineState, (void **)&p), "PSO"))
        return NULL;
    if (g_npsos >= g_cpsos) {
        g_cpsos = g_cpsos ? g_cpsos * 2 : 64;
        g_psos = (dx_pso *)realloc(g_psos, (size_t)g_cpsos * sizeof *g_psos);
    }
    g_psos[g_npsos].key = *k;
    g_psos[g_npsos].samples = samples;
    g_psos[g_npsos].depth = depth;
    g_psos[g_npsos].p = p;
    g_npsos++;
    return p;
}

static int get_sampler(const rgl_samp *s)
{
    unsigned key = (unsigned)s->mag_linear | (unsigned)s->min_linear << 1 | (unsigned)s->mip << 2
                 | (unsigned)s->wrap_s_clamp << 4 | (unsigned)s->wrap_t_clamp << 5 | (unsigned)s->aniso << 8;
    D3D12_SAMPLER_DESC d;
    int i;

    for (i = 0; i < g_nsamps; i++)
        if (g_samps[i].key == key)
            return g_samps[i].slot;
    if (g_nsamps >= 64)
        return 0;
    memset(&d, 0, sizeof d);
    if (s->aniso > 1)
        d.Filter = D3D12_FILTER_ANISOTROPIC;
    else
        d.Filter = (D3D12_FILTER)((s->min_linear ? 0x10 : 0) | (s->mag_linear ? 0x4 : 0) | (s->mip == 2 ? 0x1 : 0));
    d.AddressU = s->wrap_s_clamp ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d.AddressV = s->wrap_t_clamp ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d.MaxAnisotropy = s->aniso > 1 ? s->aniso : 1;
    d.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    d.MaxLOD = s->mip ? 16.0f : 0.0f;
    ID3D12Device_CreateSampler(g_dev, &d, cpu(g_samp_heap, g_samp_inc, g_nsamps));
    g_samps[g_nsamps].key = key;
    g_samps[g_nsamps].slot = g_nsamps;
    return g_nsamps++;
}

/* triangles or lines into the current target: the common path of draws and
 * of the scaled blit */
static void put(const rgl_pipe *k, const rgl_frag *f, ID3D12Resource *srv_res, int srv_ms,
                const rgl_samp *s, const D3D12_VIEWPORT *vp, const D3D12_RECT *sc,
                const rgl_vtx *v, int n)
{
    UINT64 bytes = (UINT64)n * sizeof *v;
    ID3D12PipelineState *p;
    D3D12_SHADER_RESOURCE_VIEW_DESC sd;
    D3D12_VERTEX_BUFFER_VIEW vb;
    ID3D12Resource *shadow_res;
    int t = g_cur, samples, depth, ss;

    if (g_voff + bytes > g_vsize) {
        /* the frame outgrew the buffer: a bigger one, so the frames after
         * draw without waiting for the GPU partway (REAL-TIME SHADOWS and
         * SMOOTH MODELS draw a frame's fighters twice and thicker) */
        UINT64 want = g_voff + bytes;
        flush();
        ID3D12Resource_Release(g_vbuf);
        while (g_vsize < want && g_vsize < (256u << 20))
            g_vsize *= 2;
        while (g_vsize < bytes)
            g_vsize *= 2;
        g_vbuf = buffer(g_vsize, D3D12_HEAP_TYPE_UPLOAD);
        ID3D12Resource_Map(g_vbuf, 0, NULL, (void **)&g_vmap);
    }
    if (g_srv_next + 2 > SRV_RING && g_list_on)
        flush();
    bind_target();
    /* t1: the shadow map for a draw that receives, white (nothing nearer
     * the light) for the rest */
    shadow_res = g_tex[g_white].r.res;
    if ((f->shadow & 15) == RGL_SHADOW_RECEIVE && g_shadow > 0 && g_shadow < g_ntg && g_tg[g_shadow].used
        && g_shadow != t) {
        to_state(&g_tg[g_shadow].color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        shadow_res = g_tg[g_shadow].color.res;
    }
    samples = t > 0 ? g_tg[t].samples : 1;
    depth = t > 0 ? g_tg[t].depth : 1;
    p = get_pso(k, samples, depth);
    if (!p)
        return;
    memset(&sd, 0, sizeof sd);
    sd.Format = COLOR_FMT;
    sd.ViewDimension = srv_ms ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = (UINT)-1;
    ID3D12Device_CreateShaderResourceView(g_dev, srv_res, &sd, cpu(g_srv_heap, g_srv_inc, g_srv_next));
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    ID3D12Device_CreateShaderResourceView(g_dev, shadow_res, &sd, cpu(g_srv_heap, g_srv_inc, g_srv_next + 1));
    ss = get_sampler(s);
    memcpy(g_vmap + g_voff, v, (size_t)bytes);
    vb.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(g_vbuf) + g_voff;
    vb.SizeInBytes = (UINT)bytes;
    vb.StrideInBytes = sizeof *v;
    g_voff += (bytes + 255) & ~(UINT64)255;

    ID3D12GraphicsCommandList_SetPipelineState(g_list, p);
    ID3D12GraphicsCommandList_RSSetViewports(g_list, 1, vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(g_list, 1, sc);
    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(g_list, 0, 4, f, 0);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(g_list, 1, gpu(g_srv_heap, g_srv_inc, g_srv_next));
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(g_list, 2, gpu(g_samp_heap, g_samp_inc, ss));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(g_list, k->topo == RGL_LINES ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
                                                                                  : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_IASetVertexBuffers(g_list, 0, 1, &vb);
    ID3D12GraphicsCommandList_DrawInstanced(g_list, (UINT)n, 1, 0, 0);
    g_srv_next += 2;
}

static void dx_draw(const rgl_pipe *k, const rgl_frag *f, int tex, const rgl_samp *s,
                    const rgl_rect *viewport, const rgl_rect *scissor, const rgl_vtx *v, int n)
{
    D3D12_VIEWPORT vp;
    D3D12_RECT sc;
    rgl_samp plain;
    int w, h;

    if (n <= 0)
        return;
    if (!g_list_on)
        check_size();
    if (no_window(g_cur))
        return;
    if (tex <= 0 || tex >= g_ntex || !g_tex[tex].used || !g_tex[tex].r.res) {
        tex = g_white;
        memset(&plain, 0, sizeof plain);
        s = &plain;
    }
    g_tex[tex].serial = (unsigned)g_serial;
    tsize(g_cur, &w, &h);
    vp.TopLeftX = (float)viewport->x;
    vp.TopLeftY = (float)(h - (viewport->y + viewport->h));
    vp.Width = (float)viewport->w;
    vp.Height = (float)viewport->h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    if (scissor)
        sc = gl_rect(scissor, w, h);
    else {
        sc.left = sc.top = 0;
        sc.right = w;
        sc.bottom = h;
    }
    if (sc.right <= sc.left || sc.bottom <= sc.top || vp.Width <= 0 || vp.Height <= 0)
        return;
    put(k, f, g_tex[tex].r.res, 0, s, &vp, &sc, v, n);
}

/* --------------------------------------------------- blits and reads */

static void dx_blit(int src, const rgl_rect *a, int dst, const rgl_rect *b, int linear)
{
    int sw, sh, dw, dh, keep = g_cur;

    if (src > 0 && (src >= g_ntg || !g_tg[src].used)) return;
    if (dst > 0 && (dst >= g_ntg || !g_tg[dst].used)) return;
    if (!g_list_on)
        check_size();
    if (no_window(src) || no_window(dst))
        return;
    frame_begin();
    tsize(src, &sw, &sh);
    tsize(dst, &dw, &dh);
    if (src > 0 && g_tg[src].samples > 1) {
        /* a multisampled source: resolved (the game resolves it whole, to a
         * buffer of the same size) */
        to_state(tcolor(src), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        to_state(tcolor(dst), D3D12_RESOURCE_STATE_RESOLVE_DEST);
        ID3D12GraphicsCommandList_ResolveSubresource(g_list, tcolor(dst)->res, 0, tcolor(src)->res, 0, COLOR_FMT);
        g_bound_target = -1;
        return;
    }
    {
        /* a quad over the destination rectangle, sampling the source's */
        rgl_pipe k;
        rgl_frag f;
        rgl_samp s;
        rgl_vtx q[6];
        D3D12_VIEWPORT vp;
        D3D12_RECT sc;
        float u0 = (float)a->x / sw, u1 = (float)(a->x + a->w) / sw;
        float v0 = 1.0f - (float)a->y / sh, v1 = 1.0f - (float)(a->y + a->h) / sh;
        int x0 = b->x < b->x + b->w ? b->x : b->x + b->w, y0 = b->y < b->y + b->h ? b->y : b->y + b->h;
        int i;

        if (b->w < 0) { float t = u0; u0 = u1; u1 = t; }
        if (b->h < 0) { float t = v0; v0 = v1; v1 = t; }
        memset(&k, 0, sizeof k);
        k.topo = RGL_TRIS;
        k.mask = 15;
        memset(&f, 0, sizeof f);
        f.tex_mode = RGL_TEX_REPLACE;
        memset(&s, 0, sizeof s);
        s.mag_linear = s.min_linear = (unsigned char)linear;
        s.wrap_s_clamp = s.wrap_t_clamp = 1;
        s.aniso = 1;
        memset(q, 0, sizeof q);
        {
            /* (x, y) in clip space, (u, v) in the source */
            const float c[6][4] = { { -1, -1, u0, v0 }, { 1, -1, u1, v0 }, { 1, 1, u1, v1 },
                                    { -1, -1, u0, v0 }, { 1, 1, u1, v1 }, { -1, 1, u0, v1 } };
            for (i = 0; i < 6; i++) {
                q[i].x = c[i][0]; q[i].y = c[i][1]; q[i].z = 0.0f; q[i].w = 1.0f;
                q[i].u = c[i][2]; q[i].v = c[i][3];
                q[i].rgba[0] = q[i].rgba[1] = q[i].rgba[2] = q[i].rgba[3] = 255;
            }
        }
        to_state(tcolor(src), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        g_cur = dst;
        vp.TopLeftX = (float)x0;
        vp.TopLeftY = (float)(dh - (y0 + abs(b->h)));
        vp.Width = (float)abs(b->w);
        vp.Height = (float)abs(b->h);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        sc.left = (LONG)vp.TopLeftX;
        sc.top = (LONG)vp.TopLeftY;
        sc.right = sc.left + (LONG)vp.Width;
        sc.bottom = sc.top + (LONG)vp.Height;
        put(&k, &f, tcolor(src)->res, 0, &s, &vp, &sc, q, 6);
        g_cur = keep;
    }
}

static void dx_read(int t, const rgl_rect *r, unsigned char *out)
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    D3D12_TEXTURE_COPY_LOCATION dst, src;
    D3D12_BOX box;
    D3D12_RECT rr;
    ID3D12Resource *rb;
    unsigned char *map;
    int tw, th, j, w, h;

    if (t > 0 && (t >= g_ntg || !g_tg[t].used))
        t = 0;
    if (!g_list_on)
        check_size();
    if (no_window(t)) {
        memset(out, 0, (size_t)r->w * r->h * 4);
        return;
    }
    frame_begin();
    tsize(t, &tw, &th);
    rr = gl_rect(r, tw, th);
    memset(out, 0, (size_t)r->w * r->h * 4);
    w = rr.right - rr.left;
    h = rr.bottom - rr.top;
    if (w <= 0 || h <= 0)
        return;
    memset(&fp, 0, sizeof fp);
    fp.Footprint.Format = COLOR_FMT;
    fp.Footprint.Width = (UINT)w;
    fp.Footprint.Height = (UINT)h;
    fp.Footprint.Depth = 1;
    fp.Footprint.RowPitch = ((UINT)w * 4 + 255) & ~255u;
    rb = buffer((UINT64)fp.Footprint.RowPitch * h, D3D12_HEAP_TYPE_READBACK);
    if (!rb)
        return;
    to_state(tcolor(t), D3D12_RESOURCE_STATE_COPY_SOURCE);
    g_bound_target = -1;
    memset(&dst, 0, sizeof dst);
    dst.pResource = rb;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = fp;
    memset(&src, 0, sizeof src);
    src.pResource = tcolor(t)->res;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    box.left = (UINT)rr.left; box.top = (UINT)rr.top; box.front = 0;
    box.right = (UINT)rr.right; box.bottom = (UINT)rr.bottom; box.back = 1;
    ID3D12GraphicsCommandList_CopyTextureRegion(g_list, &dst, 0, 0, 0, &src, &box);
    flush();
    if (SUCCEEDED(ID3D12Resource_Map(rb, 0, NULL, (void **)&map))) {
        for (j = 0; j < h; j++) {
            int gy = th - 1 - (rr.top + j) - r->y;
            if (gy < 0 || gy >= r->h)
                continue;
            memcpy(out + ((size_t)gy * r->w + (rr.left - r->x)) * 4, map + (size_t)j * fp.Footprint.RowPitch, (size_t)w * 4);
        }
        ID3D12Resource_Unmap(rb, 0, NULL);
    }
    ID3D12Resource_Release(rb);
}

static void dx_present(void)
{
    if (!g_list_on)
        check_size();
    frame_begin();
    to_state(&g_back[g_bi], D3D12_RESOURCE_STATE_PRESENT);
    flush();
    IDXGISwapChain3_Present(g_swap, 1, 0);
    wait_gpu();
    g_bi = IDXGISwapChain3_GetCurrentBackBufferIndex(g_swap);
    g_bound_target = -1;
}

static void dx_finish(void) { }

static void dx_shadow_map(int t)
{
    g_shadow = t > 0 && t < g_ntg && g_tg[t].used ? t : 0;
    if (g_shadow && g_list_on)
        to_state(&g_tg[g_shadow].color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

rgl_backend rgl_d3d12 = {
    "Direct3D 12", dx_open, dx_close, dx_device, 16384, 1,
    dx_tex_new, dx_tex_image, dx_tex_sub, dx_tex_free,
    dx_target_new, dx_target_free, dx_target_use, dx_target_size,
    dx_clear, dx_draw, dx_blit, dx_read, dx_present, dx_finish, dx_shadow_map,
};

#endif
