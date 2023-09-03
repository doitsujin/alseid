#include "../../util/util_assert.h"
#include "../../util/util_likely.h"
#include "../../util/util_math.h"

#include "gfx_vulkan_buffer.h"
#include "gfx_vulkan_command_list.h"
#include "gfx_vulkan_context.h"
#include "gfx_vulkan_descriptor_array.h"
#include "gfx_vulkan_image.h"
#include "gfx_vulkan_ray_tracing.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanContext::GfxVulkanContext(
        std::shared_ptr<GfxVulkanDevice> device,
        GfxQueue                      queue)
: m_device  (std::move(device))
, m_queue   (queue) {
  auto& vk = m_device->vk();

  VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  poolInfo.queueFamilyIndex = m_device->getQueueFamilyIndex(queue);

  VkResult vr = vk.vkCreateCommandPool(vk.device, &poolInfo, nullptr, &m_commandPool);

  if (vr)
    throw VulkanError("Vulkan: Failed to create command pool", vr);

  // Allocate an initial command buffer so that
  // the app can start recording commands
  m_cmd = allocateCommandBuffer();

  // Initialize context state
  resetState();
}


GfxVulkanContext::~GfxVulkanContext() {
  auto& vk = m_device->vk();

  vk.vkDestroyCommandPool(vk.device, m_commandPool, nullptr);
}


GfxCommandList GfxVulkanContext::endCommandList() {
  auto& vk = m_device->vk();

  // End command buffer to make it ready for submission
  VkCommandBuffer cmd = m_cmd;

  m_barrierBatch.recordCommands(vk, cmd);
  VkResult vr = vk.vkEndCommandBuffer(cmd);

  if (vr)
    throw VulkanError("Vulkan: Failed to end command buffer", vr);

  // Allocate a new command buffer that
  // can be used again right away
  m_cmd = allocateCommandBuffer();

  resetState();
  return GfxCommandList(std::make_shared<GfxVulkanCommandList>(cmd));
}


void GfxVulkanContext::reset() {
  auto& vk = m_device->vk();

  VkResult vr = vk.vkResetCommandPool(vk.device, m_commandPool, 0);

  if (vr)
    throw VulkanError("Vulkan: Failed to reset command pool", vr);

  // Recycle descriptor pools that are no longer in use
  for (auto& pool : m_descriptorPools)
    m_device->getDescriptorPoolManager().recyclePool(std::move(pool));

  m_descriptorPools.clear();
  m_scratchPages.clear();

  // Allocate a command buffer and reset context state
  m_commandBufferIndex = 0;
  m_cmd = allocateCommandBuffer();

  resetState();
}


void GfxVulkanContext::insertDebugLabel(
  const char*                         text,
        GfxColorValue                 color) {
  if (m_device->isDebugDevice()) {
    auto& vk = m_device->vk();

    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = text ? text : "[undefined]";
    label.color[0] = color.f.r;
    label.color[1] = color.f.g;
    label.color[2] = color.f.b;
    label.color[3] = color.f.a;

    vk.vkCmdInsertDebugUtilsLabelEXT(m_cmd, &label);
  }
}


void GfxVulkanContext::beginDebugLabel(
  const char*                         text,
        GfxColorValue                 color) {
  if (m_device->isDebugDevice()) {
    auto& vk = m_device->vk();

    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = text ? text : "[undefined]";
    label.color[0] = color.f.r;
    label.color[1] = color.f.g;
    label.color[2] = color.f.b;
    label.color[3] = color.f.a;

    vk.vkCmdBeginDebugUtilsLabelEXT(m_cmd, &label);
  }
}


void GfxVulkanContext::endDebugLabel() {
  if (m_device->isDebugDevice()) {
    auto& vk = m_device->vk();

    vk.vkCmdEndDebugUtilsLabelEXT(m_cmd);
  }
}


GfxScratchBuffer GfxVulkanContext::allocScratch(
        GfxUsageFlags                 usage,
        uint64_t                      size) {
  // Compute memory type based on CPU access flag and usage
  GfxMemoryType memoryType = GfxMemoryType::eVideoMemory;

  if (usage & GfxUsage::eCpuRead) {
    memoryType = GfxMemoryType::eSystemMemory;
  } else if (usage & GfxUsage::eCpuWrite) {
    memoryType = (usage - (GfxUsage::eTransferSrc | GfxUsage::eCpuWrite))
      ? GfxMemoryType::eBarMemory
      : GfxMemoryType::eSystemMemory;
  }

  // If the desired size is larger than a scratch buffer,
  // we need to create a temporary buffer. Applications
  // should never do this, however.
  if (unlikely(size > GfxScratchBufferSize)) {
    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Scratch buffer (large)";
    bufferDesc.usage = usage;
    bufferDesc.size = size;
    bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

    GfxScratchBuffer result;
    result.buffer = m_device->createBuffer(bufferDesc, memoryType);
    result.offset = 0;
    result.size = size;
    return result;
  }

  // An alignment of 256 bytes accounts for all possible use
  // cases on all Vulkan-compliant hardware. Allocating smaller
  // scratch buffer ranges is not recommended anyway.
  constexpr VkDeviceSize alignment = 256;

  // Probe existing scratch memory pages, in reverse order
  // since we're more likely to find free pages there.
  for (auto iter = m_scratchPages.rbegin(); iter != m_scratchPages.rend(); iter++) {
    if (iter->getMemoryType() != memoryType)
      continue;

    auto page = iter->alloc(size, alignment);

    if (page)
      return std::move(*page);
  }

  // If that didn't work, allocate a new scratch page.
  auto& page = m_scratchPages.emplace_back(m_device->allocScratchMemory(memoryType,
    align(size, GfxScratchPageSize) / GfxScratchPageSize));

  return *page.alloc(size, alignment);
}


