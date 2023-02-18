#pragma once

#include <array>
#include <vector>

#include "../gfx_adapter.h"

#include "gfx_vulkan_loader.h"

namespace as {

/**
 * \brief Device extensions
 *
 * Queries supported extensions and provides a convenience
 * method to generate a list of supported extensions.
 *
 * Contains an integer field for each known extension that 
 * stores the supported version of the extension, and is zero
 * if the extension is not supported by the device.
 */
class GfxVulkanDeviceExtensions {

public:

  GfxVulkanDeviceExtensions(
    const GfxVulkanProcs&               vk,
          VkPhysicalDevice              adapter);

  ~GfxVulkanDeviceExtensions();

  GfxVulkanDeviceExtensions             (const GfxVulkanDeviceExtensions&) = delete;
  GfxVulkanDeviceExtensions& operator = (const GfxVulkanDeviceExtensions&) = delete;

  uint32_t khrAccelerationStructure       = 0;
  uint32_t khrDeferredHostOperations      = 0;
  uint32_t khrPipelineLibrary             = 0;
  uint32_t khrRayQuery                    = 0;
  uint32_t khrRayTracingMaintenance1      = 0;
  uint32_t khrSwapchain                   = 0;
  uint32_t extConservativeRasterization   = 0;
  uint32_t extExtendedDynamicState2       = 0;
  uint32_t extExtendedDynamicState3       = 0;
  uint32_t extGraphicsPipelineLibrary     = 0;
  uint32_t extMeshShader                  = 0;
  uint32_t extRobustness2                 = 0;
  uint32_t extShaderStencilExport         = 0;

  /**
   * \brief Checks whether required extensions are supported
   * \returns \c true if all required extensions are supported
   */
  bool checkSupport() const;

  /**
   * \brief Queries extension list
   *
   * \param [out] extensionCount Filled with number of extensions
   * \param [out] extensionNames Filled with list of extension names
   */
  void getExtensionNames(
          uint32_t*                     extensionCount,
    const char* const**                 extensionNames) const;

private:

  struct Extension {
    uint32_t* field;
    const char* name;
    uint32_t minVersion;
    bool required;
  };

  std::vector<const char*> m_extensionList;

  const std::array<Extension, 13> s_extensions = {{
    { &khrAccelerationStructure,        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,     1, false },
    { &khrDeferredHostOperations,       VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,   1, false },
    { &khrPipelineLibrary,              VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,           1, false },
    { &khrRayQuery,                     VK_KHR_RAY_QUERY_EXTENSION_NAME,                  1, false },
    { &khrRayTracingMaintenance1,       VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME,  1, false },
    { &khrSwapchain,                    VK_KHR_SWAPCHAIN_EXTENSION_NAME,                  1, true  },
    { &extConservativeRasterization,    VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, 1, false },
    { &extExtendedDynamicState2,        VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,   1, false },
    { &extExtendedDynamicState3,        VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,   1, false },
    { &extGraphicsPipelineLibrary,      VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,  1, false },
    { &extMeshShader,                   VK_EXT_MESH_SHADER_EXTENSION_NAME,                1, false },
    { &extRobustness2,                  VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,               1, true  },
    { &extShaderStencilExport,          VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME,      1, false },
  }};

};


/**
 * \brief Device properties
 *
 * Queries Vulkan device properties depending
 * on extension support.
 */
class GfxVulkanDeviceProperties {

public:

  GfxVulkanDeviceProperties(
    const GfxVulkanProcs&               vk,
          VkPhysicalDevice              adapter,
    const GfxVulkanDeviceExtensions&    ext);

  GfxVulkanDeviceProperties             (const GfxVulkanDeviceProperties&) = delete;
  GfxVulkanDeviceProperties& operator = (const GfxVulkanDeviceProperties&) = delete;

  VkPhysicalDeviceProperties2                               core                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
  VkPhysicalDeviceVulkan11Properties                        vk11                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES };
  VkPhysicalDeviceVulkan12Properties                        vk12                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
  VkPhysicalDeviceVulkan13Properties                        vk13                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES };
  VkPhysicalDeviceAccelerationStructurePropertiesKHR        khrAccelerationStructure        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
  VkPhysicalDeviceConservativeRasterizationPropertiesEXT    extConservativeRasterization    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT };
  VkPhysicalDeviceExtendedDynamicState3PropertiesEXT        extExtendedDynamicState3        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT };
  VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT      extGraphicsPipelineLibrary      = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT };
  VkPhysicalDeviceMeshShaderPropertiesEXT                   extMeshShader                   = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT };
  VkPhysicalDeviceRobustness2PropertiesEXT                  extRobustness2                  = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT };

  VkPhysicalDeviceMemoryProperties2 memory    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };

};


