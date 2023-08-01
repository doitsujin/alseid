#pragma once

#include "../util/util_hash.h"
#include "../util/util_iface.h"

#include "gfx_render.h"
#include "gfx_shader.h"
#include "gfx_types.h"

namespace as {

constexpr uint32_t GfxMaxDescriptorSets = 8;
constexpr uint32_t GfxMaxDescriptorsPerSet = 128;

constexpr uint32_t GfxMaxVertexAttributes = 32;
constexpr uint32_t GfxMaxVertexBindings = 32;

constexpr uint32_t GfxMaxViewportCount = 16;


class GfxRenderStateData;


/**
 * \brief Mesh shader behaviour flags
 *
 * Vendor preferences passed through to the
 * mesh shader via specialization constants.
 */
enum class GfxMeshShaderFlag : uint32_t {
  ePreferLocalOutput            = (1u << 0),
  ePreferCompactVertexOutput    = (1u << 1),
  ePreferCompactPrimitiveOutput = (1u << 2),

  eFlagEnum                     = 0
};

using GfxMeshShaderFlags = Flags<GfxMeshShaderFlag>;


/**
 * \brief SPIR-V specialization constant IDs
 *
 * Specialization constants are passed to the
 * shader during pipeline compilation.
 */
enum class GfxSpecConstantId : uint32_t {
  /** Minimum subgroup size supported by the device */
  eMinSubgroupSize            = 0,
  /** Maximum subgroup size supported by the device */
  eMaxSubgroupSize            = 1,
  /** Preferred task shader workgroup size. This will try
   *  to respect vendor preferences and is the same for
   *  all task shaders created on a device, so that any
   *  shaders producing indirect task shader draws do not
   *  need to be aware of each pipeline's workgroup size. */
  eTaskShaderWorkgroupSize    = 2,
  /** Preferred mesh shader workgroup size. This will try
   *  to respect vendor preferences as well as the maximum
   *  primitive and vertex count that the shader can emit.
   *  May not be a power of two if the maximum vertex and
   *  primitive count is not a power of two. */
  eMeshShaderWorkgroupSize    = 3,
  /** Preferred mesh shader behaviour flags. This may
   *  affect the way mesh shaders cull individual
   *  primitives, if necessary. */
  eMeshShaderFlags            = 4,
};


/**
 * \brief Vertex attribute input rate
 *
 * Defines how vertex attributes will be read.
 */
enum class GfxInputRate : uint32_t {
  /** Attribute will receive different data for
   *  each vertex within a given instance. */
  ePerVertex    = 0,
  /** Attribute will receive the same data for
   *  all vertices in an instane, but different
   *  data between instances. */
  ePerInstance  = 1,
};


/**
 * \brief Vertex attribute description
 */
struct GfxVertexInputAttribute {
  /** Binding index. This defines index of the vertex
   *  buffer that will contain data for this attribute. */
  uint32_t binding = 0;
  /** Data format of this attribute. */
  GfxFormat format = GfxFormat::eUnknown;
  /** Data offset within a given vertex. Must be aligned
   *  with respect to the format's requirements. */
  uint32_t offset = 0;
  /** Vertex stride. Must be at least as large as the
   *  size of the vertex within the current binding. */
  uint32_t stride = 0;
  /** Input rate. This \e must be the same for all
   *  attributes that use the same \c binding index. */
  GfxInputRate inputRate = GfxInputRate::ePerVertex;

  bool operator == (const GfxVertexInputAttribute&) const = default;
  bool operator != (const GfxVertexInputAttribute&) const = default;

