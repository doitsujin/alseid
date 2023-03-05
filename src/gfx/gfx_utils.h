#pragma once

#include <cmath>

#include "../util/util_math.h"
#include "../util/util_types.h"

namespace as {

/**
 * \brief Computes extent of a given mip level
 *
 * \param [in] imageExtent Extent of the base mip level
 * \param [in] mipLevel Mip level
 * \returns Mip level extent
 */
inline Extent3D gfxComputeMipExtent(
        Extent3D                        imageExtent,
        uint32_t                        mipLevel) {
  return Extent3D(
    std::max(imageExtent.at<0>() >> mipLevel, 1u),
    std::max(imageExtent.at<1>() >> mipLevel, 1u),
    std::max(imageExtent.at<2>() >> mipLevel, 1u));
}


/**
 * \brief Computes extent of a given mip level
 *
 * This version operates on 2D vectors instead of 3D.
 * \param [in] imageExtent Extent of the base mip level
 * \param [in] mipLevel Mip level
 * \returns Mip level extent
 */
inline Extent2D gfxComputeMipExtent(
        Extent2D                        imageExtent,
        uint32_t                        mipLevel) {
  return Extent2D(
    std::max(imageExtent.at<0>() >> mipLevel, 1u),
    std::max(imageExtent.at<1>() >> mipLevel, 1u));
}


/**
 * \brief Computes workgroup count for a given dispatch
 *
 * Utility function to compute the number of workgroups
 * required for the given thread count.
 * \param [in] threadCount Number of threads to dispatch
 * \param [in] workgroupSize Number of threads per workgroup
 * \returns Number of workgroups to dispatch
 */
inline Extent3D gfxComputeWorkgroupCount(
        Extent3D                        threadCount,
        Extent3D                        workgroupSize) {
  return (threadCount + workgroupSize - 1) / workgroupSize;
}


/**
 * \brief Encodes shading rate tile value
 *
 * \param [in] extent Fragment size. \e Must be
 *    either 1, 2 or 4 in either dimension.
 * \returns Encoded value
 */
inline uint8_t gfxEncodeShadingRate(
        Extent2D                        extent) {
  return (findmsb(extent.at<1>()))
       | (findmsb(extent.at<0>()) << 2);
}


/**
 * \brief Computes shading rate image size
 *
 * \param [in] extent Render target size in pixels
 * \param [in] tileSizeLog2 Logarithmic representation
 *    of the shading rate tile size. Can be queried from
 *    the graphics device.
 * \returns Exact shading rate image size, in pixels
 */
inline Extent2D gfxComputeShadingRateImageSize(
        Extent2D                        extent,
        Extent2D                        tileSizeLog2) {
  Extent2D tileSize = Extent2D(1u) << tileSizeLog2;
  return (extent + tileSize - 1u) >> tileSizeLog2;
}


/**
 * \brief Checks whether the given shader stage uses workgroups
 *
 * \param [in] stage Shader stage to check
 * \returns \c true if the stage uses workgroups
 */
inline bool gfxShaderStageHasWorkgroupSize(
        GfxShaderStage                  stage) {
  return stage == GfxShaderStage::eCompute
      || stage == GfxShaderStage::eTask
      || stage == GfxShaderStage::eMesh;
}

}