/**
 * \brief Device features
 *
 * Queries Vulkan feature support
 * depending on available extensions.
 */
class GfxVulkanDeviceFeatures {

public:

  /**
   * \brief Populates feature structs for device creation
   *
   * Enables only known features that are supported, while
   * leaving features disabled that we don't need.
   */
  GfxVulkanDeviceFeatures(
    const GfxVulkanDeviceFeatures&      supported,
    const GfxVulkanDeviceExtensions&    ext);

  /**
   * \brief Queries feature support from device
   *
   * Queries Vulkan device features based on extension
   * support. This can be used to check whether a device
   * can be used for rendering.
   */
  GfxVulkanDeviceFeatures(
    const GfxVulkanProcs&               vk,
          VkPhysicalDevice              adapter,
    const GfxVulkanDeviceExtensions&    ext);

  GfxVulkanDeviceFeatures             (const GfxVulkanDeviceFeatures&) = delete;
  GfxVulkanDeviceFeatures& operator = (const GfxVulkanDeviceFeatures&) = delete;

  VkPhysicalDeviceFeatures2                               core                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
  VkPhysicalDeviceVulkan11Features                        vk11                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
  VkPhysicalDeviceVulkan12Features                        vk12                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
  VkPhysicalDeviceVulkan13Features                        vk13                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
  VkPhysicalDeviceAccelerationStructureFeaturesKHR        khrAccelerationStructure        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
  VkPhysicalDeviceRayQueryFeaturesKHR                     khrRayQuery                     = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
  VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR       khrRayTracingMaintenance1       = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR };
  VkPhysicalDeviceExtendedDynamicState2FeaturesEXT        extExtendedDynamicState2        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT };
  VkPhysicalDeviceExtendedDynamicState3FeaturesEXT        extExtendedDynamicState3        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT };
  VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT      extGraphicsPipelineLibrary      = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT };
  VkPhysicalDeviceMeshShaderFeaturesEXT                   extMeshShader                   = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
  VkPhysicalDeviceRobustness2FeaturesEXT                  extRobustness2                  = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT };

  /**
   * \brief Checks whether all required features are supported
   * \returns \c true if required features are supported.
   */
  bool checkSupport() const;

