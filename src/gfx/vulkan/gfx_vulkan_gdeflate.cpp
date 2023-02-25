#include <cstdint>

#include <cs_gdeflate.h>

#include "../../util/util_log.h"

#include "gfx_vulkan_device.h"
#include "gfx_vulkan_gdeflate.h"

namespace as {

GfxVulkanGDeflatePipeline::GfxVulkanGDeflatePipeline(
        GfxVulkanDevice&              device)
: m_device(device) {
  auto& vk = m_device.vk();
  auto& properties = device.getVkProperties();
  auto& features = device.getVkFeatures();

  VkSubgroupFeatureFlags subgroupOps =
    VK_SUBGROUP_FEATURE_BASIC_BIT |
    VK_SUBGROUP_FEATURE_VOTE_BIT |
    VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
    VK_SUBGROUP_FEATURE_BALLOT_BIT |
    VK_SUBGROUP_FEATURE_SHUFFLE_BIT;

  if (!features.core.features.shaderInt64 || !features.vk12.storageBuffer8BitAccess
   || (properties.vk11.subgroupSupportedOperations & subgroupOps) != subgroupOps) {
    Log::warn("Vulkan: Disabling GDeflate support, required features not supported.");
    return;
  }

  // TODO make the shader work with smaller subgroups
  if (properties.vk13.minSubgroupSize > 32 || properties.vk13.maxSubgroupSize < 32
   || !(properties.vk13.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT)) {
    Log::warn("Vulkan: Disabling GDeflate support, cannot enforce subgroup size.");
    return;
  }

  VkPushConstantRange pushConstantRange = { };
  pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(GfxVulkanGDeflateArgs);

  VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushConstantRange;

  VkResult vr = vk.vkCreatePipelineLayout(vk.device, &layoutInfo, nullptr, &m_pipelineLayout);

  if (vr)
    throw VulkanError("Vulkan: Failed to create GDeflate pipeline layout", vr);

  VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.basePipelineIndex = -1;

  VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  moduleInfo.codeSize = sizeof(cs_gdeflate);
  moduleInfo.pCode = cs_gdeflate;

  VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroupSizeInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO };
  subgroupSizeInfo.requiredSubgroupSize = 32;

  VkPipelineShaderStageCreateInfo& stageInfo = pipelineInfo.stage;
  stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo.pNext = &subgroupSizeInfo;
  stageInfo.flags |= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
  stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageInfo.pName = "main";

  if (device.getVkFeatures().extGraphicsPipelineLibrary.graphicsPipelineLibrary) {
    moduleInfo.pNext = std::exchange(stageInfo.pNext, &moduleInfo);
  } else {
    vr = vk.vkCreateShaderModule(vk.device, &moduleInfo, nullptr, &stageInfo.module);

    if (vr) {
      vk.vkDestroyPipelineLayout(vk.device, m_pipelineLayout, nullptr);
      throw VulkanError("Vulkan: Failed to create GDeflate shader module", vr);
    }
  }

  vr = vk.vkCreateComputePipelines(vk.device, VK_NULL_HANDLE,
    1, &pipelineInfo, nullptr, &m_pipeline);

  if (stageInfo.module)
    vk.vkDestroyShaderModule(vk.device, stageInfo.module, nullptr);

  if (vr) {
    vk.vkDestroyPipelineLayout(vk.device, m_pipelineLayout, nullptr);
    throw VulkanError("Vulkan: Failed to create GDeflate pipeline", vr);
  }
}


GfxVulkanGDeflatePipeline::~GfxVulkanGDeflatePipeline() {
  auto& vk = m_device.vk();

  vk.vkDestroyPipeline(vk.device, m_pipeline, nullptr);
  vk.vkDestroyPipelineLayout(vk.device, m_pipelineLayout, nullptr);
}

}
