/*
 * rgl_vk.c -- the Vulkan backend. Port, not in the binary.
 *
 * Draws what rgl_ff.c assembles (see rgl.h). Vulkan 1.0 and two extensions
 * every desktop driver has (VK_KHR_surface with its Win32 or SDL surface,
 * VK_KHR_swapchain), loaded at run time from vulkan-1.dll / libvulkan.so.1:
 * the game does not link against Vulkan, and a machine without it falls
 * back to OpenGL.
 *
 * How OpenGL's model is kept:
 *
 *   - One frame in flight, waited for at the start of the next: OpenGL
 *     drew with glFinish after every swap (win32_gl.c plat_swap), so the
 *     frame the player sees was never more than one behind.
 *   - Commands go into one command buffer in the order the game issues
 *     them. A texture changed after a draw that used it in the same frame,
 *     or pixels read back, first runs what is recorded and waits ("flush"),
 *     which is what an OpenGL driver does in those cases too.
 *   - Every target (the window, and each framebuffer object game_main.c
 *     makes for its 3D resolution and antialiasing) is a colour image and a
 *     depth image drawn into with a render pass that loads what is there:
 *     clears are vkCmdClearAttachments, as glClear inside the scissor.
 *   - Pipelines are made when a state is first drawn with and kept, keyed
 *     by rgl_pipe and the render pass; viewport and scissor are dynamic.
 *   - Clip space: the vertex shader turns OpenGL's (y up, z -w..w) into
 *     Vulkan's (y down, z 0..w). Vulkan decides the winding in its own
 *     y-down framebuffer space, where the flip has already been undone, so
 *     OpenGL's front faces stay VK_FRONT_FACE_COUNTER_CLOCKWISE (checked by
 *     tests/rgl_test.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_NO_PROTOTYPES
#ifdef _WIN32
#  include <windows.h>
#  define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan_core.h>
#ifdef _WIN32
#  include <vulkan/vulkan_win32.h>
#else
#  include <dlfcn.h>
#endif

#include "rgl.h"
#include "rgl_shaders_vk.h"
#include "../platform/platform.h"

const char *const *(*rgl_vk_instance_exts)(unsigned *count);
int (*rgl_vk_make_surface)(void *window, void *instance, void *surface);

/* ------------------------------------------------------------ the loader */

static PFN_vkGetInstanceProcAddr gipa;

#define VKF(n) static PFN_##n n
VKF(vkCreateInstance); VKF(vkEnumerateInstanceExtensionProperties);
VKF(vkDestroyInstance); VKF(vkEnumeratePhysicalDevices); VKF(vkGetPhysicalDeviceProperties);
VKF(vkGetPhysicalDeviceFeatures); VKF(vkGetPhysicalDeviceQueueFamilyProperties);
VKF(vkGetPhysicalDeviceMemoryProperties); VKF(vkGetPhysicalDeviceFormatProperties);
VKF(vkCreateDevice); VKF(vkGetDeviceProcAddr); VKF(vkDestroySurfaceKHR);
VKF(vkGetPhysicalDeviceSurfaceSupportKHR); VKF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
VKF(vkGetPhysicalDeviceSurfaceFormatsKHR); VKF(vkGetPhysicalDeviceSurfacePresentModesKHR);
VKF(vkEnumerateDeviceExtensionProperties);
#ifdef _WIN32
VKF(vkCreateWin32SurfaceKHR);
#endif
/* device level */
VKF(vkDestroyDevice); VKF(vkGetDeviceQueue); VKF(vkDeviceWaitIdle);
VKF(vkCreateSwapchainKHR); VKF(vkDestroySwapchainKHR); VKF(vkGetSwapchainImagesKHR);
VKF(vkAcquireNextImageKHR); VKF(vkQueuePresentKHR); VKF(vkQueueSubmit); VKF(vkQueueWaitIdle);
VKF(vkCreateImage); VKF(vkDestroyImage); VKF(vkCreateImageView); VKF(vkDestroyImageView);
VKF(vkGetImageMemoryRequirements); VKF(vkBindImageMemory);
VKF(vkCreateBuffer); VKF(vkDestroyBuffer); VKF(vkGetBufferMemoryRequirements); VKF(vkBindBufferMemory);
VKF(vkAllocateMemory); VKF(vkFreeMemory); VKF(vkMapMemory); VKF(vkUnmapMemory);
VKF(vkCreateRenderPass); VKF(vkDestroyRenderPass); VKF(vkCreateFramebuffer); VKF(vkDestroyFramebuffer);
VKF(vkCreateShaderModule); VKF(vkDestroyShaderModule); VKF(vkCreatePipelineLayout);
VKF(vkDestroyPipelineLayout); VKF(vkCreateGraphicsPipelines); VKF(vkDestroyPipeline);
VKF(vkCreatePipelineCache); VKF(vkGetPipelineCacheData); VKF(vkDestroyPipelineCache);
VKF(vkCreateDescriptorSetLayout); VKF(vkDestroyDescriptorSetLayout); VKF(vkCreateDescriptorPool);
VKF(vkDestroyDescriptorPool); VKF(vkResetDescriptorPool); VKF(vkAllocateDescriptorSets);
VKF(vkUpdateDescriptorSets); VKF(vkCreateSampler); VKF(vkDestroySampler);
VKF(vkCreateCommandPool); VKF(vkDestroyCommandPool); VKF(vkAllocateCommandBuffers);
VKF(vkResetCommandBuffer); VKF(vkBeginCommandBuffer); VKF(vkEndCommandBuffer);
VKF(vkCreateFence); VKF(vkDestroyFence); VKF(vkWaitForFences); VKF(vkResetFences);
VKF(vkCreateSemaphore); VKF(vkDestroySemaphore);
VKF(vkCmdBeginRenderPass); VKF(vkCmdEndRenderPass); VKF(vkCmdBindPipeline);
VKF(vkCmdSetViewport); VKF(vkCmdSetScissor); VKF(vkCmdPushConstants);
VKF(vkCmdBindDescriptorSets); VKF(vkCmdBindVertexBuffers); VKF(vkCmdDraw);
VKF(vkCmdClearAttachments); VKF(vkCmdPipelineBarrier); VKF(vkCmdCopyBufferToImage);
VKF(vkCmdCopyImageToBuffer); VKF(vkCmdBlitImage); VKF(vkCmdResolveImage);

static int load_vulkan(void)
{
#ifdef _WIN32
    HMODULE lib = LoadLibraryA("vulkan-1.dll");
    if (!lib)
        return 0;
    gipa = (PFN_vkGetInstanceProcAddr)(void *)GetProcAddress(lib, "vkGetInstanceProcAddr");
#else
    void *lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!lib)
        lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib)
        return 0;
    gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
#endif
    if (!gipa)
        return 0;
    vkCreateInstance = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
    vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)
        gipa(NULL, "vkEnumerateInstanceExtensionProperties");
    return vkCreateInstance != NULL;
}

/* ----------------------------------------------------------------- state */

typedef struct {
    VkImage img;
    VkDeviceMemory mem;
    VkImageView view;
    VkImageLayout layout;
    VkImageAspectFlags aspect;
    int levels;
} vk_image;

typedef struct {
    int used, w, h, levels;
    vk_image im;
    unsigned serial;                    /* the frame that last drew with it */
} vk_tex;

typedef struct {
    int used, w, h, samples, depth;
    VkFormat fmt;
    vk_image color, dep;
    int pass;                           /* index into g_pass */
    VkFramebuffer fb;
} vk_target;

typedef struct { VkFormat fmt; VkSampleCountFlagBits samples; int depth; VkRenderPass rp; } vk_pass;
typedef struct { rgl_pipe key; int pass; VkPipeline p; } vk_pipe;
typedef struct { unsigned key; VkSampler s; } vk_samp;

static VkInstance g_inst;
static VkSurfaceKHR g_surf;
static VkPhysicalDevice g_gpu;
static VkPhysicalDeviceProperties g_props;
static VkPhysicalDeviceMemoryProperties g_mem;
static VkDevice g_dev;
static VkQueue g_queue;
static unsigned g_family;
static int g_aniso_ok;
static void *g_window;

static VkSwapchainKHR g_swap;
static VkFormat g_swap_fmt;
static VkExtent2D g_extent;
static unsigned g_nimages;
static VkImage g_images[8];
static VkImageView g_views[8];
static VkImageLayout g_img_layout[8];
static VkFramebuffer g_fbs[8];
static vk_image g_wdepth;
static vk_image g_hold;                 /* the frame, for the presents after (present) */
static VkFormat g_depth_fmt;
static int g_win_pass = -1;
static unsigned g_image;                /* the swapchain image this frame */
static int g_need_resize;
static int g_made_w, g_made_h;         /* the window's size when the chain was made */

static VkCommandPool g_pool;
static VkCommandBuffer g_cmd;
static VkFence g_fence;
static VkSemaphore g_sem_acquire, g_sem_done;
static int g_frame_on, g_cmd_on, g_acquire_wait, g_pass_on, g_pass_target;
static unsigned g_serial = 1;

static VkDescriptorSetLayout g_dsl;
static VkPipelineLayout g_layout;
static VkShaderModule g_vs, g_fs;
static VkDescriptorPool g_dpool;
static int g_dpool_used;
#define DPOOL_SETS 4096

static VkBuffer g_vbuf;
static VkDeviceMemory g_vmem;
static unsigned char *g_vmap;
static VkDeviceSize g_vsize, g_voff;
static struct { VkBuffer b; VkDeviceMemory m; } g_retired[16];   /* outgrown this frame, still read */
static int g_nretired;

