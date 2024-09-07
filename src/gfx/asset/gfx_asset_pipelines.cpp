#include "gfx_asset_pipelines.h"

#include <cs_asset_group_update.h>

namespace as {

GfxAssetPipelines::GfxAssetPipelines(
        GfxDevice                     device)
: m_device(std::move(device))
, m_csUpdateAssetList(createComputePipeline("cs_asset_group_update", cs_asset_group_update)) {

}


GfxAssetPipelines::~GfxAssetPipelines() {

}


void GfxAssetPipelines::updateAssetList(
  const GfxContext&                   context,
  const GfxAssetListUpdateArgs&       args,
        bool                          initialize) const {
  GfxAssetListUpdateArgs shaderArgs = args;

  if (initialize)
    shaderArgs.handle |= 1u << 31;

  // The shader copies four dwords per thread
  uint32_t threadCount = (args.dstDwordCount + 3u) / 4u;

  context->bindPipeline(m_csUpdateAssetList);
  context->setShaderConstants(0, shaderArgs);
  context->dispatch(gfxComputeWorkgroupCount(
    Extent3D(threadCount, 1u, 1u),
    m_csUpdateAssetList->getWorkgroupSize()));
}

}
