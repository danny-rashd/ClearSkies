// Clear Skies: a Vulkan layer (VK_LAYER_CLEARSKIES_savage_wind).
//
// Savage Wind turns each of the game's shaders into GLSL, compiles it to SPIR-V with shaderc and hands it to
// vkCreateShaderModule. This layer sits between the game and the driver and watches vkCreateShaderModule. When the
// module is BOTW's HDR -> LDR composite (the fragment shader that adds bloom and tone maps with 1 - exp(-x),
// recognised by its constants: the 10000 clamp, the 0.2989 luma weight and -log2(e), and by its two textures at
// bindings 32 and 33), the layer compiles Clarity with the same shaderc the game ships (libshaderc_shared.dll) and
// passes that to the driver instead. Nothing on disk is changed: the game's own SPIR-V cache keeps the original.
//
// Settings: clearskies.ini next to this DLL (or the file named by CLEARSKIES_INI). Log: clearskies.log next to the ini.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clarity_shader.h"
#include "vk_min.h"

#define EXPORT __declspec(dllexport)
#define LAYER_NAME "VK_LAYER_CLEARSKIES_savage_wind"

// ------------------------------------------------------------------ log

static SRWLOCK g_log_lock = SRWLOCK_INIT;
static FILE *g_log;
static char g_dir[MAX_PATH];  // folder of this DLL, with a trailing backslash

static void layer_dir(void) {
    if (g_dir[0]) return;
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)(void *)&layer_dir, &self);
    DWORD n = GetModuleFileNameA(self, g_dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { strcpy(g_dir, ".\\"); return; }
    char *s = strrchr(g_dir, '\\');
    if (s) s[1] = 0;
}

static void ini_path(char *out, size_t n) {
    const char *env = getenv("CLEARSKIES_INI");
    if (env && *env) { snprintf(out, n, "%s", env); return; }
    layer_dir();
    snprintf(out, n, "%sclearskies.ini", g_dir);
}

static void LOG(const char *fmt, ...) {
    AcquireSRWLockExclusive(&g_log_lock);
    if (!g_log) {
        char p[MAX_PATH + 16], ini[MAX_PATH];
        ini_path(ini, sizeof ini);
        snprintf(p, sizeof p, "%s", ini);
        char *s = strrchr(p, '\\');
        snprintf(s ? s + 1 : p, sizeof p - (size_t)(s ? s + 1 - p : 0), "clearskies.log");
        g_log = fopen(p, "w");
    }
    if (g_log) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
    ReleaseSRWLockExclusive(&g_log_lock);
}

// ------------------------------------------------------------------ settings

static int g_preset = 0;                    // index into k_presets, or -1 for off
static float g_values[CLARITY_NPARAMS][3];  // the preset with clearskies.ini's overrides applied
static int g_noverrides;