void GfxVulkanContext::beginRendering(
  const GfxRenderingInfo&             renderingInfo,
        GfxRenderingFlags             flags) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  // Render target state description
  GfxRenderTargetStateDesc rtDesc;

  // Initialize render area with the maximum supported size. We'll
  // scale this down to the smallest attachment size as necessary.
  const auto& deviceLimits = m_device->getVkProperties().core.properties.limits;

  Extent3D renderArea(
    deviceLimits.maxFramebufferWidth,
    deviceLimits.maxFramebufferHeight,
    deviceLimits.maxFramebufferLayers);

  // Set up color attachments
  std::array<VkRenderingAttachmentInfo, GfxMaxColorAttachments> colorAttachments;
  uint32_t colorAttachmentCount = 0;

  for (size_t i = 0; i < GfxMaxColorAttachments; i++) {
    auto& color = colorAttachments[i];
    color = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

    if (renderingInfo.color[i].view) {
      auto& view = static_cast<const GfxVulkanImageView&>(*renderingInfo.color[i].view);
      color.imageView = view.getHandle();
      color.imageLayout = view.getLayout();

      // Handle resolve info. The resolve mode is determined by the view format.
      if (renderingInfo.color[i].resolveView) {
        auto& resolveView = static_cast<const GfxVulkanImageView&>(*renderingInfo.color[i].resolveView);
        color.resolveMode = getVkResolveMode(resolveView.getDesc().format,
          GfxImageAspect(resolveView.getDesc().subresource.aspects));
        color.resolveImageView = resolveView.getHandle();
        color.resolveImageLayout = resolveView.getLayout();
      }

      color.loadOp = getVkAttachmentLoadOp(renderingInfo.color[i].op);
      color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      if (color.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        color.clearValue = getVkClearValue(renderingInfo.color[i].clearValue);

      // Write back color format and sample count
      rtDesc.colorFormats[i] = view.getDesc().format;
      rtDesc.sampleCount = std::max(rtDesc.sampleCount, view.getImageSampleCount());

      // Adjust render area as necessary
      Extent3D viewExtent = view.computeMipExtent(0);

      renderArea = Extent3D(
        std::min(renderArea.at<0>(), viewExtent.at<0>()),
        std::min(renderArea.at<1>(), viewExtent.at<1>()),
        std::min(renderArea.at<2>(), std::max(viewExtent.at<2>(), view.getDesc().subresource.layerCount)));

      colorAttachmentCount = i + 1;
    }
  }

  // Set up depth-stencil attachment as necessary
  VkRenderingAttachmentInfo depth = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
  VkRenderingAttachmentInfo stencil = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

  if (renderingInfo.depthStencil.view) {
    // Common info for both depth and stencil aspects
    auto& view = static_cast<const GfxVulkanImageView&>(*renderingInfo.depthStencil.view);
    depth.imageView = stencil.imageView = view.getHandle();
    depth.imageLayout = stencil.imageLayout = view.getLayout();

    // Adjust image layouts for read-only aspects as necessary
    auto readOnlyAspects = renderingInfo.depthStencil.readOnlyAspects;

    if ((readOnlyAspects & GfxImageAspect::eDepth)
     && (depth.imageLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL))
      depth.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

    if ((readOnlyAspects & GfxImageAspect::eStencil)
     && (stencil.imageLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL))
      stencil.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

    // Handle resolve info. We only support SAMPLE_ZERO for depth-stencil.
    if (renderingInfo.depthStencil.resolveView) {
      auto& resolveView = static_cast<const GfxVulkanImageView&>(*renderingInfo.depthStencil.resolveView);
      depth.resolveMode = stencil.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
      depth.resolveImageView = stencil.resolveImageView = resolveView.getHandle();
      depth.resolveImageLayout = stencil.resolveImageLayout = resolveView.getLayout();
    }

    // Deal with load ops and clear values for the separate aspects
    depth.loadOp = getVkAttachmentLoadOp(renderingInfo.depthStencil.depthOp);
    stencil.loadOp = getVkAttachmentLoadOp(renderingInfo.depthStencil.stencilOp);

    if (depth.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
      depth.clearValue = getVkClearValue(renderingInfo.depthStencil.clearValue);

    if (stencil.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
      stencil.clearValue = getVkClearValue(renderingInfo.depthStencil.clearValue);

    // Write back format and sample count
    rtDesc.depthStencilFormat = view.getDesc().format;
    rtDesc.sampleCount = std::max(rtDesc.sampleCount, view.getImageSampleCount());

    // Adjust render area as necessary
    Extent3D viewExtent = view.computeMipExtent(0);

    renderArea = Extent3D(
      std::min(renderArea.at<0>(), viewExtent.at<0>()),
      std::min(renderArea.at<1>(), viewExtent.at<1>()),
      std::min(renderArea.at<2>(), std::max(viewExtent.at<2>(), view.getDesc().subresource.layerCount)));
  }

  // Set up shading rate image, if any
  VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRate = { VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR };

  if (renderingInfo.shadingRate.view) {
    auto& view = static_cast<const GfxVulkanImageView&>(*renderingInfo.shadingRate.view);
    shadingRate.imageView = view.getHandle();
    shadingRate.imageLayout = view.getLayout();
    shadingRate.shadingRateAttachmentTexelSize = getVkExtent2D(m_device->getShadingRateTileSize());
  }

  // Set up final Vulkan structure and begin rendering
  VkRenderingInfo info = { VK_STRUCTURE_TYPE_RENDERING_INFO };

  if (shadingRate.imageView)
    shadingRate.pNext = std::exchange(info.pNext, &shadingRate);

  if (flags & GfxRenderingFlag::eSuspend)
    info.flags |= VK_RENDERING_SUSPENDING_BIT;

  if (flags & GfxRenderingFlag::eResume)
    info.flags |= VK_RENDERING_RESUMING_BIT;

  info.renderArea.offset = { 0, 0 };
  info.renderArea.extent = { renderArea.at<0>(), renderArea.at<1>() };
  info.layerCount = renderArea.at<2>();

  if (colorAttachmentCount) {
    info.colorAttachmentCount = colorAttachmentCount;
    info.pColorAttachments = colorAttachments.data();
  }

  if (renderingInfo.depthStencil.view) {
    auto& formatInfo = Gfx::getFormatInfo(renderingInfo.depthStencil.view->getDesc().format);

    if (formatInfo.aspects & GfxImageAspect::eDepth)
      info.pDepthAttachment = &depth;

    if (formatInfo.aspects & GfxImageAspect::eStencil)
      info.pDepthAttachment = &stencil;
  }

  vk.vkCmdBeginRendering(m_cmd, &info);

  // Create and set render target state object, and
  // dirty all graphics state for the next draw call
  m_renderTargetState = &m_device->getPipelineManager().createRenderTargetState(rtDesc);

  invalidateState();
}


void GfxVulkanContext::endRendering() {
  auto& vk = m_device->vk();

  vk.vkCmdEndRendering(m_cmd);

  // Dirty all state for the next set of commands
  invalidateState();
}


void GfxVulkanContext::memoryBarrier(
        GfxUsageFlags                 srcUsage,
        GfxShaderStages               srcStages,
        GfxUsageFlags                 dstUsage,
        GfxShaderStages               dstStages) {
  VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
  std::tie(barrier.srcStageMask, barrier.srcAccessMask) = getVkStageAccessFromUsage(srcUsage, srcStages);
  std::tie(barrier.dstStageMask, barrier.dstAccessMask) = getVkStageAccessFromUsage(dstUsage, dstStages);
  m_barrierBatch.addMemoryBarrier(barrier);
}


void GfxVulkanContext::imageBarrier(
  const GfxImage&                     image,
  const GfxImageSubresource&          subresource,
        GfxUsageFlags                 srcUsage,
        GfxShaderStages               srcStages,
        GfxUsageFlags                 dstUsage,
        GfxShaderStages               dstStages,
        GfxBarrierFlags               flags) {
  auto& vk = m_device->vk();
  auto& vkImage = static_cast<const GfxVulkanImage&>(*image);

  VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
  std::tie(barrier.srcStageMask, barrier.srcAccessMask) = getVkStageAccessFromUsage(srcUsage, srcStages);
  std::tie(barrier.dstStageMask, barrier.dstAccessMask) = getVkStageAccessFromUsage(dstUsage, dstStages);

  auto stageAccess = vkImage.getStageAccessMasks();
  barrier.srcStageMask &= stageAccess.first;
  barrier.srcAccessMask &= stageAccess.second;
  barrier.dstStageMask &= stageAccess.first;
  barrier.dstAccessMask &= stageAccess.second;

  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (!(flags & GfxBarrierFlag::eDiscard))
    barrier.oldLayout = getVkImageLayoutFromUsage(vkImage, srcUsage);

  barrier.newLayout = getVkImageLayoutFromUsage(vkImage, dstUsage);
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = vkImage.getHandle();
  barrier.subresourceRange = getVkImageSubresourceRange(subresource);

  m_barrierBatch.addImageBarrier(vk, m_cmd, barrier);
}


void GfxVulkanContext::acquireImage(
  const GfxImage&                     image,
  const GfxImageSubresource&          subresource,
        GfxQueue                      srcQueue,
        GfxUsageFlags                 srcUsage,
        GfxUsageFlags                 dstUsage,
        GfxShaderStages               dstStages) {
  auto& vk = m_device->vk();
  auto& vkImage = static_cast<const GfxVulkanImage&>(*image);

  VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
  std::tie(barrier.dstStageMask, barrier.dstAccessMask) = getVkStageAccessFromUsage(dstUsage, dstStages);

  auto stageAccess = vkImage.getStageAccessMasks();
  barrier.dstStageMask &= stageAccess.first;
  barrier.dstAccessMask &= stageAccess.second;

  barrier.oldLayout = getVkImageLayoutFromUsage(vkImage, srcUsage);
  barrier.newLayout = getVkImageLayoutFromUsage(vkImage, dstUsage);
  barrier.srcQueueFamilyIndex = m_device->getQueueFamilyIndex(srcQueue);
  barrier.dstQueueFamilyIndex = m_device->getQueueFamilyIndex(m_queue);
  barrier.image = vkImage.getHandle();
  barrier.subresourceRange = getVkImageSubresourceRange(subresource);

  m_barrierBatch.addImageBarrier(vk, m_cmd, barrier);
}


void GfxVulkanContext::releaseImage(
  const GfxImage&                     image,
  const GfxImageSubresource&          subresource,
        GfxUsageFlags                 srcUsage,
        GfxShaderStages               srcStages,
        GfxQueue                      dstQueue,
        GfxUsageFlags                 dstUsage) {
  auto& vk = m_device->vk();
  auto& vkImage = static_cast<const GfxVulkanImage&>(*image);

  VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
  std::tie(barrier.srcStageMask, barrier.srcAccessMask) = getVkStageAccessFromUsage(srcUsage, srcStages);

  auto stageAccess = vkImage.getStageAccessMasks();
  barrier.srcStageMask &= stageAccess.first;
  barrier.srcAccessMask &= stageAccess.second;

  barrier.oldLayout = getVkImageLayoutFromUsage(vkImage, srcUsage);
  barrier.newLayout = getVkImageLayoutFromUsage(vkImage, dstUsage);
  barrier.srcQueueFamilyIndex = m_device->getQueueFamilyIndex(m_queue);
  barrier.dstQueueFamilyIndex = m_device->getQueueFamilyIndex(dstQueue);
  barrier.image = vkImage.getHandle();
  barrier.subresourceRange = getVkImageSubresourceRange(subresource);

  m_barrierBatch.addImageBarrier(vk, m_cmd, barrier);
}


void GfxVulkanContext::bindPipeline(
        GfxComputePipeline            pipeline) {
  auto& newPipeline = static_cast<GfxVulkanComputePipeline&>(*pipeline);

  if (m_computePipeline == &newPipeline)
    return;

  const GfxVulkanPipelineLayout* newLayout = newPipeline.getPipelineLayout();
  const GfxVulkanPipelineLayout* oldLayout = nullptr;

  if (m_computePipeline)
    oldLayout = static_cast<GfxVulkanComputePipeline&>(*m_computePipeline).getPipelineLayout();

  computeDirtySets(oldLayout, newLayout);

  m_computePipeline = &newPipeline;
  m_graphicsPipeline = nullptr;

  m_flags |= GfxVulkanContextFlag::eDirtyPipeline
          |  GfxVulkanContextFlag::eDirtyConstants;
}


void GfxVulkanContext::bindPipeline(
        GfxGraphicsPipeline           pipeline) {
  auto& newPipeline = static_cast<GfxVulkanGraphicsPipeline&>(*pipeline);

  if (m_graphicsPipeline == &newPipeline)
    return;

  const GfxVulkanPipelineLayout* newLayout = newPipeline.getPipelineLayout();
  const GfxVulkanPipelineLayout* oldLayout = nullptr;

  if (m_graphicsPipeline)
    oldLayout = static_cast<GfxVulkanGraphicsPipeline&>(*m_graphicsPipeline).getPipelineLayout();

  computeDirtySets(oldLayout, newLayout);

  m_computePipeline = nullptr;
  m_graphicsPipeline = &newPipeline;

  m_flags |= GfxVulkanContextFlag::eDirtyPipeline
          |  GfxVulkanContextFlag::eDirtyConstants;
}


void GfxVulkanContext::bindDescriptorArray(
        uint32_t                      set,
  const GfxDescriptorArray&           array) {
  VkDescriptorSet setHandle = static_cast<const GfxVulkanDescriptorArray&>(*array).getHandle();

  if (m_descriptorArrays[set] == setHandle)
    return;

  m_descriptorArrays[set] = setHandle;
  m_dirtySets |= 1u << set;
}


void GfxVulkanContext::bindDescriptors(
        uint32_t                      set,
        uint32_t                      index,
        uint32_t                      count,
  const GfxDescriptor*                descriptors) {
  if (unlikely(!count))
    return;

  size_t descriptorIndex = GfxMaxDescriptorsPerSet * set + index;

  for (uint32_t i = 0; i < count; i++)
    m_descriptors[descriptorIndex + i] = importVkDescriptor(descriptors[i]);

  m_dirtySets |= 1u << set;
}


void GfxVulkanContext::bindIndexBuffer(
  const GfxDescriptor&                descriptor,
        GfxFormat                     format) {
  auto info = importVkDescriptor(descriptor);

  m_indexBufferHandle = info.buffer.buffer;
  m_indexBufferOffset = info.buffer.offset;
  m_indexBufferFormat = getVkIndexType(format);

  m_flags |= GfxVulkanContextFlag::eDirtyIndexBuffer;
}


void GfxVulkanContext::bindVertexBuffers(
        uint32_t                      index,
        uint32_t                      count,
  const GfxDescriptor*                descriptors) {
  if (unlikely(!count))
    return;

  for (uint32_t i = 0; i < count; i++) {
    auto info = importVkDescriptor(descriptors[i]);

    m_vertexBufferHandles[index + i] = info.buffer.buffer;
    m_vertexBufferOffsets[index + i] = info.buffer.offset;
    m_vertexBufferSizes[index + i] = info.buffer.range;
  }

  m_vbosDirty |= ((2u << (count - 1u)) - 1u) << index;
}


void GfxVulkanContext::buildRayTracingBvh(
  const GfxRayTracingBvh&             bvh,
        GfxRayTracingBvhBuildMode     mode,
  const GfxRayTracingBvhData*         data) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  auto& vkBvh = static_cast<GfxVulkanRayTracingBvh&>(*bvh);

  // Allocate scratch memory. Technically we're pessimistic here and we
  // could use a global buffer, with a linear allocator per command list.
  GfxScratchBuffer scratch = allocScratch(GfxUsage::eBvhBuild, vkBvh.getScratchSize(mode));

  // Populate cached build info with the provided parameters
  GfxVulkanRayTracingBvhInfo info = vkBvh.getBuildInfo(mode, data, scratch.getGpuAddress());
  const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos = info.rangeInfos.data();

  vk.vkCmdBuildAccelerationStructuresKHR(m_cmd, 1, &info.info, &rangeInfos);
}


void GfxVulkanContext::clearBuffer(
  const GfxBuffer&                    dstBuffer,
        uint64_t                      dstOffset,
        uint64_t                      size) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  auto& vkDstBuffer = static_cast<GfxVulkanBuffer&>(*dstBuffer);
  vk.vkCmdFillBuffer(m_cmd, vkDstBuffer.getHandle(), dstOffset, size, 0u);
}