private:

  GfxVulkanDeviceFeatures(
    const GfxVulkanDeviceExtensions&    ext);

  struct Feature {
    VkBool32* feature;
    bool required;
  };

  std::array<Feature, 74> m_features = {{
    { &core.features.depthBiasClamp,                                        true  },
    { &core.features.depthBounds,                                           false },
    { &core.features.drawIndirectFirstInstance,                             true  },
    { &core.features.dualSrcBlend,                                          false },
    { &core.features.fragmentStoresAndAtomics,                              true  },
    { &core.features.fullDrawIndexUint32,                                   true  },
    { &core.features.geometryShader,                                        false },
    { &core.features.imageCubeArray,                                        true  },
    { &core.features.independentBlend,                                      true  },
    { &core.features.multiDrawIndirect,                                     true  },
    { &core.features.multiViewport,                                         true  },
    { &core.features.sampleRateShading,                                     true  },
    { &core.features.samplerAnisotropy,                                     true  },
    { &core.features.shaderClipDistance,                                    true  },
    { &core.features.shaderFloat64,                                         false },
    { &core.features.shaderInt16,                                           false },
    { &core.features.shaderInt64,                                           false },
    { &core.features.shaderSampledImageArrayDynamicIndexing,                true  },
    { &core.features.shaderStorageBufferArrayDynamicIndexing,               true  },
    { &core.features.shaderStorageImageArrayDynamicIndexing,                true  },
    { &core.features.tessellationShader,                                    false },
    { &core.features.textureCompressionBC,                                  true  },
    { &core.features.vertexPipelineStoresAndAtomics,                        false },

    { &vk11.shaderDrawParameters,                                           true  },
    { &vk11.storageBuffer16BitAccess,                                       false },

    { &vk12.bufferDeviceAddress,                                            true  },
    { &vk12.descriptorBindingPartiallyBound,                                true  },
    { &vk12.descriptorBindingSampledImageUpdateAfterBind,                   true  },
    { &vk12.descriptorBindingStorageBufferUpdateAfterBind,                  true  },
    { &vk12.descriptorBindingStorageImageUpdateAfterBind,                   true  },
    { &vk12.descriptorBindingStorageTexelBufferUpdateAfterBind,             true  },
    { &vk12.descriptorBindingUniformTexelBufferUpdateAfterBind,             true  },
    { &vk12.descriptorBindingUpdateUnusedWhilePending,                      true  },
    { &vk12.descriptorBindingVariableDescriptorCount,                       true  },
    { &vk12.descriptorIndexing,                                             true  },
    { &vk12.drawIndirectCount,                                              true  },
    { &vk12.runtimeDescriptorArray,                                         true  },
    { &vk12.samplerFilterMinmax,                                            false },
    { &vk12.samplerMirrorClampToEdge,                                       true  },
    { &vk12.scalarBlockLayout,                                              true  },
    { &vk12.separateDepthStencilLayouts,                                    true  },
    { &vk12.shaderBufferInt64Atomics,                                       false },
    { &vk12.shaderOutputLayer,                                              false },
    { &vk12.shaderOutputViewportIndex,                                      false },
    { &vk12.shaderSampledImageArrayNonUniformIndexing,                      true  },
    { &vk12.shaderStorageBufferArrayNonUniformIndexing,                     true  },
    { &vk12.shaderStorageImageArrayNonUniformIndexing,                      false },
    { &vk12.shaderStorageTexelBufferArrayDynamicIndexing,                   true  },
    { &vk12.shaderStorageTexelBufferArrayNonUniformIndexing,                false },
    { &vk12.shaderUniformTexelBufferArrayDynamicIndexing,                   true  },
    { &vk12.shaderUniformTexelBufferArrayNonUniformIndexing,                false },
    { &vk12.subgroupBroadcastDynamicId,                                     true  },
    { &vk12.timelineSemaphore,                                              true  },
    { &vk12.uniformBufferStandardLayout,                                    true  },
    { &vk12.vulkanMemoryModel,                                              true  },

    { &vk13.computeFullSubgroups,                                           true  },
    { &vk13.dynamicRendering,                                               true  },
    { &vk13.maintenance4,                                                   true  },
    { &vk13.shaderDemoteToHelperInvocation,                                 true  },
    { &vk13.shaderTerminateInvocation,                                      true  },
    { &vk13.subgroupSizeControl,                                            true  },
    { &vk13.synchronization2,                                               true  },
 
    { &khrAccelerationStructure.accelerationStructure,                      false },
    { &khrAccelerationStructure.descriptorBindingAccelerationStructureUpdateAfterBind, false },

    { &khrRayQuery.rayQuery,                                                false },

    { &khrRayTracingMaintenance1.rayTracingMaintenance1,                    false },

    { &extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable, false },
    { &extExtendedDynamicState2.extendedDynamicState2PatchControlPoints,    false },
    { &extExtendedDynamicState3.extendedDynamicState3RasterizationSamples,  false },
    { &extExtendedDynamicState3.extendedDynamicState3SampleMask,            false },

    { &extGraphicsPipelineLibrary.graphicsPipelineLibrary,                  false },

    { &extMeshShader.meshShader,                                            false },
    { &extMeshShader.taskShader,                                            false },

    { &extRobustness2.nullDescriptor,                                       true  },
  }};

};


/**
 * \brief Vulkan adapter
 *
 * Maps to a Vulkan physical device.
 */
class GfxVulkanAdapter : public GfxAdapterIface {

public:

  GfxVulkanAdapter(
    const GfxVulkanProcs&               vk,
          VkPhysicalDevice              handle);

  ~GfxVulkanAdapter();

  /**
   * \brief Queries Vulkan physical device handle
   * \returns Vulkan physical device handle
   */
  VkPhysicalDevice getHandle() const {
    return m_handle;
  }

  /**
   * \brief Queries adapter properties
   * \returns Adapter properties
   */
  GfxAdapterInfo getInfo() override;

  /**
   * \brief Checks whether the adapter is suitable
   *
   * Basically checks whether all required Vulkan
   * extensions and features are supported.
   * \returns \c true if the adapter can be used
   */
  bool isSuitable() const;

private:

  VkPhysicalDevice          m_handle;

  GfxVulkanDeviceExtensions m_extensions;
  GfxVulkanDeviceProperties m_properties;
  GfxVulkanDeviceFeatures   m_features;

};

}
