#include <spirv_cross.hpp>

#include <utility>

#include "../../util/util_assert.h"
#include "../../util/util_bitstream.h"
#include "../../util/util_log.h"
#include "../../util/util_string.h"
#include "../../util/util_math.h"

#include "../gfx.h"
#include "../gfx_spirv.h"
#include "../gfx_spirv_opt.h"

#include "gfx_vulkan_device.h"
#include "gfx_vulkan_descriptor_handle.h"
#include "gfx_vulkan_pipeline.h"
#include "gfx_vulkan_utils.h"

namespace as {

static const std::array<VkSpecializationMapEntry, 5> g_specConstantMap = {{
  { uint32_t(GfxSpecConstantId::eMinSubgroupSize),          offsetof(GfxVulkanSpecConstantData, minSubgroupSize),         sizeof(uint32_t) },
  { uint32_t(GfxSpecConstantId::eMaxSubgroupSize),          offsetof(GfxVulkanSpecConstantData, maxSubgroupSize),         sizeof(uint32_t) },
  { uint32_t(GfxSpecConstantId::eTaskShaderWorkgroupSize),  offsetof(GfxVulkanSpecConstantData, taskShaderWorkgroupSize), sizeof(uint32_t) },
  { uint32_t(GfxSpecConstantId::eMeshShaderWorkgroupSize),  offsetof(GfxVulkanSpecConstantData, meshShaderWorkgroupSize), sizeof(uint32_t) },
  { uint32_t(GfxSpecConstantId::eMeshShaderFlags),          offsetof(GfxVulkanSpecConstantData, meshShaderFlags),         sizeof(uint32_t) },
}};


GfxVulkanDynamicStates getDynamicStateFlagsFromState(
  const VkPipelineDynamicStateCreateInfo& dyState) {
  GfxVulkanDynamicStates result = 0;

  for (uint32_t i = 0; i < dyState.dynamicStateCount; i++) {
    switch (dyState.pDynamicStates[i]) {
      case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:
      case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
        result |= GfxVulkanDynamicState::eViewports;
        break;

      case VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT:
        result |= GfxVulkanDynamicState::eTessellationState;
        break;

      case VK_DYNAMIC_STATE_CULL_MODE:
      case VK_DYNAMIC_STATE_FRONT_FACE:
      case VK_DYNAMIC_STATE_DEPTH_BIAS:
      case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:
        result |= GfxVulkanDynamicState::eRasterizerState;
        break;

      case VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT:
        result |= GfxVulkanDynamicState::eConservativeRaster;
        break;

      case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:
      case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:
      case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:
      case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:
      case VK_DYNAMIC_STATE_STENCIL_OP:
      case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
      case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
        result |= GfxVulkanDynamicState::eDepthStencilState;
        break;

      case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE:
        result |= GfxVulkanDynamicState::eDepthBoundsState;
        break;

      case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
        result |= GfxVulkanDynamicState::eDepthBounds;
        break;

      case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
        result |= GfxVulkanDynamicState::eStencilRef;
        break;

      case VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT:
      case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT:
        result |= GfxVulkanDynamicState::eMultisampleState;
        break;

      case VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT:
        result |= GfxVulkanDynamicState::eAlphaToCoverage;
        break;

      case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
        result |= GfxVulkanDynamicState::eBlendConstants;
        break;

      case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
        result |= GfxVulkanDynamicState::eShadingRate;
        break;

      default:
        Log::err("Unhandled dynamic state ", dyState.pDynamicStates[i]);
        break;
    }
  }

  return result;
}


uint32_t lookupSpecConstant(
        uint32_t                      specId,
  const GfxVulkanSpecConstantData&    specConstants) {
  uint32_t specData = 0;

  for (const auto& e : g_specConstantMap) {
    if (e.constantID == specId && e.size == sizeof(specData))
      std::memcpy(&specData, reinterpret_cast<const char*>(&specConstants) + e.offset, sizeof(specData));
  }

  return specData;
}


Extent3D getActualWorkgroupSize(
  const GfxShader&                    shader,
  const GfxVulkanSpecConstantData&    specConstants) {
  Extent3D result = shader->getWorkgroupSize();
  Extent3D specIds = shader->getWorkgroupSizeSpecIds();

  if (!result.at<0>())
    result.set<0>(lookupSpecConstant(specIds.at<0>(), specConstants));

  if (!result.at<1>())
    result.set<1>(lookupSpecConstant(specIds.at<1>(), specConstants));

  if (!result.at<2>())
    result.set<2>(lookupSpecConstant(specIds.at<2>(), specConstants));

  return result;
}


size_t GfxVulkanBindingInfo::hash() const {
  HashState hash;
  hash.add(uint32_t(type));
  hash.add(uint32_t(flags));
  hash.add(binding);
  hash.add(count);
  hash.add(stages);
  return hash;
}


bool GfxVulkanDescriptorLayoutKey::operator == (const GfxVulkanDescriptorLayoutKey& other) const {
  if (bindings.size() != other.bindings.size())
    return false;

  for (size_t i = 0; i < bindings.size(); i++) {
    if (bindings[i] != other.bindings[i])
      return false;
  }

  return true;
}


bool GfxVulkanDescriptorLayoutKey::operator != (const GfxVulkanDescriptorLayoutKey& other) const {
  return !this->operator == (other);
}


size_t GfxVulkanDescriptorLayoutKey::hash() const {
  HashState hash;

  for (const auto& b : bindings)
    hash.add(b.hash());

  return hash;
}




GfxVulkanDescriptorLayout::GfxVulkanDescriptorLayout(
  const GfxVulkanDevice&              device,
  const GfxVulkanDescriptorLayoutKey& key)
: m_device(device), m_isEmpty(key.bindings.empty()) {
  auto& vk = m_device.vk();

  std::vector<VkDescriptorUpdateTemplateEntry> entries(key.bindings.size());
  std::vector<VkDescriptorSetLayoutBinding> bindings(key.bindings.size());
  std::vector<VkDescriptorBindingFlags> flags(key.bindings.size());

  for (size_t i = 0; i < key.bindings.size(); i++) {
    const auto& binding = key.bindings[i];
    m_isBindless |= (binding.flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

    VkDescriptorSetLayoutBinding& info = bindings[i];
    info.binding = binding.binding;
    info.descriptorType = binding.type;
    info.descriptorCount = binding.count;
    info.stageFlags = binding.stages;
    info.pImmutableSamplers = nullptr;

    VkDescriptorUpdateTemplateEntry& entry = entries[i];
    entry.dstBinding = binding.binding;
    entry.dstArrayElement = 0;
    entry.descriptorCount = binding.count;
    entry.descriptorType = binding.type;
    entry.offset = sizeof(GfxVulkanDescriptor) * binding.binding;
    entry.stride = sizeof(GfxVulkanDescriptor);

    flags[i] = binding.flags;
  }

  VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
  flagInfo.bindingCount = flags.size();
  flagInfo.pBindingFlags = flags.data();

  VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  layoutInfo.bindingCount = bindings.size();
  layoutInfo.pBindings = bindings.data();

  if (m_isBindless) {
    layoutInfo.pNext = &flagInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  }

  VkResult vr = vk.vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &m_layout);

  if (vr)
    throw VulkanError("Vulkan: Failed to create descriptor set layout", vr);

  if (!m_isBindless && layoutInfo.bindingCount) {
    VkDescriptorUpdateTemplateCreateInfo templateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
    templateInfo.descriptorUpdateEntryCount = entries.size();
    templateInfo.pDescriptorUpdateEntries = entries.data();
    templateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    templateInfo.descriptorSetLayout = m_layout;

    VkResult vr = vk.vkCreateDescriptorUpdateTemplate(vk.device, &templateInfo, nullptr, &m_template);

    if (vr) {
      vk.vkDestroyDescriptorSetLayout(vk.device, m_layout, nullptr);
      throw VulkanError("Vulkan: Failed to create descriptor set layout", vr);
    }
  }
}


GfxVulkanDescriptorLayout::~GfxVulkanDescriptorLayout() {
  auto& vk = m_device.vk();

  vk.vkDestroyDescriptorSetLayout(vk.device, m_layout, nullptr);
  vk.vkDestroyDescriptorUpdateTemplate(vk.device, m_template, nullptr);
}





bool GfxVulkanPipelineLayoutKey::operator == (const GfxVulkanPipelineLayoutKey& other) const {
  if (constantStages != other.constantStages
   || constantBytes != other.constantBytes
   || descriptorSetCount != other.descriptorSetCount)
    return false;

  for (size_t i = 0; i < GfxMaxDescriptorSets; i++) {
    if (descriptorSets[i] != other.descriptorSets[i])
      return false;
  }

  return true;
}


bool GfxVulkanPipelineLayoutKey::operator != (const GfxVulkanPipelineLayoutKey& other) const {
  return !this->operator == (other);
}


size_t GfxVulkanPipelineLayoutKey::hash() const {
  HashState hash;
  hash.add(uint32_t(constantStages));
  hash.add(constantBytes);
  hash.add(descriptorSetCount);

  for (auto set : descriptorSets)
    hash.add(set);

  return hash;
}




GfxVulkanPipelineLayout::GfxVulkanPipelineLayout(
  const GfxVulkanDevice&              device,
  const GfxVulkanPipelineLayoutKey&   key)
: m_device(device), m_key(key) {
  auto& vk = m_device.vk();

  std::array<VkDescriptorSetLayout, GfxMaxDescriptorSets> descriptorSets = { };

  for (size_t i = 0; i < key.descriptorSetCount; i++) {
    descriptorSets[i] = key.descriptorSets[i]->getSetLayout();

    if (!key.descriptorSets[i]->isEmpty())
      m_nonemptySetMask |= 1u << i;
  }

  VkPushConstantRange constants = { };
  constants.stageFlags = key.constantStages;
  constants.offset = 0;
  constants.size = key.constantBytes;

  VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  layoutInfo.setLayoutCount = key.descriptorSetCount;
  layoutInfo.pSetLayouts = descriptorSets.data();

  if (key.constantBytes) {
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &constants;
  }

  VkResult vr = vk.vkCreatePipelineLayout(vk.device,
    &layoutInfo, nullptr, &m_layout);

  if (vr)
    throw VulkanError("Vulkan: Failed to create pipeline layout", vr);
}


GfxVulkanPipelineLayout::~GfxVulkanPipelineLayout() {
  auto& vk = m_device.vk();

  vk.vkDestroyPipelineLayout(vk.device, m_layout, nullptr);
}




GfxVulkanRenderTargetState::GfxVulkanRenderTargetState(
        GfxVulkanPipelineManager&     mgr,
  const GfxRenderTargetStateDesc&     desc)
: GfxRenderTargetStateIface(desc) {
  // Set up color formats for all defined attachments
  for (uint32_t i = 0; i < GfxMaxColorAttachments; i++) {
    if ((m_rtFormats[i] = mgr.device().getVkFormat(desc.colorFormats[i])))
      m_rtState.colorAttachmentCount = i + 1;
  }

  if (m_rtState.colorAttachmentCount)
    m_rtState.pColorAttachmentFormats = m_rtFormats.data();

  // Set up depth-stencil format for relevant aspects
  VkFormat depthStencilFormat = mgr.device().getVkFormat(desc.depthStencilFormat);

  if (depthStencilFormat) {
    auto& formatInfo = Gfx::getFormatInfo(desc.depthStencilFormat);

    if (formatInfo.aspects & GfxImageAspect::eDepth)
      m_rtState.depthAttachmentFormat = depthStencilFormat;

    if (formatInfo.aspects & GfxImageAspect::eStencil)
      m_rtState.stencilAttachmentFormat = depthStencilFormat;
  }
}


GfxVulkanRenderTargetState::~GfxVulkanRenderTargetState() {

}




GfxVulkanRenderState::GfxVulkanRenderState(
        GfxVulkanPipelineManager&     mgr,
  const GfxRenderStateDesc&           desc)
: GfxRenderStateIface(desc) {
  if (desc.flags & GfxRenderStateFlag::ePrimitiveTopology)
    setupPrimitiveTopology(mgr, desc.primitiveTopology);

  if (desc.flags & GfxRenderStateFlag::eVertexLayout)
    setupVertexLayout(mgr, desc.vertexLayout);

  if (desc.flags & (
      GfxRenderStateFlag::eCullMode |
      GfxRenderStateFlag::eFrontFace |
      GfxRenderStateFlag::eConservativeRaster))
    setupRasterizer(mgr, desc);

  if (desc.flags & GfxRenderStateFlag::eDepthBias)
    setupDepthBias(mgr, desc.depthBias);

  if (desc.flags & GfxRenderStateFlag::eShadingRate)
    setupShadingRate(mgr, desc.shadingRate);

  if (desc.flags & GfxRenderStateFlag::eDepthTest)
    setupDepthTest(mgr, desc.depthTest);

  if (desc.flags & GfxRenderStateFlag::eStencilTest)
    setupStencilTest(mgr, desc.depthTest, desc.stencilTest);

  if (desc.flags & GfxRenderStateFlag::eMultisampling)
    setupMultisampling(mgr, desc.multisampling);

  if (desc.flags & GfxRenderStateFlag::eBlending)
    setupBlending(mgr, desc.blending);

  // Do this at the very end when all potential
  // dynamic state has been registered
  if (m_dyState.dynamicStateCount)
    m_dyState.pDynamicStates = m_dyList.data();
}


GfxVulkanRenderState::~GfxVulkanRenderState() {

}


void GfxVulkanRenderState::setupPrimitiveTopology(
        GfxVulkanPipelineManager&     mgr,
  const GfxPrimitiveTopology&         desc) {
  m_iaState.topology = getVkPrimitiveTopology(desc.primitiveType);
  m_iaState.primitiveRestartEnable = desc.isPrimitiveRestartEnabled();

  m_tsState.patchControlPoints = desc.patchVertexCount;
}


void GfxVulkanRenderState::setupVertexLayout(
        GfxVulkanPipelineManager&     mgr,
  const GfxVertexLayout&              desc) {
  // Set up all the state objects
  for (uint32_t i = 0; i < GfxMaxVertexAttributes; i++) {
    auto& info = desc.attributes[i];

    if (info.format == GfxFormat::eUnknown)
      continue;

    auto& att = m_viAttributes[m_viState.vertexAttributeDescriptionCount++];
    att.location = i;
    att.format = mgr.device().getVkFormat(info.format);
    att.offset = info.offset;
    att.binding = info.binding;

    if (!(m_vertexBindingMask & (1u << info.binding))) {
      m_vertexBindingMask |= (1u << info.binding);

      auto& bind = m_viBindings[m_viState.vertexBindingDescriptionCount++];
      bind.binding = info.binding;
      bind.stride = info.stride;
      bind.inputRate = getVkInputRate(info.inputRate);
    }
  }

  if (m_viState.vertexAttributeDescriptionCount) {
    m_viState.pVertexAttributeDescriptions = m_viAttributes.data();
    m_viState.pVertexBindingDescriptions = m_viBindings.data();
  }
}


void GfxVulkanRenderState::setupRasterizer(
        GfxVulkanPipelineManager&     mgr,
  const GfxRenderStateDesc&           desc) {
  m_rsState.depthClampEnable = VK_FALSE;
  m_rsState.rasterizerDiscardEnable = VK_FALSE;
  m_rsState.polygonMode = VK_POLYGON_MODE_FILL;

  if (desc.flags & GfxRenderStateFlag::eCullMode)
    m_rsState.cullMode = getVkCullMode(desc.cullMode);

  if (desc.flags & GfxRenderStateFlag::eFrontFace)
    m_rsState.frontFace = getVkFrontFace(desc.frontFace);

  if (desc.conservativeRaster) {
    m_rsConservative.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
    m_rsState.pNext = &m_rsConservative;
  }
}


void GfxVulkanRenderState::setupDepthBias(
        GfxVulkanPipelineManager&     mgr,
  const GfxDepthBias&                 desc) {
  m_rsState.depthBiasEnable = desc.isDepthBiasEnabled();
  m_rsState.depthBiasConstantFactor = desc.depthBias;
  m_rsState.depthBiasSlopeFactor = desc.depthBiasSlope;
  m_rsState.depthBiasClamp = desc.depthBiasClamp;
}


void GfxVulkanRenderState::setupShadingRate(
        GfxVulkanPipelineManager&     mgr,
  const GfxShadingRate&               desc) {
  m_srState.fragmentSize = getVkExtent2D(desc.shadingRate);
  m_srState.combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
  m_srState.combinerOps[1] = getVkShadingRateCombiner(desc.shadingRateOp);
}


void GfxVulkanRenderState::setupDepthTest(
        GfxVulkanPipelineManager&     mgr,
  const GfxDepthTest&                 desc) {
  m_dsState.depthTestEnable = desc.isDepthTestEnabled();
  m_dsState.depthWriteEnable = desc.enableDepthWrite;
  m_dsState.depthCompareOp = getVkCompareOp(desc.depthCompareOp);
  m_dsState.depthBoundsTestEnable = desc.enableDepthBoundsTest;

  if (m_dsState.depthBoundsTestEnable)
    m_dyList.at(m_dyState.dynamicStateCount++) = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
}


void GfxVulkanRenderState::setupStencilTest(
        GfxVulkanPipelineManager&     mgr,
  const GfxDepthTest&                 depth,
  const GfxStencilTest&               desc) {
  m_dsState.stencilTestEnable = desc.isStencilTestEnabled(depth);
  m_dsState.front = getVkStencilState(desc.front);
  m_dsState.back = getVkStencilState(desc.back);

  if (m_dsState.stencilTestEnable)
    m_dyList.at(m_dyState.dynamicStateCount++) = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
}


void GfxVulkanRenderState::setupMultisampling(
        GfxVulkanPipelineManager&     mgr,
  const GfxMultisampling&             desc) {
  m_msMask = desc.sampleMask;

  m_msState.rasterizationSamples = VkSampleCountFlagBits(desc.sampleCount);
  m_msState.pSampleMask = &m_msMask;
  m_msState.alphaToCoverageEnable = desc.enableAlphaToCoverage;
}


void GfxVulkanRenderState::setupBlending(
        GfxVulkanPipelineManager&     mgr,
  const GfxBlending&                  desc) {
  bool usesBlendConstants = false;

  m_cbState.logicOpEnable = desc.isLogicOpEnabled();
  m_cbState.logicOp = getVkLogicOp(desc.logicOp);

  for (uint32_t i = 0; i < GfxMaxColorAttachments; i++) {
    VkPipelineColorBlendAttachmentState& attachment = m_cbAttachments[i];
    attachment.colorWriteMask = getVkComponentFlags(desc.renderTargets[i].writeMask);

    if (desc.renderTargets[i].isBlendingEnabled()) {
      usesBlendConstants |= desc.renderTargets[i].usesBlendConstants();

      attachment.blendEnable = VK_TRUE;
      attachment.srcColorBlendFactor = getVkBlendFactor(desc.renderTargets[i].srcColor);
      attachment.dstColorBlendFactor = getVkBlendFactor(desc.renderTargets[i].dstColor);
      attachment.colorBlendOp = getVkBlendOp(desc.renderTargets[i].colorOp);
      attachment.srcAlphaBlendFactor = getVkBlendFactor(desc.renderTargets[i].srcAlpha);
      attachment.dstAlphaBlendFactor = getVkBlendFactor(desc.renderTargets[i].dstAlpha);
      attachment.alphaBlendOp = getVkBlendOp(desc.renderTargets[i].alphaOp);
    }
  }

  if (usesBlendConstants)
    m_dyList.at(m_dyState.dynamicStateCount++) = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
}


VkStencilOpState GfxVulkanRenderState::getVkStencilState(
  const GfxStencilDesc&               desc) {
  VkStencilOpState state = { };
  state.failOp = getVkStencilOp(desc.failOp);
  state.passOp = getVkStencilOp(desc.passOp);
  state.depthFailOp = getVkStencilOp(desc.depthFailOp);
  state.compareOp = getVkCompareOp(desc.compareOp);
  state.compareMask = desc.compareMask;
  state.writeMask = desc.writeMask;
  return state;
}




GfxVulkanVertexInputPipeline::GfxVulkanVertexInputPipeline(
        GfxVulkanPipelineManager&     mgr,
  const GfxVulkanVertexInputKey&      key)
: m_mgr(mgr) {
  auto& vk = m_mgr.device().vk();

  auto iaState = key.renderState->getIaState();
  auto viState = key.renderState->getViState();

  VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT };
  libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

  VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
  info.flags                = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
  info.pVertexInputState    = &viState;
  info.pInputAssemblyState  = &iaState;
  info.basePipelineIndex    = -1;

  VkResult vr = vk.vkCreateGraphicsPipelines(vk.device,
    VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline);

  if (vr)
    throw VulkanError("Vulkan: Failed to create vertex input pipeline library", vr);
}


GfxVulkanVertexInputPipeline::~GfxVulkanVertexInputPipeline() {
  auto& vk = m_mgr.device().vk();

  vk.vkDestroyPipeline(vk.device, m_pipeline, nullptr);
}




GfxVulkanFragmentOutputPipeline::GfxVulkanFragmentOutputPipeline(
        GfxVulkanPipelineManager&     mgr,
  const GfxVulkanFragmentOutputKey&   key)
: m_mgr(mgr) {
  auto& vk = m_mgr.device().vk();
  auto& features = m_mgr.device().getVkFeatures();

  std::array<VkPipelineColorBlendAttachmentState, GfxMaxColorAttachments> cbAttachments;

  auto rtState = key.targetState->getRtState();
  auto msState = key.renderState->getMsState(*key.targetState, key.sampleShading);
  auto cbState = key.renderState->getCbState(cbAttachments.data(), rtState.colorAttachmentCount, key.shaderIoMasks.outputMask);
  auto dyState = key.renderState->getDyState();

  small_vector<VkDynamicState, 8> dyStates;

  for (size_t i = 0; i < dyState.dynamicStateCount; i++) {
    if (dyState.pDynamicStates[i] == VK_DYNAMIC_STATE_BLEND_CONSTANTS)
      dyStates.push_back(dyState.pDynamicStates[i]);
  }

  if (key.sampleShading) {
    if (features.extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
     && features.extExtendedDynamicState3.extendedDynamicState3SampleMask) {
      dyStates.push_back(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
      dyStates.push_back(VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);
    }

    if (features.extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable)
      dyStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
  }

  dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dyState.dynamicStateCount = dyStates.size();

  if (dyState.dynamicStateCount)
    dyState.pDynamicStates = dyStates.data();

  VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &rtState };
  libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

  VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
  info.flags                = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
  info.pMultisampleState    = &msState;
  info.pColorBlendState     = &cbState;
  info.pDynamicState        = &dyState;
  info.basePipelineIndex    = -1;

  VkResult vr = vk.vkCreateGraphicsPipelines(vk.device,
    VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline);

  if (vr)
    throw VulkanError("Vulkan: Failed to create vertex input pipeline library", vr);

  m_dynamic = getDynamicStateFlagsFromState(dyState);
}