  size_t hash() const;
};


/**
 * \brief Cull mode
 */
enum class GfxCullMode : uint32_t {
  eNone   = 0,  ///< No face will be culled
  eBack   = 1,  ///< Back face culling only
  eFront  = 2,  ///< Front face culling only
};


/**
 * \brief Winding order
 */
enum class GfxFrontFace : uint32_t {
  eCcw    = 0,  ///< Counter-clockwise
  eCw     = 1,  ///< Clockwise
};


/**
 * \brief Shading rate op
 *
 * Defines how to combine global shading rate and attachment
 * shading rate. Note that primitive shading rates are not
 * supported and will be ignored.
 */
enum class GfxShadingRateOp : uint32_t {
  /** Uses the rasterization state's shading rate and
   *  ignores the bound shading rate image, if any. */
  eFixed  = 0,
  /** Uses the bound shading rate image, if any,
   *  and ignores context state. */
  eImage  = 1,
  /** Uses the minimum (more granular) shading rate
   *  between context state and attachment. */
  eMin    = 2,
  /** Uses the maximum (less granular) shading rate
   *  between context state and attachment. */
  eMax    = 3,
};


/**
 * \brief Compare op for depth or stencil test
 */
enum class GfxCompareOp : uint32_t {
  eNever        = 0,  ///< Always fail
  eLess         = 1,  ///< Pass if less than reference
  eEqual        = 2,  ///< Pass if equal to reference
  eLessEqual    = 3,  ///< Pass if less than or equal to reference
  eGreater      = 4,  ///< Pass if greater than reference
  eNotEqual     = 5,  ///< Pass if not equal to reference
  eGreaterEqual = 6,  ///< Pass if greater than or equal to reference
  eAlways       = 7,  ///< Always pass
};


/**
 * \brief Stencil write operation
 */
enum class GfxStencilOp : uint32_t {
  eKeep         = 0,  ///< Do not modify value
  eZero         = 1,  ///< Set value to 0
  eSet          = 2,  ///< Set value to stencil reference
  eIncClamp     = 3,  ///< Increment and saturate
  eDecClamp     = 4,  ///< Decrement and clamp
  eInvert       = 5,  ///< Flip all bits
  eIncWrap      = 6,  ///< Increment and wrap
  eDecWrap      = 7,  ///< Decrement and wrap
};


/**
 * \brief Stencil operation for one face
 */
struct GfxStencilDesc {
  /** Stencil operation to execute when the stencil
   *  test itself fails. */
  GfxStencilOp failOp = GfxStencilOp::eKeep;
  /** Stencil operation to execute when the stencil
   *  test and depth test both pass. */
  GfxStencilOp passOp = GfxStencilOp::eKeep;
  /** Stencil operation to execute when the stencil
   *  test passes but the depth test fails. */
  GfxStencilOp depthFailOp = GfxStencilOp::eKeep;
  /** Compare operation for the stencil fest. If this
   *  is \c GfxCompareOp::eAlways and all relevant
   *  stencil ops are \c GfxStencilOp::eKeep, the
   *  stencil test will effectively be disabled. */
  GfxCompareOp compareOp = GfxCompareOp::eAlways;
  /** Bits to read in the stencil test. */
  uint32_t compareMask = 0;
  /** Bits to write in stencil operations. */
  uint32_t writeMask = 0;

  /**
   * \brief Checks whether stencil test is used
   *
   * \param [in] depthTestCanFail Whether the depth test can fail
   * \returns \c true if stencil gets accessed
   */
  bool isStencilTestEnabled(
          bool                          depthTestCanFail) const;

  /**
   * \brief Checks whether stencil writes are enabled
   *
   * \param [in] depthTestCanFail Whether the depth test can fail
   * \returns \c true if stencil gets written
   */
  bool isStencilWriteEnabled(
          bool                          depthTestCanFail) const;