void GfxVulkanContext::copyBuffer(
  const GfxBuffer&                    dstBuffer,
        uint64_t                      dstOffset,
  const GfxBuffer&                    srcBuffer,
        uint64_t                      srcOffset,
        uint64_t                      size) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  auto& vkDstBuffer = static_cast<GfxVulkanBuffer&>(*dstBuffer);
  auto& vkSrcBuffer = static_cast<GfxVulkanBuffer&>(*srcBuffer);

  VkBufferCopy2 region = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
  region.srcOffset = srcOffset;
  region.dstOffset = dstOffset;
  region.size = size;

  VkCopyBufferInfo2 copy = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
  copy.srcBuffer = vkSrcBuffer.getHandle();
  copy.dstBuffer = vkDstBuffer.getHandle();
  copy.regionCount = 1;
  copy.pRegions = &region;

  vk.vkCmdCopyBuffer2(m_cmd, &copy);
}


void GfxVulkanContext::copyBufferToImage(
  const GfxImage&                     image,
  const GfxImageSubresource&          imageSubresource,
        Offset3D                      imageOffset,
        Extent3D                      imageExtent,
  const GfxBuffer&                    buffer,
        uint64_t                      bufferOffset,
        Extent2D                      bufferLayout) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  auto& vkImage = static_cast<GfxVulkanImage&>(*image);
  auto& vkBuffer = static_cast<GfxVulkanBuffer&>(*buffer);

  auto regions = getVkBufferImageCopyRegions(image, imageSubresource,
    imageOffset, imageExtent, buffer, bufferOffset, bufferLayout);

  VkCopyBufferToImageInfo2 copy = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
  copy.srcBuffer = vkBuffer.getHandle();
  copy.dstImage = vkImage.getHandle();
  copy.dstImageLayout = vkImage.pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy.regionCount = regions.size();
  copy.pRegions = regions.data();

  vk.vkCmdCopyBufferToImage2(m_cmd, &copy);
}


