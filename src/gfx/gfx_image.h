#pragma once

#include <array>
#include <string>

#include "../util/util_flags.h"
#include "../util/util_hash.h"
#include "../util/util_iface.h"
#include "../util/util_math.h"
#include "../util/util_stream.h"
#include "../util/util_types.h"

#include "gfx_descriptor_handle.h"
#include "gfx_format.h"
#include "gfx_memory.h"
#include "gfx_types.h"
#include "gfx_utils.h"

namespace as {

constexpr size_t GfxMaxViewFormats = 16;

class GfxImageIface;

/**
 * \brief Image view type
 */
enum class GfxImageViewType : uint32_t {
  e1D         = 0,
  e2D         = 1,
  e3D         = 2,
  eCube       = 3,
  e1DArray    = 4,
  e2DArray    = 5,
  eCubeArray  = 6,
};


/**
 * \brief Image view channel
 */
enum class GfxColorChannel : uint8_t {
  eR = 0u,  ///< Red channel
  eG = 1u,  ///< Green channel
  eB = 2u,  ///< Blue channel
  eA = 3u,  ///< Alpha channel
  e0 = 4u,  ///< Constant zero
  e1 = 5u,  ///< Constant one
};


/**
 * \brief Image view channel swizzle
 *
 * Can be used to remap channels of image views
 * before they are processed in a shader. Note
 * that this is only allowed for resource views.
 */
struct GfxColorSwizzle {
  GfxColorSwizzle() = default;

  GfxColorSwizzle(
          GfxColorChannel             r_,
          GfxColorChannel             g_,
          GfxColorChannel             b_,
          GfxColorChannel             a_)
  : r(r_), g(g_), b(b_), a(a_) { }

  GfxColorChannel r = GfxColorChannel::eR;
  GfxColorChannel g = GfxColorChannel::eG;
  GfxColorChannel b = GfxColorChannel::eB;
  GfxColorChannel a = GfxColorChannel::eA;

  bool operator == (const GfxColorSwizzle&) const = default;
  bool operator != (const GfxColorSwizzle&) const = default;

  size_t hash() const {
    return (uint32_t(r) <<  0) |
           (uint32_t(g) <<  8) |
           (uint32_t(b) << 16) |
           (uint32_t(a) << 24);
  }
};


/**
 * \brief Image view description
 *
 * The view description is also used to look up
 * views internally and therefore has comparison
 * and hash functions.
 */
struct GfxImageViewDesc {
  /** View type. Must be compatible with the image type. */
  GfxImageViewType type = GfxImageViewType::e2D;
  /** View format. Must be compatible with the image. */
  GfxFormat format = GfxFormat::eUnknown;
  /** View usage. Must be one of the usage flags specified
   *  for the image, and must be one of:
   *  - \c GfxUsage::eShaderResource
   *  - \c GfxUsage::eShaderStorage
   *  - \c GfxUsage::eRenderTarget */
  GfxUsage usage = GfxUsage::eFlagEnum;
  /** Image subresources included in the view.
   *  - If \c usage is \c GfxUsage::eShaderResource, only one
   *    image aspect can be included, but otherwise there are no
   *    restrictions on subresources.
   *  - If \c usage is \c GfxUsage::eShaderStorage, only one
   *    image aspect and one mip level can be included.
   *  - If \c usage is \c GfxUsage::eRenderTarget and if the
   *    format has both depth and stencil aspects, both aspects
   *    must be included. Otherwise, only one aspect is allowed
   *    and only one mip level is allowed. */
  GfxImageSubresource subresource;
  /** Color component swizzle for resource views. */
  GfxColorSwizzle swizzle;

  /**
   * \brief Computes hash
   * \returns Hash
   */
  size_t hash() const {
    HashState hash;
    hash.add(uint32_t(type));
    hash.add(uint32_t(format));
    hash.add(uint32_t(usage));
    hash.add(uint32_t(subresource.aspects));
    hash.add(subresource.mipIndex);
    hash.add(subresource.mipCount);
    hash.add(subresource.layerIndex);
    hash.add(subresource.layerCount);
    hash.add(swizzle.hash());
    return hash;
  }