  bool operator == (const GfxStencilDesc&) const = default;
  bool operator != (const GfxStencilDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Color component flags
 *
 * Used in write masks for blend states.
 */
enum class GfxColorComponent : uint32_t {
  eR        = (1u << 0), ///< Red
  eG        = (1u << 1), ///< Green
  eB        = (1u << 2), ///< Blue
  eA        = (1u << 3), ///< Alpha
  eRGBA     = eR | eG | eB | eA,

  eFlagEnum = 0
};

using GfxColorComponents = Flags<GfxColorComponent>;


/**
 * \brief Blend factor
 */
enum class GfxBlendFactor : uint32_t {
  eZero                   = 0,
  eOne                    = 1,
  eSrcColor               = 2,
  eOneMinusSrcColor       = 3,
  eDstColor               = 4,
  eOneMinusDstColor       = 5,
  eSrcAlpha               = 6,
  eOneMinusSrcAlpha       = 7,
  eDstAlpha               = 8,
  eOneMinusDstAlpha       = 9,
  eConstantColor          = 10,
  eOneMinusConstantColor  = 11,
  eConstantAlpha          = 12,
  eOneMinusConstantAlpha  = 13,
  eSrcAlphaSaturate       = 14,
  eSrc1Color              = 15,
  eOneMinusSrc1Color      = 16,
  eSrc1Alpha              = 17,
  eOneMinusSrc1Alpha      = 18,
};


/**
 * \brief Blend operation
 */
enum class GfxBlendOp : uint32_t {
  eAdd                    = 0,
  eSubtract               = 1,
  eReverseSubtract        = 2,
  eMin                    = 3,
  eMax                    = 4,
};


/**
 * \brief Render target blend state
 *
 * Stores blend state for a single render target. The default
 * setup chooses blend ops and factors in such a way that 
 * blending is disabled, but color writes are enabled.
 */
struct GfxRenderTargetBlend {
  /** Blend factor for source color. */
  GfxBlendFactor srcColor = GfxBlendFactor::eOne;
  /** Blend factor for destination color. */
  GfxBlendFactor dstColor = GfxBlendFactor::eZero;
  /** Color blend function. */
  GfxBlendOp colorOp = GfxBlendOp::eAdd;
  /** Blend factor for source alpha. */
  GfxBlendFactor srcAlpha = GfxBlendFactor::eOne;
  /** Blend factor for destination alpha. */
  GfxBlendFactor dstAlpha = GfxBlendFactor::eZero;
  /** Alpha blend function. */
  GfxBlendOp alphaOp = GfxBlendOp::eAdd;
  /** Component write mask. If this is 0, this render target will
   *  essentially be disabled for this pipeline. However, this is
   *  \e not equal to setting the format to \c GfxFormat::eUnknown
   *  for the given render target, since only disabling the write
   *  mask still allows the render target to be bound. */
  GfxColorComponents writeMask = GfxColorComponent::eRGBA;

  /**
   * \brief Checks whether blending is enabled
   * \returns \c true if blending is enabled
   */
  bool isBlendingEnabled() const;

  /**
   * \brief Checks whether blend constants are used
   * \returns \c true if blend constants are used
   */
  bool usesBlendConstants() const;

  /**
   * \brief Checks whether dual-source blending is used
   * \returns \c true if dual-source blending is used
   */
  bool usesDualSource() const;

  bool operator == (const GfxRenderTargetBlend&) const = default;
  bool operator != (const GfxRenderTargetBlend&) const = default;

  size_t hash() const;
};


/**
 * \brief Logic op
 */
enum class GfxLogicOp : uint32_t {
  eZero           = 0,
  eSrcAndDst      = 1,
  eSrcAndInvDst   = 2,
  eSrc            = 3,
  eInvSrcAndDst   = 4,
  eDst            = 5,
  eSrcXorDst      = 6,
  eSrcOrDst       = 7,
  eSrcNorDst      = 8,
  eSrcEqualDst    = 9,
  eInvDst         = 10,
  eSrcOrInvDst    = 11,
  eInvSrc         = 12,
  eInvSrcOrDst    = 13,
  eSrcNandDst     = 14,
  eOne            = 15,
};


/**
 * \brief Render target state decription
 *
 * Defines all render target formats as well as
 * the sample count, which must be the same for
 * all render targets during rendering.
 */
struct GfxRenderTargetStateDesc {
  /** Color formats for all render targets. If an entry
   *  is \c GfxFormat::eUnknown, no render target must
   *  be bound to that slot during rendering. */
  std::array<GfxFormat, GfxMaxColorAttachments> colorFormats = { };
  /** Format of the depth-stencil target. If this is
   *  \c GfxFormat::eUnknown, no depth-stencil attachment
   *  must be bound during rendering. */
  GfxFormat depthStencilFormat = GfxFormat::eUnknown;
  /** Render target sample count. Set to 0 if no render
   *  targets are active. */
  uint32_t sampleCount = 0;

  bool operator == (const GfxRenderTargetStateDesc&) const = default;
  bool operator != (const GfxRenderTargetStateDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Render target state interface
 */
class GfxRenderTargetStateIface {

public:

  GfxRenderTargetStateIface(
    const GfxRenderTargetStateDesc&     desc)
  : m_desc(desc) { }

  virtual ~GfxRenderTargetStateIface() { }

  /**
   * \brief Retrieves state description
   * \returns State description
   */
  GfxRenderTargetStateDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxRenderTargetStateDesc m_desc;

};

/** See GfxRenderTargetStateIface. */
using GfxRenderTargetState = PtrRef<GfxRenderTargetStateIface>;


/**
 * \brief Primitive topology state
 *
 * Defines the primitive topology used for input
 * assembly in legacy vertex shader pipelines.
 */
struct GfxPrimitiveTopology {
  /** Primitive type. Defines both rasterization behaviour and
   *  the way vertex data will be passed to the vertex shader. */
  GfxPrimitiveType primitiveType = GfxPrimitiveType::eTriangleList;
  /** Patch vertex count for tessellation pipelines. */
  uint32_t patchVertexCount = 0u;

  /**
   * \brief Checks whether primitive restart is enabled
   * \returns \c true for strip topologies.
   */
  bool isPrimitiveRestartEnabled() const {
    return primitiveType == GfxPrimitiveType::eLineStrip
        || primitiveType == GfxPrimitiveType::eTriangleStrip;
  }

  bool operator == (const GfxPrimitiveTopology&) const = default;
  bool operator != (const GfxPrimitiveTopology&) const = default;

  size_t hash() const;
};


/**
 * \brief Vertex layout description
 *
 * Defines the way vertex data is laid out in vertex buffers
 * when using legacy vertex shader pipelines.
 */
struct GfxVertexLayout {
  /** Vertex attribute descriptions. */
  std::array<GfxVertexInputAttribute, GfxMaxVertexAttributes> attributes;

  bool operator == (const GfxVertexLayout&) const = default;
  bool operator != (const GfxVertexLayout&) const = default;

  size_t hash() const;
};


/**
 * \brief Depth bias description
 *
 * Applied during rasterization. Depth bias will be
 * disabled if relevant values are 0.
 */
struct GfxDepthBias {
  float depthBias = 0.0f;
  float depthBiasSlope = 0.0f;
  float depthBiasClamp = 0.0f;

  /**
   * \brief Checks whether depth bias is enabled
   * \returns \c true if depth bias is enabled
   */
  bool isDepthBiasEnabled() const {
    return depthBias != 0.0f || depthBiasSlope != 0.0f;
  }

  bool operator == (const GfxDepthBias&) const = default;
  bool operator != (const GfxDepthBias&) const = default;

  size_t hash() const;
};


/**
 * \brief Shading rate description
 *
 * Influences fragment shader execution after rasterization.
 */
struct GfxShadingRate {
  /** Shading rate combiner with the shading rate image. */
  GfxShadingRateOp shadingRateOp = GfxShadingRateOp::eFixed;
  /** Shading rate specified for the pipeline. */
  Extent2D shadingRate = Extent2D(1, 1);

  bool operator == (const GfxShadingRate&) const = default;
  bool operator != (const GfxShadingRate&) const = default;

  size_t hash() const;
};


/**
 * \brief Depth test description
 *
 * Only relevant if a depth-stencil image is bound, otherwise
 * the depth test is considered to be disabled.
 */
struct GfxDepthTest {
  /** Enables depth writes. If disabled, depth values from
   *  rasterization will only be used for the comparison. */
  bool enableDepthWrite = false;
  /** Enables depth bounds testing. If enabled, depth values in
  *   the depth buffer will be compared to a range that can be
  *   set dynamically. */
  bool enableDepthBoundsTest = false;
  /** Depth compare op. If this is \c GfxCompareOp::eAlways
   *  and depth writes are disabled, the depth test will
   *  effectively be disabled entirely. */
  GfxCompareOp depthCompareOp = GfxCompareOp::eAlways;

  /**
   * \brief Checks whether the depth test is enabled
   * \returns \c true if depth gets accessed
   */
  bool isDepthTestEnabled() const {
    return enableDepthWrite || depthCompareOp != GfxCompareOp::eAlways;
  }

  bool operator == (const GfxDepthTest&) const = default;
  bool operator != (const GfxDepthTest&) const = default;

  size_t hash() const;
};


/**
 * \brief Stencil test description
 *
 * Related to the depth test in functionality, but kept
 * separate since stencil testing is often not needed.
 */
struct GfxStencilTest {
  /** Front face stencil operation */
  GfxStencilDesc front;
  /** Back face stencil operation */
  GfxStencilDesc back;

  /**
   * \brief Checks whether stencil test is used
   * \returns \c true if stencil gets accessed
   */
  bool isStencilTestEnabled(const GfxDepthTest& depthTest) const {
    bool depthTestCanFail = depthTest.depthCompareOp != GfxCompareOp::eAlways;

    return front.isStencilTestEnabled(depthTestCanFail)
        || back.isStencilTestEnabled(depthTestCanFail);
  }

  /**
   * \brief Checks whether stencil writes are enabled
   * \returns \c true if stencil gets written
   */
  bool isStencilWriteEnabled(const GfxDepthTest& depthTest) const {
    bool depthTestCanFail = depthTest.depthCompareOp != GfxCompareOp::eAlways;

    return front.isStencilWriteEnabled(depthTestCanFail)
        || back.isStencilWriteEnabled(depthTestCanFail);
  }

  bool operator == (const GfxStencilTest&) const = default;
  bool operator != (const GfxStencilTest&) const = default;

  size_t hash() const;
};


/**
 * \brief Multisample state description
 */
struct GfxMultisampling {
  /** Sample count override. Only has an effect when
   *  no render targets are bound to the pipeline. */
  uint32_t sampleCount = 0;
  /** Sample mask. By default, all samples are enabled. */
  uint32_t sampleMask = ~0u;
  /** Whether to enable alpha-to-coverage */
  bool enableAlphaToCoverage = false;

  bool operator == (const GfxMultisampling&) const = default;
  bool operator != (const GfxMultisampling&) const = default;

  size_t hash() const;
};


/**
 * \brief Blending description
 *
 * Defines how fragment shader outputs are combined with
 * the data already stored in bound render targets.
 */
struct GfxBlending {
  /** Logic op. If this is \c GfxLogicOp::eCopySrc, the logic
   *  op is effectively disabled. Can only be used on integer
   *  render targets. */
  GfxLogicOp logicOp = GfxLogicOp::eSrc;
  /** Blend state for individual render targets. */
  std::array<GfxRenderTargetBlend, GfxMaxColorAttachments> renderTargets;

  /**
   * \brief Checks whether logic op is enabled
   * \returns \c true if logic op is enabled
   */
  bool isLogicOpEnabled() const {
    return logicOp != GfxLogicOp::eSrc;
  }

  bool operator == (const GfxBlending&) const = default;
  bool operator != (const GfxBlending&) const = default;

  size_t hash() const;
};


/**
 * \brief Render state description
 *
 * Stores a collection of render states. Any of the given
 * pointers can be \c nullptr, which means that the state
 * in question will not be included in the object.
 *
 * Binding render state objects will only affect states that
 * are actually specified in them. This allows changing small
 * subsets of state depending on application needs using one
 * single function call, rather than using larger state blocks
 * which may not map to the granularity that the app needs,
 * or having to set each state individually.
 */
struct GfxRenderStateDesc {
  GfxRenderStateDesc() = default;

  GfxRenderStateDesc(
    const GfxRenderStateData&           data);

  /** Primitive topology. */
  const GfxPrimitiveTopology* primitiveTopology = nullptr;
  /** Vertex layout. */
  const GfxVertexLayout* vertexLayout = nullptr;
  /** Front-face for rasterization. */
  const GfxFrontFace* frontFace = nullptr;
  /** Face culling mode for rasterization. */
  const GfxCullMode* cullMode = nullptr;
  /** Conservative rasteritation. */
  const bool* conservativeRaster = nullptr;
  /** Depth bias. */
  const GfxDepthBias* depthBias = nullptr;
  /** Shading rate. */
  const GfxShadingRate* shadingRate = nullptr;
  /** Depth test. */
  const GfxDepthTest* depthTest = nullptr;
  /** Stencil test. */
  const GfxStencilTest* stencilTest = nullptr;
  /** Multisample state. */
  const GfxMultisampling* multisampling = nullptr;
  /** Color blend state. */
  const GfxBlending* blending = nullptr;
};


/**
 * \brief Render state flags
 *
 * Defines which render state flags are set in
 * a render state object.
 */
enum class GfxRenderStateFlag : uint32_t {
  ePrimitiveTopology  = (1u << 0),
  eVertexLayout       = (1u << 1),
  eFrontFace          = (1u << 2),
  eCullMode           = (1u << 3),
  eConservativeRaster = (1u << 4),
  eDepthBias          = (1u << 5),
  eShadingRate        = (1u << 6),
  eDepthTest          = (1u << 7),
  eStencilTest        = (1u << 8),
  eMultisampling      = (1u << 9),
  eBlending           = (1u << 10),

  eAll                = ((1u << 11) - 1),

  eFlagEnum           = 0,
};

using GfxRenderStateFlags = Flags<GfxRenderStateFlag>;


/**
 * \brief Render state data
 *
 * Flat data structure containing all render
 * states, except render target state.
 */
struct GfxRenderStateData {
  GfxRenderStateData() = default;

  explicit GfxRenderStateData(
    const GfxRenderStateDesc&           desc);

  /** Bit mask of active render states. */
  GfxRenderStateFlags flags = 0;
  GfxPrimitiveTopology primitiveTopology;
  GfxVertexLayout vertexLayout;
  GfxFrontFace frontFace = GfxFrontFace::eCcw;
  GfxCullMode cullMode = GfxCullMode::eNone;
  bool conservativeRaster = false;
  GfxDepthBias depthBias;
  GfxShadingRate shadingRate;
  GfxDepthTest depthTest;
  GfxStencilTest stencilTest;
  GfxMultisampling multisampling;
  GfxBlending blending;

  bool operator == (const GfxRenderStateData& other) const;
  bool operator != (const GfxRenderStateData& other) const;

  size_t hash() const;
};


/**
 * \brief Render state interface
 *
 * Can be bound to the context to update only the
 * provided subset of render states.
 */
class GfxRenderStateIface {

public:

  GfxRenderStateIface(
    const GfxRenderStateData&           desc);

  virtual ~GfxRenderStateIface();

  /**
   * \brief Returns a reference to the contained render state
   *
   * Beware of potential lifetime issues when using this.
   * \returns Render state data
   */
  const GfxRenderStateData& getState() const {
    return m_data;
  }

protected:

  GfxRenderStateData  m_data;
  GfxRenderStateDesc  m_desc;

};

/** See GfxGraphicsPipelGfxRenderStateIfaceineIface. */
using GfxRenderState = PtrRef<GfxRenderStateIface>;


/**
 * \brief Graphics pipeline description
 */
struct GfxGraphicsPipelineDesc {
  /** Debug name. If not specified, the debug name for
   *  the pipeline will be inferred from the shaders. */
  const char* debugName = nullptr;
  /** Vertex shader. \e Must be specified. */
  GfxShader vertex;
  /** Tessellation control shader. \e Must only be used
   *  together with a tessellation evaluation shader. */
  GfxShader tessControl;
  /** Tessellation evaliation shader. \e Must only be
   *  used together with a tessellation control shader. */
  GfxShader tessEval;
  /** Geometry shader. This stage is entirely optional. */
  GfxShader geometry;
  /** Fragment shader. This stage is optional when
   *  rendering only to a depth-stencil image. */
  GfxShader fragment;

  bool operator == (const GfxGraphicsPipelineDesc&) const = default;
  bool operator != (const GfxGraphicsPipelineDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Mesh shader pipeline description
 */
struct GfxMeshPipelineDesc {
  /** Debug name. If not specified, the debug name for
   *  the pipeline will be inferred from the shaders. */
  const char* debugName = nullptr;
  /** Task shader. This stage is optional. */
  GfxShader task;
  /** Mesh shader. \e Must be specified. */
  GfxShader mesh;
  /** Fragment shader. This stage is optional when
   *  rendering only to a depth-stencil image. */
  GfxShader fragment;

  bool operator == (const GfxMeshPipelineDesc&) const = default;
  bool operator != (const GfxMeshPipelineDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Graphics pipeline interface
 *
 * Provides reflection info for the shaders
 * that the pipeline was created for.
 */
class GfxGraphicsPipelineIface {

public:

  GfxGraphicsPipelineIface(
    const GfxGraphicsPipelineDesc&      desc);

  GfxGraphicsPipelineIface(
    const GfxMeshPipelineDesc&          desc);

  virtual ~GfxGraphicsPipelineIface() { }

  /**
   * \brief Queries workgroup size
   *
   * Only valid for mesh shader pipelines.
   * \returns Mesh/Task workgroup size
   */
  virtual Extent3D getWorkgroupSize() const = 0;

  /**
   * \brief Checks whether the pipeline is available
   *
   * Availability indicates that the pipeline can be used for
   * rendering operations instantly without stalling. Note that
   * this is mostly a hint and stalls may occur either way on
   * some devices.
   * \returns \c true if the pipeline is available
   */
  virtual bool isAvailable() const = 0;

  /**
   * \brief Queries shader stages
   * \returns Shader stage mask
   */
  GfxShaderStages getShaderStages() const {
    return m_stages;
  }

protected:

  std::string     m_debugName;
  GfxShaderStages m_stages = 0;

};

/** See GfxGraphicsPipelineIface. */
using GfxGraphicsPipeline = PtrRef<GfxGraphicsPipelineIface>;


/**
 * \brief Compute pipeline description
 */
struct GfxComputePipelineDesc {
  /** Debug name. If not specified, the debug name
   *  for the pipeline will be that of the shader. */
  const char* debugName = nullptr;
  /** Compute shader */
  GfxShader compute;
};


/**
 * \brief Compute pipeline interface
 *
 * Provides reflection info for the compute
 * shader that the pipeline was created for.
 */
class GfxComputePipelineIface {

public:

  GfxComputePipelineIface(
    const GfxComputePipelineDesc&       desc);

  virtual ~GfxComputePipelineIface() { }

  /**
   * \brief Queries workgroup size
   *
   * Only valid for mesh shader pipelines.
   * \returns Mesh/Task workgroup size
   */
  virtual Extent3D getWorkgroupSize() const = 0;

  /**
   * \brief Checks whether the pipeline is available
   *
   * Availability indicates that the pipeline can be used
   * for dispatch operations instantly without stalling.
   * \returns \c true if the pipeline is available
   */
  virtual bool isAvailable() const = 0;

protected:

  std::string m_debugName;

};

/** See GfxComputePipelineIface. */
using GfxComputePipeline = PtrRef<GfxComputePipelineIface>;

}