void GfxVulkanContext::copyImage(
  const GfxImage&                     dstImage,
  const GfxImageSubresource&          dstSubresource,
        Offset3D                      dstOffset,
  const GfxImage&                     srcImage,
  const GfxImageSubresource&          srcSubresource,
        Offset3D                      srcOffset,
        Extent3D                      extent) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  auto& vkDstImage = static_cast<GfxVulkanImage&>(*dstImage);
  auto& vkSrcImage = static_cast<GfxVulkanImage&>(*srcImage);

  VkImageCopy2 region = { VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
  region.srcSubresource = getVkImageSubresourceLayers(srcSubresource);
  region.srcOffset = getVkOffset3D(srcOffset);
  region.dstSubresource = getVkImageSubresourceLayers(dstSubresource);
  region.dstOffset = getVkOffset3D(dstOffset);
  region.extent = getVkExtent3D(extent);

  VkCopyImageInfo2 copy = { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
  copy.srcImage = vkSrcImage.getHandle();
  copy.srcImageLayout = vkSrcImage.pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  copy.dstImage = vkDstImage.getHandle();
  copy.dstImageLayout = vkDstImage.pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy.regionCount = 1;
  copy.pRegions = &region;

  vk.vkCmdCopyImage2(m_cmd, &copy);
}


void GfxVulkanContext::copyImageToBuffer(
  const GfxBuffer&                    buffer,
        uint64_t                      bufferOffset,
        Extent2D                      bufferLayout,
  const GfxImage&                     image,
  const GfxImageSubresource&          imageSubresource,
        Offset3D                      imageOffset,
        Extent3D                      imageExtent) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  auto& vkBuffer = static_cast<GfxVulkanBuffer&>(*buffer);
  auto& vkImage = static_cast<GfxVulkanImage&>(*image);

  auto regions = getVkBufferImageCopyRegions(image, imageSubresource,
    imageOffset, imageExtent, buffer, bufferOffset, bufferLayout);

  VkCopyImageToBufferInfo2 copy = { VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2 };
  copy.srcImage = vkImage.getHandle();
  copy.srcImageLayout = vkImage.pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  copy.dstBuffer = vkBuffer.getHandle();
  copy.regionCount = regions.size();
  copy.pRegions = regions.data();

  vk.vkCmdCopyImageToBuffer2(m_cmd, &copy);
}


void GfxVulkanContext::decompressBuffer(
  const GfxBuffer&                    dstBuffer,
        uint64_t                      dstOffset,
        uint64_t                      dstSize,
  const GfxBuffer&                    srcBuffer,
        uint64_t                      srcOffset,
        uint64_t                      srcSize) {
  auto& vk = m_device->vk();
  m_barrierBatch.recordCommands(vk, m_cmd);

  invalidateState();

  GfxVulkanGDeflateArgs args = { };
  args.srcVa = srcBuffer->getGpuAddress() + srcOffset;
  args.dstVa = dstBuffer->getGpuAddress() + dstOffset;

  auto& pipeline = m_device->getGDeflatePipeline();

  vk.vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipeline());
  vk.vkCmdPushConstants(m_cmd, pipeline.getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(args), &args);
  vk.vkCmdDispatchIndirect(m_cmd, static_cast<GfxVulkanBuffer&>(*srcBuffer).getHandle(), srcOffset);
}


void GfxVulkanContext::dispatch(
        Extent3D                      workgroupCount) {
  auto& vk = m_device->vk();

  updateComputeState(vk);

  vk.vkCmdDispatch(m_cmd,
    workgroupCount.at<0>(),
    workgroupCount.at<1>(),
    workgroupCount.at<2>());
}


void GfxVulkanContext::dispatchIndirect(
    const GfxDescriptor&                args) {
  auto& vk = m_device->vk();

  updateComputeState(vk);

  auto descriptor = importVkDescriptor(args);

  vk.vkCmdDispatchIndirect(m_cmd,
    descriptor.buffer.buffer,
    descriptor.buffer.offset);
}


void GfxVulkanContext::draw(
        uint32_t                      vertexCount,
        uint32_t                      instanceCount,
        uint32_t                      firstVertex,
        uint32_t                      firstInstance) {
  auto& vk = m_device->vk();

  updateGraphicsState(vk, false);

  vk.vkCmdDraw(m_cmd,
    vertexCount, instanceCount,
    firstVertex, firstInstance);
}


void GfxVulkanContext::drawIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) {
  auto& vk = m_device->vk();

  updateGraphicsState(vk, false);

  auto argDescriptor = importVkDescriptor(args);
  auto cntDescriptor = importVkDescriptor(count);

  if (!cntDescriptor.buffer.buffer) {
    vk.vkCmdDrawIndirect(m_cmd,
      argDescriptor.buffer.buffer,
      argDescriptor.buffer.offset,
      maxCount, sizeof(GfxDrawArgs));
  } else {
    vk.vkCmdDrawIndirectCount(m_cmd,
      argDescriptor.buffer.buffer,
      argDescriptor.buffer.offset,
      cntDescriptor.buffer.buffer,
      cntDescriptor.buffer.offset,
      maxCount, sizeof(GfxDrawArgs));
  }
}