  bool operator == (const GfxImageViewDesc&) const = default;
  bool operator != (const GfxImageViewDesc&) const = default;
};


/**
 * \brief Image flags
 */
enum class GfxImageFlag : uint32_t {
  /** Forces a dedicated allocation. This should be used sparingly,
   *  but may be useful in case the resource lifetime would cause
   *  issues with the global allocator. */
  eDedicatedAllocation  = (1u << 0),
  /** Enables sparse residency for this resource. If specified, no
   *  memory will be allocated at image creation time, instead, the
   *  app can dynamically bind memory at runtime. */
  eSparseResidency      = (1u << 1),
  /** Allows the image to be accessed from multiple queues at
   *  the same time without explicit calls to \c acquireImage
   *  or \c releaseImage on the context. */
  eSimultaneousAccess   = (1u << 2),
  /** Allows cube map views to be created for this image.
   *  Image must be a 2D image with at least 6 layers. */
  eCubeViews            = (1u << 3),
  eFlagEnum             = 0
};

using GfxImageFlags = Flags<GfxImageFlag>;


/**
 * \brief Image type
 */
enum class GfxImageType : uint32_t {
  e1D = 0,
  e2D = 1,
  e3D = 2,
};


/**
 * \brief Gets image type from view type
 */
inline GfxImageType gfxGetImageTypeForViewType(GfxImageViewType type) {
  if (type == GfxImageViewType::e1D || type == GfxImageViewType::e1DArray)
    return GfxImageType::e1D;

  if (type == GfxImageViewType::e3D)
    return GfxImageType::e3D;

  return GfxImageType::e2D;
}


/**
 * \brief Computes image dimension from type
 *
 * \param [in] type Image type
 * \returns Number of dimensions
 */
inline uint32_t gfxGetImageDimensions(GfxImageType type) {
  return uint32_t(type) + 1;
}


/**
 * \brief Computes image dimension from view type
 *
 * \param [in] type Image view type
 * \returns Number of dimensions, not accounting for array layers
 */
inline uint32_t gfxGetImageViewDimensions(GfxImageViewType type) {
  return gfxGetImageDimensions(gfxGetImageTypeForViewType(type));
}


/**
 * \brief Image description
 */
struct GfxImageDesc {
  /** Image debug name */
  const char* debugName = nullptr;
  /** Image dimensionality */
  GfxImageType type = GfxImageType::e2D;
  /** Image format. */
  GfxFormat format = GfxFormat::eUnknown;
  /** Image usage. Specifies which kind of operations
   *  the image can be used with.*/
  GfxUsageFlags usage = 0u;
  /** Image extent of the top mip level, in pixels */
  Extent3D extent = Extent3D(0u, 0u, 0u);
  /** Number of mip levels. Must be at least 1, but no
   *  more than the given image extent would allow. */
  uint32_t mips = 1u;
  /** Number of array layers. Must be at least 1. */
  uint32_t layers = 1u;
  /** Number of samples. Must be a power of two.
   *  If greater than 1, \c mips must be 1.*/
  uint32_t samples = 1u;
  /** Image flags. Sets additional compatibility flags. */
  GfxImageFlags flags = 0u;
  /** Number of additional view formats. By default, images
   *  are only compatible with views of their own format,
   *  but additional view formats can be allowed. */
  uint32_t viewFormatCount = 0u;
  /** View format array. Note that this has an upper bound. */
  std::array<GfxFormat, GfxMaxViewFormats> viewFormats = { };
};


/**
 * \brief Image view interface
 */
class GfxImageViewIface {

public:

  GfxImageViewIface(
    const GfxImageIface&                image,
    const GfxImageViewDesc&             desc);

  virtual ~GfxImageViewIface() { }

  /**
   * \brief Retrieves image view descriptor
   *
   * The resulting descriptor can be used to bind the view to a
   * shader pipeline. Descriptors may be cached as long as they
   * are not used after the view object gets destroyed.
   * \returns View descriptor
   */
  virtual GfxDescriptor getDescriptor() const = 0;

  /**
   * \brief Queries image view description
   * \returns Image view description
   */
  GfxImageViewDesc getDesc() const {
    return m_desc;
  }