// A preset by its clearskies.ini name (or Cemu's number, or a short alias); -1 off, -2 unknown.
static int find_preset(const char *s) {
    static const struct { const char *alias, *id; } aliases[] = {
        {"recommended", "serfrost"}, {"dim", "serfrost_dim"}, {"bright", "serfrost_bright"},
        {"user", "custom"}, {"user_defined", "custom"},
    };
    if (!_stricmp(s, "off") || !_stricmp(s, "disabled") || !_stricmp(s, "none")) return -1;
    for (size_t i = 0; i < sizeof aliases / sizeof aliases[0]; i++)
        if (!_stricmp(s, aliases[i].alias)) s = aliases[i].id;
    char *end;
    long n = strtol(s, &end, 10);
    for (int i = 0; i < CLARITY_NPRESETS; i++)
        if (!_stricmp(s, k_presets[i].id) || (*s && !*end && n == k_presets[i].cemu_number)) return i;
    return -2;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static void load_settings(void) {
    char path[MAX_PATH];
    ini_path(path, sizeof path);
    float over[CLARITY_NPARAMS][3];
    int have[CLARITY_NPARAMS] = {0};
    g_preset = find_preset("serfrost");
    FILE *f = fopen(path, "r");
    if (!f) LOG("no %s: using Serfrost's preset\n", path);
    char line[512], section[64] = "";
    while (f && fgets(line, sizeof line, f)) {
        char *s = trim(line);
        char *c = strpbrk(s, ";#");
        if (c) { *c = 0; s = trim(s); }
        if (!*s) continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (e) *e = 0;
            snprintf(section, sizeof section, "%s", trim(s + 1));
            continue;
        }
        if (!_stricmp(section, "launcher")) continue;  // (the launcher's own settings)
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(s), *val = trim(eq + 1);
        if (!_stricmp(key, "preset")) {
            int p = find_preset(val);
            if (p == -2) LOG("unknown preset '%s' (see clearskies.ini for the list): using serfrost\n", val);
            else g_preset = p;
            continue;
        }
        int k = -1;
        for (int i = 0; i < CLARITY_NPARAMS; i++)
            if (!_stricmp(key, k_params[i].name)) k = i;
        if (k < 0) { LOG("unknown setting '%s' ignored\n", key); continue; }
        // a number, or for colours three numbers (commas or spaces between; "vec3(...)" is fine too)
        int want = k_params[k].kind == 'v' ? 3 : 1, got = 0;
        char *q = val;
        if (!_strnicmp(q, "vec3", 4)) q += 4;
        while (got < want) {
            while (*q == ' ' || *q == '\t' || *q == ',' || *q == '(') q++;
            char *end;
            float v = strtof(q, &end);
            if (end == q) break;
            over[k][got++] = v;
            q = end;
        }
        if (got == 1 && want == 3) over[k][1] = over[k][2] = over[k][0], got = 3;  // (one number: all three)
        if (got != want) { LOG("setting '%s': '%s' is not %s\n", key, val, want == 3 ? "three numbers" : "a number"); continue; }
        have[k] = 1;
    }
    if (f) fclose(f);
    if (g_preset >= 0) {
        memcpy(g_values, k_presets[g_preset].v, sizeof g_values);
        for (int i = 0; i < CLARITY_NPARAMS; i++)
            if (have[i]) { memcpy(g_values[i], over[i], sizeof g_values[i]); g_noverrides++; }
        LOG("preset: %s (Cemu's \"%s\")%s\n", k_presets[g_preset].id, k_presets[g_preset].title,
            g_noverrides ? ", with settings changed in clearskies.ini" : "");
    } else {
        LOG("preset: off (the game's own look)\n");
    }
}

// ------------------------------------------------------------------ recognising BOTW's tone-map composite

typedef struct { int fragment, c_clamp, c_luma, c_log2e, b32, b33, b34, v0_noperspective, v0_flat; } ShaderInfo;

static int scan_spirv(const uint32_t *w, size_t n, ShaderInfo *si) {
    memset(si, 0, sizeof *si);
    if (n < 5 || w[0] != 0x07230203u) return 0;
    uint32_t bound = w[3];
    if (!bound || bound > (1u << 22)) return 0;
    unsigned char *loc0 = calloc(bound, 1), *noper = calloc(bound, 1), *flat = calloc(bound, 1), *input = calloc(bound, 1);
    if (!loc0 || !noper || !flat || !input) { free(loc0); free(noper); free(flat); free(input); return 0; }
    for (size_t i = 5; i < n;) {
        uint32_t wc = w[i] >> 16, op = w[i] & 0xFFFF;
        if (!wc || i + wc > n) break;
        const uint32_t *x = w + i;
        switch (op) {
        case 15:  // OpEntryPoint: ExecutionModel Fragment = 4
            if (wc >= 2 && x[1] == 4) si->fragment = 1;
            break;
        case 43:  // OpConstant (32-bit): result type, id, value
            if (wc == 4) {
                if (x[3] == 0x461c4000u) si->c_clamp = 1;       // 10000.0
                else if (x[3] == 0x3e99096cu) si->c_luma = 1;   // 0.2989
                else if (x[3] == 0xbfb8aa3bu) si->c_log2e = 1;  // -log2(e)
            }
            break;
        case 71:  // OpDecorate: target, decoration, literals
            if (wc >= 3 && x[1] < bound) {
                if (x[2] == 33 && wc >= 4) {  // Binding
                    if (x[3] == 32) si->b32 = 1;
                    if (x[3] == 33) si->b33 = 1;
                    if (x[3] == 34) si->b34 = 1;
                } else if (x[2] == 30 && wc >= 4 && x[3] == 0) loc0[x[1]] = 1;  // Location 0
                else if (x[2] == 13) noper[x[1]] = 1;                          // NoPerspective
                else if (x[2] == 14) flat[x[1]] = 1;                           // Flat
            }
            break;
        case 59:  // OpVariable: result type, id, storage class (Input = 1)
            if (wc >= 4 && x[2] < bound && x[3] == 1) input[x[2]] = 1;
            break;
        }
        i += wc;
    }
    for (uint32_t id = 0; id < bound; id++)
        if (input[id] && loc0[id]) { si->v0_noperspective = noper[id]; si->v0_flat = flat[id]; }
    free(loc0); free(noper); free(flat); free(input);
    return 1;
}

