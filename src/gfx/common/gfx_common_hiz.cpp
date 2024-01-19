#include "gfx_common_hiz.h"

namespace as {

GfxCommonHizImage::GfxCommonHizImage(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxCommonHizImage::~GfxCommonHizImage() {

}


GfxImageView GfxCommonHizImage::getImageView() const {
  if (!m_image)
    return GfxImageView();

  GfxImageViewDesc viewDesc;
  viewDesc.type = GfxImageViewType::e2D;
  viewDesc.format = m_image->getDesc().format;
  viewDesc.usage = GfxUsage::eShaderResource;
  viewDesc.subresource = m_image->getAvailableSubresources();
  return m_image->createView(viewDesc);
}


void GfxCommonHizImage::generate(
  const GfxContext&                   context,
  const GfxCommonPipelines&           pipelines,
  const GfxImage&                     depthImage) {
  constexpr uint32_t MaxMipsPerPass = GfxCommonPipelines::HizMipsPerPass;
  constexpr uint32_t MaxViewDescriptors = MaxMipsPerPass * 2u;

  std::array<GfxDescriptor, MaxViewDescriptors> dstDescriptors = { };

  // If the source image has been resized, recreate the hi-z image
  GfxImageDesc depthDesc = depthImage->getDesc();
  Extent3D imageExtent = depthImage->computeMipExtent(1);

  if (!m_image || m_image->getDesc().extent != imageExtent) {
    if (m_image)
      context->trackObject(m_image);

    GfxImageDesc imageDesc = { };
    imageDesc.type = GfxImageType::e2D;
    imageDesc.format = GfxFormat::eR16G16f;
    imageDesc.usage = GfxUsage::eShaderResource | GfxUsage::eShaderStorage;
    imageDesc.extent = imageExtent;
    imageDesc.layers = depthDesc.layers;
    imageDesc.mips = gfxComputeMipCount(imageExtent);
    imageDesc.flags = GfxImageFlag::eDedicatedAllocation;
    imageDesc.viewFormatCount = 1;
    imageDesc.viewFormats[0] = GfxFormat::eR16G16ui;

    m_image = m_device->createImage(imageDesc, GfxMemoryType::eAny);
  }

  // Always initialize the image since we override all of it anyway
  context->imageBarrier(m_image, m_image->getAvailableSubresources(),
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxBarrierFlag::eDiscard);

  // Create source image view. Duplicate the red channel into the green
  // channel so that the shader does not need to be aware of whether it
  // is reading the original depth image or a mip of the hi-z buffer.
  GfxImageViewDesc srcViewDesc;
  srcViewDesc.type = GfxImageViewType::e2DArray;
  srcViewDesc.format = depthDesc.format;
  srcViewDesc.usage = GfxUsage::eShaderResource;
  srcViewDesc.subresource = GfxImageSubresource(
    GfxImageAspect::eDepth, 0, 1, 0, depthDesc.layers);
  srcViewDesc.swizzle = GfxColorSwizzle(
    GfxColorChannel::eR, GfxColorChannel::eR,
    GfxColorChannel::e0, GfxColorChannel::e1);

  GfxDescriptor srcDescriptor = depthImage->createView(srcViewDesc)->getDescriptor();

  // Set up common properties for the storage image views
  GfxImageDesc imageDesc = m_image->getDesc();

  GfxImageViewDesc dstViewDesc;
  dstViewDesc.type = GfxImageViewType::e2DArray;
  dstViewDesc.format = GfxFormat::eR16G16ui;
  dstViewDesc.usage = GfxUsage::eShaderStorage;
  dstViewDesc.subresource = m_image->getAvailableSubresources().pickMip(0);

  // The shader can process images with up to 10 mips in one single pass.
  // If the destination image has more mip levels, we need to insert an
  // additional pass which processes the higher mips first.
  uint32_t mipsProcessed = 0u;

  if (imageDesc.mips > MaxViewDescriptors) {
    mipsProcessed = MaxMipsPerPass;

    // Create and bind storage image views
    GfxImageSubresource dstSubresource(GfxImageAspect::eColor,
      0, mipsProcessed, 0, imageDesc.layers);

    for (uint32_t i = 0; i < dstSubresource.mipCount; i++) {
      dstViewDesc.subresource = dstSubresource.pickMip(i);
      dstDescriptors[i] = m_image->createView(dstViewDesc)->getDescriptor();
    }

    // Dispatch first pass
    GfxCommonGenerateHizImageArgs args = { };
    args.srcExtent = depthDesc.extent.get<0, 1>();
    args.mipCount = dstSubresource.mipCount;
    args.layerCount = dstSubresource.layerCount;

    pipelines.generateHizImage(context, srcDescriptor, dstDescriptors.data(), args);

    // Transition the mip levels we just wrote so that we can read them as
    // a shader resource, and bind a source view of the smallest mip level.
    context->imageBarrier(m_image, dstSubresource,
      GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
      GfxUsage::eShaderResource, GfxShaderStage::eCompute, 0);

    GfxImageViewDesc srcViewDesc;
    srcViewDesc.type = GfxImageViewType::e2DArray;
    srcViewDesc.format = imageDesc.format;
    srcViewDesc.usage = GfxUsage::eShaderResource;
    srcViewDesc.subresource = dstSubresource.pickMip(dstSubresource.mipCount - 1u);

    srcDescriptor = m_image->createView(srcViewDesc)->getDescriptor();
  }

  // Compute set of mip levels to process in the final pass.
  GfxImageSubresource dstSubresource(GfxImageAspect::eColor,
    mipsProcessed, imageDesc.mips - mipsProcessed, 0, imageDesc.layers);

  // Bind destination mip levels for writing
  for (uint32_t i = 0; i < dstSubresource.mipCount; i++) {
    dstViewDesc.subresource = dstSubresource.pickMip(i);
    dstDescriptors[i] = m_image->createView(dstViewDesc)->getDescriptor();
  }

  // Dispatch mip tail pass
  GfxCommonGenerateHizImageArgs args = { };
  args.srcExtent = depthDesc.extent.get<0, 1>();
  args.mipCount = dstSubresource.mipCount;
  args.layerCount = dstSubresource.layerCount;

  if (dstSubresource.mipIndex)
    args.srcExtent = m_image->computeMipExtent(dstSubresource.mipIndex - 1u).get<0, 1>();

  pipelines.generateHizImage(context, srcDescriptor, dstDescriptors.data(), args);

  // Transition remaining mip levels to shader read state
  context->imageBarrier(m_image, dstSubresource,
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderResource, GfxShaderStage::eCompute, 0);
}

}