  /**
   * \brief Queries format info
   * \returns Format info
   */
  const GfxFormatInfo& getFormatInfo() const;

  /**
   * \brief Queries image sample count
   * \returns Image sample count
   */
  uint32_t getImageSampleCount() const {
    return m_imageSamples;
  }

  /**
   * \brief Computes mip extent of the given mip inside the view
   *
   * Equal to calling \c computeMipExtent on the image
   * with the mip level offset by the view's mip index.
   * \param [in] mipLevel Mip level within the view
   * \returns Extent of the given mip level
   */
  Extent3D computeMipExtent(uint32_t mipLevel) const {
    return gfxComputeMipExtent(m_imageExtent,
      m_desc.subresource.mipIndex + mipLevel);
  }

protected:

  GfxImageViewDesc  m_desc;
  Extent3D          m_imageExtent;
  uint32_t          m_imageSamples;

};

/** See GfxImageViewIface. */
using GfxImageView = PtrRef<GfxImageViewIface>;


/**
 * \brief Image resource interface
 */
class GfxImageIface : public GfxTrackable {

public:

  GfxImageIface(
    const GfxImageDesc&                 desc);

  /**
   * \brief Retrieves view with the given properties
   *
   * If a view with the given properties has already been created,
   * this will return the existing view object, so calls to this
   * function are expected to be relatively fast. View objects have
   * the same lifetime as the image, so they should not be cached
   * if doing so risks accessing stale views.
   * \param [in] desc View description
   * \returns View object
   */
  virtual GfxImageView createView(
    const GfxImageViewDesc&             desc) = 0;

  /**
   * \brief Queries memory info for the resource
   * \returns Image memory allocation info
   */
  virtual GfxMemoryInfo getMemoryInfo() const = 0;

  /**
   * \brief Queries image description
   * \returns Image description
   */
  GfxImageDesc getDesc() const {
    return m_desc;
  }

  /**
   * \brief Queries format info
   * \returns Format info
   */
  const GfxFormatInfo& getFormatInfo() const;

  /**
   * \brief Computes mip level extent
   *
   * Convenience method that computes the size
   * of a mip level within the given image.
   * \param [in] mipLevel Mip level to query
   * \returns Extent of the given mip level
   */
  Extent3D computeMipExtent(uint32_t mipLevel) const {
    return gfxComputeMipExtent(m_desc.extent, mipLevel);
  }

  /**
   * \brief Computes subresource index
   *
   * This can be used to deal with archive files.
   * \param [in] subresource Subresource to query
   * \returns Subresource index
   */
  uint32_t computeSubresourceIndex(
    const GfxImageSubresource&          subresource) const;

  /**
   * \brief Queries available subresources
   *
   * Convenience method that returns all subresources available
   * to the image based on its properties and format.
   * \returns Available subresources
   */
  GfxImageSubresource getAvailableSubresources() const;

protected:

  GfxImageDesc  m_desc;
  std::string   m_debugName;

};

/** See GfxImageIface. */
using GfxImage = IfaceRef<GfxImageIface>;


/**
 * \brief Texture info
 *
 * Stores the type, format and size of a texture, as well
 * as subresource metadata. Can be serialized and used to
 * populate image descriptions for read-only resources.
 */
struct GfxTextureDesc {
  /** Image dimensionality. */
  GfxImageViewType type = GfxImageViewType::e2D;
  /** Image data format. */
  GfxFormat format = GfxFormat::eUnknown;
  /** Image dimensions, in texels */
  Extent3D extent = Extent3D(0, 0, 0);
  /** Mip level count */
  uint32_t mips = 0;
  /** Layer count */
  uint32_t layers = 0;
  /** First mip level in the mip tail. Will have the
   *  value of \c mips if there is no mip tail. */
  uint32_t mipTailStart = 0;

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
   * \brief Fills in image description
   *
   * Sets up image description with the texture's
   * properties. Will not touch any fields other
   * than the ones provided by this structure.
   * \param [out] desc Image description
   * \param [in] mip First mip level
   */
  void fillImageDesc(
          GfxImageDesc&                 desc,
          uint32_t                      mip);

};


}