GfxVulkanFragmentOutputPipeline::~GfxVulkanFragmentOutputPipeline() {
  auto& vk = m_mgr.device().vk();

  vk.vkDestroyPipeline(vk.device, m_pipeline, nullptr);
}




GfxVulkanGraphicsShaders::GfxVulkanGraphicsShaders(
  const GfxGraphicsPipelineDesc&      desc) {
  addShader(desc.vertex);
  addShader(desc.tessControl);
  addShader(desc.tessEval);
  addShader(desc.geometry);
  addShader(desc.fragment);
}


GfxVulkanGraphicsShaders::GfxVulkanGraphicsShaders(
  const GfxMeshPipelineDesc&          desc) {
  addShader(desc.task);
  addShader(desc.mesh);
  addShader(desc.fragment);
}


void GfxVulkanGraphicsShaders::getShaderStageInfo(
        GfxVulkanGraphicsShaderStages& result,
        GfxVulkanPipelineManager&     mgr,
  const GfxVulkanShaderStagePatchInfo& patchInfo,
  const GfxVulkanSpecConstantData*    specData) const {
  mgr.initSpecializationInfo(specData, result.specInfo);

  result.extraInfo.resize(m_shaders.size());
  result.stageInfo.resize(m_shaders.size());

  for (uint32_t i = 0; i < m_shaders.size(); i++) {
    mgr.initShaderStage(m_shaders[i], &result.specInfo,
      result.stageInfo[i], result.extraInfo[i], patchInfo, specData);
  }
}


void GfxVulkanGraphicsShaders::addShader(
  const GfxShader&                    shader) {
  if (shader)
    m_shaders.push_back(shader);
}




GfxVulkanGraphicsPipelineKey::GfxVulkanGraphicsPipelineKey(
  const GfxGraphicsPipelineDesc&      desc) {
  uint32_t count = 0;

  hashes[count++] = desc.vertex->getShaderBinary().hash;

  if (desc.tessControl && desc.tessEval) {
    hashes[count++] = desc.tessControl->getShaderBinary().hash;
    hashes[count++] = desc.tessEval->getShaderBinary().hash;
  }

  if (desc.geometry)
    hashes[count++] = desc.geometry->getShaderBinary().hash;

  if (desc.fragment)
    hashes[count++] = desc.fragment->getShaderBinary().hash;
}


GfxVulkanGraphicsPipelineKey::GfxVulkanGraphicsPipelineKey(
  const GfxMeshPipelineDesc&          desc) {
  uint32_t count = 0;

  if (desc.task)
    hashes[count++] = desc.task->getShaderBinary().hash;

  hashes[count++] = desc.mesh->getShaderBinary().hash;

  if (desc.fragment)
    hashes[count++] = desc.fragment->getShaderBinary().hash;
}




GfxVulkanGraphicsPipeline::GfxVulkanGraphicsPipeline(
        GfxVulkanPipelineManager&     mgr,
  const GfxVulkanPipelineLayout&      layout,
  const GfxGraphicsPipelineDesc&      desc)
: GfxGraphicsPipelineIface(desc)
, m_mgr               (mgr)
, m_layout            (layout)
, m_shaders           (desc)
, m_specConstants     (mgr.getDefaultSpecConstants())
, m_sampleRateShading (hasSampleRateShading(desc.fragment))
, m_canLink           (canFastLink())
, m_isAvailable       (!m_canLink) {
  if (desc.vertex)
    m_shaderIoMask.inputMask = desc.vertex->getIoMask().outputMask;
  if (desc.fragment)
    m_shaderIoMask.outputMask = desc.fragment->getIoMask().outputMask;
}