static int is_tonemap(const ShaderInfo *si) {
    return si->fragment && si->c_clamp && si->c_luma && si->c_log2e && si->b32 && si->b33 && !si->b34;
}

// ------------------------------------------------------------------ compiling Clarity with the game's shaderc

typedef void *(*PFN_sc_init)(void);
typedef void (*PFN_sc_release)(void *);
typedef void (*PFN_sc_set_env)(void *, int, uint32_t);
typedef void *(*PFN_sc_compile)(void *, const char *, size_t, int, const char *, const char *, void *);
typedef int (*PFN_sc_status)(void *);
typedef size_t (*PFN_sc_length)(void *);
typedef const char *(*PFN_sc_bytes)(void *);

static HMODULE find_shaderc(void) {
    HMODULE m = GetModuleHandleA("libshaderc_shared.dll");
    if (m) return m;
    char p[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, p, MAX_PATH);  // the game's folder
    if (n && n < MAX_PATH) {
        char *s = strrchr(p, '\\');
        if (s && (size_t)(s - p) + 32 < sizeof p) {
            strcpy(s + 1, "libshaderc_shared.dll");
            if ((m = LoadLibraryA(p))) return m;
        }
    }
    if ((m = LoadLibraryA("libshaderc_shared.dll"))) return m;
    return LoadLibraryA("shaderc_shared.dll");  // (the Vulkan SDK's name)
}

// Clarity's GLSL for the current settings; v0 interpolation as in the shader it replaces.
static int round_int(float v) { return (int)(v + (v < 0 ? -0.5f : 0.5f)); }

static char *clarity_source(const ShaderInfo *si) {
    size_t cap = sizeof k_interface + sizeof k_cemu_functions + sizeof k_main + 16384;
    char *s = malloc(cap);
    if (!s) return NULL;
    int n = snprintf(s, cap, "#version 460\n// Clarity (Cemu graphic packs, CC0) for Savage Wind: %s\n",
                     k_presets[g_preset].title);
    for (int i = 0; i < CLARITY_NPARAMS; i++) {
        const char *nm = k_params[i].name;
        const float *v = g_values[i];
        switch (k_params[i].kind) {
        case 'd': n += snprintf(s + n, cap - n, "#define %s %d\n", nm, round_int(v[0])); break;
        case 'i': n += snprintf(s + n, cap - n, "const int %s = %d;\n", nm, round_int(v[0])); break;
        case 'v': n += snprintf(s + n, cap - n, "const vec3 %s = vec3(%.6f, %.6f, %.6f);\n", nm, v[0], v[1], v[2]); break;
        default: n += snprintf(s + n, cap - n, "const float %s = %.6f;\n", nm, v[0]); break;
        }
    }
    n += snprintf(s + n, cap - n, "#define V0_INTERP %s\n", si->v0_flat ? "flat" : si->v0_noperspective ? "noperspective" : "");
    memcpy(s + n, k_interface, sizeof k_interface - 1), n += (int)sizeof k_interface - 1;
    memcpy(s + n, k_cemu_functions, sizeof k_cemu_functions - 1), n += (int)sizeof k_cemu_functions - 1;
    memcpy(s + n, k_main, sizeof k_main);
    return s;
}

