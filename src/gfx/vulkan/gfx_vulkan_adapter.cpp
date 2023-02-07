#include "../../util/util_string.h"

#include "gfx_vulkan_adapter.h"

namespace as {

struct GfxVulkanPropertyChain {
  void* field;
  const uint32_t* extension;
};


GfxVulkanDeviceExtensions::GfxVulkanDeviceExtensions(
  const GfxVulkanProcs&               vk,
        VkPhysicalDevice              adapter) {
  uint32_t count = 0;
  VkResult vr;

  if ((vr = vk.vkEnumerateDeviceExtensionProperties(adapter,
      nullptr, &count, nullptr)))
    throw VulkanError("Failed to enumerate device extensions", vr);

  std::vector<VkExtensionProperties> extInfos(count);

  if ((vr = vk.vkEnumerateDeviceExtensionProperties(adapter,
      nullptr, &count, extInfos.data())))
    throw VulkanError("Failed to enumerate device extensions", vr);

  for (auto ext : s_extensions) {
    for (auto info : extInfos) {
      if (!std::strncmp(info.extensionName, ext.name, VK_MAX_EXTENSION_NAME_SIZE)) {
        *ext.field = info.specVersion;
        break;
      }
    }
  }

  // Initialize list of available extension names
  m_extensionList.reserve(s_extensions.size());

  for (auto ext : s_extensions) {
    if (*ext.field >= ext.minVersion)
      m_extensionList.push_back(ext.name);
  }
}


GfxVulkanDeviceExtensions::~GfxVulkanDeviceExtensions() {

}


bool GfxVulkanDeviceExtensions::checkSupport() const {
  for (auto ext : s_extensions) {
    if (ext.required && !(*ext.field))
      return false;
  }

  return true;
}


void GfxVulkanDeviceExtensions::getExtensionNames(
        uint32_t*                     extensionCount,
  const char* const**                 extensionNames) const {
  if (extensionCount)
    *extensionCount = uint32_t(m_extensionList.size());

  if (extensionNames)
    *extensionNames = m_extensionList.data();
}




GfxVulkanDeviceProperties::GfxVulkanDeviceProperties(
  const GfxVulkanProcs&               vk,
        VkPhysicalDevice              adapter,
  const GfxVulkanDeviceExtensions&    ext) {
  const std::array<GfxVulkanPropertyChain, 8> fields = {{
    { &vk11 },
    { &vk12 },
    { &vk13 },
    { &khrAccelerationStructure,        &ext.khrAccelerationStructure },
    { &extConservativeRasterization,    &ext.extConservativeRasterization },
    { &extExtendedDynamicState3,        &ext.extExtendedDynamicState3 },
    { &extGraphicsPipelineLibrary,      &ext.extGraphicsPipelineLibrary },
    { &extRobustness2,                  &ext.extRobustness2 },
  }};

  for (const auto& f : fields) {
    if (!f.extension || *(f.extension)) {
      auto ptr = reinterpret_cast<VkBaseOutStructure*>(f.field);
      ptr->pNext = reinterpret_cast<VkBaseOutStructure*>(core.pNext);
      core.pNext = ptr;
    }
  }

  vk.vkGetPhysicalDeviceProperties2(adapter, &core);
  vk.vkGetPhysicalDeviceMemoryProperties2(adapter, &memory);
}




GfxVulkanDeviceFeatures::GfxVulkanDeviceFeatures(
  const GfxVulkanDeviceExtensions&    ext) {
  const std::array<GfxVulkanPropertyChain, 10> fields = {{
    { &vk11 },
    { &vk12 },
    { &vk13 },
    { &khrAccelerationStructure,        &ext.khrAccelerationStructure },
    { &khrRayQuery,                     &ext.khrRayQuery },
    { &khrRayTracingMaintenance1,       &ext.khrRayTracingMaintenance1 },
    { &extExtendedDynamicState2,        &ext.extExtendedDynamicState2 },
    { &extExtendedDynamicState3,        &ext.extExtendedDynamicState3 },
    { &extGraphicsPipelineLibrary,      &ext.extGraphicsPipelineLibrary },
    { &extRobustness2,                  &ext.extRobustness2 },
  }};

  for (const auto& f : fields) {
    if (!f.extension || *(f.extension)) {
      auto ptr = reinterpret_cast<VkBaseOutStructure*>(f.field);
      ptr->pNext = reinterpret_cast<VkBaseOutStructure*>(core.pNext);
      core.pNext = ptr;
    }
  }
}


GfxVulkanDeviceFeatures::GfxVulkanDeviceFeatures(
  const GfxVulkanDeviceFeatures&      supported,
  const GfxVulkanDeviceExtensions&    ext)
: GfxVulkanDeviceFeatures(ext) {
  for (size_t i = 0; i < m_features.size(); i++)
    *m_features[i].feature = *supported.m_features[i].feature;
}


GfxVulkanDeviceFeatures::GfxVulkanDeviceFeatures(
  const GfxVulkanProcs&               vk,
        VkPhysicalDevice              adapter,
  const GfxVulkanDeviceExtensions&    ext)
: GfxVulkanDeviceFeatures(ext) {
  vk.vkGetPhysicalDeviceFeatures2(adapter, &core);
}


bool GfxVulkanDeviceFeatures::checkSupport() const {
  for (const auto& feature : m_features) {
    if (feature.required && !(*feature.feature))
      return false;
  }

  return true;
}




GfxVulkanAdapter::GfxVulkanAdapter(
  const GfxVulkanProcs&               vk,
        VkPhysicalDevice              handle)
: m_handle      (handle)
, m_extensions  (vk, handle)
, m_properties  (vk, handle, m_extensions)
, m_features    (vk, handle, m_extensions) {

}


GfxVulkanAdapter::~GfxVulkanAdapter() {

}


GfxAdapterInfo GfxVulkanAdapter::getInfo() {
  GfxAdapterInfo result = { };
  result.deviceName = m_properties.core.properties.deviceName;
  result.driverInfo = strcat(m_properties.vk12.driverName, " (", m_properties.vk12.driverInfo, ")");
  result.deviceId = m_properties.core.properties.deviceID;
  result.vendorId = m_properties.core.properties.vendorID;

  for (uint32_t i = 0; i < m_properties.memory.memoryProperties.memoryHeapCount; i++) {
    const auto& heap = m_properties.memory.memoryProperties.memoryHeaps[i];

    if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
      result.totalDeviceMemory = std::max<uint64_t>(result.totalDeviceMemory, heap.size);
    else
      result.totalSharedMemory += heap.size;
  }

  if (!result.totalSharedMemory)
    result.totalSharedMemory = result.totalDeviceMemory;

  return result;
}


bool GfxVulkanAdapter::isSuitable() const {
  return m_extensions.checkSupport()
      && m_features.checkSupport();
}

}