void GfxVulkanContext::drawIndexed(
        uint32_t                      indexCount,
        uint32_t                      instanceCount,
        uint32_t                      firstIndex,
        int32_t                       firstVertex,
        uint32_t                      firstInstance) {
  auto& vk = m_device->vk();

  updateGraphicsState(vk, true);

  vk.vkCmdDrawIndexed(m_cmd,
    indexCount, instanceCount,
    firstIndex, firstVertex, firstInstance);
}


void GfxVulkanContext::drawIndexedIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) {
  auto& vk = m_device->vk();

  updateGraphicsState(vk, false);

  auto argDescriptor = importVkDescriptor(args);
  auto cntDescriptor = importVkDescriptor(count);

  if (!cntDescriptor.buffer.buffer) {
    vk.vkCmdDrawIndexedIndirect(m_cmd,
      argDescriptor.buffer.buffer,
      argDescriptor.buffer.offset,
      maxCount, sizeof(GfxDrawIndexedArgs));
  } else {
    vk.vkCmdDrawIndexedIndirectCount(m_cmd,
      argDescriptor.buffer.buffer,
      argDescriptor.buffer.offset,
      cntDescriptor.buffer.buffer,
      cntDescriptor.buffer.offset,
      maxCount, sizeof(GfxDrawIndexedArgs));
  }
}


void GfxVulkanContext::drawMesh(
        Extent3D                      workgroupCount) {
  auto& vk = m_device->vk();

  updateGraphicsState(vk, false);

  vk.vkCmdDrawMeshTasksEXT(m_cmd,
    workgroupCount.at<0>(),
    workgroupCount.at<1>(),
    workgroupCount.at<2>());
}


void GfxVulkanContext::drawMeshIndirect(
  const GfxDescriptor&                args,
  const GfxDescriptor&                count,
        uint32_t                      maxCount) {
  auto& vk = m_device->vk();

  updateGraphicsState(vk, false);

  auto argDescriptor = importVkDescriptor(args);
  auto cntDescriptor = importVkDescriptor(count);

  if (!cntDescriptor.buffer.buffer) {
    vk.vkCmdDrawMeshTasksIndirectEXT(m_cmd,
      argDescriptor.buffer.buffer,
      argDescriptor.buffer.offset,
      maxCount, sizeof(GfxDispatchArgs));
  } else {
    vk.vkCmdDrawMeshTasksIndirectCountEXT(m_cmd,
      argDescriptor.buffer.buffer,
      argDescriptor.buffer.offset,
      cntDescriptor.buffer.buffer,
      cntDescriptor.buffer.offset,
      maxCount, sizeof(GfxDispatchArgs));
  }
}


void GfxVulkanContext::setBlendConstants(
        GfxColorValue                 constants) {
  m_blendConstants = getVkClearValue(constants).color;

  m_dynamicStatesDirty |= GfxVulkanDynamicState::eBlendConstants;
}


void GfxVulkanContext::setDepthBounds(
        float                         minDepth,
        float                         maxDepth) {
  m_depthBoundsMin = minDepth;
  m_depthBoundsMax = maxDepth;

  m_dynamicStatesDirty |= GfxVulkanDynamicState::eDepthBounds;
}


void GfxVulkanContext::setRenderState(
        GfxRenderState                state) {
  const GfxRenderStateData& data = state->getState();

  m_flags |= GfxVulkanContextFlag::eDirtyPipeline;

  if (data.flags & GfxRenderStateFlag::ePrimitiveTopology)
    m_renderState.primitiveTopology = data.primitiveTopology;

  if (data.flags & GfxRenderStateFlag::eVertexLayout)
    m_renderState.vertexLayout = data.vertexLayout;

  if (data.flags & GfxRenderStateFlag::eFrontFace) {
    m_renderState.frontFace = data.frontFace;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eRasterizerState;
  }

  if (data.flags & GfxRenderStateFlag::eCullMode) {
    m_renderState.cullMode = data.cullMode;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eRasterizerState;
  }

  if (data.flags & GfxRenderStateFlag::eConservativeRaster) {
    m_renderState.conservativeRaster = data.conservativeRaster;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eConservativeRaster;
  }

  if (data.flags & GfxRenderStateFlag::eDepthBias) {
    m_renderState.depthBias = data.depthBias;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eRasterizerState;
  }

  if (data.flags & GfxRenderStateFlag::eShadingRate) {
    m_renderState.shadingRate = data.shadingRate;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eShadingRate;
  }

  if (data.flags & GfxRenderStateFlag::eDepthTest) {
    m_renderState.depthTest = data.depthTest;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eDepthStencilState
                         |  GfxVulkanDynamicState::eDepthBoundsState;
  }

  if (data.flags & GfxRenderStateFlag::eStencilTest) {
    m_renderState.stencilTest = data.stencilTest;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eDepthStencilState;
  }

  if (data.flags & GfxRenderStateFlag::eMultisampling) {
    m_renderState.multisampling = data.multisampling;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eMultisampleState
                         |  GfxVulkanDynamicState::eAlphaToCoverage
                         |  GfxVulkanDynamicState::eShadingRate;
  }

  if (data.flags & GfxRenderStateFlag::eBlending) {
    m_renderState.blending = data.blending;
    m_dynamicStatesDirty |= GfxVulkanDynamicState::eBlendConstants;
  }
}


void GfxVulkanContext::setShaderConstants(
        uint32_t                      offset,
        uint32_t                      size,
  const void*                         data) {
  if (unlikely(uint64_t(offset) + uint64_t(size) > uint64_t(m_shaderConstants.size())))
    return;

  std::memcpy(&m_shaderConstants[offset], data, size);
  m_flags |= GfxVulkanContextFlag::eDirtyConstants;
}


void GfxVulkanContext::setStencilReference(
        uint32_t                      front,
        uint32_t                      back) {
  m_stencilRefFront = front;
  m_stencilRefBack = back;

  m_dynamicStatesDirty |= GfxVulkanDynamicState::eStencilRef;
}


void GfxVulkanContext::setViewports(
        uint32_t                      count,
  const GfxViewport*                  viewports) {
  m_viewportCount = count;

  for (uint32_t i = 0; i < count; i++)
    std::tie(m_viewports[i], m_scissors[i]) = getVkViewportAndScissor(viewports[i]);

  m_dynamicStatesDirty |= GfxVulkanDynamicState::eViewports;   
}