static SRWLOCK g_compile_lock = SRWLOCK_INIT;
static uint32_t *g_spv[3];  // by v0 interpolation: smooth, noperspective, flat
static size_t g_spv_words[3];
static int g_failed[3];

// Clarity's SPIR-V (compiled on first use, kept for the session), or NULL to leave the game's shader alone.
static const uint32_t *clarity_spirv(const ShaderInfo *si, size_t *nwords) {
    int v = si->v0_flat ? 2 : si->v0_noperspective ? 1 : 0;
    AcquireSRWLockExclusive(&g_compile_lock);
    if (!g_spv[v] && !g_failed[v]) {
        g_failed[v] = 1;
        HMODULE sc = find_shaderc();
        PFN_sc_init init = sc ? (PFN_sc_init)(void *)GetProcAddress(sc, "shaderc_compiler_initialize") : NULL;
        PFN_sc_release comp_release = sc ? (PFN_sc_release)(void *)GetProcAddress(sc, "shaderc_compiler_release") : NULL;
        PFN_sc_init opt_init = sc ? (PFN_sc_init)(void *)GetProcAddress(sc, "shaderc_compile_options_initialize") : NULL;
        PFN_sc_release opt_release = sc ? (PFN_sc_release)(void *)GetProcAddress(sc, "shaderc_compile_options_release") : NULL;
        PFN_sc_set_env set_env = sc ? (PFN_sc_set_env)(void *)GetProcAddress(sc, "shaderc_compile_options_set_target_env") : NULL;
        PFN_sc_compile compile = sc ? (PFN_sc_compile)(void *)GetProcAddress(sc, "shaderc_compile_into_spv") : NULL;
        PFN_sc_status status = sc ? (PFN_sc_status)(void *)GetProcAddress(sc, "shaderc_result_get_compilation_status") : NULL;
        PFN_sc_length length = sc ? (PFN_sc_length)(void *)GetProcAddress(sc, "shaderc_result_get_length") : NULL;
        PFN_sc_bytes bytes = sc ? (PFN_sc_bytes)(void *)GetProcAddress(sc, "shaderc_result_get_bytes") : NULL;
        PFN_sc_bytes errmsg = sc ? (PFN_sc_bytes)(void *)GetProcAddress(sc, "shaderc_result_get_error_message") : NULL;
        PFN_sc_release res_release = sc ? (PFN_sc_release)(void *)GetProcAddress(sc, "shaderc_result_release") : NULL;
        char *src = clarity_source(si);
        if (!init || !comp_release || !opt_init || !opt_release || !set_env || !compile || !status || !length || !bytes ||
            !errmsg || !res_release) {
            LOG("libshaderc_shared.dll not found next to the game: Clarity is off\n");
        } else if (src) {
            void *c = init(), *o = opt_init();
            set_env(o, 0 /* vulkan */, (1u << 22) | (3u << 12) /* 1.3 */);
            void *r = compile(c, src, strlen(src), 1 /* fragment */, "clarity.frag", "main", o);
            if (status(r) == 0) {
                size_t len = length(r);
                g_spv[v] = malloc(len);
                if (g_spv[v]) {
                    memcpy(g_spv[v], bytes(r), len);
                    g_spv_words[v] = len / 4;
                    g_failed[v] = 0;
                    LOG("Clarity compiled (%zu bytes of SPIR-V)\n", len);
                }
            } else {
                LOG("Clarity failed to compile (check the values in clearskies.ini):\n%s\n", errmsg(r));
            }
            res_release(r);
            opt_release(o);
            comp_release(c);
        }
        free(src);
    }
    const uint32_t *out = g_spv[v];
    *nwords = g_spv_words[v];
    ReleaseSRWLockExclusive(&g_compile_lock);
    return out;
}

// ------------------------------------------------------------------ dispatch

static void *key_of(const void *h) { return *(void *const *)h; }

typedef struct { void *key; VkInstance inst; PFN_vkGetInstanceProcAddr gipa; PFN_vkDestroyInstance destroy; } InstEntry;
typedef struct { void *key; PFN_vkGetDeviceProcAddr gdpa; PFN_vkCreateShaderModule create_shader; PFN_vkDestroyDevice destroy; } DevEntry;

