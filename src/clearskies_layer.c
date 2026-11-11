// Clear Skies: a Vulkan layer (VK_LAYER_CLEARSKIES_savage_wind).
//
// Savage Wind turns each of the game's shaders into GLSL, compiles it to SPIR-V with shaderc and hands it to
// vkCreateShaderModule. This layer sits between the game and the driver and watches vkCreateShaderModule. When the
// module is BOTW's HDR -> LDR composite (the fragment shader that adds bloom and tone maps with 1 - exp(-x),
// recognised by its constants: the 10000 clamp, the 0.2989 luma weight and -log2(e), and by its two textures at
// bindings 32 and 33), the layer compiles Clarity with the same shaderc the game ships (libshaderc_shared.dll on
// Windows, libshaderc_shared.so.1 inside the Linux AppImage) and passes that to the driver instead. Nothing on disk is
// changed: the game's own SPIR-V cache keeps the original.
//
// Settings: clearskies.ini next to this library (or the file named by CLEARSKIES_INI). Log: clearskies.log next to
// the ini. Builds as clearskies_layer.dll on Windows and libclearskies_layer.so on Linux.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // (dladdr, RTLD_DEFAULT)
#endif
#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clarity_shader.h"
#include "vk_min.h"

#define LAYER_NAME "VK_LAYER_CLEARSKIES_savage_wind"

// ------------------------------------------------------------------ platform

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#define PATH_LEN MAX_PATH
#define SEP '\\'
#define SHADERC_NAME "libshaderc_shared.dll"
typedef SRWLOCK RwLock;
#define RWLOCK_INIT SRWLOCK_INIT
#define lock_excl(l) AcquireSRWLockExclusive(l)
#define unlock_excl(l) ReleaseSRWLockExclusive(l)
#define lock_shared(l) AcquireSRWLockShared(l)
#define unlock_shared(l) ReleaseSRWLockShared(l)
typedef HMODULE Lib;
static void *lib_sym(Lib l, const char *name) { return (void *)GetProcAddress(l, name); }
#else
#define EXPORT __attribute__((visibility("default")))
#define PATH_LEN PATH_MAX
#define SEP '/'
#define SHADERC_NAME "libshaderc_shared.so.1"
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
typedef pthread_rwlock_t RwLock;
#define RWLOCK_INIT PTHREAD_RWLOCK_INITIALIZER
#define lock_excl(l) pthread_rwlock_wrlock(l)
#define unlock_excl(l) pthread_rwlock_unlock(l)
#define lock_shared(l) pthread_rwlock_rdlock(l)
#define unlock_shared(l) pthread_rwlock_unlock(l)
typedef void *Lib;
static void *lib_sym(Lib l, const char *name) { return dlsym(l, name); }
#endif

// The running program's full path ("?" if unknown).
static void exe_path(char *out, size_t n) {
    snprintf(out, n, "?");
#ifdef _WIN32
    DWORD k = GetModuleFileNameA(NULL, out, (DWORD)n);
    if (!k || k >= n) snprintf(out, n, "?");
#else
    ssize_t k = readlink("/proc/self/exe", out, n - 1);
    if (k > 0) out[k] = 0;
    else snprintf(out, n, "?");
#endif
}

// ------------------------------------------------------------------ log

static RwLock g_log_lock = RWLOCK_INIT;
static FILE *g_log;
static char g_dir[PATH_LEN];  // folder of this library, with a trailing separator

static void layer_dir(void) {
    if (g_dir[0]) return;
#ifdef _WIN32
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)(void *)&layer_dir, &self);
    DWORD n = GetModuleFileNameA(self, g_dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { strcpy(g_dir, ".\\"); return; }
#else
    Dl_info info;
    if (!dladdr((void *)&layer_dir, &info) || !info.dli_fname || !strchr(info.dli_fname, '/')) {
        strcpy(g_dir, "./");
        return;
    }
    if (!realpath(info.dli_fname, g_dir)) snprintf(g_dir, sizeof g_dir, "%s", info.dli_fname);
#endif
    char *s = strrchr(g_dir, SEP);
    if (s) s[1] = 0;
}