void GfxVulkanContext::updateGraphicsState(
  const GfxVulkanProcs&               vk,
        bool                          indexed) {
  if (m_flags & GfxVulkanContextFlag::eDirtyPipeline) {
    m_flags -= GfxVulkanContextFlag::eDirtyPipeline;

    // Disable vertex state for mesh shading pipelines,
    // and look up a compatible render state object.
    GfxRenderStateFlags vertexStateFlags =
      GfxRenderStateFlag::ePrimitiveTopology |
      GfxRenderStateFlag::eVertexLayout;

    if (m_graphicsPipeline->getShaderStages() & GfxShaderStage::eVertex)
      m_renderState.flags |= vertexStateFlags;
    else
      m_renderState.flags -= vertexStateFlags;

    m_renderStateObject = &m_device->getPipelineManager().createRenderState(m_renderState);

    // Dirty vertex buffers that have changed if necessary.
    if (m_graphicsPipeline->getShaderStages() & GfxShaderStage::eVertex) {
      uint32_t vbosActive = m_renderStateObject->getVertexBindingMask();
      m_vbosDirty = vbosActive & (vbosActive ^ m_vbosActive);
      m_vbosActive = vbosActive;
    } else {
      m_renderState.flags -= vertexStateFlags;

      m_vbosActive = 0;
      m_vbosDirty = 0;
    }

    // This may link or compile a pipeline on demand
    GfxVulkanGraphicsPipelineVariantKey key;
    key.renderState = m_renderStateObject;
    key.targetState = m_renderTargetState;

    auto variant = m_graphicsPipeline->getVariant(key);
    vk.vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, variant.pipeline);

    // Mark all states dirty that were not dynamic in the previous pipeline
    m_dynamicStatesDirty |= variant.dynamicStates ^ m_dynamicStatesActive;
    m_dynamicStatesDirty &= variant.dynamicStates;

    m_dynamicStatesActive = variant.dynamicStates;
  }

  // Update dynamic state as necessary
  GfxVulkanDynamicStates dynamicStateMask = m_dynamicStatesDirty & m_dynamicStatesActive;

  if (dynamicStateMask) {
    m_dynamicStatesDirty -= dynamicStateMask;

    if (dynamicStateMask & GfxVulkanDynamicState::eViewports) {
      vk.vkCmdSetViewportWithCount(m_cmd, m_viewportCount, m_viewports.data());
      vk.vkCmdSetScissorWithCount(m_cmd, m_viewportCount, m_scissors.data());
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eTessellationState) {
      auto tsState = m_renderStateObject->getTsState();
      vk.vkCmdSetPatchControlPointsEXT(m_cmd, tsState.patchControlPoints);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eRasterizerState) {
      auto rsState = m_renderStateObject->getRsState();

      vk.vkCmdSetCullMode(m_cmd, rsState.cullMode);
      vk.vkCmdSetFrontFace(m_cmd, rsState.frontFace);
      vk.vkCmdSetDepthBiasEnable(m_cmd, rsState.depthBiasEnable);
      vk.vkCmdSetDepthBias(m_cmd, rsState.depthBiasConstantFactor,
        rsState.depthBiasSlopeFactor, rsState.depthBiasClamp);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eConservativeRaster) {
      auto rsConservative = m_renderStateObject->getRsConservativeState();
      vk.vkCmdSetConservativeRasterizationModeEXT(m_cmd, rsConservative.conservativeRasterizationMode);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eDepthStencilState) {
      auto dsState = m_renderStateObject->getDsState();

      vk.vkCmdSetDepthTestEnable(m_cmd, dsState.depthTestEnable);      
      vk.vkCmdSetDepthWriteEnable(m_cmd, dsState.depthWriteEnable);      
      vk.vkCmdSetDepthCompareOp(m_cmd, dsState.depthCompareOp);      

      vk.vkCmdSetStencilTestEnable(m_cmd, dsState.stencilTestEnable);      
      vk.vkCmdSetStencilOp(m_cmd, VK_STENCIL_FACE_FRONT_BIT,
        dsState.front.failOp, dsState.front.passOp,
        dsState.front.depthFailOp, dsState.front.compareOp);
      vk.vkCmdSetStencilCompareMask(m_cmd, VK_STENCIL_FACE_FRONT_BIT, dsState.front.compareMask);
      vk.vkCmdSetStencilWriteMask(m_cmd, VK_STENCIL_FACE_FRONT_BIT, dsState.front.writeMask);
      vk.vkCmdSetStencilOp(m_cmd, VK_STENCIL_FACE_BACK_BIT,
        dsState.back.failOp, dsState.back.passOp,
        dsState.back.depthFailOp, dsState.back.compareOp);
      vk.vkCmdSetStencilCompareMask(m_cmd, VK_STENCIL_FACE_BACK_BIT, dsState.back.compareMask);
      vk.vkCmdSetStencilWriteMask(m_cmd, VK_STENCIL_FACE_BACK_BIT, dsState.back.writeMask);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eDepthBoundsState) {
      auto dsState = m_renderStateObject->getDsState();
      vk.vkCmdSetDepthBoundsTestEnable(m_cmd, dsState.depthBoundsTestEnable);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eDepthBounds)
      vk.vkCmdSetDepthBounds(m_cmd, m_depthBoundsMin, m_depthBoundsMax);

    if (dynamicStateMask & GfxVulkanDynamicState::eStencilRef) {
      vk.vkCmdSetStencilReference(m_cmd, VK_STENCIL_FACE_FRONT_BIT, m_stencilRefFront);
      vk.vkCmdSetStencilReference(m_cmd, VK_STENCIL_FACE_BACK_BIT, m_stencilRefBack);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eMultisampleState) {
      auto msState = m_renderStateObject->getMsState(*m_renderTargetState,
        m_graphicsPipeline->hasSampleRateShading());

      vk.vkCmdSetRasterizationSamplesEXT(m_cmd, msState.rasterizationSamples);
      vk.vkCmdSetSampleMaskEXT(m_cmd, msState.rasterizationSamples, msState.pSampleMask);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eAlphaToCoverage) {
      auto msState = m_renderStateObject->getMsState(*m_renderTargetState,
        m_graphicsPipeline->hasSampleRateShading());

      vk.vkCmdSetAlphaToCoverageEnableEXT(m_cmd, msState.alphaToCoverageEnable);
    }

    if (dynamicStateMask & GfxVulkanDynamicState::eBlendConstants)
      vk.vkCmdSetBlendConstants(m_cmd, m_blendConstants.float32);

    if (dynamicStateMask & GfxVulkanDynamicState::eShadingRate) {
      auto srState = m_renderStateObject->getSrState();

      if (!m_device->supportsFragmentShadingRateWithState(*m_renderStateObject)) {
        srState.fragmentSize = VkExtent2D { 1u, 1u };
        srState.combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
        srState.combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
      }

      vk.vkCmdSetFragmentShadingRateKHR(m_cmd, &srState.fragmentSize, srState.combinerOps);
    }
  }

  // Update index buffer as necessary
  if (indexed && (m_flags & GfxVulkanContextFlag::eDirtyIndexBuffer)) {
    m_flags -= GfxVulkanContextFlag::eDirtyIndexBuffer;

    vk.vkCmdBindIndexBuffer(m_cmd,
      m_indexBufferHandle,
      m_indexBufferOffset,
      m_indexBufferFormat);
  }

  // Update vertex buffers as necessary
  if (m_graphicsPipeline->getShaderStages() & GfxShaderStage::eVertex) {
    uint32_t vboMask = m_vbosDirty & m_vbosActive;

    if (vboMask) {
      m_vbosDirty -= vboMask;
      uint32_t offset = 0;

      while (vboMask) {
        uint32_t first = tzcnt(vboMask);
        uint32_t count = tzcnt(~(vboMask >> first));
        uint32_t index = offset + first;

        vk.vkCmdBindVertexBuffers2(m_cmd, index, count,
          &m_vertexBufferHandles[index], &m_vertexBufferOffsets[index],
          &m_vertexBufferSizes[index], nullptr);

        offset += first + count;
        vboMask >>= first + count;
      }
    }
  }

  // Update descriptor sets and push constants as necessary
  auto* pipelineLayout = m_graphicsPipeline->getPipelineLayout();

  if (m_dirtySets & pipelineLayout->getNonemptySetMask())
    updateDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout);

  if (m_flags & GfxVulkanContextFlag::eDirtyConstants)
    updatePushConstants(pipelineLayout);
}


