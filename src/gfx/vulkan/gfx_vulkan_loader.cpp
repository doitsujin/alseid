#include "gfx_vulkan_loader.h"

namespace as {

GfxVulkanProcs::GfxVulkanProcs() {

}


GfxVulkanProcs::GfxVulkanProcs(PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr)
: vkGetInstanceProcAddr(pfnVkGetInstanceProcAddr) {

}


GfxVulkanProcs::GfxVulkanProcs(const GfxVulkanProcs& loader, VkInstance instanceHandle)
: instance(instanceHandle)
, vkGetInstanceProcAddr(loader.vkGetInstanceProcAddr) {

}


GfxVulkanProcs::GfxVulkanProcs(const GfxVulkanProcs& loader, VkPhysicalDevice adapterHandle, VkDevice deviceHandle)
: instance(loader.instance), adapter(adapterHandle), device(deviceHandle)
, vkGetInstanceProcAddr(loader.vkGetInstanceProcAddr) {

}


GfxVulkanProcs::~GfxVulkanProcs() {

}


PFN_vkVoidFunction GfxVulkanProcs::getLoaderProc(const char* name) const {
  return vkGetInstanceProcAddr
    ? vkGetInstanceProcAddr(VK_NULL_HANDLE, name)
    : nullptr;
}


PFN_vkVoidFunction GfxVulkanProcs::getInstanceProc(const char* name) const {
  return vkGetInstanceProcAddr && instance
    ? vkGetInstanceProcAddr(instance, name)
    : nullptr;
}


PFN_vkVoidFunction GfxVulkanProcs::getDeviceProc(const char* name) const {
  return vkGetDeviceProcAddr && device
    ? vkGetDeviceProcAddr(device, name)
    : nullptr;
}

}
