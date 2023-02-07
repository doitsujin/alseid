#pragma once

#include <array>

#include "../util/util_flags.h"

#include "gfx_image.h"
#include "gfx_types.h"

namespace as {

constexpr size_t GfxMaxColorAttachments = 8;

/**
 * \brief Rendering flags
 */
enum class GfxRenderingFlag : uint32_t {
  eSuspend  = (1u << 0),
  eResume   = (1u << 1),
  eFlagEnum = 0
};

using GfxRenderingFlags = Flags<GfxRenderingFlag>;


/**
 * \brief Render target operation when beginning rendering
 */
enum class GfxRenderTargetOp : uint32_t {
  /** Loads the render target and preserves its contents. This
   *  should only be used if the existing contents are needed. */
  eLoad     = 0,
  /** Discards the render target's contents and leaves it
   *  undefined This is the preferred option when rendering
   *  operations are guaranteed to overwrite the entire image. */
  eDiscard  = 1,
  /** Clears the render target contents to the specified clear
   *  value. Ths is preferred when previous contents should be
   *  discarded but not the entire image will get written. */
  eClear    = 2,
};


/**
 * \brief Depth-stencil clear value
 */
struct GfxDepthStencilValue {
  GfxDepthStencilValue()
  : d(0.0f), s(0.0f) { }

  explicit GfxDepthStencilValue(float d_, uint32_t s_)
  : d(d_), s(s_) { }

  /** Depth value */
  float d;
  /** Stencil value */
  uint32_t s;
};


/**
 * \brief Typed color clear value
 */
template<typename T>
struct GfxTypedColorValue {
  GfxTypedColorValue() = default;

  GfxTypedColorValue(T r_, T g_, T b_, T a_)
  : r(r_), g(g_), b(b_), a(a_) { }

  /** Color values in R, G, B, A order */
  T r, g, b, a;
};


/**
 * \brief Color clear value
 */
union GfxColorValue {
  GfxColorValue()
  : f(0.0f, 0.0f, 0.0f, 0.0f) { }

  GfxColorValue(float r, float g, float b, float a)
  : f(r, g, b, a) { }

  GfxColorValue(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
  : u(r, g, b, a) { }

  GfxColorValue(int32_t r, int32_t g, int32_t b, int32_t a)
  : i(r, g, b, a) { }

  GfxTypedColorValue<float>     f;  ///< Float representation
  GfxTypedColorValue<uint32_t>  u;  ///< Unsigned integer representation
  GfxTypedColorValue<int32_t>   i;  ///< Signed integer  representation
};


/**
 * \brief Color attachment info
 *
 * Stores information about a single render target for rendering.
 */
struct GfxColorAttachmentInfo {
  /** Render target operation when beginning rendering */
  GfxRenderTargetOp op = GfxRenderTargetOp::eLoad;
  /** Image view. The view must have been created with
   *  \c GfxUsage::eRenderTarget in order to be valid.
   *  If the view is \c nullptr, this render target will
   *  be ignored. */
  GfxImageView view;
  /** Resolve view. At the end of a render pass, \c view will
   *  automatically be resolved if this is set. The view must
   *  have been created with \c GfxUsage::eRenderTarget,
   *  and \c view must have been created from a multisampled
   *  image for this to be valid. Both views must have the
   *  same extent, format, and layer count. */
  GfxImageView resolveView;
  /** Color clear value. The data must be formatted correctly
   *  for the view format. Ignored if \c op is not
   *  \c GfxRenderTargetOp::eClear. */
  GfxColorValue clearValue;
};


/**
 * \brief Depth-stencil attachment info
 */
struct GfxDepthStencilAttachmentInfo {
  /** Depth aspect operation when beginning rendering */
  GfxRenderTargetOp depthOp = GfxRenderTargetOp::eLoad;
  /** Stencil aspect operation when beginning rendering */
  GfxRenderTargetOp stencilOp = GfxRenderTargetOp::eLoad;
  /** Image view. The view must have been created with
   *  \c GfxUsage::eRenderTarget in order to be valid.
   *  If the view is \c nullptr, no depth-stencil image will
   *  be bound and fragment tests not be performed. */
  GfxImageView view;
  /** Resolve view. At the end of a render pass, \c view will
   *  automatically be resolved if this is set. The view must
   *  have been created with \c GfxUsage::eRenderTarget,
   *  and \c view must have been created from a multisampled
   *  image for this to be valid. Both views must have the
   *  same extent, format, and layer count. */
  GfxImageView resolveView;
  /** Aspects of \c view that should be treated as read-only.
   *  Setting an aspect as read-only allows it to be bound
   *  simultaneously for rendering and as a shader resource as
   *  long as no rendering operations write to that aspect. The
   *  corresponding subresources \e must be transitioned with an
   *  image barrier setting both \c GfxUsage::eRenderTarget
   *  and \c GfxUsage::eShaderResource as \c dstUsage. */
  GfxImageAspects readOnlyAspects = 0;
  /** Depth-stencil clear values. Ignored if neither \c depthOp
   *  nor \c stencilOp are \c GfxRenderTargetOp::eClear. */
  GfxDepthStencilValue clearValue;
};


/**
 * \brief Rendering info
 *
 * Stores a set of render targets to bind.
 */
struct GfxRenderingInfo {
  /** Color target info. Any color attachment with a null
   *  view are considered unbound and will be ignored. */
  std::array<GfxColorAttachmentInfo, GfxMaxColorAttachments> color;
  /** Depth-stencil target info. Will be ignored in case the
   *  view is null, and fragment tests will not be performed. */
  GfxDepthStencilAttachmentInfo depthStencil;
};

}