static void ini_path(char *out, size_t n) {
    const char *env = getenv("CLEARSKIES_INI");
    if (env && *env) { snprintf(out, n, "%s", env); return; }
    layer_dir();
    snprintf(out, n, "%sclearskies.ini", g_dir);
}

static void LOG(const char *fmt, ...) {
    lock_excl(&g_log_lock);
    if (!g_log) {
        char p[PATH_LEN + 64], ini[PATH_LEN + 32];
        ini_path(ini, sizeof ini);
        snprintf(p, sizeof p, "%s", ini);
        char *s = strrchr(p, SEP);
        snprintf(s ? s + 1 : p, sizeof p - (size_t)(s ? s + 1 - p : 0), "clearskies.log");
        g_log = fopen(p, "w");
    }
    if (g_log) {
#ifdef _WIN32
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
#else
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        fprintf(g_log, "[%02d:%02d:%02d] ", t.tm_hour, t.tm_min, t.tm_sec);
#endif
        va_list ap;
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
    unlock_excl(&g_log_lock);
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
    char path[PATH_LEN + 32];
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

#ifdef _WIN32
static Lib find_shaderc(void) {
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
#else
static Lib open_lib(const char *path, const char *how) {
    Lib m = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (m && !dlsym(m, "shaderc_compile_into_spv")) { dlclose(m); m = NULL; }
    if (m) LOG("using shaderc %s (%s)\n", how, path);
    return m;
}

// The AppImage's game links libshaderc_shared.so.1 (bundled in its usr/lib), so it is normally loaded already.
static Lib find_shaderc(void) {
    Lib m = dlopen(NULL, RTLD_NOW);  // (the game and everything it links)
    if (m && dlsym(m, "shaderc_compile_into_spv")) return m;
    char p[PATH_LEN + 64];
    const char *appdir = getenv("APPDIR");  // (set by the AppImage runtime)
    if (appdir && *appdir) {
        snprintf(p, sizeof p, "%s/usr/lib/libshaderc_shared.so.1", appdir);
        if ((m = open_lib(p, "from the AppImage"))) return m;
    }
    exe_path(p, PATH_LEN);  // (an extracted AppImage, or a build of Savage Wind: <dir>/bin/savagewind)
    char *s = strrchr(p, '/');
    if (s) {
        strcpy(s + 1, "../lib/libshaderc_shared.so.1");
        if ((m = open_lib(p, "next to the game"))) return m;
        strcpy(s + 1, "libshaderc_shared.so.1");
        if ((m = open_lib(p, "next to the game"))) return m;
    }
    static const char *const system_names[] = {"libshaderc_shared.so.1", "libshaderc_shared.so", "libshaderc.so.1",
                                               "libshaderc.so"};
    for (size_t i = 0; i < sizeof system_names / sizeof system_names[0]; i++)
        if ((m = open_lib(system_names[i], "from the system"))) return m;
    return NULL;
}
#endif

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

static RwLock g_compile_lock = RWLOCK_INIT;
static uint32_t *g_spv[3];  // by v0 interpolation: smooth, noperspective, flat
static size_t g_spv_words[3];
static int g_failed[3];

// Clarity's SPIR-V (compiled on first use, kept for the session), or NULL to leave the game's shader alone.
static const uint32_t *clarity_spirv(const ShaderInfo *si, size_t *nwords) {
    int v = si->v0_flat ? 2 : si->v0_noperspective ? 1 : 0;
    lock_excl(&g_compile_lock);
    if (!g_spv[v] && !g_failed[v]) {
        g_failed[v] = 1;
        Lib sc = find_shaderc();
        PFN_sc_init init = sc ? (PFN_sc_init)lib_sym(sc, "shaderc_compiler_initialize") : NULL;
        PFN_sc_release comp_release = sc ? (PFN_sc_release)lib_sym(sc, "shaderc_compiler_release") : NULL;
        PFN_sc_init opt_init = sc ? (PFN_sc_init)lib_sym(sc, "shaderc_compile_options_initialize") : NULL;
        PFN_sc_release opt_release = sc ? (PFN_sc_release)lib_sym(sc, "shaderc_compile_options_release") : NULL;
        PFN_sc_set_env set_env = sc ? (PFN_sc_set_env)lib_sym(sc, "shaderc_compile_options_set_target_env") : NULL;
        PFN_sc_compile compile = sc ? (PFN_sc_compile)lib_sym(sc, "shaderc_compile_into_spv") : NULL;
        PFN_sc_status status = sc ? (PFN_sc_status)lib_sym(sc, "shaderc_result_get_compilation_status") : NULL;
        PFN_sc_length length = sc ? (PFN_sc_length)lib_sym(sc, "shaderc_result_get_length") : NULL;
        PFN_sc_bytes bytes = sc ? (PFN_sc_bytes)lib_sym(sc, "shaderc_result_get_bytes") : NULL;
        PFN_sc_bytes errmsg = sc ? (PFN_sc_bytes)lib_sym(sc, "shaderc_result_get_error_message") : NULL;
        PFN_sc_release res_release = sc ? (PFN_sc_release)lib_sym(sc, "shaderc_result_release") : NULL;
        char *src = clarity_source(si);
        if (!init || !comp_release || !opt_init || !opt_release || !set_env || !compile || !status || !length || !bytes ||
            !errmsg || !res_release) {
            LOG(SHADERC_NAME " not found next to the game: Clarity is off\n");
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
    unlock_excl(&g_compile_lock);
    return out;
}

// ------------------------------------------------------------------ dispatch

static void *key_of(const void *h) { return *(void *const *)h; }

typedef struct { void *key; VkInstance inst; PFN_vkGetInstanceProcAddr gipa; PFN_vkDestroyInstance destroy; } InstEntry;
typedef struct { void *key; PFN_vkGetDeviceProcAddr gdpa; PFN_vkCreateShaderModule create_shader; PFN_vkDestroyDevice destroy; } DevEntry;

#define MAX_ENTRIES 16
static RwLock g_map_lock = RWLOCK_INIT;
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

static void first_instance(void) {
    char exe[PATH_LEN];
    exe_path(exe, sizeof exe);
    LOG("Clear Skies loaded into %s\n", exe);
    load_settings();
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

#ifdef _WIN32
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    BOOL pending;
    if (InitOnceBeginInitialize(&once, 0, &pending, NULL) && pending) {
        first_instance();
        InitOnceComplete(&once, 0, NULL);
    }
#else
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, first_instance);
#endif
    lock_excl(&g_map_lock);
    InstEntry *e = inst_find(NULL);
    if (e) *e = (InstEntry){key_of(*out), *out, next, (PFN_vkDestroyInstance)(void *)next(*out, "vkDestroyInstance")};
    unlock_excl(&g_map_lock);
    if (!e) LOG("too many Vulkan instances: this one is not watched\n");
    return VK_SUCCESS;
}

static void VKAPI_CALL layer_DestroyInstance(VkInstance inst, const VkAllocationCallbacks *a) {
    if (!inst) return;
    lock_excl(&g_map_lock);
    InstEntry *e = inst_find(key_of(inst)), copy = e ? *e : (InstEntry){0};
    if (e) memset(e, 0, sizeof *e);
    unlock_excl(&g_map_lock);
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

    lock_shared(&g_map_lock);
    InstEntry *ie = inst_find(key_of(phys));
    VkInstance inst = ie ? ie->inst : NULL;
    if (!inst)  // (not found by key: the only / latest instance)
        for (int i = 0; i < MAX_ENTRIES; i++)
            if (g_inst[i].inst) inst = g_inst[i].inst;
    unlock_shared(&g_map_lock);

    PFN_vkCreateDevice create = (PFN_vkCreateDevice)(void *)next_gipa(inst, "vkCreateDevice");
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    VkResult r = create(phys, ci, a, out);
    if (r != VK_SUCCESS) return r;

    DevEntry d = {key_of(*out), next_gdpa, (PFN_vkCreateShaderModule)(void *)next_gdpa(*out, "vkCreateShaderModule"),
                  (PFN_vkDestroyDevice)(void *)next_gdpa(*out, "vkDestroyDevice")};
    lock_excl(&g_map_lock);
    DevEntry *e = dev_find(NULL);
    if (e) *e = d;
    unlock_excl(&g_map_lock);
    if (!e) LOG("too many Vulkan devices: this one is not watched\n");
    else LOG("watching shaders on a new Vulkan device\n");
    return VK_SUCCESS;
}

static void VKAPI_CALL layer_DestroyDevice(VkDevice dev, const VkAllocationCallbacks *a) {
    if (!dev) return;
    lock_excl(&g_map_lock);
    DevEntry *e = dev_find(key_of(dev)), copy = e ? *e : (DevEntry){0};
    if (e) memset(e, 0, sizeof *e);
    unlock_excl(&g_map_lock);
    if (copy.destroy) copy.destroy(dev, a);
}

static VkResult VKAPI_CALL layer_CreateShaderModule(VkDevice dev, const VkShaderModuleCreateInfo *ci, const VkAllocationCallbacks *a, VkShaderModule *out) {
    lock_shared(&g_map_lock);
    DevEntry *e = dev_find(key_of(dev));
    PFN_vkCreateShaderModule next = e ? e->create_shader : NULL;
    unlock_shared(&g_map_lock);
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
#ifdef _WIN32
    static volatile LONG count;
    LONG c = InterlockedIncrement(&count);
#else
    static long count;
    long c = __atomic_add_fetch(&count, 1, __ATOMIC_SEQ_CST);
#endif
    if (r != VK_SUCCESS) {
        LOG("the driver refused Clarity's shader (%d): using the game's own\n", r);
        return next(dev, ci, a, out);
    }
    if (c <= 8) LOG("Clarity replaces the tone-mapping composite (shader %ld)%s\n", (long)c, c == 8 ? "; not logging further ones" : "");
    return r;
}

// ------------------------------------------------------------------ entry points
//
// The loader calls the exported functions; the layer hands out its own static copies, so that on Linux a call never
// resolves to the Vulkan loader's functions of the same name (already loaded into the game).

static PFN_vkVoidFunction own_function(const char *name);

static PFN_vkVoidFunction VKAPI_CALL layer_GetDeviceProcAddr(VkDevice dev, const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "vkGetDeviceProcAddr") || !strcmp(name, "vkCreateShaderModule") || !strcmp(name, "vkDestroyDevice"))
        return own_function(name);
    lock_shared(&g_map_lock);
    DevEntry *e = dev ? dev_find(key_of(dev)) : NULL;
    PFN_vkGetDeviceProcAddr next = e ? e->gdpa : NULL;
    unlock_shared(&g_map_lock);
    return next ? next(dev, name) : NULL;
}

static PFN_vkVoidFunction VKAPI_CALL layer_GetInstanceProcAddr(VkInstance inst, const char *name) {
    if (!name) return NULL;
    PFN_vkVoidFunction own = own_function(name);
    if (own) return own;
    if (!inst) return NULL;
    lock_shared(&g_map_lock);
    InstEntry *e = inst_find(key_of(inst));
    PFN_vkGetInstanceProcAddr next = e ? e->gipa : NULL;
    unlock_shared(&g_map_lock);
    return next ? next(inst, name) : NULL;
}

static PFN_vkVoidFunction own_function(const char *name) {
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)layer_GetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)layer_GetDeviceProcAddr;
    if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction)layer_CreateInstance;
    if (!strcmp(name, "vkDestroyInstance")) return (PFN_vkVoidFunction)layer_DestroyInstance;
    if (!strcmp(name, "vkCreateDevice")) return (PFN_vkVoidFunction)layer_CreateDevice;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)layer_DestroyDevice;
    if (!strcmp(name, "vkCreateShaderModule")) return (PFN_vkVoidFunction)layer_CreateShaderModule;
    return NULL;
}

EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice dev, const char *name) {
    return layer_GetDeviceProcAddr(dev, name);
}

EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance inst, const char *name) {
    return layer_GetInstanceProcAddr(inst, name);
}

EXPORT VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v) {
    if (!v || v->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) return VK_ERROR_INITIALIZATION_FAILED;
    if (v->loaderLayerInterfaceVersion > 2) v->loaderLayerInterfaceVersion = 2;
    v->pfnGetInstanceProcAddr = layer_GetInstanceProcAddr;
    v->pfnGetDeviceProcAddr = layer_GetDeviceProcAddr;
    v->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
