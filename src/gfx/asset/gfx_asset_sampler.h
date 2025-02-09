#pragma once

#include "../../util/util_stream.h"

#include "../gfx_sampler.h"

namespace as {

/**
 * \brief Asset sampler description
 *
 * Stores part of a sampler description with some ways to
 * allow for application-defined overrides.
 */
struct GfxAssetSamplerDesc {
  /** Sampler type */
  GfxSamplerType type = GfxSamplerType::eDefault;
  /** Magnification filter */
  GfxFilter magFilter = GfxFilter::eLinear;
  /** Minification filter */
  GfxFilter minFilter = GfxFilter::eLinear;
  /** Mip map filter */
  GfxMipFilter mipFilter = GfxMipFilter::eLinear;
  /** Address mode in horiziontal dimension */
  GfxAddressMode addressModeU = GfxAddressMode::eRepeat;
  /** Address mode in vertical dimension */
  GfxAddressMode addressModeV = GfxAddressMode::eRepeat;
  /** Address mode in depth dimension */
  GfxAddressMode addressModeW = GfxAddressMode::eRepeat;
  /** Border color for clamp-to-border address modes */
  GfxBorderColor borderColor = GfxBorderColor::eFloatTransparent;
  /** Mip map LOD bias. */
  float lodBias = 0.0f;
  /** Whether to allow anisotropic filtering */
  bool allowAnisotropy = true;
  /** Whether to allow LOD biasing. */
  bool allowLodBias = true;
  /** Depth compare op. Only has an effect if the sampler
   *  type is \c GfxSamplerType::eDepthCompare. */
  GfxCompareOp compareOp = GfxCompareOp::eAlways;

  /**
   * \brief Serializes texture info to a stream
   *
   * \param [in] output Stream to write to
   * \returns \c true on success
   */
  bool serialize(
          WrBufferedStream&             output);

  /**
   * \brief Reads serialized texture info
   *
   * \param [in] in Stream to read from
   * \returns \c true on success
   */
  bool deserialize(
          RdMemoryView                  input);

  /**
   * \brief Fills in sampler description
   *
   * Sets up sampler description with the sampler's properties.
   * The structure must be initialized with app preferences.
   * \param [out] desc Sampler description
   */
  void fillSamplerDesc(
          GfxSamplerDesc&               desc);
};

}
