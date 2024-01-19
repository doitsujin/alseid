#pragma once

#include "../gfx_device.h"

namespace as {

/**
 * \brief Shader arguments for generating the Hi-Z image
 */
struct GfxCommonGenerateHizImageArgs {
  /** Source image size. */
  Extent2D srcExtent;
  /** Number of mip levels to process in a dispatch. */
  uint32_t mipCount;
  /** Number of array layers to process in a dispatch. */
  uint32_t layerCount;
};

static_assert(sizeof(GfxCommonGenerateHizImageArgs) == 16u);


/**
 * \brief Common shader pipelines
 */
class GfxCommonPipelines {

public:

  constexpr static uint32_t HizMipsPerPass = 6u;

  /**
   * \brief Initializes pipeline object
   * \param [in] device Device object
   */
  explicit GfxCommonPipelines(
          GfxDevice                     device);

  ~GfxCommonPipelines();

  GfxCommonPipelines             (const GfxCommonPipelines&) = delete;
  GfxCommonPipelines& operator = (const GfxCommonPipelines&) = delete;

  /**
   * \brief Dispatches shader to generate a Hi-Z image
   *
   * A single dispatch can generate up to 10 mip levels in one go.
   * If more are required, a separate dispatch must process the
   * larger mip levels first, up to 5 mips at once.
   * \param [in] context Context object
   * \param [in] srcViewDescriptor Shader resource descriptor of
   *    the depth image, or of an already processed hi-z mip level.
   * \param [in] dstViewDescriptors Storage image descriptors for
   *    each of the mip levels to generate. The descriptor count
   *    is determined by the mip level count in the argument array.
   */
  void generateHizImage(
    const GfxContext&                   context,
    const GfxDescriptor&                srcViewDescriptor,
    const GfxDescriptor*                dstViewDescriptors,
    const GfxCommonGenerateHizImageArgs& args) const;

private:

  GfxDevice           m_device;

  GfxComputePipeline  m_csGenerateHizImage;

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
