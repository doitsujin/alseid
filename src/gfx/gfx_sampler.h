#pragma once

#include <string>

#include "../util/util_iface.h"

#include "gfx_descriptor_handle.h"
#include "gfx_pipeline.h"
#include "gfx_types.h"
#include "gfx_utils.h"

namespace as {

/**
 * \brief Sampler type
 */
enum class GfxSamplerType : uint32_t {
  /** Regular sampler that interpolates
   *  values stored in the texture */
  eDefault      = 0,
  /** Sampler that interpolates results
   *  of depth compare operations */
  eDepthCompare = 1,
};


/**
 * \brief Filter
 */
enum class GfxFilter : uint32_t {
  /** Nearest neighbour filtering */
  eNearest      = 0,
  /** Bi-linear interpolation */
  eLinear       = 1,
};


/**
 * \brief Mip map filter
 */
enum class GfxMipFilter : uint32_t {
  /** Sample nearest mip level only */
  eNearest      = 0,
  /** Interpolate between mip levels */
  eLinear       = 1,
};


/**
 * \brief Texture address mode
 */
enum class GfxAddressMode : uint32_t {
  eRepeat             = 0,
  eMirror             = 1,
  eClampToEdge        = 2,
  eClampToBorder      = 3,
  eMirrorClampToEdge  = 4,
};


/**
 * \brief Border color
 *
 * Note that border colors are typed, with \c eFloat variants
 * being valid for floating point, unorm and snorm formats, and
 * \c eInt variants being valid for signed and unsigned integer
 * formats. Sampling a texture of an incompatible format results
 * in undefined behaviour.
 */
enum class GfxBorderColor : uint32_t {
  /** All components are 0 */
  eFloatTransparent   = 0,
  /** Alpha component is 1 */
  eFloatBlack         = 1,
  /** All components are 1 */
  eFloatWhite         = 2,
  /** All components are 0 */
  eIntTransparent     = 3,
  /** Alpha component is the maximum
   *  representable integer value */
  eIntBlack           = 4,
  /** All component are the maximum
   *  representable integer value */
  eIntWhite           = 5,
};


/**
 * \brief Sampler description
 */
struct GfxSamplerDesc {
  /** Sampler debug name */
  const char* debugName;
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
  /** Mip map LOD bias */
  float lodBias = 0.0f;
  /** Minimum LOD */
  float minLod = -std::numeric_limits<float>::max();
  /** Maximum LOD */
  float maxLod = std::numeric_limits<float>::max();
  /** Anisotropy. If the value is 1 or less,
   *  anisotropic filtering is disabled. */
  uint32_t anisotropy = 0;
  /** Depth compare op. Only has an effect if the sampler
   *  type is \c GfxSamplerType::eDepthCompare. */
  GfxCompareOp compareOp = GfxCompareOp::eAlways;
};


/**
 * \brief Sampler interface
 */
class GfxSamplerIface {

public:

  GfxSamplerIface(
    const GfxSamplerDesc&               desc)
  : m_desc(desc) {
    if (m_desc.debugName) {
      m_debugName = m_desc.debugName;
      m_desc.debugName = m_debugName.c_str();
    }
  }

  virtual ~GfxSamplerIface() { }

  /**
   * \brief Retrieves sampler descriptor
   *
   * The resulting descriptor can be used to bind the sampler
   * to a shader pipeline. Descriptors may be cached as long
   * as they are not used after the object gets destroyed.
   * \returns Sampler descriptor
   */
  virtual GfxDescriptor getDescriptor() const = 0;

  /**
   * \brief Queries sampler description
   * \returns Sampler description
   */
  GfxSamplerDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxSamplerDesc  m_desc;
  std::string     m_debugName;

};

using GfxSampler = IfaceRef<GfxSamplerIface>;

}