void GfxVulkanContext::updateComputeState(
  const GfxVulkanProcs&               vk) {
  m_barrierBatch.recordCommands(vk, m_cmd);

  if (m_flags & GfxVulkanContextFlag::eDirtyPipeline) {
    m_flags -= GfxVulkanContextFlag::eDirtyPipeline;

    vk.vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_computePipeline->getHandle());
  }

  // Update descriptor sets and push constants as necessary
  auto* pipelineLayout = m_computePipeline->getPipelineLayout();

  if (m_dirtySets & pipelineLayout->getNonemptySetMask())
    updateDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout);

  if (m_flags & GfxVulkanContextFlag::eDirtyConstants)
    updatePushConstants(pipelineLayout);
}


void GfxVulkanContext::updateDescriptorSets(
        VkPipelineBindPoint           bindPoint,
  const GfxVulkanPipelineLayout*      pipelineLayout) {
  auto& vk = m_device->vk();

  std::array<VkDescriptorSet, GfxMaxDescriptorSets> setHandles = { };
  std::array<VkDescriptorSetLayout, GfxMaxDescriptorSets> setLayouts = { };
  std::array<VkDescriptorUpdateTemplate, GfxMaxDescriptorSets> setTemplates = { };

  // Gather involved set layouts and allocate descriptor sets
  uint32_t setMask = m_dirtySets & pipelineLayout->getNonemptySetMask();
  m_dirtySets -= setMask;

  if (unlikely(!setMask))
    return;

  for (uint32_t i = setMask; i; i &= i - 1) {
    uint32_t setIndex = tzcnt(i);

    auto* setLayout = pipelineLayout->getSetLayout(setIndex);

    if (setLayout->isBindless()) {
      // Get bindless set directly from the descriptor array
      setHandles[setIndex] = m_descriptorArrays[setIndex];
    } else {
      setLayouts[setIndex] = setLayout->getSetLayout();
      setTemplates[setIndex] = setLayout->getTemplate();
    }
  }

  allocateDescriptorSets(GfxMaxDescriptorSets,
    setLayouts.data(), setHandles.data());

  // Update and bind the newly allocated descriptor sets
  uint32_t bindFirst = tzcnt(setMask);

  for (uint32_t i = setMask; i; i &= i - 1) {
    uint32_t setBit = i & -i;
    uint32_t setIndex = tzcnt(i);

    if (setTemplates[setIndex]) {
      vk.vkUpdateDescriptorSetWithTemplate(vk.device, setHandles[setIndex],
        setTemplates[setIndex], &m_descriptors[GfxMaxDescriptorsPerSet * setIndex]);
    }

    if (!(setMask & (setBit << 1))) {
      // Bind consecutive dirty descriptor sets in one go to reduce api calls
      vk.vkCmdBindDescriptorSets(m_cmd, bindPoint, pipelineLayout->getLayout(),
        bindFirst, setIndex - bindFirst + 1, &setHandles[bindFirst], 0, nullptr);

      bindFirst = tzcnt(i & (i - 1));
    }
  }
}


void GfxVulkanContext::updatePushConstants(
  const GfxVulkanPipelineLayout*      pipelineLayout) {
  auto& vk = m_device->vk();
  auto info = pipelineLayout->getPushConstantInfo();

  if (info.size) {
    vk.vkCmdPushConstants(m_cmd,
      pipelineLayout->getLayout(),
      info.stageFlags, 0, info.size,
      m_shaderConstants.data());
  }

  m_flags -= GfxVulkanContextFlag::eDirtyConstants;
}


void GfxVulkanContext::computeDirtySets(
  const GfxVulkanPipelineLayout*      oldLayout,
  const GfxVulkanPipelineLayout*      newLayout) {
  uint32_t oldSetCount = oldLayout ? oldLayout->getSetCount() : 0;
  uint32_t newSetCount = newLayout->getSetCount();

  // Mark sets as dirty that only exist in one of the layouts
  uint32_t mask = ((1u << oldSetCount) - 1) ^ ((1u << newSetCount) - 1);

  // If an old layout is specified, find the first set with
  // a different layout and mark all higher sets as dirty.
  for (uint32_t i = 0; i < std::min(oldSetCount, newSetCount); i++) {
    if (oldLayout->getSetLayout(i) != newLayout->getSetLayout(i)) {
      mask |= ~((1u << i) - 1) & ((1u << newSetCount) - 1);
      break;
    }
  }

  m_dirtySets |= mask;
}


void GfxVulkanContext::invalidateState() {
  m_flags |= GfxVulkanContextFlag::eDirtyPipeline
          |  GfxVulkanContextFlag::eDirtyConstants
          |  GfxVulkanContextFlag::eDirtyIndexBuffer;

  m_dirtySets = (2u << (GfxMaxDescriptorSets - 1)) - 1;

  m_vbosDirty = 0;
  m_vbosActive = 0;

  m_dynamicStatesActive = 0;
  m_dynamicStatesDirty = 0;
}


void GfxVulkanContext::resetState() {
  m_renderState = GfxRenderStateData();
  m_renderState.flags = GfxRenderStateFlag::eAll;
  m_renderStateObject = nullptr;

  m_graphicsPipeline = nullptr;
  m_computePipeline = nullptr;

  for (auto& descriptor : m_descriptors)
    descriptor = GfxVulkanDescriptor();

  for (auto& descriptorArray : m_descriptorArrays)
    descriptorArray = VK_NULL_HANDLE;

  m_viewportCount = 1;

  for (uint32_t i = 0; i < GfxMaxViewportCount; i++) {
    m_viewports[i] = VkViewport();
    m_scissors[i] = VkRect2D();
  }

  m_indexBufferHandle = VK_NULL_HANDLE;
  m_indexBufferOffset = 0;
  m_indexBufferFormat = VK_INDEX_TYPE_UINT16;

  for (uint32_t i = 0; i < GfxMaxVertexBindings; i++) {
    m_vertexBufferHandles[i] = VK_NULL_HANDLE;
    m_vertexBufferOffsets[i] = 0;
    m_vertexBufferSizes[i] = 0;
  }

  m_depthBoundsMin = 0.0f;
  m_depthBoundsMax = 1.0f;

  m_stencilRefBack = 0;
  m_stencilRefFront = 0;

  m_blendConstants = VkClearColorValue();

  std::memset(m_shaderConstants.data(), 0, m_shaderConstants.size());

  invalidateState();
}


VkCommandBuffer GfxVulkanContext::allocateCommandBuffer() {
  auto& vk = m_device->vk();

  if (m_commandBufferIndex >= m_commandBuffers.size()) {
    VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkResult vr = vk.vkAllocateCommandBuffers(vk.device, &allocInfo, &commandBuffer);

    if (vr)
      throw VulkanError("Vulkan: Failed to allocate command buffer", vr);

    m_commandBuffers.push_back(commandBuffer);
  }

  // Begin command buffer so we can use it right away
  VkCommandBuffer cmd = m_commandBuffers.at(m_commandBufferIndex++);

  VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VkResult vr = vk.vkBeginCommandBuffer(cmd, &beginInfo);

  if (vr)
    throw VulkanError("Vulkan: Failed to begin command buffer", vr);

  return cmd;
}