static vk_tex *g_tex;
static int g_ntex;
static vk_target *g_tg;
static int g_ntg;
static vk_pass g_pass[16];
static int g_npass;
static vk_pipe *g_pipes;
static int g_npipes, g_cpipes;
static vk_samp g_samps[64];
static int g_nsamps;
static int g_cur_target;
static int g_white;                     /* a 1x1 white texture for untextured draws */
static int g_shadow;                    /* the target sampled as the shadow map (binding 1) */

static int vk_ok(VkResult r, const char *what)
{
    if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR)
        return 1;
    fprintf(stderr, "vulkan: %s failed (%d)\n", what, (int)r);
    return 0;
}

static int mem_type(unsigned bits, VkMemoryPropertyFlags want)
{
    unsigned i;
    for (i = 0; i < g_mem.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (g_mem.memoryTypes[i].propertyFlags & want) == want)
            return (int)i;
    return -1;
}

static int alloc(VkMemoryRequirements *req, VkMemoryPropertyFlags want, VkDeviceMemory *out)
{
    VkMemoryAllocateInfo ai;
    int t = mem_type(req->memoryTypeBits, want);
    if (t < 0)
        return 0;
    memset(&ai, 0, sizeof ai);
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req->size;
    ai.memoryTypeIndex = (unsigned)t;
    return vk_ok(vkAllocateMemory(g_dev, &ai, NULL, out), "vkAllocateMemory");
}

static int make_buffer(VkDeviceSize size, VkBufferUsageFlags use, VkBuffer *b, VkDeviceMemory *m, void **map)
{
    VkBufferCreateInfo bi;
    VkMemoryRequirements req;

    memset(&bi, 0, sizeof bi);
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = use;
    if (!vk_ok(vkCreateBuffer(g_dev, &bi, NULL, b), "vkCreateBuffer"))
        return 0;
    vkGetBufferMemoryRequirements(g_dev, *b, &req);
    if (!alloc(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m)) {
        vkDestroyBuffer(g_dev, *b, NULL);
        return 0;
    }
    vkBindBufferMemory(g_dev, *b, *m, 0);
    if (map && !vk_ok(vkMapMemory(g_dev, *m, 0, VK_WHOLE_SIZE, 0, map), "vkMapMemory"))
        return 0;
    return 1;
}

static void free_buffer(VkBuffer b, VkDeviceMemory m)
{
    if (b) vkDestroyBuffer(g_dev, b, NULL);
    if (m) vkFreeMemory(g_dev, m, NULL);
}

static int make_image(vk_image *im, int w, int h, int levels, VkFormat fmt, VkSampleCountFlagBits samples,
                      VkImageUsageFlags use, VkImageAspectFlags aspect)
{
    VkImageCreateInfo ii;
    VkImageViewCreateInfo vi;
    VkMemoryRequirements req;

    memset(im, 0, sizeof *im);
    memset(&ii, 0, sizeof ii);
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = fmt;
    ii.extent.width = (unsigned)w;
    ii.extent.height = (unsigned)h;
    ii.extent.depth = 1;
    ii.mipLevels = (unsigned)levels;
    ii.arrayLayers = 1;
    ii.samples = samples;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = use;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(g_dev, &ii, NULL, &im->img), "vkCreateImage"))
        return 0;
    vkGetImageMemoryRequirements(g_dev, im->img, &req);
    if (!alloc(&req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &im->mem))
        return 0;
    vkBindImageMemory(g_dev, im->img, im->mem, 0);
    memset(&vi, 0, sizeof vi);
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = im->img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.levelCount = (unsigned)levels;
    vi.subresourceRange.layerCount = 1;
    if (!vk_ok(vkCreateImageView(g_dev, &vi, NULL, &im->view), "vkCreateImageView"))
        return 0;
    im->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    im->aspect = aspect;
    im->levels = levels;
    return 1;
}

static int g_ucb_on;                    /* uploads recorded, not yet run (upload) */
static VkCommandBuffer g_ucb;
static VkFence g_ufence;
static struct { VkBuffer b; VkDeviceMemory m; } *g_ustage;
static int g_nustage, g_custage;
static VkDeviceSize g_ubytes;
static void upload_submit(void);

static void free_image(vk_image *im)
{
    if (im->img && g_ucb_on)            /* a recorded upload may write it */
        upload_submit();
    if (im->view) vkDestroyImageView(g_dev, im->view, NULL);
    if (im->img) vkDestroyImage(g_dev, im->img, NULL);
    if (im->mem) vkFreeMemory(g_dev, im->mem, NULL);
    memset(im, 0, sizeof *im);
}

/* everything before, then the layout: simple and always right */
static void barrier(VkImage img, VkImageAspectFlags aspect, int levels, VkImageLayout from, VkImageLayout to)
{
    VkImageMemoryBarrier b;

    if (from == to && to != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        return;
    memset(&b, 0, sizeof b);
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = (unsigned)levels;
    b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(g_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, NULL, 0, NULL, 1, &b);
}

static void to_layout(vk_image *im, VkImageLayout l)
{
    barrier(im->img, im->aspect, im->levels, im->layout, l);
    im->layout = l;
}

/* ------------------------------------------------------------ the frame */

static int recreate_swapchain(void);
static int g_made_vsync = 1;          /* the chain's VSYNC (rgl_vsync) */
static void end_pass(void);

static void cmd_begin(void)
{
    VkCommandBufferBeginInfo bi;

    if (g_cmd_on)
        return;
    vkResetCommandBuffer(g_cmd, 0);
    memset(&bi, 0, sizeof bi);
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_cmd, &bi);
    g_cmd_on = 1;
}

/* the frame's commands so far, run and waited for; recording goes on in a
 * fresh buffer (the swapchain image stays acquired) */
static void flush(int signal_done)
{
    VkSubmitInfo si;
    VkPipelineStageFlags st = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    upload_submit();                    /* the textures these commands draw with */
    if (!g_cmd_on)
        return;
    end_pass();
    vkEndCommandBuffer(g_cmd);
    g_cmd_on = 0;
    memset(&si, 0, sizeof si);
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_cmd;
    if (g_acquire_wait) {
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &g_sem_acquire;
        si.pWaitDstStageMask = &st;
        g_acquire_wait = 0;
    }
    if (signal_done) {
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &g_sem_done;
    }
    vkResetFences(g_dev, 1, &g_fence);
    vk_ok(vkQueueSubmit(g_queue, 1, &si, g_fence), "vkQueueSubmit");
    vkWaitForFences(g_dev, 1, &g_fence, VK_TRUE, ~0ull);
    while (g_nretired > 0) {            /* the buffers the frame outgrew */
        g_nretired--;
        free_buffer(g_retired[g_nretired].b, g_retired[g_nretired].m);
    }
    vkResetDescriptorPool(g_dev, g_dpool, 0);
    g_dpool_used = 0;
    g_voff = 0;
    g_serial++;
}

static void frame_begin(void)
{
    VkResult r;
    int w = 0, h = 0;

    if (g_frame_on)
        return;
    plat_size(&w, &h);
    /* against the window's size when the chain was made, not the chain's
     * own: the surface can give an extent a pixel off the window's, and the
     * chain was then made again every frame */
    if (w > 0 && h > 0 && (g_need_resize || !g_swap || w != g_made_w || h != g_made_h
                           || g_made_vsync != rgl_vsync))
        recreate_swapchain();
    if (!g_swap)                        /* none for now: tried next frame */
        return;
    r = vkAcquireNextImageKHR(g_dev, g_swap, ~0ull, g_sem_acquire, VK_NULL_HANDLE, &g_image);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!recreate_swapchain())
            return;
        r = vkAcquireNextImageKHR(g_dev, g_swap, ~0ull, g_sem_acquire, VK_NULL_HANDLE, &g_image);
    }
    if (!vk_ok(r, "vkAcquireNextImageKHR")) {
        g_need_resize = 1;
        return;
    }
    g_acquire_wait = 1;
    g_img_layout[g_image] = VK_IMAGE_LAYOUT_UNDEFINED;
    g_frame_on = 1;
    cmd_begin();
}

/* the whole window from one image to another of its size */
static void copy_window(VkImage from, VkImage to)
{
    VkImageBlit r;

    memset(&r, 0, sizeof r);
    r.srcSubresource.aspectMask = r.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    r.srcSubresource.layerCount = r.dstSubresource.layerCount = 1;
    r.srcOffsets[1].x = r.dstOffsets[1].x = (int)g_extent.width;
    r.srcOffsets[1].y = r.dstOffsets[1].y = (int)g_extent.height;
    r.srcOffsets[1].z = r.dstOffsets[1].z = 1;
    vkCmdBlitImage(g_cmd, from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   to, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r, VK_FILTER_NEAREST);
}

/* FIFO has no swap interval: on a 120/180/240 Hz screen (rgl_swap_interval
 * 2/3/4) the frame is kept and presented again once for each refresh after
 * the first, so every frame stays up the same time. One present each, a frame
 * ending near a refresh was shown one refresh late now and then -- 22 ms and
 * then 11, a stutter several times a second (Mary, 180 Hz). */
