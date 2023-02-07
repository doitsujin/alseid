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
  /** Input rate. This \e must be the same for all
   *  attributes that use the same \c binding index. */
  GfxInputRate inputRate = GfxInputRate::ePerVertex;

  bool operator == (const GfxVertexInputAttribute&) const = default;
  bool operator != (const GfxVertexInputAttribute&) const = default;

  size_t hash() const;
};


/**
 * \brief Vertex input state description
 */
struct GfxVertexInputStateDesc {
  /** Primitive topology. Defines both rasterization behaviour and
   *  the way vertex data will be passed to the vertex shader. */
  GfxPrimitiveType primitiveTopology = GfxPrimitiveType::eTriangleList;
  /** Patch vertex count for tessellation pipelines. */
  uint32_t patchVertexCount = 0;
  /** Vertex attributes. The \c n-th entry in this array will define
   *  the data source for input location \c n in the vertex shader,
   *  so this array may be sparely populated. Entries with a format
   *  of \c GfxFormat::eUnknown are considered to be unused. */
  std::array<GfxVertexInputAttribute, GfxMaxVertexAttributes> attributes;

  /**
   * \brief Checks whether primitive restart is enabled
   * \returns \c true for strip topologies.
   */
  bool isPrimitiveRestartEnabled() const {
    return primitiveTopology == GfxPrimitiveType::eLineStrip
        || primitiveTopology == GfxPrimitiveType::eTriangleStrip;
  }

