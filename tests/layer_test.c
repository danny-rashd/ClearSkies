// Clear Skies: a test of the layer outside the game.
//
//   layer_test.exe <folder with libshaderc_shared.dll>      (Windows)
//   layer_test [folder with libshaderc_shared.so.1]          (Linux: without a folder, the system's shaderc)
//
// Run with the layer turned on (tests/run_tests.ps1 or tests/run_tests.sh does that). Compiles a stand-in for the tone-map composite as
// Savage Wind writes it (the same constants, bindings and interface) and an ordinary fragment shader, creates a
// Vulkan device on the first GPU and calls vkCreateShaderModule with both. The layer's log (clearskies.log) then
// says whether Clarity was compiled and swapped in; run_tests checks it.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define SHADERC_FILE "libshaderc_shared.dll"
#define VULKAN_FILE "vulkan-1.dll"
#else
#include <dlfcn.h>
typedef void *HMODULE;
#define MAX_PATH 4096
#define GetProcAddress dlsym
// RTLD_GLOBAL: the game links shaderc, so the layer finds it already loaded; the test does the same
#define LoadLibraryA(p) dlopen((p), RTLD_NOW | RTLD_GLOBAL)
#define SHADERC_FILE "libshaderc_shared.so.1"
#define VULKAN_FILE "libvulkan.so.1"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/vk_min.h"

typedef struct { VkStructureType sType; const void *pNext; const char *app; uint32_t appv; const char *eng; uint32_t engv; uint32_t api; } AppInfo;
typedef struct { VkStructureType sType; const void *pNext; VkFlags flags; const AppInfo *app; uint32_t nl; const char *const *l; uint32_t ne; const char *const *e; } InstInfo;
typedef struct { VkStructureType sType; const void *pNext; VkFlags flags; uint32_t family, count; const float *prio; } QueueInfo;
typedef struct { VkStructureType sType; const void *pNext; VkFlags flags; uint32_t nq; const QueueInfo *q; uint32_t nl; const char *const *l; uint32_t ne; const char *const *e; const void *features; } DevInfo;

static const char k_tonemap[] =  // shaped like Savage Wind's translation of BOTW's composite
    "#version 460\n"
    "layout(set = 1, binding = 32) uniform sampler2D t0;\n"
    "layout(set = 1, binding = 33) uniform sampler2D t1;\n"
    "layout(location = 0) in vec4 v0;\n"
    "layout(location = 0) out vec4 c0;\n"
    "void main() {\n"
    "  vec3 c = min(texture(t0, v0.xy).rgb, vec3(uintBitsToFloat(0x461c4000u))) + texture(t1, v0.xy).rgb;\n"
    "  float l = dot(c, vec3(uintBitsToFloat(0x3e99096cu), 0.5866, 0.1145));\n"
    "  c0 = vec4(1.0 - exp2(c * uintBitsToFloat(0xbfb8aa3bu)), l);\n"
    "}\n";

static const char k_other[] =  // any other fragment shader (three textures: not the composite)
    "#version 460\n"
    "layout(set = 1, binding = 32) uniform sampler2D t0;\n"
    "layout(set = 1, binding = 33) uniform sampler2D t1;\n"
    "layout(set = 1, binding = 34) uniform sampler2D t2;\n"
    "layout(location = 0) in vec4 v0;\n"
    "layout(location = 0) out vec4 c0;\n"
    "void main() { c0 = texture(t0, v0.xy) + texture(t1, v0.xy) * texture(t2, v0.xy) * uintBitsToFloat(0x461c4000u); }\n";

typedef void *(*PFN_init)(void);
typedef void (*PFN_set_env)(void *, int, uint32_t);
typedef void *(*PFN_compile)(void *, const char *, size_t, int, const char *, const char *, void *);
typedef int (*PFN_status)(void *);
typedef size_t (*PFN_len)(void *);
typedef const char *(*PFN_bytes)(void *);

static HMODULE g_sc;
static uint32_t *spirv(const char *src, size_t *bytes) {
    PFN_init ci = (PFN_init)(void *)GetProcAddress(g_sc, "shaderc_compiler_initialize");
    PFN_init oi = (PFN_init)(void *)GetProcAddress(g_sc, "shaderc_compile_options_initialize");
    PFN_set_env env = (PFN_set_env)(void *)GetProcAddress(g_sc, "shaderc_compile_options_set_target_env");
    PFN_compile comp = (PFN_compile)(void *)GetProcAddress(g_sc, "shaderc_compile_into_spv");
    PFN_status st = (PFN_status)(void *)GetProcAddress(g_sc, "shaderc_result_get_compilation_status");
    PFN_len len = (PFN_len)(void *)GetProcAddress(g_sc, "shaderc_result_get_length");
    PFN_bytes by = (PFN_bytes)(void *)GetProcAddress(g_sc, "shaderc_result_get_bytes");
    PFN_bytes err = (PFN_bytes)(void *)GetProcAddress(g_sc, "shaderc_result_get_error_message");
    void *c = ci(), *o = oi();
    env(o, 0, (1u << 22) | (3u << 12));
    void *r = comp(c, src, strlen(src), 1, "test.frag", "main", o);
    if (st(r)) { printf("FAIL: test shader didn't compile: %s\n", err(r)); return NULL; }
    *bytes = len(r);
    uint32_t *out = malloc(*bytes);
    memcpy(out, by(r), *bytes);
    return out;
}