GfxVulkanGraphicsPipeline::GfxVulkanGraphicsPipeline(
        GfxVulkanPipelineManager&     mgr,
  const GfxVulkanPipelineLayout&      layout,
  const GfxMeshPipelineDesc&          desc)
: GfxGraphicsPipelineIface(desc)
, m_mgr               (mgr)
, m_layout            (layout)
, m_shaders           (desc)
, m_specConstants     (mgr.getDefaultSpecConstants())
, m_sampleRateShading (hasSampleRateShading(desc.fragment))
, m_canLink           (canFastLink())
, m_isAvailable       (!m_canLink) {
  if (desc.fragment)
    m_shaderIoMask.outputMask = desc.fragment->getIoMask().outputMask;

  // If the mesh shader emits significantly fewer vertices and primitives than
  // it has invocations, lower the workgroup size. Also check for the special
  // value of (1,1), in which case we patch the shader at runtime to emit one
  // vertex and primitive per thread based on the preferred workgroup size.
  GfxShaderMeshOutputInfo meshOutputs = desc.mesh->getMeshOutputInfo();

  uint32_t maxOutputCount = std::max(
    meshOutputs.maxVertexCount,
    meshOutputs.maxPrimitiveCount);

  if (meshOutputs.maxVertexCount == 1u && meshOutputs.maxPrimitiveCount == 1u) {
    m_patchInfo.maxVertexCount = m_specConstants.meshShaderWorkgroupSize;
    m_patchInfo.maxPrimitiveCount = m_specConstants.meshShaderWorkgroupSize;
  } else {
    const auto& properties = m_mgr.device().getVkProperties();

    // Ignore size preferences if the implementation wants local export, and
    // ensure that the workgroup size remains a multiple of the subgroup size.
    if (properties.extMeshShader.prefersLocalInvocationPrimitiveOutput ||
        properties.extMeshShader.prefersLocalInvocationVertexOutput ||
        maxOutputCount < m_specConstants.meshShaderWorkgroupSize) {
      m_specConstants.meshShaderWorkgroupSize = std::min(
        align(maxOutputCount, properties.vk13.maxSubgroupSize),
        properties.extMeshShader.maxMeshWorkGroupSize[0]);

      if (maxOutputCount < properties.vk13.minSubgroupSize)
        m_specConstants.meshShaderWorkgroupSize = properties.vk13.maxSubgroupSize;
    }
  }

  // Compute actual workgroup size based on specialization constants
  m_workgroupSize = getActualWorkgroupSize(
    desc.task ? desc.task : desc.mesh, m_specConstants);
}