  bool operator == (const GfxVertexInputStateDesc&) const = default;
  bool operator != (const GfxVertexInputStateDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Vertex input state interface
 */
class GfxVertexInputStateIface {

public:

  GfxVertexInputStateIface(
    const GfxVertexInputStateDesc&      desc);

  virtual ~GfxVertexInputStateIface() { }

  /**
   * \brief Retrieves state description
   * \returns State description
   */
  GfxVertexInputStateDesc getDesc() const {
    return m_desc;
  }

  /**
   * \brief Retrieves vertex buffer mask
   * \returns Vertex buffer mask
   */
  uint32_t getVertexBufferMask() const {
    return m_vertexBufferMask;
  }

protected:

  GfxVertexInputStateDesc m_desc;
  uint32_t m_vertexBufferMask = 0;

};

using GfxVertexInputState = PtrRef<GfxVertexInputStateIface>;


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
 * \brief Rasterizer state description
 */
struct GfxRasterizerStateDesc {
  /** Primitive winding order */
  GfxFrontFace frontFace = GfxFrontFace::eCcw;
  /** Primitive cull mode */
  GfxCullMode cullMode = GfxCullMode::eNone;
  /** Conservative rasterization (overestimate) */
  bool conservativeRasterization = false;
  /** Depth bias state. */
  float depthBias = 0.0f;
  float depthBiasSlope = 0.0f;
  float depthBiasClamp = 0.0f;

  /**
   * \brief Checks whether depth bias is enabled
   *
   * Convenience method for backends.
   * \returns \c true if depth bias is enabled
   */
  bool isDepthBiasEnabled() const;

  bool operator == (const GfxRasterizerStateDesc&) const = default;
  bool operator != (const GfxRasterizerStateDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Rasterizer state interface
 */
class GfxRasterizerStateIface {

public:

  GfxRasterizerStateIface(
    const GfxRasterizerStateDesc&       desc)
  : m_desc(desc) { }

  virtual ~GfxRasterizerStateIface() { }

  /**
   * \brief Retrieves state description
   * \returns State description
   */
  GfxRasterizerStateDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxRasterizerStateDesc m_desc;

};

using GfxRasterizerState = PtrRef<GfxRasterizerStateIface>;


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
 * \brief Depth-stencil state description
 */
struct GfxDepthStencilStateDesc {
  /** Enables depth writes. */
  bool enableDepthWrite = false;
  /** Enables the depth bounds test. Depth bound values to
   *  test against can be set dynamically on the context. */
  bool enableDepthBoundsTest = false;
  /** Depth compare op. If this is \c GfxCompareOp::eAlways
   *  and depth writes are disabled, the depth test will
   *  effectively be disabled entirely. */
  GfxCompareOp depthCompareOp = GfxCompareOp::eAlways;
  /** Front face stencil operation */
  GfxStencilDesc front;
  /** Back face stencil operation */
  GfxStencilDesc back;

  /**
   * \brief Checks whether the depth test is enabled
   * \returns \c true if depth gets accessed
   */
  bool isDepthTestEnabled() const;

  /**
   * \brief Checks whether stencil test is used
   * \returns \c true if stencil gets accessed
   */
  bool isStencilTestEnabled() const;

  /**
   * \brief Checks whether stencil writes are enabled
   * \returns \c true if stencil gets written
   */
  bool isStencilWriteEnabled() const;

  bool operator == (const GfxDepthStencilStateDesc&) const = default;
  bool operator != (const GfxDepthStencilStateDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Depth-stencil state interface
 */
class GfxDepthStencilStateIface {

public:

  GfxDepthStencilStateIface(
    const GfxDepthStencilStateDesc&     desc)
  : m_desc(desc) { }

  virtual ~GfxDepthStencilStateIface() { }

  /**
   * \brief Retrieves state description
   * \returns State description
   */
  GfxDepthStencilStateDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxDepthStencilStateDesc m_desc;

};

using GfxDepthStencilState = PtrRef<GfxDepthStencilStateIface>;


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
 * \brief Blend state description
 *
 * Stores blend state for individual render
 * targets, as well as logic op state.
 */
struct GfxColorBlendStateDesc {
  /** Logic op. If this is \c GfxLogicOp::eCopySrc,
   *  the logic op is effectively disabled. */
  GfxLogicOp logicOp = GfxLogicOp::eSrc;
  /** Blend state for individual render targets. */
  std::array<GfxRenderTargetBlend, GfxMaxColorAttachments> renderTargets;

  /**
   * \brief Checks whether logic op is enabled
   * \returns \c true if logic op is enabled
   */
  bool isLogicOpEnabled() const;

  bool operator == (const GfxColorBlendStateDesc&) const = default;
  bool operator != (const GfxColorBlendStateDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Blend state interface
 */
class GfxColorBlendStateIface {

public:

  GfxColorBlendStateIface(
    const GfxColorBlendStateDesc&       desc)
  : m_desc(desc) { }

  virtual ~GfxColorBlendStateIface() { }

  /**
   * \brief Retrieves state description
   * \returns State description
   */
  GfxColorBlendStateDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxColorBlendStateDesc m_desc;

};

using GfxColorBlendState = PtrRef<GfxColorBlendStateIface>;


/**
 * \brief Multisample state description
 *
 * Defines a sample count override in case no render
 * targets are bound, as well as other related state.
 */
struct GfxMultisampleStateDesc {
  /** Sample count override. Only has an effect when
   *  no render targets are bound to the pipeline. */
  uint32_t sampleCount = 0;
  /** Sample mask. By default, all samples are enabled. */
  uint32_t sampleMask = ~0u;
  /** Whether to enable alpha-to-coverage */
  bool enableAlphaToCoverage = false;

  bool operator == (const GfxMultisampleStateDesc&) const = default;
  bool operator != (const GfxMultisampleStateDesc&) const = default;

  size_t hash() const;
};


/**
 * \brief Multisample state interface
 */
class GfxMultisampleStateIface {

public:

  GfxMultisampleStateIface(
    const GfxMultisampleStateDesc&      desc)
  : m_desc(desc) { }

  virtual ~GfxMultisampleStateIface() { }

  /**
   * \brief Retrieves state description
   * \returns State description
   */
  GfxMultisampleStateDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxMultisampleStateDesc m_desc;

};

using GfxMultisampleState = PtrRef<GfxMultisampleStateIface>;


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

using GfxRenderTargetState = PtrRef<GfxRenderTargetStateIface>;


/**
 * \brief Graphics state description
 *
 * Accumulates all graphics state objects. This is only used to
 * link graphics pipelines with a given set of render state.
 */
struct GfxGraphicsStateDesc {
  /** Vertex input state. If the graphics pipeline uses mesh
   *  shaders, this \e must be \c nullptr, otherwise this
   *  \e must be a vertex input state object compatible with
   *  the vertex shader's stage I/O. */
  GfxVertexInputState vertexInputState;
  /** Rasterization state. */
  GfxRasterizerState rasterizerState;
  /** Depth-stencil state. If no depth-stencil attachment
   *  format is specified, this \e must be \c nullptr. */
  GfxDepthStencilState depthStencilState;
  /** Color blend state. */
  GfxColorBlendState colorBlendState;
  /** Multisample state. */
  GfxMultisampleState multisampleState;
  /** Render target state. */
  GfxRenderTargetState renderTargetState;

  bool operator == (const GfxGraphicsStateDesc&) const = default;
  bool operator != (const GfxGraphicsStateDesc&) const = default;

  size_t hash() const;
};


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
   * \brief Compiles a pipeline variant with the given state
   *
   * Compilation will be performed asynchronously, if at all.
   * This may be useful for devices that do not support fast
   * linking for pipelines.
   * \param [in] state Pipeline state vector
   */
  virtual void compileVariant(
    const GfxGraphicsStateDesc&         state) = 0;

  /**
   * \brief Queries workgroup size
   *
   * Only valid for mesh shader pipelines.
   * \returns Mesh/Task workgroup size
   */
  Extent3D getWorkgroupSize() const {
    return m_workgroupSize;
  }

  /**
   * \brief Queries shader stages
   * \returns Shader stage mask
   */
  GfxShaderStages getShaderStages() const {
    return m_stages;
  }

protected:

  std::string     m_debugName;
  Extent3D        m_workgroupSize = Extent3D(0, 0, 0);
  GfxShaderStages m_stages        = 0;

};

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
   * \brief Checks whether the pipeline is available
   *
   * Availability indicates that the pipeline can be used
   * for dispatch operations instantly without stalling.
   * \returns \c true if the pipeline is available
   */
  virtual bool isAvailable() const = 0;

  /**
   * \brief Queries workgroup size
   * \returns Compute workgroup size
   */
  Extent3D getWorkgroupSize() const {
    return m_workgroupSize;
  }

protected:

  std::string m_debugName;
  Extent3D    m_workgroupSize;

};

using GfxComputePipeline = PtrRef<GfxComputePipelineIface>;

}