#define GET(type, name) type name = (type)(void *)gipa(inst, #name)

int main(int argc, char **argv) {
    char p[MAX_PATH];
#ifdef _WIN32
    if (argc < 2) { printf("usage: layer_test <folder with libshaderc_shared.dll>\n"); return 2; }
    snprintf(p, sizeof p, "%s\\" SHADERC_FILE, argv[1]);
    g_sc = LoadLibraryA(p);
#else
    static const char *const system_names[] = {"libshaderc_shared.so.1", "libshaderc_shared.so", "libshaderc.so.1"};
    if (argc >= 2) {
        snprintf(p, sizeof p, "%s/" SHADERC_FILE, argv[1]);
        g_sc = LoadLibraryA(p);
    } else {
        for (size_t i = 0; !g_sc && i < sizeof system_names / sizeof system_names[0]; i++) {
            snprintf(p, sizeof p, "%s", system_names[i]);
            g_sc = LoadLibraryA(p);
        }
    }
#endif
    if (!g_sc) { printf("FAIL: can't load %s\n", p); return 1; }
    size_t nt, no;
    uint32_t *tonemap = spirv(k_tonemap, &nt), *other = spirv(k_other, &no);
    if (!tonemap || !other) return 1;

    HMODULE vk = LoadLibraryA(VULKAN_FILE);
    if (!vk) { printf("FAIL: no Vulkan loader (" VULKAN_FILE ")\n"); return 1; }
    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)(void *)GetProcAddress(vk, "vkGetInstanceProcAddr");
    VkInstance inst = NULL;
    PFN_vkCreateInstance create_inst = (PFN_vkCreateInstance)(void *)gipa(NULL, "vkCreateInstance");
    AppInfo app = {0, NULL, "clearskies-test", 1, NULL, 0, (1u << 22) | (3u << 12)};
    InstInfo ii = {1, NULL, 0, &app, 0, NULL, 0, NULL};
    VkResult r = create_inst((const VkInstanceCreateInfo *)&ii, NULL, &inst);
    if (r) { printf("FAIL: vkCreateInstance %d\n", r); return 1; }

    typedef VkResult (VKAPI_PTR *PFN_enum)(VkInstance, uint32_t *, VkPhysicalDevice *);
    GET(PFN_enum, vkEnumeratePhysicalDevices);
    GET(PFN_vkCreateDevice, vkCreateDevice);
    GET(PFN_vkGetDeviceProcAddr, vkGetDeviceProcAddr);
    uint32_t n = 1;
    VkPhysicalDevice phys = NULL;
    vkEnumeratePhysicalDevices(inst, &n, &phys);
    if (!phys) { printf("FAIL: no GPU\n"); return 1; }
    float prio = 1.0f;
    QueueInfo qi = {2, NULL, 0, 0, 1, &prio};
    DevInfo di = {3, NULL, 0, 1, &qi, 0, NULL, 0, NULL, NULL};
    VkDevice dev = NULL;
    if ((r = vkCreateDevice(phys, (const VkDeviceCreateInfo *)&di, NULL, &dev))) { printf("FAIL: vkCreateDevice %d\n", r); return 1; }

    PFN_vkCreateShaderModule csm = (PFN_vkCreateShaderModule)(void *)vkGetDeviceProcAddr(dev, "vkCreateShaderModule");
    typedef void (VKAPI_PTR *PFN_dsm)(VkDevice, VkShaderModule, const void *);
    PFN_dsm dsm = (PFN_dsm)(void *)vkGetDeviceProcAddr(dev, "vkDestroyShaderModule");
    VkShaderModuleCreateInfo mi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, nt, tonemap};
    VkShaderModule m = 0, m2 = 0;
    int fails = 0;
    if ((r = csm(dev, &mi, NULL, &m)) || !m) { printf("FAIL: tone-map module %d\n", r); fails++; }
    mi.codeSize = no, mi.pCode = other;
    if ((r = csm(dev, &mi, NULL, &m2)) || !m2) { printf("FAIL: other module %d\n", r); fails++; }
    if (m) dsm(dev, m, NULL);
    if (m2) dsm(dev, m2, NULL);
    PFN_vkDestroyDevice dd = (PFN_vkDestroyDevice)(void *)vkGetDeviceProcAddr(dev, "vkDestroyDevice");
    dd(dev, NULL);
    GET(PFN_vkDestroyInstance, vkDestroyInstance);
    vkDestroyInstance(inst, NULL);
    if (!fails) printf("ok: both shader modules created\n");
    return fails ? 1 : 0;
}