#define MAX_ENTRIES 16
static SRWLOCK g_map_lock = SRWLOCK_INIT;
static InstEntry g_inst[MAX_ENTRIES];
static DevEntry g_dev[MAX_ENTRIES];

static InstEntry *inst_find(void *key) {
    for (int i = 0; i < MAX_ENTRIES; i++)
        if (g_inst[i].key == key) return &g_inst[i];
    return NULL;
}
static DevEntry *dev_find(void *key) {
    for (int i = 0; i < MAX_ENTRIES; i++)
        if (g_dev[i].key == key) return &g_dev[i];
    return NULL;
}

static VkResult VKAPI_CALL layer_CreateInstance(const VkInstanceCreateInfo *ci, const VkAllocationCallbacks *a, VkInstance *out) {
    const VkBaseInStructure *p = ((const VkBaseInStructure *)ci)->pNext;
    VkLayerInstanceCreateInfo *link = NULL;
    for (; p; p = p->pNext)
        if (p->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && ((VkLayerInstanceCreateInfo *)p)->function == VK_LAYER_LINK_INFO) {
            link = (VkLayerInstanceCreateInfo *)p;
            break;
        }
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr next = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)(void *)next(NULL, "vkCreateInstance");
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    VkResult r = create(ci, a, out);
    if (r != VK_SUCCESS) return r;

    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    BOOL pending;
    if (InitOnceBeginInitialize(&once, 0, &pending, NULL) && pending) {
        char exe[MAX_PATH] = "?";
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        LOG("Clear Skies loaded into %s\n", exe);
        load_settings();
        InitOnceComplete(&once, 0, NULL);
    }
    AcquireSRWLockExclusive(&g_map_lock);
    InstEntry *e = inst_find(NULL);
    if (e) *e = (InstEntry){key_of(*out), *out, next, (PFN_vkDestroyInstance)(void *)next(*out, "vkDestroyInstance")};
    ReleaseSRWLockExclusive(&g_map_lock);
    if (!e) LOG("too many Vulkan instances: this one is not watched\n");
    return VK_SUCCESS;
}

static void VKAPI_CALL layer_DestroyInstance(VkInstance inst, const VkAllocationCallbacks *a) {
    if (!inst) return;
    AcquireSRWLockExclusive(&g_map_lock);
    InstEntry *e = inst_find(key_of(inst)), copy = e ? *e : (InstEntry){0};
    if (e) memset(e, 0, sizeof *e);
    ReleaseSRWLockExclusive(&g_map_lock);
    if (copy.destroy) copy.destroy(inst, a);
}

static VkResult VKAPI_CALL layer_CreateDevice(VkPhysicalDevice phys, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *a, VkDevice *out) {
    const VkBaseInStructure *p = ((const VkBaseInStructure *)ci)->pNext;
    VkLayerDeviceCreateInfo *link = NULL;
    for (; p; p = p->pNext)
        if (p->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && ((VkLayerDeviceCreateInfo *)p)->function == VK_LAYER_LINK_INFO) {
            link = (VkLayerDeviceCreateInfo *)p;
            break;
        }
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr next_gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr next_gdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;

    AcquireSRWLockShared(&g_map_lock);
    InstEntry *ie = inst_find(key_of(phys));
    VkInstance inst = ie ? ie->inst : NULL;
    if (!inst)  // (not found by key: the only / latest instance)
        for (int i = 0; i < MAX_ENTRIES; i++)
            if (g_inst[i].inst) inst = g_inst[i].inst;
    ReleaseSRWLockShared(&g_map_lock);

    PFN_vkCreateDevice create = (PFN_vkCreateDevice)(void *)next_gipa(inst, "vkCreateDevice");
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    VkResult r = create(phys, ci, a, out);
    if (r != VK_SUCCESS) return r;

    DevEntry d = {key_of(*out), next_gdpa, (PFN_vkCreateShaderModule)(void *)next_gdpa(*out, "vkCreateShaderModule"),
                  (PFN_vkDestroyDevice)(void *)next_gdpa(*out, "vkDestroyDevice")};
    AcquireSRWLockExclusive(&g_map_lock);
    DevEntry *e = dev_find(NULL);
    if (e) *e = d;
    ReleaseSRWLockExclusive(&g_map_lock);
    if (!e) LOG("too many Vulkan devices: this one is not watched\n");
    else LOG("watching shaders on a new Vulkan device\n");
    return VK_SUCCESS;
}

