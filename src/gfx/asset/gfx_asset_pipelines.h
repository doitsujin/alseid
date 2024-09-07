#pragma once

#include "gfx_asset_group.h"

#include "../gfx_device.h"

namespace as {

/**
 * \brief Shader arguments for generating the Hi-Z image
 */
struct GfxAssetListUpdateArgs {
  uint64_t dstAssetListVa;
  uint64_t srcAssetDataVa;
  uint32_t dstDwordCount;
  uint32_t srcDwordCount;
  uint32_t handle;
  uint32_t frameId;
};


/**
 * \brief Asset-related shader pipelines
 */
class GfxAssetPipelines {

public:

  /**
   * \brief Initializes pipeline object
   * \param [in] device Device object
   */
  explicit GfxAssetPipelines(
          GfxDevice                     device);

  ~GfxAssetPipelines();

  GfxAssetPipelines             (const GfxAssetPipelines&) = delete;
  GfxAssetPipelines& operator = (const GfxAssetPipelines&) = delete;

  /**
   * \brief Updates an asset list
   *
   * Copies data into the asset list buffer and updates
   * the header as necessary.
   * \param [in] context Context
   * \param [in] args Shader arguments
   * \param [in] initialize \c true if the asset list has
   *    been newly created and needs to be initialized.
   */
  void updateAssetList(
    const GfxContext&                   context,
    const GfxAssetListUpdateArgs&       args,
          bool                          initialize) const;

private:

  GfxDevice           m_device;

  GfxComputePipeline  m_csUpdateAssetList;

  template<size_t CsSize>
  GfxComputePipeline createComputePipeline(
    const char*                         name,
    const uint32_t                      (&cs)[CsSize]) const {
    GfxComputePipelineDesc pipelineDesc = { };
    pipelineDesc.debugName = name;
    pipelineDesc.compute = GfxShader::createBuiltIn(
      GfxShaderFormat::eVulkanSpirv, CsSize * sizeof(uint32_t), cs);

    return m_device->createComputePipeline(pipelineDesc);
  }

};

}
