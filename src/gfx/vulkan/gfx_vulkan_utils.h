#pragma once

#include "../gfx.h"
#include "../gfx_image.h"
#include "../gfx_sampler.h"
#include "../gfx_shader.h"
#include "../gfx_utils.h"

#include "gfx_vulkan_include.h"
#include "gfx_vulkan_image.h"

namespace as {

/**
 * \brief Converts common aspect flags to Vulkan flags
 *
 * \param [in] aspects Common image aspect flags
 * \returns Corresponding Vulkan image aspects
 */
inline VkImageAspectFlags getVkImageAspects(
        GfxImageAspects                 aspects) {
  return (VkImageAspectFlags(aspects & (GfxImageAspect::eColor | GfxImageAspect::eDepth | GfxImageAspect::eStencil)))
       | (VkImageAspectFlags(aspects & (GfxImageAspect::ePlane0 | GfxImageAspect::ePlane1 | GfxImageAspect::ePlane2)) << 1);
}


/**
 * \brief Converts common offset to Vulkan offset
 *
 * \param [in] offset Common offset
 * \returns Vulkan offset
 */
inline VkOffset3D getVkOffset3D(Offset3D offset) {
  return VkOffset3D {
    offset.at<0>(),
    offset.at<1>(),
    offset.at<2>() };
}


/**
 * \brief Converts common extent to Vulkan extent
 *
 * \param [in] extent Common extent
 * \returns Vulkan extent
 */
inline VkExtent3D getVkExtent3D(Extent3D extent) {
  return VkExtent3D {
    extent.at<0>(),
    extent.at<1>(),
    extent.at<2>() };
}


/**
 * \brief Converts common subresource set to Vulkan subresource range
 *
 * \param [in] subresource Subresource set
 * \returns Vulkan subresource range
 */
inline VkImageSubresourceRange getVkImageSubresourceRange(
  const GfxImageSubresource&            subresource) {
  VkImageSubresourceRange result;
  result.aspectMask = getVkImageAspects(subresource.aspects);
  result.baseMipLevel = subresource.mipIndex;
  result.levelCount = subresource.mipCount;
  result.baseArrayLayer = subresource.layerIndex;
  result.layerCount = subresource.layerCount;
  return result;
}


/**
 * \brief Converts common subresource set to Vulkan subresource layers
 *
 * \param [in] subresource Subresource set
 * \returns Vulkan subresource layers
 */
inline VkImageSubresourceLayers getVkImageSubresourceLayers(
  const GfxImageSubresource&            subresource) {
  VkImageSubresourceLayers result;
  result.aspectMask = getVkImageAspects(subresource.aspects);
  result.mipLevel = subresource.mipIndex;
  result.baseArrayLayer = subresource.layerIndex;
  result.layerCount = subresource.layerCount;
  return result;
}


/**
 * \brief Converts common subresource set to Vulkan subresource
 *
 * \param [in] subresource Subresource set
 * \returns Vulkan subresource
 */
inline VkImageSubresource getVkImageSubresource(
  const GfxImageSubresource&            subresource) {
  VkImageSubresource result;
  result.aspectMask = getVkImageAspects(subresource.aspects);
  result.mipLevel = subresource.mipIndex;
  result.arrayLayer = subresource.layerIndex;
  return result;
}


/**
 * \brief Converts common image type to Vulkan image type
 *
 * \param [in] imageType Common image type
 * \returns Vulkan image type
 */
inline VkImageType getVkImageType(
        GfxImageType                    imageType) {
  switch (imageType) {
    case GfxImageType::e1D: return VK_IMAGE_TYPE_1D;
    case GfxImageType::e2D: return VK_IMAGE_TYPE_2D;
    case GfxImageType::e3D: return VK_IMAGE_TYPE_3D;
  }

  return VkImageType(0);
}


/**
 * \brief Converts common image view type to Vulkan view type
 *
 * \param [in] viewType Common view type
 * \returns Vulkan view type
 */
inline VkImageViewType getVkImageViewType(
        GfxImageViewType                viewType) {
  switch (viewType) {
    case GfxImageViewType::e1D:         return VK_IMAGE_VIEW_TYPE_1D;
    case GfxImageViewType::e2D:         return VK_IMAGE_VIEW_TYPE_2D;
    case GfxImageViewType::e3D:         return VK_IMAGE_VIEW_TYPE_3D;
    case GfxImageViewType::eCube:       return VK_IMAGE_VIEW_TYPE_CUBE;
    case GfxImageViewType::e1DArray:    return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case GfxImageViewType::e2DArray:    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case GfxImageViewType::eCubeArray:  return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
  }

  return VkImageViewType(0);
}


/**
 * \brief Converts common usage flags to image usage
 *
 * \param [in] usage Usage bits
 * \returns Vulkan image usage flags
 */
inline VkImageUsageFlags getVkImageUsage(
        GfxFormat                       format,
        GfxUsageFlags                   usage) {
  VkImageUsageFlags result = 0;

  if (usage & GfxUsage::eTransferSrc)
    result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if (usage & GfxUsage::eTransferDst)
    result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (usage & GfxUsage::eShaderResource)
    result |= VK_IMAGE_USAGE_SAMPLED_BIT;

  if (usage & GfxUsage::eShaderStorage)
    result |= VK_IMAGE_USAGE_STORAGE_BIT;

  if (usage & GfxUsage::eRenderTarget) {
    const auto& formatInfo = Gfx::getFormatInfo(format);

    result |= (formatInfo.aspects & (GfxImageAspect::eDepth | GfxImageAspect::eStencil))
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  if (usage & GfxUsage::eShadingRate)
    result |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

  return result;
}


/**
 * \brief Converts common usage flags to buffer usage
 *
 * \param [in] usage Usage bits
 * \returns Vulkan buffer usage flags
 */
inline VkBufferUsageFlags getVkBufferUsage(
        GfxUsageFlags                   usage) {
  VkBufferUsageFlags result = 0;

  if (usage & GfxUsage::eTransferSrc)
    result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  if (usage & GfxUsage::eTransferDst)
    result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if (usage & GfxUsage::eParameterBuffer)
    result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

  if (usage & GfxUsage::eIndexBuffer)
    result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  if (usage & GfxUsage::eVertexBuffer)
    result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if (usage & GfxUsage::eConstantBuffer) {
    result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
           |  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (usage & GfxUsage::eShaderResource) {
    result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
           |  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
           |  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (usage & GfxUsage::eShaderStorage) {
    result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
           |  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
           |  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (usage & (GfxUsage::eBvhTraversal | GfxUsage::eBvhBuild))
    result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

  return result;
}


/**
 * \brief Gets suitable image layout for usage flags
 *
 * \param [in] image The image
 * \param [in] gfxUsage Usage flags
 * \returns Compatible Vulkan image layout
 */
inline VkImageLayout getVkImageLayoutFromUsage(
  const GfxVulkanImage&                 image,
        GfxUsageFlags                   gfxUsage) {
  switch (uint32_t(gfxUsage)) {
    case 0:
      return VK_IMAGE_LAYOUT_UNDEFINED;

    case uint32_t(GfxUsage::eTransferSrc):
      return image.pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    case uint32_t(GfxUsage::eTransferDst):
      return image.pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    case uint32_t(GfxUsage::eShaderResource):
    case uint32_t(GfxUsage::eShaderResource) | uint32_t(GfxUsage::eRenderTarget):
      return image.pickLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    case uint32_t(GfxUsage::eShaderStorage):
      return image.pickLayout(VK_IMAGE_LAYOUT_GENERAL);

    case uint32_t(GfxUsage::eRenderTarget):
      return image.pickLayout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);

    case uint32_t(GfxUsage::eShadingRate):
      return image.pickLayout(VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR);

    case uint32_t(GfxUsage::ePresent):
      return image.isSwapChainImage()
        ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        : image.pickLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    default:
      return VK_IMAGE_LAYOUT_GENERAL;
  }
}


/**
 * \brief Computes appropriate resolve mode for format
 *
 * \param [in] format Format
 * \param [in] aspect Aspect to query
 * \returns Resolve mode
 */
inline VkResolveModeFlagBits getVkResolveMode(
        GfxFormat                       format,
        GfxImageAspect                  aspect) {
  // Depth and stencil can't be resolved with AVERAGE.
  // We also do not support min/max resolve modes.
  if (aspect == GfxImageAspect::eDepth || aspect == GfxImageAspect::eStencil)
    return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

  // Resolve color images based on their format: Float images
  // with AVERAGE, integer images with SAMPLE_ZERO.
  const auto& formatInfo = Gfx::getFormatInfo(format);

  return (formatInfo.getAspectInfo(aspect).type == GfxFormatType::eFloat)
    ? VK_RESOLVE_MODE_AVERAGE_BIT
    : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
}


/**
 * \brief Converts common attachment op to Vulkan attachment load op
 *
 * \param [in] op Common attachment operation
 * \returns Vulkan attachment load op
 */
inline VkAttachmentLoadOp getVkAttachmentLoadOp(
        GfxRenderTargetOp               op) {
  switch (op) {
    case GfxRenderTargetOp::eLoad:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case GfxRenderTargetOp::eDiscard:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case GfxRenderTargetOp::eClear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
  }

  return VK_ATTACHMENT_LOAD_OP_LOAD;
}


/**
 * \brief Converts color value to Vulkan clear value
 *
 * \param [in] value Color value
 * \returns Vulkan clear value
 */
inline VkClearValue getVkClearValue(
  const GfxColorValue&                  value) {
  VkClearValue result = { };
  std::memcpy(&result.color, &value, sizeof(value));
  return result;
}


/**
 * \brief Converts depth-stencil value to Vulkan clear value
 *
 * \param [in] value Depth-stencil value
 * \returns Vulkan clear value
 */
inline VkClearValue getVkClearValue(
  const GfxDepthStencilValue&           value) {
  VkClearValue result = { };
  result.depthStencil.depth = value.d;
  result.depthStencil.stencil = value.s;
  return result;
}


/**
 * \brief Converts binding type to Vulkan descriptor type
 *
 * \param [in] type Binding type
 * \returns Descriptor type
 */
inline VkDescriptorType getVkDescriptorType(
        GfxShaderBindingType            type) {
  switch (type) {
    case GfxShaderBindingType::eSampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;

    case GfxShaderBindingType::eConstantBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    case GfxShaderBindingType::eResourceBuffer:
    case GfxShaderBindingType::eStorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    case GfxShaderBindingType::eResourceBufferView:
      return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

    case GfxShaderBindingType::eStorageBufferView:
      return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

    case GfxShaderBindingType::eResourceImageView:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    case GfxShaderBindingType::eStorageImageView:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    case GfxShaderBindingType::eBvh:
      return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    case GfxShaderBindingType::eUnknown:
      return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }

  // Should be unreachable
  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}


/**
 * \brief Converts shader stage to Vulkan shader stage
 *
 * \param [in] stage Common shader stage
 * \returns Shader stage
 */
inline VkShaderStageFlagBits getVkShaderStage(
        GfxShaderStage                  stage) {
  switch (stage) {
    case GfxShaderStage::eVertex:
      return VK_SHADER_STAGE_VERTEX_BIT;

    case GfxShaderStage::eTessControl:
      return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

    case GfxShaderStage::eTessEval:
      return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    case GfxShaderStage::eGeometry:
      return VK_SHADER_STAGE_GEOMETRY_BIT;

    case GfxShaderStage::eTask:
      return VK_SHADER_STAGE_TASK_BIT_EXT;

    case GfxShaderStage::eMesh:
      return VK_SHADER_STAGE_MESH_BIT_EXT;

    case GfxShaderStage::eFragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;

    case GfxShaderStage::eCompute:
      return VK_SHADER_STAGE_COMPUTE_BIT;

    // Ignore compound stages
    case GfxShaderStage::eMeshTask:
    case GfxShaderStage::ePreRaster:
    case GfxShaderStage::eFlagEnum:
      return VkShaderStageFlagBits(0);
  }

  // Should be unreachable
  return VkShaderStageFlagBits(0);
}


/**
 * \brief Converts vertex input rate to Vulkan input rate
 *
 * \param [in] inputRate Common input rate
 * \returns Vulkan input rate
 */
inline VkVertexInputRate getVkInputRate(
        GfxInputRate                  inputRate) {
  switch (inputRate) {
    case GfxInputRate::ePerVertex:
      return VK_VERTEX_INPUT_RATE_VERTEX;

    case GfxInputRate::ePerInstance:
      return VK_VERTEX_INPUT_RATE_INSTANCE;
  }

  // Should be unreachable
  return VK_VERTEX_INPUT_RATE_MAX_ENUM;
}


/**
 * \brief Converts primitive topology to Vulkan topology
 *
 * \param [in] topology Primitive topology
 * \returns Vulkan primitive topology
 */
inline VkPrimitiveTopology getVkPrimitiveTopology(
        GfxPrimitiveType              topology) {
  switch (topology) {
    case GfxPrimitiveType::ePointList:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    case GfxPrimitiveType::eLineList:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    case GfxPrimitiveType::eLineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;

    case GfxPrimitiveType::eTriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    case GfxPrimitiveType::eTriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    case GfxPrimitiveType::ePatchList:
      return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  }

  // Should be unreachable
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}


/**
 * \brief Converts logic op to Vulkan topology
 *
 * \param [in] op Logic op
 * \returns Vulkan logic op
 */
inline VkLogicOp getVkLogicOp(GfxLogicOp op) {
  switch (op) {
    case GfxLogicOp::eZero:
      return VK_LOGIC_OP_CLEAR;

    case GfxLogicOp::eSrcAndDst:
      return VK_LOGIC_OP_AND;

    case GfxLogicOp::eSrcAndInvDst:
      return VK_LOGIC_OP_AND_REVERSE;

    case GfxLogicOp::eSrc:
      return VK_LOGIC_OP_COPY;

    case GfxLogicOp::eInvSrcAndDst:
      return VK_LOGIC_OP_AND_INVERTED;

    case GfxLogicOp::eDst:
      return VK_LOGIC_OP_NO_OP;

    case GfxLogicOp::eSrcXorDst:
      return VK_LOGIC_OP_XOR;

    case GfxLogicOp::eSrcOrDst:
      return VK_LOGIC_OP_OR;

    case GfxLogicOp::eSrcNorDst:
      return VK_LOGIC_OP_NOR;

    case GfxLogicOp::eSrcEqualDst:
      return VK_LOGIC_OP_EQUIVALENT;

    case GfxLogicOp::eInvDst:
      return VK_LOGIC_OP_INVERT;

    case GfxLogicOp::eSrcOrInvDst:
      return VK_LOGIC_OP_OR_REVERSE;

    case GfxLogicOp::eInvSrc:
      return VK_LOGIC_OP_COPY_INVERTED;

    case GfxLogicOp::eInvSrcOrDst:
      return VK_LOGIC_OP_OR_INVERTED;

    case GfxLogicOp::eSrcNandDst:
      return VK_LOGIC_OP_NAND;

    case GfxLogicOp::eOne:
      return VK_LOGIC_OP_SET;
  }

  // Should be unreachable
  return VK_LOGIC_OP_COPY;
}


/**
 * \brief Converts color write mask to Vulkan component flags
 *
 * \param [in] components Component mask
 * \returns Vulkan component mask
 */
inline VkColorComponentFlags getVkComponentFlags(GfxColorComponents components) {
  // The internal enum is bit-compatible with the Vulkan enum
  return VkColorComponentFlags(uint32_t(components));
}


/**
 * \brief Converts blend factor to Vulkan blend factor
 *
 * \param [in] blendFactor Blend factor
 * \returns Vulkan blend factor
 */
inline VkBlendFactor getVkBlendFactor(GfxBlendFactor blendFactor) {
  switch (blendFactor) {
    case GfxBlendFactor::eZero:
      return VK_BLEND_FACTOR_ZERO;

    case GfxBlendFactor::eOne:
      return VK_BLEND_FACTOR_ONE;

    case GfxBlendFactor::eSrcColor:
      return VK_BLEND_FACTOR_SRC_COLOR;

    case GfxBlendFactor::eOneMinusSrcColor:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;

    case GfxBlendFactor::eDstColor:
      return VK_BLEND_FACTOR_DST_COLOR;

    case GfxBlendFactor::eOneMinusDstColor:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

    case GfxBlendFactor::eSrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;

    case GfxBlendFactor::eOneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    case GfxBlendFactor::eDstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;

    case GfxBlendFactor::eOneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;

    case GfxBlendFactor::eConstantColor:
      return VK_BLEND_FACTOR_CONSTANT_COLOR;

    case GfxBlendFactor::eOneMinusConstantColor:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;

    case GfxBlendFactor::eConstantAlpha:
      return VK_BLEND_FACTOR_CONSTANT_ALPHA;

    case GfxBlendFactor::eOneMinusConstantAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

    case GfxBlendFactor::eSrcAlphaSaturate:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;

    case GfxBlendFactor::eSrc1Color:
      return VK_BLEND_FACTOR_SRC1_COLOR;

    case GfxBlendFactor::eOneMinusSrc1Color:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;

    case GfxBlendFactor::eSrc1Alpha:
      return VK_BLEND_FACTOR_SRC1_ALPHA;

    case GfxBlendFactor::eOneMinusSrc1Alpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  }

  return VK_BLEND_FACTOR_ZERO;
}


/**
 * \brief Converts blend op to Vulkan blend op
 *
 * \param [in] blendOp Blend op
 * \returns Vulkan blend op
 */
inline VkBlendOp getVkBlendOp(GfxBlendOp blendOp) {
  switch (blendOp) {
    case GfxBlendOp::eAdd:
      return VK_BLEND_OP_ADD;

    case GfxBlendOp::eSubtract:
      return VK_BLEND_OP_SUBTRACT;

    case GfxBlendOp::eReverseSubtract:
      return VK_BLEND_OP_REVERSE_SUBTRACT;

    case GfxBlendOp::eMin:
      return VK_BLEND_OP_MIN;

    case GfxBlendOp::eMax:
      return VK_BLEND_OP_MAX;
  }

  // Should be unreachable
  return VK_BLEND_OP_ADD;
}


/**
 * \brief Converts compare op to Vulkan compare op
 *
 * \param [in] compareOp Compare op
 * \returns Vulkan compare op
 */
inline VkCompareOp getVkCompareOp(GfxCompareOp compareOp) {
  switch (compareOp) {
    case GfxCompareOp::eNever:
      return VK_COMPARE_OP_NEVER;

    case GfxCompareOp::eLess:
      return VK_COMPARE_OP_LESS;

    case GfxCompareOp::eEqual:
      return VK_COMPARE_OP_EQUAL;

    case GfxCompareOp::eLessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;

    case GfxCompareOp::eGreater:
      return VK_COMPARE_OP_GREATER;

    case GfxCompareOp::eNotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;

    case GfxCompareOp::eGreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;

    case GfxCompareOp::eAlways:
      return VK_COMPARE_OP_ALWAYS;
  }

  // Should be unreachable
  return VK_COMPARE_OP_ALWAYS;
}


/**
 * \brief Converts stencil op to Vulkan stencil op
 *
 * \param [in] stencilOp Stencil op
 * \returns Vulkan stencil op
 */
inline VkStencilOp getVkStencilOp(GfxStencilOp stencilOp) {
  switch (stencilOp) {
    case GfxStencilOp::eKeep:
      return VK_STENCIL_OP_KEEP;

    case GfxStencilOp::eZero:
      return VK_STENCIL_OP_ZERO;

    case GfxStencilOp::eSet:
      return VK_STENCIL_OP_REPLACE;

    case GfxStencilOp::eIncClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;

    case GfxStencilOp::eDecClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;

    case GfxStencilOp::eInvert:
      return VK_STENCIL_OP_INVERT;

    case GfxStencilOp::eIncWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;

    case GfxStencilOp::eDecWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }

  // Should be unreachable
  return VK_STENCIL_OP_KEEP;
}


/**
 * \brief Converts cull mode to Vulkan cull mode flags
 *
 * \param [in] cullMode Cull mode
 * \returns Vulkan cull mode flags
 */
inline VkCullModeFlags getVkCullMode(GfxCullMode cullMode) {
  switch (cullMode) {
    case GfxCullMode::eNone:
      return VK_CULL_MODE_NONE;
    case GfxCullMode::eBack:
      return VK_CULL_MODE_BACK_BIT;
    case GfxCullMode::eFront:
      return VK_CULL_MODE_FRONT_BIT;
  }

  return VK_CULL_MODE_NONE;
}


/**
 * \brief Converts front face to Vulkan front face
 *
 * \param [in] frontFace Front face
 * \returns Vulkan front face
 */
inline VkFrontFace getVkFrontFace(GfxFrontFace frontFace) {
  return frontFace == GfxFrontFace::eCcw
    ? VK_FRONT_FACE_COUNTER_CLOCKWISE
    : VK_FRONT_FACE_CLOCKWISE;
}


/**
 * \brief Converts a viewport to a Vulkan viewport and scissor
 *
 * \param [in] viewport Viewport
 * \returns Viewport and scissor pair
 */
inline std::pair<VkViewport, VkRect2D> getVkViewportAndScissor(const GfxViewport& viewport) {
  // The front-end assumes D3D style viewports, so flip the Y coordinates
  std::pair<VkViewport, VkRect2D> result;
  result.first.x = viewport.offset.at<0>();
  result.first.y = viewport.offset.at<1>() + viewport.extent.at<1>();
  result.first.width = viewport.extent.at<0>();
  result.first.height = -viewport.extent.at<1>();
  result.first.minDepth = viewport.minDepth;
  result.first.maxDepth = viewport.maxDepth;

  result.second.offset.x = viewport.scissor.offset.at<0>();
  result.second.offset.y = viewport.scissor.offset.at<1>();
  result.second.extent.width = viewport.scissor.extent.at<0>();
  result.second.extent.height = viewport.scissor.extent.at<1>();
  return result;
}


/**
 * \brief Converts format to Vulkan index type
 *
 * \param [in] format Format
 * \returns Index type
 */
inline VkIndexType getVkIndexType(GfxFormat format) {
  switch (format) {
    case GfxFormat::eUnknown:
      return VK_INDEX_TYPE_NONE_KHR;

    case GfxFormat::eR16ui:
      return VK_INDEX_TYPE_UINT16;

    case GfxFormat::eR32ui:
      return VK_INDEX_TYPE_UINT32;

    default:
      return VK_INDEX_TYPE_MAX_ENUM;
  }
}


/**
 * \brief Converts filter to Vulkan filter
 *
 * \param [in] filter Filter
 * \returns Vulkan filter
 */
inline VkFilter getVkFilter(GfxFilter filter) {
  switch (filter) {
    case GfxFilter::eNearest:
      return VK_FILTER_NEAREST;

    case GfxFilter::eLinear:
      return VK_FILTER_LINEAR;
  }

  // Should be unreachable
  return VK_FILTER_MAX_ENUM;
}


/**
 * \brief Converts mip filter to Vulkan mipmap mode
 *
 * \param [in] filter Mip filter
 * \returns Vulkan mipmap mode
 */
inline VkSamplerMipmapMode getVkMipmapMode(GfxMipFilter filter) {
  switch (filter) {
    case GfxMipFilter::eNearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case GfxMipFilter::eLinear:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }

  // Should be unreachable
  return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
}


/**
 * \brief Converts address mode to Vulkan sampler address mode
 *
 * \param [in] mode Address mode
 * \returns Vulkan sampler address mode
 */
inline VkSamplerAddressMode getVkAddressMode(GfxAddressMode mode) {
  switch (mode) {
    case GfxAddressMode::eRepeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;

    case GfxAddressMode::eMirror:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    case GfxAddressMode::eClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    case GfxAddressMode::eClampToBorder:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

    case GfxAddressMode::eMirrorClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  }

  // Should be unreachable
  return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
}


/**
 * \brief Converts border color to Vulkan border color
 *
 * \param [in] borderColor Border color
 * \returns Border color
 */
inline VkBorderColor getVkBorderColor(GfxBorderColor borderColor) {
  switch (borderColor) {
    case GfxBorderColor::eFloatTransparent:
      return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    case GfxBorderColor::eFloatBlack:
      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    case GfxBorderColor::eFloatWhite:
      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    case GfxBorderColor::eIntTransparent:
      return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;

    case GfxBorderColor::eIntBlack:
      return VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    case GfxBorderColor::eIntWhite:
      return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
  }

  // Should be unreachable
  return VK_BORDER_COLOR_MAX_ENUM;
}


/**
 * \brief Creates shader from built-in binary
 *
 * \param [in] size Code size, in bytes
 * \param [in] code Shader code
 * \returns Shader object
 */
GfxShader createVkBuiltInShader(size_t size, const void* code);


/**
 * \brief Creates shader from built-in binary
 *
 * \param [in] binary Shader binary
 * \returns Shader object
 */
template<size_t N>
GfxShader createVkBuiltInShader(const uint32_t (&binary)[N]) {
  return createVkBuiltInShader(N * sizeof(uint32_t), binary);
}

}
