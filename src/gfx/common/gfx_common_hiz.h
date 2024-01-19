#pragma once

#include "gfx_common_pipelines.h"

#include "../gfx_device.h"

namespace as {

/**
 * \brief Hi-Z image
 *
 * Stores a hierarchical representation of the depth buffer, with
 * the red component storing the minimum (farthest) depth value of
 * the corresponding pixels in more detailed mip levels, and the
 * green component storing the minimum (closest) depth value.
 * Depth values are stored with reduced precision.
 */
class GfxCommonHizImage {

public:

  GfxCommonHizImage(
          GfxDevice                     device);

  GfxCommonHizImage(const GfxCommonHizImage&) = delete;
  GfxCommonHizImage& operator = (const GfxCommonHizImage&) = delete;

  ~GfxCommonHizImage();

  /**
   * \brief Retrieves image view
   *
   * Returns a view that can be bound to shaders for reading.
   * May be \c nullptr if the image has not been generated yet.
   * \returns Image view object
   */
  GfxImageView getImageView() const;

  /**
   * \brief Generates image
   *
   * If necessary, recreates the image with the appropriate size and
   * mip level count, and dispatches a compute shader to generate mip
   * levels. Note that the most detailed mip is not part of the Hi-Z
   * buffer, as it should be read from the depth buffer itself.
   *
   * After this operation completes, the image will be ready to be
   * read by compute shaders only. Inserting additional barriers may
   * be required when accessing it from the graphics pipeline.
   * \param [in] context Context object
   * \param [in] pipelines Common shader pipeline object
   * \param [in] depthImage Depth image to generate the hi-z buffer for
   */
  void generate(
    const GfxContext&                   context,
    const GfxCommonPipelines&           pipelines,
    const GfxImage&                     depthImage);

private:

  GfxDevice             m_device;
  GfxImage              m_image;

};

}