static void VKAPI_CALL layer_DestroyDevice(VkDevice dev, const VkAllocationCallbacks *a) {
    if (!dev) return;
    AcquireSRWLockExclusive(&g_map_lock);
    DevEntry *e = dev_find(key_of(dev)), copy = e ? *e : (DevEntry){0};
    if (e) memset(e, 0, sizeof *e);
    ReleaseSRWLockExclusive(&g_map_lock);
    if (copy.destroy) copy.destroy(dev, a);
}

static VkResult VKAPI_CALL layer_CreateShaderModule(VkDevice dev, const VkShaderModuleCreateInfo *ci, const VkAllocationCallbacks *a, VkShaderModule *out) {
    AcquireSRWLockShared(&g_map_lock);
    DevEntry *e = dev_find(key_of(dev));
    PFN_vkCreateShaderModule next = e ? e->create_shader : NULL;
    ReleaseSRWLockShared(&g_map_lock);
    if (!next) return VK_ERROR_INITIALIZATION_FAILED;
    if (g_preset < 0 || !ci || !ci->pCode) return next(dev, ci, a, out);

    ShaderInfo si;
    if (!scan_spirv(ci->pCode, ci->codeSize / 4, &si) || !is_tonemap(&si)) return next(dev, ci, a, out);
    size_t nwords = 0;
    const uint32_t *spv = clarity_spirv(&si, &nwords);
    if (!spv) return next(dev, ci, a, out);
    VkShaderModuleCreateInfo mine = *ci;
    mine.codeSize = nwords * 4;
    mine.pCode = spv;
    VkResult r = next(dev, &mine, a, out);
    static volatile LONG count;
    LONG c = InterlockedIncrement(&count);
    if (r != VK_SUCCESS) {
        LOG("the driver refused Clarity's shader (%d): using the game's own\n", r);
        return next(dev, ci, a, out);
    }
    if (c <= 8) LOG("Clarity replaces the tone-mapping composite (shader %ld)%s\n", (long)c, c == 8 ? "; not logging further ones" : "");
    return r;
}

// ------------------------------------------------------------------ entry points

static PFN_vkVoidFunction own_function(const char *name);

EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice dev, const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "vkGetDeviceProcAddr") || !strcmp(name, "vkCreateShaderModule") || !strcmp(name, "vkDestroyDevice"))
        return own_function(name);
    AcquireSRWLockShared(&g_map_lock);
    DevEntry *e = dev ? dev_find(key_of(dev)) : NULL;
    PFN_vkGetDeviceProcAddr next = e ? e->gdpa : NULL;
    ReleaseSRWLockShared(&g_map_lock);
    return next ? next(dev, name) : NULL;
}

EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance inst, const char *name) {
    if (!name) return NULL;
    PFN_vkVoidFunction own = own_function(name);
    if (own) return own;
    if (!inst) return NULL;
    AcquireSRWLockShared(&g_map_lock);
    InstEntry *e = inst_find(key_of(inst));
    PFN_vkGetInstanceProcAddr next = e ? e->gipa : NULL;
    ReleaseSRWLockShared(&g_map_lock);
    return next ? next(inst, name) : NULL;
}

static PFN_vkVoidFunction own_function(const char *name) {
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction)layer_CreateInstance;
    if (!strcmp(name, "vkDestroyInstance")) return (PFN_vkVoidFunction)layer_DestroyInstance;
    if (!strcmp(name, "vkCreateDevice")) return (PFN_vkVoidFunction)layer_CreateDevice;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)layer_DestroyDevice;
    if (!strcmp(name, "vkCreateShaderModule")) return (PFN_vkVoidFunction)layer_CreateShaderModule;
    return NULL;
}

EXPORT VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v) {
    if (!v || v->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) return VK_ERROR_INITIALIZATION_FAILED;
    if (v->loaderLayerInterfaceVersion > 2) v->loaderLayerInterfaceVersion = 2;
    v->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    v->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    v->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
