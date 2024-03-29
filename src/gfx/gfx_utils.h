#pragma once

#include <cmath>

#include "../util/util_math.h"
#include "../util/util_types.h"

namespace as {

/**
 * \brief Trackable object interface
 *
 * Classes that can be used with lifetime tracking should inherit
 * from this class. This only provides a virtual destructor for
 * the purpose of storing a pointer to the object in a list, and
 * destroying it when it is safe to do so.
 */
class GfxTrackable {

public:

  virtual ~GfxTrackable() { }

};


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
 * \brief Computes maximum mip count for an image
 *
 * \param [in] imageExtent Extent of the image
 * \returns Maximum number of mips for the image
 */
inline uint32_t gfxComputeMipCount(
        Extent3D                        imageExtent) {
  uint32_t maxCoord = std::max(std::max(
    imageExtent.at<0>(), imageExtent.at<1>()),
    imageExtent.at<2>());

  return uint32_t(findmsb(maxCoord) + 1);
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
  return stage & (
    GfxShaderStage::eCompute |
    GfxShaderStage::eTask |
    GfxShaderStage::eMesh);
}


/**
 * \brief Checks whether the given shader stage has input variables
 *
 * \param [in] stage Shader stage to check
 * \returns \c true if the stage uses input variables
 */
inline bool gfxShaderStageHasInputVariables(
        GfxShaderStage                  stage) {
  return stage & (
    GfxShaderStage::eVertex |
    GfxShaderStage::eTessControl |
    GfxShaderStage::eTessEval |
    GfxShaderStage::eGeometry |
    GfxShaderStage::eFragment);
}


/**
 * \brief Checks whether the given shader stage has output variables
 *
 * \param [in] stage Shader stage to check
 * \returns \c true if the stage uses output variables
 */
inline bool gfxShaderStageHasOutputVariables(
        GfxShaderStage                  stage) {
  return stage & (
    GfxShaderStage::eVertex |
    GfxShaderStage::eTessControl |
    GfxShaderStage::eTessEval |
    GfxShaderStage::eGeometry |
    GfxShaderStage::eMesh |
    GfxShaderStage::eFragment);
}

}