static void present(void)
{
    VkPresentInfoKHR pi;
    VkResult r;
    int k, n = rgl_swap_interval < 1 ? 1 : rgl_swap_interval > 4 ? 4 : rgl_swap_interval;

    if (!g_hold.img || !rgl_vsync)
        n = 1;
    for (k = 0; k < n; k++) {
        frame_begin();
        if (!g_frame_on)
            return;
        cmd_begin();
        if (k == 0) {
            end_pass();
            if (n > 1) {
                barrier(g_images[g_image], VK_IMAGE_ASPECT_COLOR_BIT, 1, g_img_layout[g_image],
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                g_img_layout[g_image] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                to_layout(&g_hold, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                copy_window(g_images[g_image], g_hold.img);
                to_layout(&g_hold, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            }
        } else {
            barrier(g_images[g_image], VK_IMAGE_ASPECT_COLOR_BIT, 1, g_img_layout[g_image],
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            g_img_layout[g_image] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copy_window(g_hold.img, g_images[g_image]);
        }
        barrier(g_images[g_image], VK_IMAGE_ASPECT_COLOR_BIT, 1, g_img_layout[g_image], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        g_img_layout[g_image] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        flush(1);
        memset(&pi, 0, sizeof pi);
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &g_sem_done;
        pi.swapchainCount = 1;
        pi.pSwapchains = &g_swap;
        pi.pImageIndices = &g_image;
        r = vkQueuePresentKHR(g_queue, &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
            g_need_resize = 1;
        g_frame_on = 0;
        if (g_need_resize)
            return;
    }
}

/* ---------------------------------------------------------- render passes */

static int get_pass(VkFormat fmt, VkSampleCountFlagBits samples, int depth)
{
    VkAttachmentDescription at[2];
    VkAttachmentReference cr, dr;
    VkSubpassDescription sp;
    VkRenderPassCreateInfo ri;
    int i;

    for (i = 0; i < g_npass; i++)
        if (g_pass[i].fmt == fmt && g_pass[i].samples == samples && g_pass[i].depth == depth)
            return i;
    if (g_npass >= 16)
        return 0;
    memset(at, 0, sizeof at);
    at[0].format = fmt;
    at[0].samples = samples;
    at[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    at[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    at[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at[0].initialLayout = at[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at[1] = at[0];
    at[1].format = g_depth_fmt;
    at[1].initialLayout = at[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    cr.attachment = 0;
    cr.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    dr.attachment = 1;
    dr.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    memset(&sp, 0, sizeof sp);
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    sp.pDepthStencilAttachment = depth ? &dr : NULL;
    memset(&ri, 0, sizeof ri);
    ri.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ri.attachmentCount = depth ? 2 : 1;
    ri.pAttachments = at;
    ri.subpassCount = 1;
    ri.pSubpasses = &sp;
    if (!vk_ok(vkCreateRenderPass(g_dev, &ri, NULL, &g_pass[g_npass].rp), "vkCreateRenderPass"))
        return 0;
    g_pass[g_npass].fmt = fmt;
    g_pass[g_npass].samples = samples;
    g_pass[g_npass].depth = depth;
    return g_npass++;
}

static VkFramebuffer make_fb(int pass, VkImageView c, VkImageView d, int w, int h)
{
    VkFramebufferCreateInfo fi;
    VkImageView v[2];
    VkFramebuffer fb = VK_NULL_HANDLE;

    v[0] = c;
    v[1] = d;
    memset(&fi, 0, sizeof fi);
    fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fi.renderPass = g_pass[pass].rp;
    fi.attachmentCount = d ? 2 : 1;
    fi.pAttachments = v;
    fi.width = (unsigned)w;
    fi.height = (unsigned)h;
    fi.layers = 1;
    vk_ok(vkCreateFramebuffer(g_dev, &fi, NULL, &fb), "vkCreateFramebuffer");
    return fb;
}

static void end_pass(void)
{
    if (g_pass_on) {
        vkCmdEndRenderPass(g_cmd);
        g_pass_on = 0;
    }
}

static void tsize(int t, int *w, int *h)
{
    if (t > 0 && t < g_ntg && g_tg[t].used) { *w = g_tg[t].w; *h = g_tg[t].h; }
    else { *w = (int)g_extent.width; *h = (int)g_extent.height; }
}

/* the current target, in a render pass */
static int begin_pass(void)
{
    VkRenderPassBeginInfo bi;
    int t = g_cur_target, w, h;

    frame_begin();
    if (!g_frame_on)
        return 0;
    cmd_begin();
    if (g_pass_on && g_pass_target == t)
        return 1;
    end_pass();
    memset(&bi, 0, sizeof bi);
    bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    tsize(t, &w, &h);
    if (t > 0) {
        vk_target *g = &g_tg[t];
        to_layout(&g->color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        if (g->depth)
            to_layout(&g->dep, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        bi.renderPass = g_pass[g->pass].rp;
        bi.framebuffer = g->fb;
    } else {
        barrier(g_images[g_image], VK_IMAGE_ASPECT_COLOR_BIT, 1, g_img_layout[g_image],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        g_img_layout[g_image] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        to_layout(&g_wdepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        bi.renderPass = g_pass[g_win_pass].rp;
        bi.framebuffer = g_fbs[g_image];
    }
    bi.renderArea.extent.width = (unsigned)w;
    bi.renderArea.extent.height = (unsigned)h;
    vkCmdBeginRenderPass(g_cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    g_pass_on = 1;
    g_pass_target = t;
    return 1;
}

/* ------------------------------------------------------------ swapchain */

static void free_swapchain(void)
{
    unsigned i;
    for (i = 0; i < g_nimages; i++) {
        if (g_fbs[i]) vkDestroyFramebuffer(g_dev, g_fbs[i], NULL);
        if (g_views[i]) vkDestroyImageView(g_dev, g_views[i], NULL);
        g_fbs[i] = VK_NULL_HANDLE;
        g_views[i] = VK_NULL_HANDLE;
    }
    free_image(&g_wdepth);
    free_image(&g_hold);
}

static int recreate_swapchain(void)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR si;
    VkSwapchainKHR old = g_swap;
    VkResult r;
    int w = 0, h = 0;
    unsigned i, n;

    vkDeviceWaitIdle(g_dev);
    free_swapchain();
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_gpu, g_surf, &caps);
    plat_size(&w, &h);
    if (caps.currentExtent.width != 0xFFFFFFFFu) {
        w = (int)caps.currentExtent.width;
        h = (int)caps.currentExtent.height;
    }
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    g_extent.width = (unsigned)w;
    g_extent.height = (unsigned)h;
    memset(&si, 0, sizeof si);
    si.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    si.surface = g_surf;
    si.minImageCount = caps.minImageCount < 2 ? 2 : caps.minImageCount;
    if (caps.maxImageCount && si.minImageCount > caps.maxImageCount)
        si.minImageCount = caps.maxImageCount;
    si.imageFormat = g_swap_fmt;
    si.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    si.imageExtent = g_extent;
    si.imageArrayLayers = 1;
    si.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    si.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    si.preTransform = caps.currentTransform;
    si.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                      ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : (VkCompositeAlphaFlagBitsKHR)
                        (caps.supportedCompositeAlpha & -caps.supportedCompositeAlpha);
    si.presentMode = VK_PRESENT_MODE_FIFO_KHR;     /* every driver has it: vsync */
    if (!rgl_vsync) {                   /* VSYNC off: mailbox, else immediate */
        VkPresentModeKHR modes[16];
        uint32_t nm = 16, k;
        int best = -1;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(g_gpu, g_surf, &nm, modes) >= 0)
            for (k = 0; k < nm; k++) {
                if (modes[k] == VK_PRESENT_MODE_MAILBOX_KHR) best = 0;
                else if (modes[k] == VK_PRESENT_MODE_IMMEDIATE_KHR && best < 0) best = 1;
            }
        if (best >= 0)
            si.presentMode = best == 0 ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    g_made_vsync = rgl_vsync;
    si.clipped = VK_TRUE;
    si.oldSwapchain = old;
    /* the old chain is retired even when the new one fails: then there is
     * none, and the next frame tries again (Mary: resizing and moving the
     * window a few times closed the game) */
    g_swap = VK_NULL_HANDLE;
    g_nimages = 0;
    r = vkCreateSwapchainKHR(g_dev, &si, NULL, &g_swap);
    if (old)
        vkDestroySwapchainKHR(g_dev, old, NULL);
    if (!vk_ok(r, "vkCreateSwapchainKHR")) {
        g_swap = VK_NULL_HANDLE;
        g_need_resize = 1;
        return 0;
    }
    plat_size(&g_made_w, &g_made_h);
    n = 8;
    vkGetSwapchainImagesKHR(g_dev, g_swap, &n, g_images);
    g_nimages = n;
    make_image(&g_wdepth, w, h, 1, g_depth_fmt, VK_SAMPLE_COUNT_1_BIT,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    make_image(&g_hold, w, h, 1, g_swap_fmt, VK_SAMPLE_COUNT_1_BIT,
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
               VK_IMAGE_ASPECT_COLOR_BIT);
    g_win_pass = get_pass(g_swap_fmt, VK_SAMPLE_COUNT_1_BIT, 1);
    for (i = 0; i < n; i++) {
        VkImageViewCreateInfo vi;
        memset(&vi, 0, sizeof vi);
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = g_images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = g_swap_fmt;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(g_dev, &vi, NULL, &g_views[i]);
        g_fbs[i] = make_fb(g_win_pass, g_views[i], g_wdepth.view, w, h);
        g_img_layout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    g_need_resize = 0;
    return 1;
}

/* ------------------------------------------------------------- open/close */

static int pick_depth(void)
{
    static const VkFormat k[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32,
                                  VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM };
    size_t i;
    for (i = 0; i < sizeof k / sizeof k[0]; i++) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(g_gpu, k[i], &fp);
        if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            g_depth_fmt = k[i];
            return 1;
        }
    }
    return 0;
}

static VkShaderModule shader(const unsigned *code, size_t bytes)
{
    VkShaderModuleCreateInfo ci;
    VkShaderModule m = VK_NULL_HANDLE;
    memset(&ci, 0, sizeof ci);
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = bytes;
    ci.pCode = code;
    vkCreateShaderModule(g_dev, &ci, NULL, &m);
    return m;
}

static int vk_tex_new(void);
static void vk_tex_image(int t, int w, int h, int levels, const unsigned char *const *rgba);

#define LOAD_I(n) n = (PFN_##n)gipa(g_inst, #n)
#define LOAD_D(n) n = (PFN_##n)vkGetDeviceProcAddr(g_dev, #n)

/* The pipelines the driver built, kept in the game's folder between runs
 * (Khronos' Vulkan-Samples, pipeline_cache): a combination of states seen
 * before is not compiled again the first time it is drawn. The driver checks
 * the file's header and ignores one from another driver or GPU. */
#define PCACHE_FILE "umk3-pipelines-vk.bin"
#define PIPES_FILE "umk3-pipelines-vk.txt"
static VkPipelineCache g_pcache;
static int g_preloading;                /* the list made again: not noted again */
static void preload(void);
#include "rgl_pipes_vk.h"

static void cache_open(void)
{
    VkPipelineCacheCreateInfo ci;
    void *data = NULL;
    long n = 0;
    char path[1100];
    FILE *f;

    rgl_path(path, sizeof path, PCACHE_FILE);
    f = fopen(path, "rb");

    if (f) {
        if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0
            && (data = malloc((size_t)n)) != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) {
            free(data);
            data = NULL;
        }
        fclose(f);
    }
    memset(&ci, 0, sizeof ci);
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    ci.initialDataSize = data ? (size_t)n : 0;
    ci.pInitialData = data;
    if (vkCreatePipelineCache(g_dev, &ci, NULL, &g_pcache) != VK_SUCCESS && data) {
        ci.initialDataSize = 0;         /* a file it would not take: start empty */
        ci.pInitialData = NULL;
        if (vkCreatePipelineCache(g_dev, &ci, NULL, &g_pcache) != VK_SUCCESS)
            g_pcache = VK_NULL_HANDLE;
    }
    free(data);
}

static void cache_close(void)
{
    size_t n = 0;
    void *data;

    if (!g_pcache)
        return;
    if (vkGetPipelineCacheData(g_dev, g_pcache, &n, NULL) == VK_SUCCESS && n
        && (data = malloc(n)) != NULL) {
        if (vkGetPipelineCacheData(g_dev, g_pcache, &n, data) == VK_SUCCESS) {
            char path[1100];
            FILE *f;
            rgl_path(path, sizeof path, PCACHE_FILE);
            f = fopen(path, "wb");
            if (f) {
                fwrite(data, 1, n, f);
                fclose(f);
            }
        }
        free(data);
    }
    vkDestroyPipelineCache(g_dev, g_pcache, NULL);
    g_pcache = VK_NULL_HANDLE;
}

static int vk_open(void *window, int width, int height)
{
    VkApplicationInfo app;
    VkInstanceCreateInfo ii;
    const char *iexts[8];
    unsigned nie = 0, i, n = 0;
    VkPhysicalDevice gpus[16];
    VkDeviceQueueCreateInfo qi;
    VkDeviceCreateInfo di;
    VkPhysicalDeviceFeatures have, want;
    const char *dexts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    float prio = 1.0f;
    int best = -1, best_score = -1;
    VkSurfaceFormatKHR fmts[64];

    (void)width; (void)height;
    g_window = window;
    if (!load_vulkan()) {
        printf("vulkan: no vulkan-1.dll / libvulkan.so.1\n");
        return 0;
    }
    memset(&app, 0, sizeof app);
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Ultimate Mortal Kombat 3";
    app.pEngineName = "LIME (UMK3 PC port)";
    app.apiVersion = VK_API_VERSION_1_0;
    iexts[nie++] = VK_KHR_SURFACE_EXTENSION_NAME;
#ifdef _WIN32
    iexts[nie++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#else
    if (rgl_vk_instance_exts) {
        unsigned k, cnt = 0;
        const char *const *e = rgl_vk_instance_exts(&cnt);
        nie = 0;
        for (k = 0; k < cnt && nie < 8; k++)
            iexts[nie++] = e[k];
    }
#endif
    memset(&ii, 0, sizeof ii);
    ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ii.pApplicationInfo = &app;
    ii.enabledExtensionCount = nie;
    ii.ppEnabledExtensionNames = iexts;
    if (!vk_ok(vkCreateInstance(&ii, NULL, &g_inst), "vkCreateInstance"))
        return 0;
    LOAD_I(vkDestroyInstance); LOAD_I(vkEnumeratePhysicalDevices); LOAD_I(vkGetPhysicalDeviceProperties);
    LOAD_I(vkGetPhysicalDeviceFeatures); LOAD_I(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_I(vkGetPhysicalDeviceMemoryProperties); LOAD_I(vkGetPhysicalDeviceFormatProperties);
    LOAD_I(vkCreateDevice); LOAD_I(vkGetDeviceProcAddr); LOAD_I(vkDestroySurfaceKHR);
    LOAD_I(vkGetPhysicalDeviceSurfaceSupportKHR); LOAD_I(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_I(vkGetPhysicalDeviceSurfaceFormatsKHR); LOAD_I(vkGetPhysicalDeviceSurfacePresentModesKHR);
    LOAD_I(vkEnumerateDeviceExtensionProperties);

#ifdef _WIN32
    {
        VkWin32SurfaceCreateInfoKHR si;
        LOAD_I(vkCreateWin32SurfaceKHR);
        memset(&si, 0, sizeof si);
        si.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        si.hinstance = GetModuleHandleA(NULL);
        si.hwnd = (HWND)window;
        if (!vkCreateWin32SurfaceKHR || !vk_ok(vkCreateWin32SurfaceKHR(g_inst, &si, NULL, &g_surf), "surface"))
            return 0;
    }
#else
    if (!rgl_vk_make_surface || !rgl_vk_make_surface(window, (void *)g_inst, (void *)&g_surf)) {
        printf("vulkan: SDL made no surface\n");
        return 0;
    }
#endif

    /* the GPU: one that can draw and present to this window, a discrete
     * one first */
    n = 16;
    vkEnumeratePhysicalDevices(g_inst, &n, gpus);
    for (i = 0; i < n; i++) {
        VkQueueFamilyProperties qf[16];
        unsigned nq = 16, q;
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(gpus[i], &p);
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &nq, qf);
        for (q = 0; q < nq; q++) {
            VkBool32 pres = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpus[i], q, g_surf, &pres);
            if ((qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && pres) {
                int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
                          : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
                if (score > best_score) {
                    best_score = score;
                    best = (int)i;
                    g_family = q;
                }
                break;
            }
        }
    }
    if (best < 0) {
        printf("vulkan: no GPU can draw to this window\n");
        return 0;
    }
    g_gpu = gpus[best];
    vkGetPhysicalDeviceProperties(g_gpu, &g_props);
    vkGetPhysicalDeviceMemoryProperties(g_gpu, &g_mem);
    vkGetPhysicalDeviceFeatures(g_gpu, &have);
    memset(&want, 0, sizeof want);
    want.samplerAnisotropy = have.samplerAnisotropy;
    g_aniso_ok = have.samplerAnisotropy != 0;

    memset(&qi, 0, sizeof qi);
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = g_family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    memset(&di, 0, sizeof di);
    di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = 1;
    di.ppEnabledExtensionNames = dexts;
    di.pEnabledFeatures = &want;
    if (!vk_ok(vkCreateDevice(g_gpu, &di, NULL, &g_dev), "vkCreateDevice"))
        return 0;
    LOAD_D(vkDestroyDevice); LOAD_D(vkGetDeviceQueue); LOAD_D(vkDeviceWaitIdle);
    LOAD_D(vkCreateSwapchainKHR); LOAD_D(vkDestroySwapchainKHR); LOAD_D(vkGetSwapchainImagesKHR);
    LOAD_D(vkAcquireNextImageKHR); LOAD_D(vkQueuePresentKHR); LOAD_D(vkQueueSubmit); LOAD_D(vkQueueWaitIdle);
    LOAD_D(vkCreateImage); LOAD_D(vkDestroyImage); LOAD_D(vkCreateImageView); LOAD_D(vkDestroyImageView);
    LOAD_D(vkGetImageMemoryRequirements); LOAD_D(vkBindImageMemory);
    LOAD_D(vkCreateBuffer); LOAD_D(vkDestroyBuffer); LOAD_D(vkGetBufferMemoryRequirements); LOAD_D(vkBindBufferMemory);
    LOAD_D(vkAllocateMemory); LOAD_D(vkFreeMemory); LOAD_D(vkMapMemory); LOAD_D(vkUnmapMemory);
    LOAD_D(vkCreateRenderPass); LOAD_D(vkDestroyRenderPass); LOAD_D(vkCreateFramebuffer); LOAD_D(vkDestroyFramebuffer);
    LOAD_D(vkCreateShaderModule); LOAD_D(vkDestroyShaderModule); LOAD_D(vkCreatePipelineLayout);
    LOAD_D(vkDestroyPipelineLayout); LOAD_D(vkCreateGraphicsPipelines); LOAD_D(vkDestroyPipeline);
    LOAD_D(vkCreatePipelineCache); LOAD_D(vkGetPipelineCacheData); LOAD_D(vkDestroyPipelineCache);
    LOAD_D(vkCreateDescriptorSetLayout); LOAD_D(vkDestroyDescriptorSetLayout); LOAD_D(vkCreateDescriptorPool);
    LOAD_D(vkDestroyDescriptorPool); LOAD_D(vkResetDescriptorPool); LOAD_D(vkAllocateDescriptorSets);
    LOAD_D(vkUpdateDescriptorSets); LOAD_D(vkCreateSampler); LOAD_D(vkDestroySampler);
    LOAD_D(vkCreateCommandPool); LOAD_D(vkDestroyCommandPool); LOAD_D(vkAllocateCommandBuffers);
    LOAD_D(vkResetCommandBuffer); LOAD_D(vkBeginCommandBuffer); LOAD_D(vkEndCommandBuffer);
    LOAD_D(vkCreateFence); LOAD_D(vkDestroyFence); LOAD_D(vkWaitForFences); LOAD_D(vkResetFences);
    LOAD_D(vkCreateSemaphore); LOAD_D(vkDestroySemaphore);
    LOAD_D(vkCmdBeginRenderPass); LOAD_D(vkCmdEndRenderPass); LOAD_D(vkCmdBindPipeline);
    LOAD_D(vkCmdSetViewport); LOAD_D(vkCmdSetScissor); LOAD_D(vkCmdPushConstants);
    LOAD_D(vkCmdBindDescriptorSets); LOAD_D(vkCmdBindVertexBuffers); LOAD_D(vkCmdDraw);
    LOAD_D(vkCmdClearAttachments); LOAD_D(vkCmdPipelineBarrier); LOAD_D(vkCmdCopyBufferToImage);
    LOAD_D(vkCmdCopyImageToBuffer); LOAD_D(vkCmdBlitImage); LOAD_D(vkCmdResolveImage);
    vkGetDeviceQueue(g_dev, g_family, 0, &g_queue);

    /* the window's format: plain 8-bit BGRA or RGBA, as OpenGL's */
    n = 64;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_gpu, g_surf, &n, fmts);
    g_swap_fmt = n ? fmts[0].format : VK_FORMAT_B8G8R8A8_UNORM;
    for (i = 0; i < n; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            g_swap_fmt = fmts[i].format;
            break;
        }
    if (g_swap_fmt == VK_FORMAT_UNDEFINED)
        g_swap_fmt = VK_FORMAT_B8G8R8A8_UNORM;
    if (!pick_depth()) {
        printf("vulkan: no depth format\n");
        return 0;
    }

    {
        VkCommandPoolCreateInfo pi;
        VkCommandBufferAllocateInfo ai;
        VkFenceCreateInfo fi;
        VkSemaphoreCreateInfo si;
        VkDescriptorSetLayoutBinding b[2];
        VkDescriptorSetLayoutCreateInfo dl;
        VkPushConstantRange pc;
        VkPipelineLayoutCreateInfo pl;
        VkDescriptorPoolSize ps;
        VkDescriptorPoolCreateInfo dp;

        memset(&pi, 0, sizeof pi);
        pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pi.queueFamilyIndex = g_family;
        vkCreateCommandPool(g_dev, &pi, NULL, &g_pool);
        memset(&ai, 0, sizeof ai);
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = g_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        vkAllocateCommandBuffers(g_dev, &ai, &g_cmd);
        memset(&fi, 0, sizeof fi);
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(g_dev, &fi, NULL, &g_fence);
        memset(&si, 0, sizeof si);
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(g_dev, &si, NULL, &g_sem_acquire);
        vkCreateSemaphore(g_dev, &si, NULL, &g_sem_done);

        /* 0 the texture, 1 the shadow map */
        memset(b, 0, sizeof b);
        b[0].binding = 0;
        b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[0].descriptorCount = 1;
        b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[1] = b[0];
        b[1].binding = 1;
        memset(&dl, 0, sizeof dl);
        dl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dl.bindingCount = 2;
        dl.pBindings = b;
        vkCreateDescriptorSetLayout(g_dev, &dl, NULL, &g_dsl);
        pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size = sizeof(rgl_frag);
        memset(&pl, 0, sizeof pl);
        pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &g_dsl;
        pl.pushConstantRangeCount = 1;
        pl.pPushConstantRanges = &pc;
        vkCreatePipelineLayout(g_dev, &pl, NULL, &g_layout);
        cache_open();
        ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = DPOOL_SETS * 2;
        memset(&dp, 0, sizeof dp);
        dp.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dp.maxSets = DPOOL_SETS;
        dp.poolSizeCount = 1;
        dp.pPoolSizes = &ps;
        vkCreateDescriptorPool(g_dev, &dp, NULL, &g_dpool);
    }
    g_vs = shader(k_vk_vs, sizeof k_vk_vs);
    g_fs = shader(k_vk_fs, sizeof k_vk_fs);
    g_vsize = 4u << 20;
    if (!make_buffer(g_vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &g_vbuf, &g_vmem, (void **)&g_vmap))
        return 0;
    if (!recreate_swapchain())
        return 0;

    rgl_vk.max_texture = (int)g_props.limits.maxImageDimension2D;
    {
        VkSampleCountFlags s = g_props.limits.framebufferColorSampleCounts
                             & g_props.limits.framebufferDepthSampleCounts;
        rgl_vk.max_samples = s & VK_SAMPLE_COUNT_16_BIT ? 16 : s & VK_SAMPLE_COUNT_8_BIT ? 8
                           : s & VK_SAMPLE_COUNT_4_BIT ? 4 : s & VK_SAMPLE_COUNT_2_BIT ? 2 : 1;
    }
    {
        static const unsigned char white[4] = { 255, 255, 255, 255 };
        const unsigned char *lv[1];
        lv[0] = white;
        g_white = vk_tex_new();
        vk_tex_image(g_white, 1, 1, 1, lv);
    }
    preload();
    printf("vulkan: %s, window %ux%u, %s\n", g_props.deviceName, g_extent.width, g_extent.height,
           g_swap_fmt == VK_FORMAT_B8G8R8A8_UNORM ? "BGRA8" : "RGBA8");
    return 1;
}

static void vk_close(void)
{
    int i;

    g_shadow = 0;
    if (!g_dev)
        return;
    upload_submit();
    vkDeviceWaitIdle(g_dev);
    if (g_ufence) vkDestroyFence(g_dev, g_ufence, NULL);
    g_ufence = VK_NULL_HANDLE;
    g_ucb = VK_NULL_HANDLE;             /* freed with the pool */
    free(g_ustage);
    g_ustage = NULL;
    g_custage = 0;
    for (i = 0; i < g_npipes; i++)
        vkDestroyPipeline(g_dev, g_pipes[i].p, NULL);
    for (i = 0; i < g_nsamps; i++)
        vkDestroySampler(g_dev, g_samps[i].s, NULL);
    for (i = 0; i < g_ntex; i++)
        if (g_tex[i].used)
            free_image(&g_tex[i].im);
    for (i = 0; i < g_ntg; i++)
        if (g_tg[i].used) {
            vkDestroyFramebuffer(g_dev, g_tg[i].fb, NULL);
            free_image(&g_tg[i].color);
            free_image(&g_tg[i].dep);
        }
    free_swapchain();
    for (i = 0; i < g_npass; i++)
        vkDestroyRenderPass(g_dev, g_pass[i].rp, NULL);
    vkDestroySwapchainKHR(g_dev, g_swap, NULL);
    while (g_nretired > 0) {
        g_nretired--;
        free_buffer(g_retired[g_nretired].b, g_retired[g_nretired].m);
    }
    free_buffer(g_vbuf, g_vmem);
    vkDestroyShaderModule(g_dev, g_vs, NULL);
    vkDestroyShaderModule(g_dev, g_fs, NULL);
    vkDestroyDescriptorPool(g_dev, g_dpool, NULL);
    vkDestroyPipelineLayout(g_dev, g_layout, NULL);
    cache_close();
    vkDestroyDescriptorSetLayout(g_dev, g_dsl, NULL);
    vkDestroySemaphore(g_dev, g_sem_acquire, NULL);
    vkDestroySemaphore(g_dev, g_sem_done, NULL);
    vkDestroyFence(g_dev, g_fence, NULL);
    vkDestroyCommandPool(g_dev, g_pool, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroySurfaceKHR(g_inst, g_surf, NULL);
    vkDestroyInstance(g_inst, NULL);
    g_dev = VK_NULL_HANDLE;
}

static const char *vk_device(void) { return g_props.deviceName; }

/* ------------------------------------------------------------- textures */

static int vk_tex_new(void)
{
    int i;
    for (i = 1; i < g_ntex; i++)
        if (!g_tex[i].used)
            break;
    if (i >= g_ntex) {
        int n = g_ntex ? g_ntex * 2 : 256;
        g_tex = (vk_tex *)realloc(g_tex, (size_t)n * sizeof *g_tex);
        memset(g_tex + g_ntex, 0, (size_t)(n - g_ntex) * sizeof *g_tex);
        if (!g_ntex)
            i = 1;
        g_ntex = n;
    }
    memset(&g_tex[i], 0, sizeof g_tex[i]);
    g_tex[i].used = 1;
    return i;
}

/* Rows into an image's levels through a staging buffer, recorded outside any
 * render pass in a buffer of their own that runs before the frame's next
 * commands (flush, the same queue) -- or sooner, past 64 MB of staging or
 * when an image is freed. Each upload used to submit its own buffer and wait
 * on a fence of its own: one wait per texture, hundreds in a load. The
 * frame's commands that came before keep the texture as it was only if they
 * already ran, so those run first when they used it (before_write). */
static void upload_submit(void)
{
    VkSubmitInfo si;
    int i;

    if (!g_ucb_on)
        return;
    g_ucb_on = 0;
    vkEndCommandBuffer(g_ucb);
    memset(&si, 0, sizeof si);
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_ucb;
    vkResetFences(g_dev, 1, &g_ufence);
    vk_ok(vkQueueSubmit(g_queue, 1, &si, g_ufence), "vkQueueSubmit (uploads)");
    vkWaitForFences(g_dev, 1, &g_ufence, VK_TRUE, ~0ull);
    for (i = 0; i < g_nustage; i++)
        free_buffer(g_ustage[i].b, g_ustage[i].m);
    g_nustage = 0;
    g_ubytes = 0;
}

static int upload_begin(VkDeviceSize bytes)
{
    VkCommandBufferBeginInfo bi;

    if (g_ucb_on && g_ubytes + bytes > ((VkDeviceSize)64 << 20))
        upload_submit();
    if (!g_ucb) {
        VkCommandBufferAllocateInfo ai;
        VkFenceCreateInfo fi;
        memset(&ai, 0, sizeof ai);
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = g_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (!vk_ok(vkAllocateCommandBuffers(g_dev, &ai, &g_ucb), "upload buffer"))
            return 0;
        memset(&fi, 0, sizeof fi);
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(g_dev, &fi, NULL, &g_ufence);
    }
    if (g_nustage == g_custage) {
        int n = g_custage ? g_custage * 2 : 256;
        void *nb = realloc(g_ustage, (size_t)n * sizeof *g_ustage);
        if (!nb)
            return 0;
        g_ustage = nb;
        g_custage = n;
    }
    if (!g_ucb_on) {
        memset(&bi, 0, sizeof bi);
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(g_ucb, &bi);       /* the pool resets it */
        g_ucb_on = 1;
    }
    return 1;
}

static void upload(vk_image *im, int x, int y, int w, int h, int levels, const unsigned char *const *rgba)
{
    VkBuffer sb;
    VkDeviceMemory sm;
    unsigned char *map;
    VkBufferImageCopy cp[16];
    VkDeviceSize total = 0, off = 0;
    VkCommandBuffer keep = g_cmd;
    int l, lw = w, lh = h;

    for (l = 0; l < levels; l++) {
        total += (VkDeviceSize)lw * lh * 4;
        lw = lw > 1 ? lw / 2 : 1;
        lh = lh > 1 ? lh / 2 : 1;
    }
    if (!upload_begin(total))
        return;
    if (!make_buffer(total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &sb, &sm, (void **)&map))
        return;
    lw = w;
    lh = h;
    memset(cp, 0, sizeof cp);
    for (l = 0; l < levels && l < 16; l++) {
        memcpy(map + off, rgba[l], (size_t)lw * lh * 4);
        cp[l].bufferOffset = off;
        cp[l].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp[l].imageSubresource.mipLevel = (unsigned)l;
        cp[l].imageSubresource.layerCount = 1;
        cp[l].imageOffset.x = l ? 0 : x;
        cp[l].imageOffset.y = l ? 0 : y;
        cp[l].imageExtent.width = (unsigned)lw;
        cp[l].imageExtent.height = (unsigned)lh;
        cp[l].imageExtent.depth = 1;
        off += (VkDeviceSize)lw * lh * 4;
        lw = lw > 1 ? lw / 2 : 1;
        lh = lh > 1 ? lh / 2 : 1;
    }
    g_ustage[g_nustage].b = sb;
    g_ustage[g_nustage].m = sm;
    g_nustage++;
    g_ubytes += total;
    g_cmd = g_ucb;
    to_layout(im, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdCopyBufferToImage(g_ucb, sb, im->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (unsigned)(levels < 16 ? levels : 16), cp);
    to_layout(im, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    g_cmd = keep;
}

static PFN_vkFreeCommandBuffers g_free_cbs;
void vk_free_cb(VkCommandBuffer cb)
{
    if (!g_free_cbs)
        g_free_cbs = (PFN_vkFreeCommandBuffers)vkGetDeviceProcAddr(g_dev, "vkFreeCommandBuffers");
    if (g_free_cbs)
        g_free_cbs(g_dev, g_pool, 1, &cb);
}

/* a texture the recorded commands still read: run them first */
static void before_write(vk_tex *t)
{
    if (t->serial == g_serial && g_cmd_on)
        flush(0);
}

static void vk_tex_image(int i, int w, int h, int levels, const unsigned char *const *rgba)
{
    vk_tex *t;

    if (i <= 0 || i >= g_ntex || !g_tex[i].used)
        return;
    t = &g_tex[i];
    before_write(t);
    if (t->im.img && (t->w != w || t->h != h || t->levels != levels))
        free_image(&t->im);
    if (!t->im.img && !make_image(&t->im, w, h, levels, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                  VK_IMAGE_ASPECT_COLOR_BIT))
        return;
    t->w = w;
    t->h = h;
    t->levels = levels;
    t->im.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    upload(&t->im, 0, 0, w, h, levels, rgba);
}

static void vk_tex_sub(int i, int x, int y, int w, int h, const void *rgba)
{
    const unsigned char *lv[1];
    vk_tex *t;

    if (i <= 0 || i >= g_ntex || !g_tex[i].used || !g_tex[i].im.img)
        return;
    t = &g_tex[i];
    before_write(t);
    lv[0] = (const unsigned char *)rgba;
    {
        int keep = t->im.levels;
        t->im.levels = keep;            /* the barrier covers every level */
        upload(&t->im, x, y, w, h, 1, lv);
    }
}

static void vk_tex_free(int i)
{
    if (i <= 0 || i >= g_ntex || !g_tex[i].used)
        return;
    before_write(&g_tex[i]);
    free_image(&g_tex[i].im);
    g_tex[i].used = 0;
}

/* -------------------------------------------------------------- targets */

static VkSampleCountFlagBits samples_of(int s)
{
    VkSampleCountFlags ok = g_props.limits.framebufferColorSampleCounts
                          & g_props.limits.framebufferDepthSampleCounts;
    if (s >= 16 && (ok & VK_SAMPLE_COUNT_16_BIT)) return VK_SAMPLE_COUNT_16_BIT;
    if (s >= 8 && (ok & VK_SAMPLE_COUNT_8_BIT)) return VK_SAMPLE_COUNT_8_BIT;
    if (s >= 4 && (ok & VK_SAMPLE_COUNT_4_BIT)) return VK_SAMPLE_COUNT_4_BIT;
    if (s >= 2 && (ok & VK_SAMPLE_COUNT_2_BIT)) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

static int vk_target_new(int w, int h, int samples, int depth)
{
    vk_target *g;
    VkSampleCountFlagBits sc = samples_of(samples);
    int i;

    for (i = 1; i < g_ntg; i++)
        if (!g_tg[i].used)
            break;
    if (i >= g_ntg) {
        int n = g_ntg ? g_ntg * 2 : 8;
        g_tg = (vk_target *)realloc(g_tg, (size_t)n * sizeof *g_tg);
        memset(g_tg + g_ntg, 0, (size_t)(n - g_ntg) * sizeof *g_tg);
        if (!g_ntg)
            i = 1;
        g_ntg = n;
    }
    g = &g_tg[i];
    memset(g, 0, sizeof *g);
    g->w = w;
    g->h = h;
    g->samples = (int)sc;
    g->depth = depth;
    g->fmt = VK_FORMAT_R8G8B8A8_UNORM;
    if (!make_image(&g->color, w, h, 1, g->fmt, sc, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
        return 0;
    if (depth && !make_image(&g->dep, w, h, 1, g_depth_fmt, sc,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
        return 0;
    g->pass = get_pass(g->fmt, sc, depth);
    g->fb = make_fb(g->pass, g->color.view, depth ? g->dep.view : VK_NULL_HANDLE, w, h);
    g->used = 1;
    return i;
}

static void vk_target_free(int i)
{
    if (i <= 0 || i >= g_ntg || !g_tg[i].used)
        return;
    if (g_cmd_on)
        flush(0);
    if (g_cur_target == i)
        g_cur_target = 0;
    if (g_shadow == i)
        g_shadow = 0;
    vkDestroyFramebuffer(g_dev, g_tg[i].fb, NULL);
    free_image(&g_tg[i].color);
    free_image(&g_tg[i].dep);
    g_tg[i].used = 0;
}

static void vk_target_use(int t)
{
    if (t < 0 || (t > 0 && (t >= g_ntg || !g_tg[t].used)))
        t = 0;
    if (t != g_cur_target)
        end_pass();
    g_cur_target = t;
}

static void vk_target_size(int t, int *w, int *h)
{
    if (t == 0 && !g_frame_on) {
        int pw = 0, ph = 0;
        plat_size(&pw, &ph);
        if (pw > 0 && ph > 0) { *w = pw; *h = ph; return; }
    }
    tsize(t, w, h);
}

/* ------------------------------------------------------------------ draws */

static VkRect2D gl_rect(const rgl_rect *r, int tw, int th)
{
    VkRect2D o;
    int x0 = r->x, y0 = th - (r->y + r->h), x1 = r->x + r->w, y1 = th - r->y;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > tw) x1 = tw;
    if (y1 > th) y1 = th;
    o.offset.x = x0;
    o.offset.y = y0;
    o.extent.width = (unsigned)(x1 > x0 ? x1 - x0 : 0);
    o.extent.height = (unsigned)(y1 > y0 ? y1 - y0 : 0);
    return o;
}

static void vk_clear(const rgl_rect *sc, int color, const float rgba[4], int depth, float z)
{
    VkClearAttachment ca[2];
    VkClearRect cr;
    int n = 0, w, h, t;

    if (!color && !depth)
        return;
    if (!begin_pass())
        return;
    t = g_cur_target;
    tsize(t, &w, &h);
    memset(ca, 0, sizeof ca);
    if (color) {
        ca[n].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ca[n].colorAttachment = 0;
        memcpy(ca[n].clearValue.color.float32, rgba, 4 * sizeof(float));
        n++;
    }
    if (depth && (t == 0 || g_tg[t].depth)) {
        ca[n].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        ca[n].clearValue.depthStencil.depth = z;
        n++;
    }
    if (!n)
        return;
    memset(&cr, 0, sizeof cr);
    if (sc)
        cr.rect = gl_rect(sc, w, h);
    else {
        cr.rect.extent.width = (unsigned)w;
        cr.rect.extent.height = (unsigned)h;
    }
    cr.layerCount = 1;
    if (cr.rect.extent.width && cr.rect.extent.height)
        vkCmdClearAttachments(g_cmd, (unsigned)n, ca, 1, &cr);
}

static VkBlendFactor vk_bf(int f)
{
    static const VkBlendFactor k[] = {
        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_SRC_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_DST_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR };
    return f >= 0 && f < 10 ? k[f] : VK_BLEND_FACTOR_ONE;
}

static VkPipeline get_pipe(const rgl_pipe *k, int pass)
{
    VkPipelineShaderStageCreateInfo st[2];
    VkVertexInputBindingDescription vb;
    VkVertexInputAttributeDescription va[4];
    VkPipelineVertexInputStateCreateInfo vi;
    VkPipelineInputAssemblyStateCreateInfo ia;
    VkPipelineViewportStateCreateInfo vp;
    VkPipelineRasterizationStateCreateInfo rs;
    VkPipelineMultisampleStateCreateInfo ms;
    VkPipelineDepthStencilStateCreateInfo ds;
    VkPipelineColorBlendAttachmentState ba;
    VkPipelineColorBlendStateCreateInfo cb;
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dy;
    VkGraphicsPipelineCreateInfo pi;
    VkPipeline p = VK_NULL_HANDLE;
    int i;

    for (i = 0; i < g_npipes; i++)
        if (g_pipes[i].pass == pass && memcmp(&g_pipes[i].key, k, sizeof *k) == 0)
            return g_pipes[i].p;

    memset(st, 0, sizeof st);
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = g_vs;
    st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = g_fs;
    st[1].pName = "main";
    vb.binding = 0;
    vb.stride = sizeof(rgl_vtx);
    vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    va[0].location = 0; va[0].binding = 0; va[0].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[0].offset = 0;
    va[1].location = 1; va[1].binding = 0; va[1].format = VK_FORMAT_R8G8B8A8_UNORM; va[1].offset = 16;
    va[2].location = 2; va[2].binding = 0; va[2].format = VK_FORMAT_R32G32_SFLOAT; va[2].offset = 20;
    va[3].location = 3; va[3].binding = 0; va[3].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[3].offset = 28;
    memset(&vi, 0, sizeof vi);
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 4;
    vi.pVertexAttributeDescriptions = va;
    memset(&ia, 0, sizeof ia);
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = k->topo == RGL_LINES ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    memset(&vp, 0, sizeof vp);
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    memset(&rs, 0, sizeof rs);
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = k->cull == RGL_CULL_FRONT ? VK_CULL_MODE_FRONT_BIT
                : k->cull == RGL_CULL_BACK ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    memset(&ms, 0, sizeof ms);
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = g_pass[pass].samples;
    memset(&ds, 0, sizeof ds);
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = k->depth_test && g_pass[pass].depth;
    ds.depthWriteEnable = k->depth_write && g_pass[pass].depth;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    memset(&ba, 0, sizeof ba);
    ba.blendEnable = k->blend;
    ba.srcColorBlendFactor = ba.srcAlphaBlendFactor = vk_bf(k->src);
    ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = vk_bf(k->dst);
    ba.colorBlendOp = ba.alphaBlendOp = VK_BLEND_OP_ADD;
    ba.colorWriteMask = k->mask;
    memset(&cb, 0, sizeof cb);
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;
    memset(&dy, 0, sizeof dy);
    dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    memset(&pi, 0, sizeof pi);
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount = 2;
    pi.pStages = st;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;
    pi.pDynamicState = &dy;
    pi.layout = g_layout;
    pi.renderPass = g_pass[pass].rp;
    if (!vk_ok(vkCreateGraphicsPipelines(g_dev, g_pcache, 1, &pi, NULL, &p), "vkCreateGraphicsPipelines"))
        return VK_NULL_HANDLE;
    if (g_npipes >= g_cpipes) {
        g_cpipes = g_cpipes ? g_cpipes * 2 : 64;
        g_pipes = (vk_pipe *)realloc(g_pipes, (size_t)g_cpipes * sizeof *g_pipes);
    }
    g_pipes[g_npipes].key = *k;
    g_pipes[g_npipes].pass = pass;
    g_pipes[g_npipes].p = p;
    g_npipes++;
    if (!g_preloading)
        rgl_pipes_note(PIPES_FILE, k, sizeof *k, g_pass[pass].fmt, g_pass[pass].samples, g_pass[pass].depth);
    return p;
}

/* a pipeline of the list: its render pass, then itself */
static void preload_one(const void *key, int fmt, int samples, int depth)
{
    get_pipe((const rgl_pipe *)key, get_pass((VkFormat)fmt, (VkSampleCountFlagBits)samples, depth));
}

static void preload(void)
{
    double t0 = rgl_now_ms();
    int n0 = g_npipes;

    g_preloading = 1;
    rgl_pipes_each(PIPES_FILE, k_seed_pipes, sizeof(rgl_pipe), preload_one);
    g_preloading = 0;
    printf("vulkan: %d pipelines ready in %.0f ms\n", g_npipes - n0, rgl_now_ms() - t0);
}

static VkSampler get_sampler(const rgl_samp *s)
{
    unsigned key = (unsigned)s->mag_linear | (unsigned)s->min_linear << 1 | (unsigned)s->mip << 2
                 | (unsigned)s->wrap_s_clamp << 4 | (unsigned)s->wrap_t_clamp << 5
                 | (unsigned)(g_aniso_ok ? s->aniso : 1) << 8;
    VkSamplerCreateInfo si;
    VkSampler out = VK_NULL_HANDLE;
    int i;

    for (i = 0; i < g_nsamps; i++)
        if (g_samps[i].key == key)
            return g_samps[i].s;
    memset(&si, 0, sizeof si);
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = s->mag_linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.minFilter = s->min_linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.mipmapMode = s->mip == 2 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = s->wrap_s_clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV = s->wrap_t_clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxLod = s->mip ? 16.0f : 0.25f;   /* 0.25: level 0 only, as GL_LINEAR */
    if (g_aniso_ok && s->aniso > 1) {
        si.anisotropyEnable = VK_TRUE;
        si.maxAnisotropy = s->aniso > g_props.limits.maxSamplerAnisotropy
                         ? g_props.limits.maxSamplerAnisotropy : (float)s->aniso;
    }
    vkCreateSampler(g_dev, &si, NULL, &out);
    if (g_nsamps < 64) {
        g_samps[g_nsamps].key = key;
        g_samps[g_nsamps].s = out;
        g_nsamps++;
    }
    return out;
}

static VkDescriptorSet get_set(int tex, const rgl_samp *s, VkImageView shadow)
{
    static int last_tex = -1;
    static rgl_samp last_s;
    static VkDescriptorSet last;
    static VkImageView last_shadow;
    static unsigned last_serial;
    VkDescriptorSetAllocateInfo ai;
    VkDescriptorImageInfo ii[2];
    VkWriteDescriptorSet w[2];
    VkDescriptorSet set = VK_NULL_HANDLE;
    rgl_samp point;

    if (tex == last_tex && last_serial == g_serial && g_dpool_used && shadow == last_shadow
        && memcmp(&last_s, s, sizeof *s) == 0)
        return last;
    if (g_dpool_used >= DPOOL_SETS) {
        flush(0);
        begin_pass();
    }
    memset(&ai, 0, sizeof ai);
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_dpool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &g_dsl;
    if (!vk_ok(vkAllocateDescriptorSets(g_dev, &ai, &set), "vkAllocateDescriptorSets"))
        return VK_NULL_HANDLE;
    g_dpool_used++;
    ii[0].sampler = get_sampler(s);
    ii[0].imageView = g_tex[tex].im.view;
    ii[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    /* the shadow map texel by texel, clamped; white when there is none */
    memset(&point, 0, sizeof point);
    point.wrap_s_clamp = point.wrap_t_clamp = 1;
    point.aniso = 1;
    ii[1].sampler = get_sampler(&point);
    ii[1].imageView = shadow ? shadow : g_tex[g_white].im.view;
    ii[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    memset(w, 0, sizeof w);
    w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w[0].dstSet = set;
    w[0].descriptorCount = 1;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[0].pImageInfo = &ii[0];
    w[1] = w[0];
    w[1].dstBinding = 1;
    w[1].pImageInfo = &ii[1];
    vkUpdateDescriptorSets(g_dev, 2, w, 0, NULL);
    last_tex = tex;
    last_s = *s;
    last_shadow = shadow;
    last = set;
    last_serial = g_serial;
    return set;
}

static void vk_draw(const rgl_pipe *k, const rgl_frag *f, int tex, const rgl_samp *s,
                    const rgl_rect *viewport, const rgl_rect *scissor, const rgl_vtx *v, int n)
{
    VkDeviceSize bytes = (VkDeviceSize)n * sizeof *v, off;
    VkPipeline p;
    VkViewport vp;
    VkRect2D sc;
    VkDescriptorSet set;
    VkImageView shadow;
    rgl_samp plain;
    int w, h, pass;

    if (n <= 0)
        return;
    if (g_voff + bytes > g_vsize) {
        /* the frame outgrew the buffer: a bigger one, as big as the frame
         * needs, so the frames after fit (REAL-TIME SHADOWS and SMOOTH
         * MODELS draw a frame's fighters twice and thicker). The commands
         * already recorded keep reading the old one, freed at the next
         * flush, so the GPU is not waited for partway through the frame.
         * There was a 256 MB ceiling, past which every overflow waited and
         * made the buffer again. */
        VkDeviceSize want = g_voff + bytes, size = g_vsize;
        VkBuffer nb = VK_NULL_HANDLE;
        VkDeviceMemory nm = VK_NULL_HANDLE;
        void *nmap = NULL;
        while (size < want)
            size *= 2;
        if (g_nretired < (int)(sizeof g_retired / sizeof *g_retired)
            && make_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &nb, &nm, &nmap)) {
            g_retired[g_nretired].b = g_vbuf;
            g_retired[g_nretired++].m = g_vmem;
            g_vbuf = nb;
            g_vmem = nm;
            g_vmap = nmap;
            g_vsize = size;
            g_voff = 0;
        } else {                        /* no memory for it: wait and reuse */
            if (g_cmd_on)
                flush(0);
            if (bytes > g_vsize)
                return;
        }
    }
    if (!begin_pass())
        return;
    pass = g_cur_target ? g_tg[g_cur_target].pass : g_win_pass;
    p = get_pipe(k, pass);
    if (!p)
        return;
    if (tex <= 0 || tex >= g_ntex || !g_tex[tex].used || !g_tex[tex].im.img) {
        tex = g_white;
        memset(&plain, 0, sizeof plain);
        s = &plain;
    }
    g_tex[tex].serial = g_serial;
    /* a draw that receives samples the shadow map, readable: its layout
     * changes outside a render pass, once a frame */
    shadow = VK_NULL_HANDLE;
    if ((f->shadow & 15) == RGL_SHADOW_RECEIVE && g_shadow > 0 && g_shadow < g_ntg && g_tg[g_shadow].used
        && g_shadow != g_cur_target) {
        vk_image *im = &g_tg[g_shadow].color;
        if (im->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            end_pass();
            to_layout(im, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (!begin_pass())
                return;
        }
        shadow = im->view;
    }
    set = get_set(tex, s, shadow);
    if (!set)
        return;
    off = g_voff;
    memcpy(g_vmap + off, v, (size_t)bytes);
    g_voff += (bytes + 15) & ~(VkDeviceSize)15;

    tsize(g_cur_target, &w, &h);
    vp.x = (float)viewport->x;
    vp.y = (float)(h - (viewport->y + viewport->h));
    vp.width = (float)viewport->w;
    vp.height = (float)viewport->h;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    if (scissor)
        sc = gl_rect(scissor, w, h);
    else {
        sc.offset.x = sc.offset.y = 0;
        sc.extent.width = (unsigned)w;
        sc.extent.height = (unsigned)h;
    }
    if (!sc.extent.width || !sc.extent.height || vp.width <= 0 || vp.height <= 0)
        return;
    vkCmdBindPipeline(g_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
    vkCmdSetViewport(g_cmd, 0, 1, &vp);
    vkCmdSetScissor(g_cmd, 0, 1, &sc);
    vkCmdPushConstants(g_cmd, g_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof *f, f);
    vkCmdBindDescriptorSets(g_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_layout, 0, 1, &set, 0, NULL);
    vkCmdBindVertexBuffers(g_cmd, 0, 1, &g_vbuf, &off);
    vkCmdDraw(g_cmd, (unsigned)n, 1, 0, 0);
}

/* ---------------------------------------------------- blits and reads */

/* a target's colour image, its layout tracked, for transfers */
static VkImage timg(int t, VkImageLayout l)
{
    if (t > 0) {
        to_layout(&g_tg[t].color, l);
        return g_tg[t].color.img;
    }
    barrier(g_images[g_image], VK_IMAGE_ASPECT_COLOR_BIT, 1, g_img_layout[g_image], l);
    g_img_layout[g_image] = l;
    return g_images[g_image];
}

static void vk_blit(int src, const rgl_rect *a, int dst, const rgl_rect *b, int linear)
{
    int sw, sh, dw, dh;
    VkImage si, di;

    frame_begin();
    if (!g_frame_on)
        return;
    cmd_begin();
    end_pass();
    if (src > 0 && (src >= g_ntg || !g_tg[src].used)) return;
    if (dst > 0 && (dst >= g_ntg || !g_tg[dst].used)) return;
    tsize(src, &sw, &sh);
    tsize(dst, &dw, &dh);
    si = timg(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    di = timg(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    if (src > 0 && g_tg[src].samples > 1) {
        /* glBlitFramebuffer from a multisampled buffer: a resolve, same size */
        VkImageResolve r;
        VkRect2D ra = gl_rect(a, sw, sh), rb = gl_rect(b, dw, dh);
        memset(&r, 0, sizeof r);
        r.srcSubresource.aspectMask = r.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        r.srcSubresource.layerCount = r.dstSubresource.layerCount = 1;
        r.srcOffset.x = ra.offset.x; r.srcOffset.y = ra.offset.y;
        r.dstOffset.x = rb.offset.x; r.dstOffset.y = rb.offset.y;
        r.extent.width = ra.extent.width < rb.extent.width ? ra.extent.width : rb.extent.width;
        r.extent.height = ra.extent.height < rb.extent.height ? ra.extent.height : rb.extent.height;
        r.extent.depth = 1;
        vkCmdResolveImage(g_cmd, si, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, di,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
    } else {
        VkImageBlit r;
        memset(&r, 0, sizeof r);
        r.srcSubresource.aspectMask = r.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        r.srcSubresource.layerCount = r.dstSubresource.layerCount = 1;
        /* GL's corners, bottom-up, as Vulkan's top-down ones: a flip in
         * either stays a flip */
        r.srcOffsets[0].x = a->x;           r.srcOffsets[0].y = sh - a->y;
        r.srcOffsets[1].x = a->x + a->w;    r.srcOffsets[1].y = sh - (a->y + a->h);
        r.dstOffsets[0].x = b->x;           r.dstOffsets[0].y = dh - b->y;
        r.dstOffsets[1].x = b->x + b->w;    r.dstOffsets[1].y = dh - (b->y + b->h);
        r.srcOffsets[1].z = r.dstOffsets[1].z = 1;
        vkCmdBlitImage(g_cmd, si, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, di,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r,
                       linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
    }
}

static void vk_read(int t, const rgl_rect *r, unsigned char *out)
{
    VkBuffer b;
    VkDeviceMemory m;
    unsigned char *map;
    VkBufferImageCopy cp;
    VkRect2D rr;
    int tw, th, j, i, bgra;

    frame_begin();
    if (!g_frame_on)
        return;
    if (t > 0 && (t >= g_ntg || !g_tg[t].used))
        t = 0;
    tsize(t, &tw, &th);
    rr = gl_rect(r, tw, th);
    memset(out, 0, (size_t)r->w * r->h * 4);
    if (!rr.extent.width || !rr.extent.height)
        return;
    if (!make_buffer((VkDeviceSize)rr.extent.width * rr.extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     &b, &m, (void **)&map))
        return;
    cmd_begin();
    end_pass();
    memset(&cp, 0, sizeof cp);
    cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cp.imageSubresource.layerCount = 1;
    cp.imageOffset.x = rr.offset.x;
    cp.imageOffset.y = rr.offset.y;
    cp.imageExtent.width = rr.extent.width;
    cp.imageExtent.height = rr.extent.height;
    cp.imageExtent.depth = 1;
    vkCmdCopyImageToBuffer(g_cmd, timg(t, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b, 1, &cp);
    flush(0);
    bgra = t == 0 && g_swap_fmt == VK_FORMAT_B8G8R8A8_UNORM;
    /* rows from the bottom, as glReadPixels; the clipped part where it falls */
    for (j = 0; j < (int)rr.extent.height; j++) {
        int gy = th - 1 - (rr.offset.y + j) - r->y;   /* row in the caller's rect, from its bottom */
        const unsigned char *s = map + (size_t)j * rr.extent.width * 4;
        unsigned char *d;
        if (gy < 0 || gy >= r->h)
            continue;
        d = out + ((size_t)gy * r->w + (rr.offset.x - r->x)) * 4;
        for (i = 0; i < (int)rr.extent.width; i++, s += 4, d += 4) {
            d[0] = bgra ? s[2] : s[0];
            d[1] = s[1];
            d[2] = bgra ? s[0] : s[2];
            d[3] = s[3];
        }
    }
    free_buffer(b, m);
}

static void vk_present(void) { present(); }
static void vk_finish(void) { }
static void vk_shadow_map(int t) { g_shadow = t > 0 && t < g_ntg && g_tg[t].used ? t : 0; }

rgl_backend rgl_vk = {
    "Vulkan", vk_open, vk_close, vk_device, 4096, 1,
    vk_tex_new, vk_tex_image, vk_tex_sub, vk_tex_free,
    vk_target_new, vk_target_free, vk_target_use, vk_target_size,
    vk_clear, vk_draw, vk_blit, vk_read, vk_present, vk_finish, vk_shadow_map,
};