GfxVulkanGraphicsPipeline::~GfxVulkanGraphicsPipeline() {
  auto& vk = m_mgr.device().vk();

  for (auto& v : m_optimizedVariants)
    vk.vkDestroyPipeline(vk.device, v.pipeline, nullptr);

  for (auto& v : m_linkedVariants)
    vk.vkDestroyPipeline(vk.device, v.variant.pipeline, nullptr);

  vk.vkDestroyPipeline(vk.device, m_library.pipeline, nullptr);
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::getVariant(
  const GfxVulkanGraphicsPipelineVariantKey& key) {
  LookupResult result = lookupOptimized(key);

  if (likely(result.variant.pipeline))
    return result.variant;

  if (likely(m_canLink)) {
    result.variant = lookupLinked(key);

    if (likely(result.variant.pipeline))
      return result.variant;

    if (canLinkVariant(key)) {
      if (!result.found)
        deferCreateVariant(key);

      result.variant = linkVariant(key);
      return result.variant;
    }
  }

  result.variant = createVariant(key);
  return result.variant;
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::createVariant(
  const GfxVulkanGraphicsPipelineVariantKey& key) {
  std::lock_guard lock(m_optimizedMutex);
  auto iter = m_optimizedVariants.end();

  // Find existing entry for the variant, or create a new one if
  // necessary. If the variant has already been compiled, exit.
  for (auto i = m_optimizedVariants.begin(); i != m_optimizedVariants.end(); i++) {
    if (i->key == key) {
      auto variant = i->getVariant();

      if (variant.pipeline)
        return variant;

      iter = i;
      break;
    }
  }

  if (iter == m_optimizedVariants.end())
    iter = m_optimizedVariants.emplace(key, GfxVulkanGraphicsPipelineVariant());

  GfxVulkanGraphicsPipelineVariant variant = createVariantLocked(key);
  iter->setVariant(variant);
  return variant;
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::createLibrary() {
  std::lock_guard lock(m_linkedMutex);
  return createLibraryLocked();
}


Extent3D GfxVulkanGraphicsPipeline::getWorkgroupSize() const {
  return m_workgroupSize;
}


bool GfxVulkanGraphicsPipeline::isAvailable() const {
  return m_isAvailable.load();
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::lookupLinked(
  const GfxVulkanGraphicsPipelineVariantKey& key) const {
  for (auto& v : m_linkedVariants) {
    if (v.key == key)
      return v.variant;
  }

  return GfxVulkanGraphicsPipelineVariant();
}


GfxVulkanGraphicsPipeline::LookupResult GfxVulkanGraphicsPipeline::lookupOptimized(
  const GfxVulkanGraphicsPipelineVariantKey& key) const {
  for (auto& v : m_optimizedVariants) {
    if (v.key == key)
      return { v.getVariant(), VK_TRUE };
  }

  return LookupResult();
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::createLibraryLocked() {
  if (likely(m_library.pipeline))
    return m_library;

  auto& vk = m_mgr.device().vk();
  auto& extensions = m_mgr.device().getVkExtensions();
  auto& features = m_mgr.device().getVkFeatures();

  // Set up shader stages. Since this path will only ever be hit
  // if graphics pipeline libraries are supported, we don't need
  // to worry about destroying shader modules later.
  GfxVulkanGraphicsShaderStages shaderStages;
  m_shaders.getShaderStageInfo(shaderStages, m_mgr, m_patchInfo, &m_specConstants);

  // All depth-stencil and rasteriaztion state is dynamic. Additionally,
  // multisample state is dynamic if sample rate shading is used.
  small_vector<VkDynamicState, 32> dyStates = {
    VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    VK_DYNAMIC_STATE_CULL_MODE,
    VK_DYNAMIC_STATE_FRONT_FACE,
    VK_DYNAMIC_STATE_DEPTH_BIAS,
    VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
    VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
    VK_DYNAMIC_STATE_STENCIL_OP,
    VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
    VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    VK_DYNAMIC_STATE_STENCIL_REFERENCE
  };

  // Set up viewport state. All of this is dynamic.
  VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

  // Set up rasterization state. Most of this is dynamic.
  VkPipelineRasterizationConservativeStateCreateInfoEXT rsConservative = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT };
  rsConservative.extraPrimitiveOverestimationSize = 0.0f;

  VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rsState.depthClampEnable = VK_FALSE;
  rsState.rasterizerDiscardEnable = VK_FALSE;
  rsState.polygonMode = VK_POLYGON_MODE_FILL;
  rsState.lineWidth = 1.0f;

  if (extensions.extConservativeRasterization && features.extExtendedDynamicState3.extendedDynamicState3ConservativeRasterizationMode) {
    rsConservative.pNext = std::exchange(rsState.pNext, &rsConservative);
    dyStates.push_back(VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT);
  }

  // Set up tessellation state. This is dynamic for tessellation
  // pipeline libraries if the device supports it.
  VkPipelineTessellationStateCreateInfo tsState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };

  if (m_stages & GfxShaderStage::eTessControl)
    dyStates.push_back(VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT);

  // Set up depth-stencil state. All of this is dynamic.
  VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

  if (features.core.features.depthBounds) {
    dyStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
    dyStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
  }

  // Set up multisample state. This is dynamic if the device
  // supports it, otherwise we should assume sane defaults.
  VkSampleMask msMask = ~0u;

  VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  msState.pSampleMask = &msMask;
  msState.alphaToCoverageEnable = VK_FALSE;
  msState.alphaToOneEnable = VK_FALSE;

  if (m_sampleRateShading) {
    if (features.extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
     && features.extExtendedDynamicState3.extendedDynamicState3SampleMask) {
      dyStates.push_back(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
      dyStates.push_back(VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);
    }

    if (features.extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable)
      dyStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
  }

  // Set up rendering info. Only the view mask is used.
  VkPipelineRenderingCreateInfo rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

  // Set up shading rate state
  VkPipelineCreateFlags flags = 0;

  if (features.khrFragmentShadingRate.attachmentFragmentShadingRate) {
    flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    if (supportsFragmentShadingRate())
      dyStates.push_back(VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);
  }

  // Set up dynamic state
  VkPipelineDynamicStateCreateInfo dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dyState.dynamicStateCount = dyStates.size();
  dyState.pDynamicStates = dyStates.data();

  // Create actual graphics pipeline libary
  VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &rtState };
  libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT
                            | VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

  VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
  info.flags                = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | flags;
  info.layout               = m_layout.getLayout();
  info.stageCount           = shaderStages.stageInfo.size();
  info.pStages              = shaderStages.stageInfo.data();
  info.pViewportState       = &vpState;
  info.pRasterizationState  = &rsState;
  info.pDepthStencilState   = &dsState;
  info.pDynamicState        = &dyState;
  info.basePipelineIndex    = -1;

  if (m_stages & GfxShaderStage::eTessControl)
    info.pTessellationState = &tsState;

  if (m_sampleRateShading)
    info.pMultisampleState = &msState;

  VkResult vr = vk.vkCreateGraphicsPipelines(vk.device,
    VK_NULL_HANDLE, 1, &info, nullptr, &m_library.pipeline);

  if (vr)
    throw VulkanError("Vulkan: Failed to create shader library", vr);

  m_library.dynamicStates = getDynamicStateFlagsFromState(dyState);
  m_mgr.device().setDebugName(m_library.pipeline, std::string(m_debugName + " [library]").c_str());

  m_isAvailable.store(true, std::memory_order_release);
  return m_library;
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::createVariantLocked(
  const GfxVulkanGraphicsPipelineVariantKey& key) const {
  auto& vk = m_mgr.device().vk();
  auto& features = m_mgr.device().getVkFeatures();

  // Set up shader stages.
  GfxVulkanGraphicsShaderStages shaderStages;
  m_shaders.getShaderStageInfo(shaderStages, m_mgr, m_patchInfo, &m_specConstants);

  // Set up state objects. We typically don't have
  // a large number of dynamic states here.
  small_vector<VkDynamicState, 8> dyStates;
  dyStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
  dyStates.push_back(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);

  std::array<VkPipelineColorBlendAttachmentState, GfxMaxColorAttachments> cbAttachments;

  VkPipelineRenderingCreateInfo rtState = key.targetState->getRtState();
  VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  VkPipelineTessellationStateCreateInfo tsState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
  VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  VkPipelineRasterizationStateCreateInfo rsState = key.renderState->getRsState();
  VkPipelineFragmentShadingRateStateCreateInfoKHR srState = key.renderState->getSrState();
  VkPipelineDepthStencilStateCreateInfo dsState = key.renderState->getDsState();
  VkPipelineMultisampleStateCreateInfo msState = key.renderState->getMsState(*key.targetState, m_sampleRateShading);
  VkPipelineColorBlendStateCreateInfo cbState = key.renderState->getCbState(cbAttachments.data(), rtState.colorAttachmentCount, m_shaderIoMask.outputMask);
  VkPipelineDynamicStateCreateInfo dyState = key.renderState->getDyState();

  if (m_stages & GfxShaderStage::eVertex) {
    viState = key.renderState->getViState();
    iaState = key.renderState->getIaState();
    tsState = key.renderState->getTsState();
  }

  for (size_t i = 0; i < dyState.dynamicStateCount; i++)
    dyStates.push_back(dyState.pDynamicStates[i]);

  // Set up shading rate state
  VkPipelineCreateFlags flags = 0;
  bool usesShadingRate = false;

  if (features.khrFragmentShadingRate.attachmentFragmentShadingRate) {
    flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    usesShadingRate = srState.fragmentSize.width != 1 || srState.fragmentSize.height != 1
      || srState.combinerOps[0] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR
      || srState.combinerOps[1] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;

    if (usesShadingRate) {
      usesShadingRate = supportsFragmentShadingRate()
        && m_mgr.device().supportsFragmentShadingRateWithState(*key.renderState);
    }
  }

  // Set up dynamic state info
  dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dyState.dynamicStateCount = dyStates.size();

  if (dyState.dynamicStateCount)
    dyState.pDynamicStates = dyStates.data();

  // Set up pipeline create info
  VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtState };
  info.flags = flags;
  info.layout = m_layout.getLayout();
  info.stageCount = shaderStages.stageInfo.size();
  info.pStages = shaderStages.stageInfo.data();

  if (m_stages & GfxShaderStage::eVertex) {
    info.pVertexInputState = &viState;
    info.pInputAssemblyState = &iaState;
  }

  if (m_stages & GfxShaderStage::eTessControl)
    info.pTessellationState = &tsState;

  info.pViewportState = &vpState;
  info.pRasterizationState = &rsState;
  info.pDepthStencilState = &dsState;
  info.pMultisampleState = &msState;
  info.pColorBlendState = &cbState;
  info.pDynamicState = &dyState;

  if (usesShadingRate)
    srState.pNext = std::exchange(info.pNext, &srState);

  // Create actual Vulkan pipeline
  GfxVulkanGraphicsPipelineVariant variant;
  variant.dynamicStates = getDynamicStateFlagsFromState(dyState);

  VkResult vr = vk.vkCreateGraphicsPipelines(vk.device,
    VK_NULL_HANDLE, 1, &info, nullptr, &variant.pipeline);

  for (size_t i = 0; i < shaderStages.stageInfo.size(); i++) {
    if (shaderStages.stageInfo[i].module)
      vk.vkDestroyShaderModule(vk.device, shaderStages.stageInfo[i].module, nullptr);
  }

  if (vr)
    throw VulkanError("Vulkan: Failed to create graphics pipeline", vr);

  m_mgr.device().setDebugName(variant.pipeline, std::string(m_debugName + " [variant]").c_str());
  return variant;
}


GfxVulkanGraphicsPipelineVariant GfxVulkanGraphicsPipeline::linkVariant(
  const GfxVulkanGraphicsPipelineVariantKey& key) {
  auto& vk = m_mgr.device().vk();
  auto& features = m_mgr.device().getVkFeatures();

  std::lock_guard lock(m_linkedMutex);
  GfxVulkanGraphicsPipelineVariant variant = lookupLinked(key);

  if (variant.pipeline)
    return variant;

  // Always include the base shader library
  GfxVulkanGraphicsPipelineVariant library = createLibraryLocked();

  small_vector<VkPipeline, 3> libraries;
  libraries.push_back(library.pipeline);
  variant.dynamicStates = library.dynamicStates;

  // Look up vertex input state library
  if (m_stages & GfxShaderStage::eVertex) {
    auto& viLibrary = m_mgr.createVertexInputPipeline(*key.renderState);
    libraries.push_back(viLibrary.getHandle());
  }

  // Look up fragment output state library
  auto& foLibrary = m_mgr.createFragmentOutputPipeline(
    *key.targetState, *key.renderState, m_sampleRateShading, m_shaderIoMask);

  libraries.push_back(foLibrary.getHandle());
  variant.dynamicStates |= foLibrary.getDynamicStateFlags();

  // Create actual Vulkan pipeline, but without link-time
  // optimization in order to avoid stutter.
  VkPipelineLibraryCreateInfoKHR libInfo = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR };
  libInfo.libraryCount = libraries.size();
  libInfo.pLibraries = libraries.data();

  VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
  info.layout = m_layout.getLayout();
  info.basePipelineIndex = -1;

  // Shouldn't be necessary according to spec, but
  // validation complains if we don't set this here.
  if (features.khrFragmentShadingRate.attachmentFragmentShadingRate)
    info.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

  VkResult vr = vk.vkCreateGraphicsPipelines(vk.device,
    VK_NULL_HANDLE, 1, &info, nullptr, &variant.pipeline);

  if (vr)
    throw VulkanError("Vulkan: Failed to link graphics pipeline", vr);

  m_linkedVariants.emplace(key, variant);

  m_mgr.device().setDebugName(variant.pipeline, std::string(m_debugName + " [linked]").c_str());
  return variant;
}


bool GfxVulkanGraphicsPipeline::canLinkVariant(
  const GfxVulkanGraphicsPipelineVariantKey& key) const {
  auto& features = m_mgr.device().getVkFeatures();

  // If sample shading is enabled, dynamic multisample state will be
  // required if multisample state does not match the assumed defaults
  if (m_sampleRateShading) {
    auto msState = key.renderState->getMsState(*key.targetState, m_sampleRateShading);

    if (!features.extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
     || !features.extExtendedDynamicState3.extendedDynamicState3SampleMask) {
      VkSampleCountFlagBits sampleCount = key.targetState->getSampleCount();

      if (!sampleCount)
        sampleCount = msState.rasterizationSamples;

      if (sampleCount != VK_SAMPLE_COUNT_1_BIT)
        return false;

      uint32_t allSampleMask = (1u << sampleCount) - 1;

      if ((key.renderState->getSampleMask() & allSampleMask) != allSampleMask)
        return false;
    }

    if (!features.extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable) {
      if (msState.alphaToCoverageEnable)
        return false;
    }
  }

  return true;
}


void GfxVulkanGraphicsPipeline::deferCreateVariant(
  const GfxVulkanGraphicsPipelineVariantKey& key) {
  // We need to lock here since we want to prevent other
  // threads from adding an entry for the same pipeline
  std::unique_lock lock(m_optimizedMutex);

  // Check whether the scenario described above happened
  LookupResult result = lookupOptimized(key);

  if (result.found)
    return;

  // If not, add an entry to the list with a null pipeline.
  // This prevents subsequent lookups from trying to add
  // the same pipeline again.
  m_optimizedVariants.emplace(key, GfxVulkanGraphicsPipelineVariant());
  lock.unlock();

  // Enqueue job to create the optimized pipeline
  m_mgr.deferCreateGraphicsPipelineVariant(*this, key);
}


bool GfxVulkanGraphicsPipeline::canFastLink() const {
  auto& features = m_mgr.device().getVkFeatures();

  if (!features.extGraphicsPipelineLibrary.graphicsPipelineLibrary)
    return false;

  // Fast-linking tessellation pipelines requires support
  // for dynamic patch control point count
  if ((m_stages & GfxShaderStage::eTessControl)
   && !features.extExtendedDynamicState2.extendedDynamicState2PatchControlPoints)
    return false;

  return true;
}


bool GfxVulkanGraphicsPipeline::supportsFragmentShadingRate() const {
  if (m_sampleRateShading)
    return false;

  return true;
}


bool GfxVulkanGraphicsPipeline::hasSampleRateShading(
  const GfxShader&                    fragmentShader) {
  return fragmentShader && (fragmentShader->getFlags() & GfxShaderFlag::eSampleRate);
}




GfxVulkanComputePipeline::GfxVulkanComputePipeline(
        GfxVulkanPipelineManager&     mgr,
  const GfxVulkanPipelineLayout&      layout,
  const GfxComputePipelineDesc&       desc)
: GfxComputePipelineIface(desc)
, m_mgr           (mgr)
, m_layout        (layout)
, m_desc          (desc)
, m_specConstants (mgr.getDefaultSpecConstants()) {
  // Compute actual workgroup size based on specialization constants
  m_workgroupSize = getActualWorkgroupSize(m_desc.compute, m_specConstants);
}


GfxVulkanComputePipeline::~GfxVulkanComputePipeline() {
  auto& vk = m_mgr.device().vk();

  vk.vkDestroyPipeline(vk.device, m_pipeline.load(), nullptr);
}


Extent3D GfxVulkanComputePipeline::getWorkgroupSize() const {
  return m_workgroupSize;
}


bool GfxVulkanComputePipeline::isAvailable() const {
  return m_pipeline.load() != VK_NULL_HANDLE;
}


VkPipeline GfxVulkanComputePipeline::createPipeline() {
  std::lock_guard lock(m_mutex);
  VkPipeline pipeline = m_pipeline.load();

  if (!pipeline)
    pipeline = createPipelineLocked();

  return pipeline;
}


VkPipeline GfxVulkanComputePipeline::createPipelineLocked() {
  auto& vk = m_mgr.device().vk();

  // Set up specialization constants
  VkSpecializationInfo specInfo = { };

  m_mgr.initSpecializationInfo(&m_specConstants, specInfo);

  // Set up basic compute pipeline info
  GfxVulkanShaderStageExtraInfo extraInfo = { };

  VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
  pipelineInfo.layout = m_layout.getLayout();
  pipelineInfo.basePipelineIndex = -1;

  m_mgr.initShaderStage(m_desc.compute, &specInfo,
    pipelineInfo.stage, extraInfo, m_patchInfo, &m_specConstants);

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult vr = vk.vkCreateComputePipelines(vk.device,
    VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

  // Destroy the shader module if we had to create one
  if (pipelineInfo.stage.module)
    vk.vkDestroyShaderModule(vk.device, pipelineInfo.stage.module, nullptr);

  if (vr)
    throw VulkanError(strcat("Vulkan: Failed to create compute pipeline (shader: ", m_desc.compute->getDebugName(), ")").c_str(), vr);

  m_mgr.device().setDebugName(pipeline, m_debugName.c_str());
  m_pipeline.store(pipeline);

  // Reset the contained pipeline description so that
  // we don't unnecessarily hold the shader object
  m_desc = GfxComputePipelineDesc();
  return pipeline;
}





GfxVulkanPipelineManager::GfxVulkanPipelineManager(
        GfxVulkanDevice&              device)
: m_device(device) {
  uint32_t threadCount = std::thread::hardware_concurrency();
  m_compilerThreads.reserve(threadCount);

  for (uint32_t i = 0; i < threadCount; i++)
    m_compilerThreads.emplace_back([this] { runWorker(); });
}


GfxVulkanPipelineManager::~GfxVulkanPipelineManager() {
  std::unique_lock lock(m_compilerMutex);
  m_compilerStopped = true;
  m_compilerCond.notify_all();
  lock.unlock();

  for (auto& thread : m_compilerThreads)
    thread.join();
}


GfxVulkanSpecConstantData GfxVulkanPipelineManager::getDefaultSpecConstants() const {
  const auto& properties = m_device.getVkProperties();

  GfxMeshShaderFlags meshShaderFlags = 0;

  if (properties.extMeshShader.prefersLocalInvocationVertexOutput)
    meshShaderFlags |= GfxMeshShaderFlag::ePreferLocalVertexOutput;

  if (properties.extMeshShader.prefersLocalInvocationPrimitiveOutput)
    meshShaderFlags |= GfxMeshShaderFlag::ePreferLocalPrimitiveOutput;

  if (properties.extMeshShader.prefersCompactVertexOutput)
    meshShaderFlags |= GfxMeshShaderFlag::ePreferCompactVertexOutput;

  if (properties.extMeshShader.prefersCompactPrimitiveOutput)
    meshShaderFlags |= GfxMeshShaderFlag::ePreferCompactPrimitiveOutput;

  GfxVulkanSpecConstantData result = { };
  result.minSubgroupSize = properties.vk13.minSubgroupSize;
  result.maxSubgroupSize = properties.vk13.maxSubgroupSize;

  // Limit task shader workgroup sizes for subgroup optimizations,
  // and to provide a general upper bound that is easy to work with.
  result.taskShaderWorkgroupSize = clamp(std::min(
    properties.extMeshShader.maxPreferredTaskWorkGroupInvocations,
    properties.vk13.maxSubgroupSize), 16u, 64u);

  // For mesh shaders, clamp the workgroup size into a wide range.
  // On AMD, testing shows higher perf when running one subgroup.
  result.meshShaderWorkgroupSize = clamp(
    properties.extMeshShader.maxPreferredMeshWorkGroupInvocations,
    32u, 256u);

  if (properties.core.properties.vendorID == GfxAdapterVendorId::eAmd &&
      properties.vk13.maxSubgroupSize > 32u)
    result.meshShaderWorkgroupSize = properties.vk13.maxSubgroupSize;

  result.meshShaderFlags = uint32_t(meshShaderFlags);
  return result;
}


void GfxVulkanPipelineManager::initShaderStage(
  const GfxShader&                    shader,
  const VkSpecializationInfo*         specInfo,
        VkPipelineShaderStageCreateInfo& stageInfo,
        GfxVulkanShaderStageExtraInfo& extraInfo,
  const GfxVulkanShaderStagePatchInfo& patchInfo,
  const GfxVulkanSpecConstantData*    specData) const {
  auto& vk = m_device.vk();

  GfxShaderStage stage = shader->getShaderStage();
  GfxShaderBinary binary = shader->getShaderBinary();

  SpirvCodeBuffer codeBuffer;

  switch (binary.format) {
    case GfxShaderFormat::eVulkanSpirv: {
      codeBuffer = SpirvCodeBuffer(binary.size,
        reinterpret_cast<const uint32_t*>(binary.data));
    } break;

    case GfxShaderFormat::eVulkanSpirvCompressed: {
      RdMemoryView compressed(binary.data, binary.size);
      size_t codeSize = spirvGetDecodedSize(compressed);

      std::vector<uint32_t> code(codeSize / sizeof(uint32_t));
      spirvDecodeBinary(WrMemoryView(code.data(), codeSize), compressed);

      codeBuffer = SpirvCodeBuffer(std::move(code));
    } break;

    default:
      throw VulkanError("Vulkan: Unsupported shader binary format", VK_ERROR_UNKNOWN);
  }

  stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  stageInfo.stage = getVkShaderStage(stage);
  stageInfo.pName = "main";
  stageInfo.pSpecializationInfo = specInfo;

  // Initialize optimizer so we can pull information from it
  SpirvOptimizer optimizer(std::move(codeBuffer));

  // Ensure that gl_SubgroupSize and friends actually behave as expected in
  // compute and mesh shaders, and ensure full subgroups whenever possible.
  const auto& deviceProperties = m_device.getVkProperties();

  uint32_t minSubgroupSize = deviceProperties.vk13.minSubgroupSize;
  uint32_t maxSubgroupSize = deviceProperties.vk13.maxSubgroupSize;

  if (gfxShaderStageHasWorkgroupSize(stage)) {
    stageInfo.flags |= VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT;

    Extent3D localSize = specData
      ? getActualWorkgroupSize(shader, *specData)
      : shader->getWorkgroupSize();

    if (!(localSize.at<0>() & (deviceProperties.vk13.maxSubgroupSize - 1u)))
      stageInfo.flags |= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;

    if (deviceProperties.vk13.requiredSubgroupSizeStages & stageInfo.stage) {
      uint32_t requiredSubgroupSize = 0u;

      // If the workgroup size is less than the maximum subgroup size, we should
      // ensure that the driver does not give us larger subgroups in order to
      // benefit from subgroup-optimized code paths.
      if ((localSize.at<0>() < deviceProperties.vk13.maxSubgroupSize) &&
          (localSize.at<0>() >= deviceProperties.vk13.minSubgroupSize) &&
          (!(localSize.at<0>() & (localSize.at<0>() - 1u))) &&
          (localSize.at<1>() == 1u && localSize.at<2>() == 1u))
        requiredSubgroupSize = localSize.at<0>();

      // If we're on RADV and the shader uses a lot of shuffles, and the workgroup
      // size is larger than 64 so we'd be running multiple subgroups anyway, it may
      // be useful to force the shader into wave32 mode. AMDVLK has its own heuristics.
      if ((deviceProperties.vk12.driverID == VK_DRIVER_ID_MESA_RADV) &&
          (deviceProperties.vk13.minSubgroupSize == 32u) &&
          (localSize.at<0>() > deviceProperties.vk13.maxSubgroupSize) &&
          (localSize.at<0>() & (deviceProperties.vk13.minSubgroupSize - 1u)) == 0u &&
          (optimizer.prefersWave32Amd()))
        requiredSubgroupSize = deviceProperties.vk13.minSubgroupSize;

      // Apply structure only if we actually require a specific subgroup size
      if (requiredSubgroupSize) {
        stageInfo.flags &= ~VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT;

        extraInfo.requiredSubgroupSize.pNext = const_cast<void*>(std::exchange(stageInfo.pNext, &extraInfo.requiredSubgroupSize));
        extraInfo.requiredSubgroupSize.requiredSubgroupSize = requiredSubgroupSize;

        minSubgroupSize = requiredSubgroupSize;
        maxSubgroupSize = requiredSubgroupSize;
      }
    }
  }

  // With all the information provided above, we can patch the SPIR-V binary
  optimizer.setSubgroupSize(minSubgroupSize, maxSubgroupSize);

  for (uint32_t i = 0; i < specInfo->mapEntryCount; i++) {
    auto specDwords = reinterpret_cast<const uint32_t*>(specInfo->pData);

    if (specInfo->pMapEntries[i].size == sizeof(uint32_t) &&
        specInfo->pMapEntries[i].offset + specInfo->pMapEntries[i].size <= specInfo->dataSize) {
      optimizer.setSpecConstant(specInfo->pMapEntries[i].constantID,
        specDwords[specInfo->pMapEntries[i].offset / sizeof(*specDwords)]);
    }
  }

  if (stage == GfxShaderStage::eMesh) {
    optimizer.adjustMeshOutputCounts(
      patchInfo.maxVertexCount,
      patchInfo.maxPrimitiveCount);
  }

  extraInfo.codeBuffer = optimizer.getCodeBuffer();

  // Set up shader module info and create module if necessary
  extraInfo.moduleInfo.codeSize = extraInfo.codeBuffer.getSize();
  extraInfo.moduleInfo.pCode = extraInfo.codeBuffer.getCode();

  if (m_device.getVkFeatures().extGraphicsPipelineLibrary.graphicsPipelineLibrary) {
    extraInfo.moduleInfo.pNext = std::exchange(stageInfo.pNext, &extraInfo.moduleInfo);
  } else {
    VkResult vr = vk.vkCreateShaderModule(vk.device,
      &extraInfo.moduleInfo, nullptr, &stageInfo.module);

    if (vr)
      throw VulkanError("Vulkan: Failed to create shader module", vr);
  }
}


void GfxVulkanPipelineManager::initSpecializationInfo(
  const GfxVulkanSpecConstantData*    specData,
        VkSpecializationInfo&         specInfo) const {
  specInfo = { };
  specInfo.mapEntryCount = g_specConstantMap.size();
  specInfo.pMapEntries = g_specConstantMap.data();
  specInfo.dataSize = sizeof(*specData);
  specInfo.pData = specData;
}


const GfxVulkanDescriptorLayout* GfxVulkanPipelineManager::getDescriptorArrayLayout(
        GfxShaderBindingType          type) {
  std::lock_guard lock(m_mutex);

  return getDescriptorArrayLayoutLocked(type);
}


GfxVulkanComputePipeline& GfxVulkanPipelineManager::createComputePipeline(
  const GfxComputePipelineDesc&       desc) {
  std::unique_lock lock(m_mutex);

  auto hash = desc.compute->getShaderBinary().hash;
  auto entry = m_computePipelines.find(hash);

  if (entry != m_computePipelines.end())
    return entry->second;

  auto pipelineLayout = getPipelineLayoutForShadersLocked(1, &desc.compute);

  auto insert = m_computePipelines.emplace(std::piecewise_construct,
    std::forward_as_tuple(hash),
    std::forward_as_tuple(*this, *pipelineLayout, desc));

  lock.unlock();
  lock = std::unique_lock(m_compilerMutex);

  m_compilerQueue.emplace(insert.first->second);
  m_compilerCond.notify_one();

  return insert.first->second;
}


GfxVulkanGraphicsPipeline& GfxVulkanPipelineManager::createGraphicsPipeline(
  const GfxGraphicsPipelineDesc&      desc) {
  return createGraphicsPipelineTyped(desc);
}


GfxVulkanGraphicsPipeline& GfxVulkanPipelineManager::createGraphicsPipeline(
  const GfxMeshPipelineDesc&          desc) {
  return createGraphicsPipelineTyped(desc);
}


GfxVulkanVertexInputPipeline& GfxVulkanPipelineManager::createVertexInputPipeline(
  const GfxVulkanRenderState&         renderState) {
  GfxRenderStateDesc normalizedData = renderState.getState();
  normalizedData.flags = GfxRenderStateFlag::ePrimitiveTopology
                       | GfxRenderStateFlag::eVertexLayout;

  GfxVulkanRenderState& normalizedState = createRenderState(normalizedData);

  std::lock_guard lock(m_mutex);

  GfxVulkanVertexInputKey key = { };
  key.renderState = &normalizedState;

  auto entry = m_vertexInputPipelines.find(key);

  if (entry != m_vertexInputPipelines.end())
    return entry->second;

  auto insert = m_vertexInputPipelines.emplace(std::piecewise_construct,
    std::forward_as_tuple(key),
    std::forward_as_tuple(*this, key));
  return insert.first->second;
}


GfxVulkanFragmentOutputPipeline& GfxVulkanPipelineManager::createFragmentOutputPipeline(
  const GfxVulkanRenderTargetState&   targetState,
  const GfxVulkanRenderState&         renderState,
        VkBool32                      sampleShading,
  const GfxShaderIoMask&              shaderIoMasks) {
  GfxRenderStateDesc normalizedData = renderState.getState();
  normalizedData.flags = GfxRenderStateFlag::eMultisampling
                       | GfxRenderStateFlag::eBlending;

  GfxVulkanRenderState& normalizedState = createRenderState(normalizedData);

  std::lock_guard lock(m_mutex);

  GfxVulkanFragmentOutputKey key = { };
  key.targetState = &targetState;
  key.renderState = &normalizedState;
  key.sampleShading = sampleShading;
  key.shaderIoMasks = shaderIoMasks;

  auto entry = m_fragmentOutputPipelines.find(key);

  if (entry != m_fragmentOutputPipelines.end())
    return entry->second;

  auto insert = m_fragmentOutputPipelines.emplace(std::piecewise_construct,
    std::forward_as_tuple(key),
    std::forward_as_tuple(*this, key));
  return insert.first->second;
}


void GfxVulkanPipelineManager::deferCreateGraphicsPipelineVariant(
        GfxVulkanGraphicsPipeline&    pipeline,
  const GfxVulkanGraphicsPipelineVariantKey& key) {
  std::unique_lock lock(m_compilerMutex);

  m_compilerQueue.emplace(pipeline, key);
  m_compilerCond.notify_one();
}


const GfxVulkanDescriptorLayout* GfxVulkanPipelineManager::getDescriptorLayoutLocked(
  const GfxVulkanDescriptorLayoutKey& key) {
  auto entry = m_descriptorSetLayouts.emplace(std::piecewise_construct,
    std::forward_as_tuple(key),
    std::forward_as_tuple(m_device, key));

  return &entry.first->second;
}


const GfxVulkanDescriptorLayout* GfxVulkanPipelineManager::getDescriptorArrayLayoutLocked(
        GfxShaderBindingType          type) {
  GfxVulkanBindingInfo binding;
  binding.type = getVkDescriptorType(type);
  binding.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
  binding.binding = 0;
  binding.count = getMaxDescriptorCountForType(binding.type);
  binding.stages = VK_SHADER_STAGE_ALL;

  GfxVulkanDescriptorLayoutKey key;
  key.bindings.push_back(binding);

  return getDescriptorLayoutLocked(key);
}


const GfxVulkanPipelineLayout* GfxVulkanPipelineManager::getPipelineLayoutLocked(
  const GfxVulkanPipelineLayoutKey&   key) {
  auto entry = m_pipelineLayouts.emplace(std::piecewise_construct,
    std::forward_as_tuple(key),
    std::forward_as_tuple(m_device, key));

  return &entry.first->second;
}


const GfxVulkanPipelineLayout* GfxVulkanPipelineManager::getPipelineLayoutForShadersLocked(
        uint32_t                      shaderCount,
  const GfxShader*                    shaders) {
  std::array<GfxVulkanDescriptorLayoutKey, GfxMaxDescriptorSets> setKeys;
  std::array<uint32_t, 5> shaderBindingIndices = { };

  uint32_t shadersProcessedMask = 0u;
  uint32_t shadersIncompleteMask = (1u << shaderCount) - 1u;

  uint32_t setCount = 0;

  while (true) {
    // Mark shaders as complete if the current binding index
    // for that shader is equal to its total binding count
    for (uint32_t i = shadersIncompleteMask; i; i &= i - 1) {
      uint32_t shaderBit = i & -i;
      uint32_t shaderIndex = tzcnt(shaderBit);

      // If the previous iteration processed a binding
      // from this shader, increment its binding index
      if (shadersProcessedMask & shaderBit)
        shaderBindingIndices[shaderIndex] += 1;

      if (shaderBindingIndices[shaderIndex] == shaders[shaderIndex]->getBindingCount())
        shadersIncompleteMask &= ~shaderBit;
    }

    if (!shadersIncompleteMask)
      break;

    // Iterate over all shaders again to find the ones
    // with the smallest next set and binding index
    VkShaderStageFlags shaderStages = 0u;

    VkDescriptorType nextDescriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint32_t nextDescriptorCount = 0;

    uint32_t nextBinding = ~0u;
    uint32_t nextSet = ~0u;

    shadersProcessedMask = 0;

    for (uint32_t i = shadersIncompleteMask; i; i &= i - 1) {
      uint32_t shaderBit = i & -i;
      uint32_t shaderIndex = tzcnt(shaderBit);

      const auto& bindingInfo = shaders[shaderIndex]->getBinding(
        shaderBindingIndices[shaderIndex]);

      uint32_t set = bindingInfo.descriptorSet;
      uint32_t binding = bindingInfo.descriptorIndex;

      if (set > nextSet || (set == nextSet && binding > nextBinding))
        continue;

      if (set < nextSet || binding < nextBinding) {
        nextBinding = binding;
        nextSet = set;

        shadersProcessedMask = 0;
        shaderStages = 0;

        nextDescriptorCount = 0;
        nextDescriptorType = getVkDescriptorType(bindingInfo.type);
      }

      shadersProcessedMask |= shaderBit;
      shaderStages |= getVkShaderStage(shaders[shaderIndex]->getShaderStage());

      nextDescriptorCount = std::max(nextDescriptorCount, bindingInfo.descriptorCount);
    }

    setCount = nextSet + 1;

    // Add the actual binding info
    GfxVulkanBindingInfo bindingInfo;
    bindingInfo.type = nextDescriptorType;
    bindingInfo.flags = 0;
    bindingInfo.binding = nextBinding;
    bindingInfo.count = nextDescriptorCount;
    bindingInfo.stages = shaderStages;

    if (!bindingInfo.count) {
      bindingInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                        | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                        | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                        | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
      bindingInfo.count = getMaxDescriptorCountForType(bindingInfo.type);
      bindingInfo.stages = VK_SHADER_STAGE_ALL;
    }

    setKeys[nextSet].bindings.push_back(bindingInfo);
  }

  // Create actual Vulkan pipeline layout
  GfxVulkanPipelineLayoutKey pipelineLayoutKey;
  pipelineLayoutKey.descriptorSetCount = setCount;

  for (uint32_t i = 0; i < shaderCount; i++) {
    uint32_t constantBytes = shaders[i]->getConstantSize();

    if (constantBytes) {
      pipelineLayoutKey.constantStages |= getVkShaderStage(shaders[i]->getShaderStage());
      pipelineLayoutKey.constantBytes = std::max(pipelineLayoutKey.constantBytes, constantBytes);
    }
  }

  for (uint32_t i = 0; i < setCount; i++)
    pipelineLayoutKey.descriptorSets[i] = getDescriptorLayoutLocked(setKeys[i]);

  return getPipelineLayoutLocked(pipelineLayoutKey);
}


const GfxVulkanPipelineLayout* GfxVulkanPipelineManager::getGraphicsPipelineLayoutLocked(
  const GfxGraphicsPipelineDesc&      desc) {
  small_vector<GfxShader, 5> shaders;
  shaders.push_back(desc.vertex);

  if (desc.tessControl && desc.tessEval) {
    shaders.push_back(desc.tessControl);
    shaders.push_back(desc.tessEval);
  }

  if (desc.geometry)
    shaders.push_back(desc.geometry);

  if (desc.fragment)
    shaders.push_back(desc.fragment);

  return getPipelineLayoutForShadersLocked(shaders.size(), shaders.data());
}


const GfxVulkanPipelineLayout* GfxVulkanPipelineManager::getGraphicsPipelineLayoutLocked(
  const GfxMeshPipelineDesc&          desc) {
  small_vector<GfxShader, 3> shaders;

  if (desc.task)
    shaders.push_back(desc.task);

  shaders.push_back(desc.mesh);

  if (desc.fragment)
    shaders.push_back(desc.fragment);

  return getPipelineLayoutForShadersLocked(shaders.size(), shaders.data());
}


template<typename T>
GfxVulkanGraphicsPipeline& GfxVulkanPipelineManager::createGraphicsPipelineTyped(
  const T&                            desc) {
  std::unique_lock lock(m_mutex);

  GfxVulkanGraphicsPipelineKey key(desc);
  auto entry = m_graphicsPipelines.find(key);

  if (entry != m_graphicsPipelines.end())
    return entry->second;

  auto pipelineLayout = getGraphicsPipelineLayoutLocked(desc);

  auto insert = m_graphicsPipelines.emplace(std::piecewise_construct,
    std::forward_as_tuple(key),
    std::forward_as_tuple(*this, *pipelineLayout, desc));

  lock.unlock();

  if (insert.first->second.supportsFastLink()) {
    std::unique_lock lock(m_compilerMutex);

    m_compilerQueue.emplace(insert.first->second);
    m_compilerCond.notify_one();
  }

  return insert.first->second;
}


uint32_t GfxVulkanPipelineManager::getMaxDescriptorCountForType(
        VkDescriptorType              type) const {
  const auto& vk12 = m_device.getVkProperties().vk12;
  const auto& rtas = m_device.getVkProperties().khrAccelerationStructure;

  // Divide everything by 2 so we can use regular sets as well
  switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
      return vk12.maxPerStageDescriptorUpdateAfterBindSamplers / 2;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return vk12.maxPerStageDescriptorUpdateAfterBindUniformBuffers / 2;

    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return vk12.maxPerStageDescriptorUpdateAfterBindStorageBuffers / 2;

    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return vk12.maxPerStageDescriptorUpdateAfterBindSampledImages / 2;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return vk12.maxPerStageDescriptorUpdateAfterBindStorageImages / 2;

    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
      return rtas.maxPerStageDescriptorUpdateAfterBindAccelerationStructures / 2;

    default:
      return 0;
  }
}


void GfxVulkanPipelineManager::runWorker() {
  while (true) {
    std::unique_lock lock(m_compilerMutex);

    m_compilerCond.wait(lock, [this] {
      return !m_compilerQueue.empty() || m_compilerStopped;
    });

    if (m_compilerStopped)
      return;

    WorkItem item = std::move(m_compilerQueue.front());
    m_compilerQueue.pop();
    lock.unlock();

    switch (item.type) {
      case WorkItemType::eComputePipeline:
        item.computePipeline->createPipeline();
        break;

      case WorkItemType::eGraphicsPipeline:
        item.graphicsPipeline->createLibrary();
        break;

      case WorkItemType::eGraphicsVariant:
        item.graphicsPipeline->createVariant(item.graphicsState);
        break;
    }
  }
}

}