void GfxVulkanContext::allocateDescriptorSets(
        uint32_t                      setCount,
  const VkDescriptorSetLayout*        setLayouts,
        VkDescriptorSet*              sets) {
  // Gather non-null descriptor layouts
  std::array<VkDescriptorSetLayout, GfxMaxDescriptorSets> layoutHandles = { };
  uint32_t layoutCount = 0;

  for (uint32_t i = 0; i < setCount; i++) {
    if (setLayouts[i])
      layoutHandles[layoutCount++] = setLayouts[i];
  }

  if (!layoutCount)
    return;

  // Allocate descriptor sets
  std::array<VkDescriptorSet, GfxMaxDescriptorSets> setHandles = { };
  bool success = false;

  if (!m_descriptorPools.empty()) {
    success = m_descriptorPools.back()->allocateSets(
      layoutCount, layoutHandles.data(), setHandles.data());
  }

  if (!success) {
    m_descriptorPools.push_back(m_device->getDescriptorPoolManager().getPool());

    success = m_descriptorPools.back()->allocateSets(
      layoutCount, layoutHandles.data(), setHandles.data());

    if (!success)
      throw VulkanError("Vulkan: Failed to allocate descriptor sets", VK_ERROR_OUT_OF_POOL_MEMORY);
  }

  // Unpack and return allocated sets
  uint32_t setIndex = 0;

  for (uint32_t i = 0; i < setCount; i++) {
    if (setLayouts[i])
      sets[i] = setHandles[setIndex++];
  }
}


small_vector<VkBufferImageCopy2, 16> GfxVulkanContext::getVkBufferImageCopyRegions(
  const GfxImage&                     image,
  const GfxImageSubresource&          imageSubresource,
        Offset3D                      imageOffset,
        Extent3D                      imageExtent,
  const GfxBuffer&                    buffer,
        uint64_t                      bufferOffset,
        Extent2D                      bufferLayout) {
  small_vector<VkBufferImageCopy2, 16> result;

  auto& formatInfo = image->getFormatInfo();

  for (uint32_t i = 0; i < imageSubresource.mipCount; i++) {
    Offset3D mipOffset = imageOffset >> i;
    Extent3D mipExtent = gfxComputeMipExtent(imageExtent, i);

    // There are no subsampled block-compressed formats,
    // so disregard any edge cases in that regard.
    Extent2D srcExtent = gfxComputeMipExtent(bufferLayout, i);
    Extent2D srcBlocks = (srcExtent + formatInfo.blockExtent - 1) >> formatInfo.blockExtentLog2;

    // Realign source extent with block size for Vulkan
    srcExtent = srcBlocks << formatInfo.blockExtentLog2;

    for (auto aspect : imageSubresource.aspects) {
      auto& aspectInfo = formatInfo.getAspectInfo(aspect);

      Extent2D srcPlaneExtent = srcExtent >> aspectInfo.subsampleLog2;
      Extent2D srcPlaneBlocks = srcBlocks >> aspectInfo.subsampleLog2;

      VkBufferImageCopy2 region = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
      region.bufferOffset = bufferOffset;
      region.bufferRowLength = srcPlaneExtent.at<0>();
      region.bufferImageHeight = srcPlaneExtent.at<1>();
      region.imageSubresource = getVkImageSubresourceLayers(imageSubresource.pickAspects(aspect).pickMip(i));
      region.imageOffset = getVkOffset3D(mipOffset >> Offset3D(aspectInfo.subsampleLog2, 0));
      region.imageExtent = getVkExtent3D(mipExtent >> Extent3D(aspectInfo.subsampleLog2, 0));
      result.push_back(region);

      bufferOffset += srcPlaneBlocks.at<0>() * srcPlaneBlocks.at<1>() *
        mipExtent.at<2>() * imageSubresource.layerCount * aspectInfo.elementSize;
    }
  }

  return result;
}


std::pair<VkPipelineStageFlags2, VkAccessFlags2> GfxVulkanContext::getVkStageAccessFromUsage(
        GfxUsageFlags                 gfxUsage,
        GfxShaderStages               gfxStages) {
  VkPipelineStageFlags2 vkStages = 0;
  VkAccessFlags2 vkAccess = 0;

  if (gfxStages) {
    if (gfxStages & GfxShaderStage::eVertex)
      vkStages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;

    if (gfxStages & GfxShaderStage::eTessControl)
      vkStages |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;

    if (gfxStages & GfxShaderStage::eTessEval)
      vkStages |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;

    if (gfxStages & GfxShaderStage::eGeometry)
      vkStages |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;

    if (gfxStages & GfxShaderStage::eTask)
      vkStages |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;

    if (gfxStages & GfxShaderStage::eMesh)
      vkStages |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;

    if (gfxStages & GfxShaderStage::eFragment)
      vkStages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    if (gfxStages & GfxShaderStage::eCompute)
      vkStages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }

  for (auto bit : gfxUsage) {
    switch (bit) {
      case GfxUsage::eTransferSrc:
        vkStages |= VK_PIPELINE_STAGE_2_COPY_BIT;
        vkAccess |= VK_ACCESS_2_TRANSFER_READ_BIT;
        break;

      case GfxUsage::eTransferDst:
        vkStages |= VK_PIPELINE_STAGE_2_COPY_BIT;
        vkAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        break;

      case GfxUsage::eParameterBuffer:
        vkStages |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        vkAccess |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        break;

      case GfxUsage::eIndexBuffer:
        vkStages |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
        vkAccess |= VK_ACCESS_2_INDEX_READ_BIT;
        break;

      case GfxUsage::eVertexBuffer:
        vkStages |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
        vkAccess |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        break;

      case GfxUsage::eConstantBuffer:
        if (gfxStages)
          vkAccess |= VK_ACCESS_2_UNIFORM_READ_BIT;
        break;

      case GfxUsage::eShaderResource:
        if (gfxStages)
          vkAccess |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        break;

      case GfxUsage::eShaderStorage:
        if (gfxStages) {
          vkAccess |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT
                   |  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        }
        break;

      case GfxUsage::eRenderTarget:
        vkStages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                 |  VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                 |  VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        vkAccess |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                 |  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                 |  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                 |  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

      case GfxUsage::eShadingRate:
        vkStages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        vkAccess |= VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        break;

      case GfxUsage::eBvhTraversal:
        vkAccess |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        break;

      case GfxUsage::eBvhBuild:
        vkStages |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        vkAccess |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
                 |  VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        break;

      case GfxUsage::eCpuRead:
        vkStages |= VK_PIPELINE_STAGE_2_HOST_BIT;
        vkAccess |= VK_ACCESS_2_HOST_READ_BIT;
        break;

      case GfxUsage::eDecompressionSrc:
        vkStages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                 |  VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        vkAccess |= VK_ACCESS_2_SHADER_READ_BIT
                 |  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        break;

      case GfxUsage::eDecompressionDst:
        vkStages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        vkAccess |= VK_ACCESS_2_SHADER_READ_BIT
                 |  VK_ACCESS_2_SHADER_WRITE_BIT;
        break;

      default:
      case GfxUsage::ePresent:
        // Nothing to do here
        break;
    }
  }

  return std::make_pair(vkStages, vkAccess);
}

}
