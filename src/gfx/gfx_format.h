#pragma once

#include <array>

#include "../util/util_assert.h"
#include "../util/util_math.h"
#include "../util/util_types.h"

#include "gfx_types.h"

namespace as {

/**
 * \brief Data formats
 */
enum class GfxFormat : uint32_t {
  eUnknown,
  eR4G4B4A4un,
  eR8un,
  eR8sn,
  eR8ui,
  eR8si,
  eR8G8un,
  eR8G8sn,
  eR8G8ui,
  eR8G8si,
  eR8G8B8A8un,
  eR8G8B8A8sn,
  eR8G8B8A8ui,
  eR8G8B8A8si,
  eR8G8B8A8srgb,
  eB8G8R8A8un,
  eB8G8R8A8sn,
  eB8G8R8A8ui,
  eB8G8R8A8si,
  eB8G8R8A8srgb,
  eR9G9B9E5f,
  eR10G10B10A2un,
  eR10G10B10A2ui,
  eB10G10R10A2un,
  eB10G10R10A2ui,
  eR11G11B10f,
  eR16un,
  eR16sn,
  eR16ui,
  eR16si,
  eR16f,
  eR16G16un,
  eR16G16sn,
  eR16G16ui,
  eR16G16si,
  eR16G16f,
  eR16G16B16A16un,
  eR16G16B16A16sn,
  eR16G16B16A16ui,
  eR16G16B16A16si,
  eR16G16B16A16f,
  eR32ui,
  eR32si,
  eR32f,
  eR32G32ui,
  eR32G32si,
  eR32G32f,
  eR32G32B32ui,
  eR32G32B32si,
  eR32G32B32f,
  eR32G32B32A32ui,
  eR32G32B32A32si,
  eR32G32B32A32f,
  eBc1un,
  eBc1srgb,
  eBc2un,
  eBc2srgb,
  eBc3un,
  eBc3srgb,
  eBc4un,
  eBc4sn,
  eBc5un,
  eBc5sn,
  eBc6Huf,
  eBc6Hsf,
  eBc7un,
  eBc7srgb,
  eD16,
  eD24,
  eD24S8,
  eD32,
  eD32S8,

  eCount
};


/**
 * \brief Format flags
 */
enum class GfxFormatFlag : uint32_t {
  /** Format is an sRGB format, and values read from or written to
   *  a view of this format will be converted to or from linear
   *  space automatically. */
  eSrgb           = (1u << 0),
  /** Format is block-compressed. */
  eCompressed     = (1u << 1),

  eFlagEnum       = 0
};

using GfxFormatFlags = Flags<GfxFormatFlag>;


/**
 * \brief Format data type
 */
enum class GfxFormatType : uint32_t {
  eFloat          = 0,  ///< Sampling the format returns floats
  eUint           = 1,  ///< Sampling the format returns unsigned ints
  eSint           = 2,  ///< Sampling the format returns signed ints
};


/**
 * \brief Aspect info for a given format
 */
struct GfxFormatAspectInfo {
  /** Image aspect that this struct applies to */
  GfxImageAspect aspect;  
  /** Block size within this aspect, in bytes */
  uint32_t elementSize;
  /** Log2 of the subsampling factor. If either component
   *  is non-zero, this aspect is subsampled. */
  Extent2D subsampleLog2;
  /** Subsampling factor */
  Extent2D subsample;
  /** Format data type */
  GfxFormatType type;
};


/**
 * \brief Format info
 *
 * Stores metadata about all supported image formats.
 */
struct GfxFormatInfo {
  /** Mask of all image aspects that are part of this
   *  format. There will be one plane struct per aspect. */
  GfxImageAspects aspects;
  /** Number of planes or aspects. Equivalent to the
   *  number of bits set in the aspect mask. */
  uint32_t planeCount;
  /** Log2 of the compressed block extent, in pixels.
   *  Only relevant for block-compressed formats. */
  Extent2D blockExtentLog2;
  /** Compressed block extent, in pixels. */
  Extent2D blockExtent;
  /** Format flags */
  GfxFormatFlags flags;
  /** Plane info, one entry per plane. */
  std::array<GfxFormatAspectInfo, 3> planes;

  /**
   * \brief Computes plane index for a given aspect
   *
   * \param [in] aspect Aspect to query
   * \returns Plane index
   */
  uint32_t computePlaneIndex(GfxImageAspect aspect) const {
    dbg_assert(aspects & aspect);

    return popcnt((uint32_t(aspect) - 1) & uint32_t(aspects));
  }

  /**
   * \brief Retrieves plane info for a given aspect
   *
   * \param [in] aspect The aspect to query
   * \returns Reference to that aspect
   */
  const GfxFormatAspectInfo& getAspectInfo(GfxImageAspect aspect) const {
    return planes[computePlaneIndex(aspect)];
  }
};


/**
 * \brief Format map
 *
 * Stores arbitrarry data for each supported format.
 * The intended use for this is as a base class, and
 * the constructor of any subclass should populate
 * the map with data.
 */
template<typename T>
class GfxFormatMap {

public:

  /**
   * \brief Looks up a format
   *
   * \param [in] format The format to query
   * \returns Data stored about that format
   */
  const T& get(
          GfxFormat                     format) const {
    return m_entries[uint32_t(format)];
  }

  /**
   * \brief Adds a format entry
   *
   * \param [in] format The format to add
   * \param [in] entry Format data
   */
  void set(
          GfxFormat                     format,
    const T&                            entry) {
    m_entries[uint32_t(format)] = entry;
  }

private:

  std::array<T, uint32_t(GfxFormat::eCount)> m_entries = { };

};


/**
 * \brief Format metadata
 *
 * Stores useful information about each format.
 */
class GfxFormatMetadataMap : public GfxFormatMap<GfxFormatInfo> {

public:

  GfxFormatMetadataMap();

private:

  void addFormat(
          GfxFormat         format,
          GfxImageAspects   aspects,
          Extent2D          blockExtentLog2,
          GfxFormatFlags    flags,
          std::tuple<uint32_t, Extent2D, GfxFormatType> plane0Info,
          std::tuple<uint32_t, Extent2D, GfxFormatType> plane1Info = { },
          std::tuple<uint32_t, Extent2D, GfxFormatType> plane2Info = { });

};

}
