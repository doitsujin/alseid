#include "gfx_common_pipelines.h"

#include <cs_common_hiz.h>

namespace as {

GfxCommonPipelines::GfxCommonPipelines(
        GfxDevice                     device)
: m_device                  (std::move(device))
, m_csGenerateHizImage      (createComputePipeline("cs_common_hiz", cs_common_hiz)) {

}


GfxCommonPipelines::~GfxCommonPipelines() {

}


void GfxCommonPipelines::generateHizImage(
  const GfxContext&                   context,
  const GfxDescriptor&                srcViewDescriptor,
  const GfxDescriptor*                dstViewDescriptors,
  const GfxCommonGenerateHizImageArgs& args) const {
  constexpr uint32_t MaxDstDescriptors = 2u * HizMipsPerPass;

  // Compute number of workgroups based on the lowest written mip level.
  // If we write 5 or more mip levels, each workgroup processes exactly
  // one pixel of the 5th mip counting from the top (index 4), otherwise
  // the calculation is still the same but the shader will skip iterations.
  Extent2D workgroupCount = gfxComputeMipExtent(args.srcExtent, HizMipsPerPass);

  // If we're processing the mip tail in the same dispatch, allocate
  // and bind the scratch buffer, otherwise bind a null descriptor.
  GfxDescriptor scratchDescriptor = { };

  if (args.mipCount > HizMipsPerPass) {
    GfxScratchBuffer scratch = context->allocScratch(
      GfxUsage::eCpuWrite | GfxUsage::eShaderStorage,
      sizeof(uint32_t) * args.layerCount);

    auto counts = reinterpret_cast<uint32_t*>(scratch.map(GfxUsage::eCpuWrite, 0));

    for (uint32_t i = 0; i < args.layerCount; i++)
      counts[i] = workgroupCount.at<0>() * workgroupCount.at<1>();

    scratchDescriptor = scratch.getDescriptor(GfxUsage::eShaderStorage);
  }

  // Bind all resources and record the actual dispatch command.
  context->bindPipeline(m_csGenerateHizImage);

  context->bindDescriptor(0, 0, srcViewDescriptor);
  context->bindDescriptor(0, 1, scratchDescriptor);
  context->bindDescriptors(0, 2, args.mipCount, dstViewDescriptors);

  for (uint32_t i = args.mipCount; i < MaxDstDescriptors; i++)
    context->bindDescriptor(0, 2 + i, GfxDescriptor());

  context->setShaderConstants(0, args);
  context->dispatch(Extent3D(workgroupCount, args.layerCount));
}

}
