#pragma once

#include "gfx_vulkan_loader.h"

namespace as {

class GfxVulkanDevice;

/**
 * \brief Shader arguments
 */
struct GfxVulkanGDeflateArgs {
  /* GPU address of the input buffer */
  uint64_t srcVa;
  /* GPU address of the output buffer */
  uint64_t dstVa;
};


/**
 * \brief GDeflate pipeline
 *
 * Creates and manages the a compute
 * pipeline for GPU decompression.
 */
class GfxVulkanGDeflatePipeline {

public:

  GfxVulkanGDeflatePipeline(
          GfxVulkanDevice&              device);

  ~GfxVulkanGDeflatePipeline();

  VkPipeline getPipeline() const {
    return m_pipeline;
  }

  VkPipelineLayout getLayout() const {
    return m_pipelineLayout;
  }

private:

  GfxVulkanDevice&  m_device;

  VkPipeline        m_pipeline        = VK_NULL_HANDLE;
  VkPipelineLayout  m_pipelineLayout  = VK_NULL_HANDLE;

};

}
