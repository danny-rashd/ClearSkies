// The few Vulkan and loader-layer declarations this layer needs, so it builds without the Vulkan SDK.
// Values and layouts are from the Vulkan headers (vulkan_core.h, vk_layer.h); they are part of the stable ABI.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define VKAPI_CALL __stdcall
#define VKAPI_PTR VKAPI_CALL

typedef int32_t VkResult;
#define VK_SUCCESS 0
#define VK_ERROR_INITIALIZATION_FAILED (-3)

typedef int32_t VkStructureType;
#define VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO 16
#define VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO 47
#define VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO 48

typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkDevice_T *VkDevice;
typedef uint64_t VkShaderModule;
typedef uint32_t VkFlags;

typedef struct VkBaseInStructure {
    VkStructureType sType;
    const struct VkBaseInStructure *pNext;
} VkBaseInStructure;

typedef struct VkAllocationCallbacks VkAllocationCallbacks;  // (only passed through)
typedef struct VkInstanceCreateInfo VkInstanceCreateInfo;    // (only its pNext chain is read)
typedef struct VkDeviceCreateInfo VkDeviceCreateInfo;

typedef struct VkShaderModuleCreateInfo {
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    size_t codeSize;
    const uint32_t *pCode;
} VkShaderModuleCreateInfo;

typedef void (VKAPI_PTR *PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_vkGetInstanceProcAddr)(VkInstance, const char *);
typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_vkGetDeviceProcAddr)(VkDevice, const char *);
typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_GetPhysicalDeviceProcAddr)(VkInstance, const char *);
typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
typedef void (VKAPI_PTR *PFN_vkDestroyInstance)(VkInstance, const VkAllocationCallbacks *);
typedef VkResult (VKAPI_PTR *PFN_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *);
typedef void (VKAPI_PTR *PFN_vkDestroyDevice)(VkDevice, const VkAllocationCallbacks *);
typedef VkResult (VKAPI_PTR *PFN_vkCreateShaderModule)(VkDevice, const VkShaderModuleCreateInfo *, const VkAllocationCallbacks *, VkShaderModule *);

// vk_layer.h
typedef enum VkLayerFunction_ { VK_LAYER_LINK_INFO = 0, VK_LOADER_DATA_CALLBACK = 1 } VkLayerFunction;

typedef struct VkLayerInstanceLink_ {
    struct VkLayerInstanceLink_ *pNext;
    PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
    PFN_GetPhysicalDeviceProcAddr pfnNextGetPhysicalDeviceProcAddr;
} VkLayerInstanceLink;

typedef struct VkLayerInstanceCreateInfo {
    VkStructureType sType;  // VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO
    const void *pNext;
    VkLayerFunction function;
    union { VkLayerInstanceLink *pLayerInfo; void *pfnSetInstanceLoaderData; } u;
} VkLayerInstanceCreateInfo;

typedef struct VkLayerDeviceLink_ {
    struct VkLayerDeviceLink_ *pNext;
    PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr;
} VkLayerDeviceLink;

typedef struct VkLayerDeviceCreateInfo {
    VkStructureType sType;  // VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO
    const void *pNext;
    VkLayerFunction function;
    union { VkLayerDeviceLink *pLayerInfo; void *pfnSetDeviceLoaderData; } u;
} VkLayerDeviceCreateInfo;

// Loader <-> layer interface negotiation (loader interface version 2)
typedef enum VkNegotiateLayerStructType { LAYER_NEGOTIATE_UNINTIALIZED = 0, LAYER_NEGOTIATE_INTERFACE_STRUCT = 1 } VkNegotiateLayerStructType;
typedef struct VkNegotiateLayerInterface {
    VkNegotiateLayerStructType sType;
    void *pNext;
    uint32_t loaderLayerInterfaceVersion;
    PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr;
    PFN_GetPhysicalDeviceProcAddr pfnGetPhysicalDeviceProcAddr;
} VkNegotiateLayerInterface;
