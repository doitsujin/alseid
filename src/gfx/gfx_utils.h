#pragma once

#include <cmath>

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

}
