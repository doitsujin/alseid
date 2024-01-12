#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../util/util_hash.h"
#include "../../util/util_lock_free.h"
#include "../../util/util_small_vector.h"
#include "../../util/util_stream.h"

#include "../gfx_pipeline.h"
#include "../gfx_shader.h"

#include "gfx_vulkan_loader.h"

namespace as {

class GfxVulkanDevice;
class GfxVulkanPipelineManager;
class GfxVulkanRenderTargetState;

/**
 * \brief Dynamic state flags
 */
enum class GfxVulkanDynamicState : uint32_t {
  eViewports              = (1u << 0),
  eTessellationState      = (1u << 1),
  eRasterizerState        = (1u << 2),
  eConservativeRaster     = (1u << 3),
  eDepthStencilState      = (1u << 4),
  eDepthBoundsState       = (1u << 5),
  eDepthBounds            = (1u << 6),
  eStencilRef             = (1u << 7),
  eMultisampleState       = (1u << 8),
  eAlphaToCoverage        = (1u << 9),
  eBlendConstants         = (1u << 10),
  eShadingRate            = (1u << 11),
  eFlagEnum               = 0
};

using GfxVulkanDynamicStates = Flags<GfxVulkanDynamicState>;


/**
 * \brief Vulkan shader spec constant data
 *
 * Everything neatly laid out so that passing
 * this data to a shader is trivial.
 */
struct GfxVulkanSpecConstantData {
  uint32_t minSubgroupSize;
  uint32_t maxSubgroupSize;
  uint32_t taskShaderWorkgroupSize;
  uint32_t meshShaderWorkgroupSize;
  uint32_t meshShaderFlags;
};


/**
 * \brief Vulkan binding info
 */
struct GfxVulkanBindingInfo {
  /** Vulkan descriptor type */
  VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  /** Descriptor flags */
  VkDescriptorBindingFlags flags = 0;
  /** Binding index */
  uint32_t binding = 0;
  /** Descriptor count */
  uint32_t count = 0;
  /** Shader stages using this binding */
  VkShaderStageFlags stages = 0;

  bool operator == (const GfxVulkanBindingInfo&) const = default;
  bool operator != (const GfxVulkanBindingInfo&) const = default;

  size_t hash() const;
};


/**
 * \brief Vulkan descriptor set layout key
 */
struct GfxVulkanDescriptorLayoutKey {
  std::vector<GfxVulkanBindingInfo> bindings;

  bool operator == (const GfxVulkanDescriptorLayoutKey& other) const;
  bool operator != (const GfxVulkanDescriptorLayoutKey& other) const;

  size_t hash() const;
};


/**
 * \brief Vulkan descriptor set layout and template
 */
class GfxVulkanDescriptorLayout {

public:

  GfxVulkanDescriptorLayout(
    const GfxVulkanDevice&              device,
    const GfxVulkanDescriptorLayoutKey& key);

  ~GfxVulkanDescriptorLayout();

  /**
   * \brief Retrieves set layout
   * \returns Vulkan descriptor set layout
   */
  VkDescriptorSetLayout getSetLayout() const {
    return m_layout;
  }

  /**
   * \brief Retrieves update template
   * \returns Vulkan descriptor update tempate
   */
  VkDescriptorUpdateTemplate getTemplate() const {
    return m_template;
  }

  /**
   * \brief Checks whether the set layout is empty
   * \returns \c true if it contains no descriptors
   */
  bool isEmpty() const {
    return m_isEmpty;
  }

  /**
   * \brief Checks if the layout is a bindless layout
   * \returns \c true if this is a bindless layout
   */
  bool isBindless() const {
    return m_isBindless;
  }

private:

  const GfxVulkanDevice&      m_device;
  bool                        m_isEmpty     = false;
  bool                        m_isBindless  = false;

  VkDescriptorSetLayout       m_layout      = VK_NULL_HANDLE;
  VkDescriptorUpdateTemplate  m_template    = VK_NULL_HANDLE;

};


/**
 * \brief Vulkan pipeline layout key
 */
struct GfxVulkanPipelineLayoutKey {
  VkShaderStageFlags constantStages = 0;
  uint32_t constantBytes = 0;
  uint32_t descriptorSetCount = 0;

  std::array<const GfxVulkanDescriptorLayout*, GfxMaxDescriptorSets> descriptorSets = { };

  bool operator == (const GfxVulkanPipelineLayoutKey& other) const;
  bool operator != (const GfxVulkanPipelineLayoutKey& other) const;

  size_t hash() const;
};


/**
 * \brief Vulkan pipeline layout
 */
class GfxVulkanPipelineLayout {

public:

  GfxVulkanPipelineLayout(
    const GfxVulkanDevice&              device,
    const GfxVulkanPipelineLayoutKey&   key);

  ~GfxVulkanPipelineLayout();

  /**
   * \brief Retrieves pipeline layout
   * \returns Vulkan pipeline layout
   */
  VkPipelineLayout getLayout() const {
    return m_layout;
  }

  /**
   * \brief Queries number of descriptor sets
   * \returns Descriptor set count
   */
  uint32_t getSetCount() const {
    return m_key.descriptorSetCount;
  }

  /**
   * \brief Retrieves descriptor set layout
   *
   * \param [in] set Descriptor set index
   * \returns Descriptor set layout, may be \c nullptr
   */
  const GfxVulkanDescriptorLayout* getSetLayout(uint32_t set) const {
    return m_key.descriptorSets[set];
  }

  /**
   * \brief Queries non-empty descriptor set mask
   * \returns Mask of descriptor sets with non-zero descriptor counts.
   */
  uint32_t getNonemptySetMask() const {
    return m_nonemptySetMask;
  }

  /**
   * \brief Retrieves push constant info
   * \returns Push constant range
   */
  VkPushConstantRange getPushConstantInfo() const {
    VkPushConstantRange range;
    range.offset = 0;
    range.size = m_key.constantBytes;
    range.stageFlags = m_key.constantStages;
    return range;
  }

private:

  const GfxVulkanDevice&      m_device;
  GfxVulkanPipelineLayoutKey  m_key;
  uint32_t                    m_nonemptySetMask = 0;

  VkPipelineLayout            m_layout  = VK_NULL_HANDLE;

};


/**
 * \brief Render target output state
 */
class GfxVulkanRenderTargetState : public GfxRenderTargetStateIface {

public:

  GfxVulkanRenderTargetState(
          GfxVulkanPipelineManager&     mgr,
    const GfxRenderTargetStateDesc&     desc);

  ~GfxVulkanRenderTargetState();

  /**
   * \brief Queries Vulkan sample count
   * \returns Vulkan sample count
   */
  VkSampleCountFlagBits getSampleCount() const {
    return VkSampleCountFlagBits(m_desc.sampleCount);
  }

  /**
   * \brief Retrieves Vulkan rendering info
   * \returns Vulkan rendering info
   */
  VkPipelineRenderingCreateInfo getRtState() const {
    return m_rtState;
  }

private:

  std::array<VkFormat, GfxMaxColorAttachments> m_rtFormats = { };

  VkPipelineRenderingCreateInfo m_rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

};


/**
 * \brief Vulkan render state object
 *
 * Stores Vulkan pipeline state for all state
 * contained in the state object.
 */
class GfxVulkanRenderState : public GfxRenderStateIface {

public:

  GfxVulkanRenderState(
          GfxVulkanPipelineManager&     mgr,
    const GfxRenderStateData&           desc);

  ~GfxVulkanRenderState();

  /**
   * \brief Retrieves vertex input state
   * \returns Vertex input state
   */
  VkPipelineVertexInputStateCreateInfo getViState() const {
    return m_viState;
  }

  /**
   * \brief Retrieves input assembly state
   * \returns Input assembly state
   */
  VkPipelineInputAssemblyStateCreateInfo getIaState() const {
    return m_iaState;
  }

  /**
   * \brief Retrieves tessellation state
   * \returns Tessellation state
   */
  VkPipelineTessellationStateCreateInfo getTsState() const {
    return m_tsState;
  }

  /**
   * \brief Retrieves Vulkan rasterizer info
   * \returns Vulkan rasterizer info
   */
  VkPipelineRasterizationStateCreateInfo getRsState() const {
    return m_rsState;
  }

  /**
   * \brief Retrieves Vulkan conservative rasterization info
   * \returns Vulkan conservative rasterization info
   */
  VkPipelineRasterizationConservativeStateCreateInfoEXT getRsConservativeState() const {
    return m_rsConservative;
  }

  /**
   * \brief Retrieves Vulkan shading rate info
   * \returns Vulkan shading rate info
   */
  VkPipelineFragmentShadingRateStateCreateInfoKHR getSrState() const {
    return m_srState;
  }

  /**
   * \brief Retrieves Vulkan depth-stencil info
   * \returns Vulkan depth-stencil info
   */
  VkPipelineDepthStencilStateCreateInfo getDsState() const {
    return m_dsState;
  }

  /**
   * \brief Retrieves Vulkan color blend info
   *
   * \param [in] rtCount Number of render targets
   * \returns Vulkan color blend info
   */
  VkPipelineColorBlendStateCreateInfo getCbState(
          VkPipelineColorBlendAttachmentState* attachments,
          uint32_t                      rtCount,
          uint32_t                      fsOutputMask) const {
    for (uint32_t i = 0; i < rtCount; i++) {
      attachments[i] = (fsOutputMask & (1u << i))
        ? m_cbAttachments[i]
        : VkPipelineColorBlendAttachmentState();
    }

    VkPipelineColorBlendStateCreateInfo result = m_cbState;
    result.attachmentCount = rtCount;
    result.pAttachments = rtCount ? attachments : nullptr;
    return result;
  }

  /**
   * \brief Queries Vulkan sample count
   * \returns Vulkan sample count
   */
  VkSampleCountFlagBits getSampleCount() const {
    return m_msState.rasterizationSamples;
  }

  /**
   * \brief Retrieves sample mask
   * \returns Sample mask
   */
  VkSampleMask getSampleMask() const {
    return m_msMask;
  }

  /**
   * \brief Retrieves Vulkan multisample info
   *
   * \param [in] rtState Render target state to get sample count from
   * \param [in] sampleShading Whether to enable sample shading
   * \returns Vulkan multisample info
   */
  VkPipelineMultisampleStateCreateInfo getMsState(
    const GfxVulkanRenderTargetState&   rtState,
          bool                          sampleShading) const {
    VkPipelineMultisampleStateCreateInfo result = m_msState;

    if (rtState.getSampleCount())
      result.rasterizationSamples = rtState.getSampleCount();

    result.sampleShadingEnable = sampleShading;
    result.minSampleShading = sampleShading ? 1.0f : 0.0f;
    return result;
  }

  /**
   * \brief Queries dynamic state
   * \returns Dynamic state
   */
  VkPipelineDynamicStateCreateInfo getDyState() const {
    return m_dyState;
  }

  /**
   * \brief Queries vertex buffer mask
   *
   * The resulting bit mask will have a bit set for
   * each \c binding value that appears in the vertex
   * attribute array.
   * \returns Used vertex buffer mask
   */
  uint32_t getVertexBindingMask() const {
    return m_vertexBindingMask;
  }

private:

  std::array<VkVertexInputAttributeDescription, GfxMaxVertexAttributes> m_viAttributes = { };
  std::array<VkVertexInputBindingDescription,   GfxMaxVertexBindings>   m_viBindings   = { };

  VkPipelineVertexInputStateCreateInfo                  m_viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  VkPipelineInputAssemblyStateCreateInfo                m_iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  VkPipelineTessellationStateCreateInfo                 m_tsState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };

  VkPipelineRasterizationConservativeStateCreateInfoEXT m_rsConservative = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT };
  VkPipelineRasterizationStateCreateInfo                m_rsState        = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  VkPipelineFragmentShadingRateStateCreateInfoKHR       m_srState        = { VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR };

  VkPipelineDepthStencilStateCreateInfo                 m_dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

  std::array<VkPipelineColorBlendAttachmentState, GfxMaxColorAttachments> m_cbAttachments = { };

  VkPipelineColorBlendStateCreateInfo                   m_cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };

  VkSampleMask                                          m_msMask  = 0;
  VkPipelineMultisampleStateCreateInfo                  m_msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

  std::array<VkDynamicState, 8>                         m_dyList = { };
  VkPipelineDynamicStateCreateInfo                      m_dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

  uint32_t                                              m_vertexBindingMask = 0;

  void setupPrimitiveTopology(
          GfxVulkanPipelineManager&     mgr,
    const GfxPrimitiveTopology&         desc);

  void setupVertexLayout(
          GfxVulkanPipelineManager&     mgr,
    const GfxVertexLayout&              desc);

  void setupRasterizer(
          GfxVulkanPipelineManager&     mgr,
    const GfxRenderStateData&           desc);

  void setupDepthBias(
          GfxVulkanPipelineManager&     mgr,
    const GfxDepthBias&                 desc);

  void setupShadingRate(
          GfxVulkanPipelineManager&     mgr,
    const GfxShadingRate&               desc);

  void setupDepthTest(
          GfxVulkanPipelineManager&     mgr,
    const GfxDepthTest&                 desc);

  void setupStencilTest(
          GfxVulkanPipelineManager&     mgr,
    const GfxDepthTest&                 depth,
    const GfxStencilTest&               desc);

  void setupMultisampling(
          GfxVulkanPipelineManager&     mgr,
    const GfxMultisampling&             desc);

  void setupBlending(
          GfxVulkanPipelineManager&     mgr,
    const GfxBlending&                  desc);

  static VkStencilOpState getVkStencilState(
    const GfxStencilDesc&               desc);

};


/**
 * \brief Vertex input pipeline key
 */
struct GfxVulkanVertexInputKey {
  /** Pointer to normalized render state */
  const GfxVulkanRenderState* renderState = nullptr;

  bool operator == (const GfxVulkanVertexInputKey&) const = default;
  bool operator != (const GfxVulkanVertexInputKey&) const = default;

  size_t hash() const {
    return reinterpret_cast<uintptr_t>(renderState);
  }
};


/**
 * \brief Vulkan vertex input pipeline
 *
 * Manages a pipeline library object.
 */
class GfxVulkanVertexInputPipeline {

public:

  GfxVulkanVertexInputPipeline(
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanVertexInputKey&      key);

  ~GfxVulkanVertexInputPipeline();

  /**
   * \brief Retrieves Vulkan pipeline library
   *
   * May be \c VK_NULL_HANDLE if pipeline
   * libraries are not supported.
   * \returns Vulkan pipeline library
   */
  VkPipeline getHandle() const {
    return m_pipeline;
  }

private:

  GfxVulkanPipelineManager& m_mgr;
  VkPipeline                m_pipeline  = VK_NULL_HANDLE;

};


/**
 * \brief Fragment output pipeline key
 */
struct GfxVulkanFragmentOutputKey {
  /** Pointer to normalized render state */
  const GfxVulkanRenderState* renderState = nullptr;
  /** Pointer to render target state */
  const GfxVulkanRenderTargetState* targetState = nullptr;
  /** Whether to enable sample rate shading */
  VkBool32 sampleShading = VK_FALSE;
  /** Fragment shader output mask */
  GfxShaderIoMask shaderIoMasks;

  bool operator == (const GfxVulkanFragmentOutputKey&) const = default;
  bool operator != (const GfxVulkanFragmentOutputKey&) const = default;

  size_t hash() const {
    HashState result;
    result.add(reinterpret_cast<uintptr_t>(renderState));
    result.add(reinterpret_cast<uintptr_t>(targetState));
    result.add(uint32_t(sampleShading));
    result.add(uint32_t(shaderIoMasks.inputMask));
    result.add(uint32_t(shaderIoMasks.outputMask));
    return result;
  }
};


/**
 * \brief Vulkan fragment output pipeline
 *
 * Manages a pipeline library object.
 */
class GfxVulkanFragmentOutputPipeline {

public:

  GfxVulkanFragmentOutputPipeline(
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanFragmentOutputKey&   key);

  ~GfxVulkanFragmentOutputPipeline();

  /**
   * \brief Retrieves Vulkan pipeline library
   *
   * May be \c VK_NULL_HANDLE if pipeline
   * libraries are not supported.
   * \returns Vulkan pipeline library
   */
  VkPipeline getHandle() const {
    return m_pipeline;
  }

  /**
   * \brief Retrieves dynamic state flags
   * \returns Dynamic state flags
   */
  GfxVulkanDynamicStates getDynamicStateFlags() const {
    return m_dynamic;
  }

private:

  GfxVulkanPipelineManager& m_mgr;
  GfxVulkanDynamicStates    m_dynamic   = 0;
  VkPipeline                m_pipeline  = VK_NULL_HANDLE;

};


/**
 * \brief Additional shader module parameters
 */
struct GfxVulkanShaderStageExtraInfo {
  VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  VkPipelineShaderStageRequiredSubgroupSizeCreateInfo requiredSubgroupSize = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO };
};


/**
 * \brief Helper to build shader stage arrays
 */
struct GfxVulkanGraphicsShaderStages {
  uint32_t freeMask = 0;
  VkSpecializationInfo specInfo = { };
  small_vector<GfxVulkanShaderStageExtraInfo, 5> extaInfo;
  small_vector<VkPipelineShaderStageCreateInfo, 5> stageInfo;
};


/**
 * \brief Graphics shaders
 */
class GfxVulkanGraphicsShaders {

public:

  explicit GfxVulkanGraphicsShaders(
    const GfxGraphicsPipelineDesc&      desc);

  explicit GfxVulkanGraphicsShaders(
    const GfxMeshPipelineDesc&          desc);

  /**
   * \brief Sets up shader stage info
   *
   * \param [out] result Shader stage info
   * \param [in] manager Pipeline manager
   * \param [in] specData Specialization data
   */
  void getShaderStageInfo(
          GfxVulkanGraphicsShaderStages& result,
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanSpecConstantData*    specData) const;

private:

  small_vector<GfxShader, 5> m_shaders;

  void addShader(
    const GfxShader&                    shader);

};




/**
 * \brief Graphics pipeline key
 *
 * Stores hashes or graphics pipeline shaders.
 */
struct GfxVulkanGraphicsPipelineKey {
  GfxVulkanGraphicsPipelineKey() { }

  GfxVulkanGraphicsPipelineKey(
    const GfxGraphicsPipelineDesc&      desc);

  GfxVulkanGraphicsPipelineKey(
    const GfxMeshPipelineDesc&          desc);

  std::array<UniqueHash, 5> hashes;

  size_t hash() const {
    HashState hash;
    for (const auto& h : hashes)
      hash.add(h.hash());
    return hash;
  }

  bool operator == (const GfxVulkanGraphicsPipelineKey&) const = default;
  bool operator != (const GfxVulkanGraphicsPipelineKey&) const = default;
};


/**
 * \brief Vulkan graphics pipeline variant key
 */
struct GfxVulkanGraphicsPipelineVariantKey {
  /** Render state object */
  const GfxVulkanRenderState* renderState = nullptr;
  /** Render target state object */
  const GfxVulkanRenderTargetState* targetState = nullptr;

  bool operator == (const GfxVulkanGraphicsPipelineVariantKey&) const = default;
  bool operator != (const GfxVulkanGraphicsPipelineVariantKey&) const = default;
};


/**
 * \brief Vulkan graphics pipeline variant info
 */
struct GfxVulkanGraphicsPipelineVariant {
  /** Linked or compiled pipeline handle */
  VkPipeline pipeline = VK_NULL_HANDLE;
  /** Dynamic states used by this pipeline */
  GfxVulkanDynamicStates  dynamicStates = 0;
};


/**
 * \brief Vulkan graphics pipeline
 */
class GfxVulkanGraphicsPipeline : public GfxGraphicsPipelineIface {

public:

  GfxVulkanGraphicsPipeline(
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanPipelineLayout&      layout,
    const GfxGraphicsPipelineDesc&      desc);

  GfxVulkanGraphicsPipeline(
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanPipelineLayout&      layout,
    const GfxMeshPipelineDesc&          desc);

  ~GfxVulkanGraphicsPipeline();

  /**
   * \brief Checks whether fast linking is supported
   * \returns \c true if the pipeline can be linked
   */
  bool supportsFastLink() const {
    return m_canLink;
  }

  /**
   * \brief Checks whether pipeline has sample rate shading
   * \returns \c true if the fragment shader runs at sample rate
   */
  bool hasSampleRateShading() const {
    return m_sampleRateShading;
  }

  /**
   * \brief Retrieves pipeline layout
   * \returns Pipeline layout
   */
  const GfxVulkanPipelineLayout* getPipelineLayout() const {
    return &m_layout;
  }

  /**
   * \brief Retrieves pipeline variant with the given state
   *
   * Will perform the following actions in order, until
   * a valid Vulkan pipeline handle is found:
   * - Look up existing optimized pipeline variant
   * - Look up existing linked pipeline variant
   * - Fast-link the pipeline if possible
   * - Compile optimized variant (will cause stutter)
   * \param [in] key Variant key
   * \returns Pipeline handle
   */
  GfxVulkanGraphicsPipelineVariant getVariant(
    const GfxVulkanGraphicsPipelineVariantKey& key);

  /**
   * \brief Compiles pipeline variant with the given state
   *
   * \param [in] key Variant key
   * \returns Pipeline handle
   */
  GfxVulkanGraphicsPipelineVariant createVariant(
    const GfxVulkanGraphicsPipelineVariantKey& key);

  /**
   * \brief Compiles shader pipeline library
   * \returns Pipeline library handle
   */
  GfxVulkanGraphicsPipelineVariant createLibrary();

  /**
   * \brief Queries actual workgroup size
   * \returns Workgroup size
   */
  Extent3D getWorkgroupSize() const override;

  /**
   * \brief Checks whether the pipeline is available
   * \returns \c true if the pipeline is available
   */
  bool isAvailable() const override;

private:

  struct LinkedVariant {
    LinkedVariant(
      const GfxVulkanGraphicsPipelineVariantKey& k,
      const GfxVulkanGraphicsPipelineVariant& v)
    : key(k), variant(v) { }

    GfxVulkanGraphicsPipelineVariantKey key;
    GfxVulkanGraphicsPipelineVariant    variant;
  };

  struct OptimizedVariant {
    OptimizedVariant(
      const GfxVulkanGraphicsPipelineVariantKey& k,
      const GfxVulkanGraphicsPipelineVariant& v)
    : key(k), dynamicStates(v.dynamicStates), pipeline(v.pipeline) { }

    GfxVulkanGraphicsPipelineVariantKey key;
    GfxVulkanDynamicStates              dynamicStates = 0;
    std::atomic<VkPipeline>             pipeline = { VK_NULL_HANDLE };

    GfxVulkanGraphicsPipelineVariant getVariant() {
      GfxVulkanGraphicsPipelineVariant result;
      result.pipeline = pipeline.load(std::memory_order_acquire);
      result.dynamicStates = dynamicStates;
      return result;
    }

    void setVariant(const GfxVulkanGraphicsPipelineVariant& v) {
      dynamicStates = v.dynamicStates;

      // Set this last and make sure that prior writes are visible,
      // since threads may concurrently scan the pipeline list
      pipeline.store(v.pipeline, std::memory_order_release);
    }
  };

  struct LookupResult {
    GfxVulkanGraphicsPipelineVariant  variant;
    VkBool32                          found;
  };

  GfxVulkanPipelineManager&         m_mgr;
  const GfxVulkanPipelineLayout&    m_layout;
  GfxVulkanGraphicsShaders          m_shaders;
  GfxVulkanSpecConstantData         m_specConstants;

  Extent3D                          m_workgroupSize = Extent3D(0, 0, 0);
  GfxShaderIoMask                   m_shaderIoMask;

  GfxVulkanGraphicsPipelineVariant  m_library;
  VkBool32                          m_sampleRateShading = VK_FALSE;
  VkBool32                          m_canLink           = VK_FALSE;

  std::mutex                        m_linkedMutex;
  LockFreeList<LinkedVariant>       m_linkedVariants;

  std::mutex                        m_optimizedMutex;
  LockFreeList<OptimizedVariant>    m_optimizedVariants;

  std::atomic<bool>                 m_isAvailable = { false };

  GfxVulkanGraphicsPipelineVariant lookupLinked(
    const GfxVulkanGraphicsPipelineVariantKey& key) const;

  LookupResult lookupOptimized(
    const GfxVulkanGraphicsPipelineVariantKey& key) const;

  GfxVulkanGraphicsPipelineVariant createLibraryLocked();

  GfxVulkanGraphicsPipelineVariant createVariantLocked(
    const GfxVulkanGraphicsPipelineVariantKey& key) const;

  GfxVulkanGraphicsPipelineVariant linkVariant(
    const GfxVulkanGraphicsPipelineVariantKey& key);

  bool canLinkVariant(
    const GfxVulkanGraphicsPipelineVariantKey& key) const;

  void deferCreateVariant(
    const GfxVulkanGraphicsPipelineVariantKey& key);

  bool canFastLink() const;

  bool supportsFragmentShadingRate() const;

  static bool hasSampleRateShading(
    const GfxShader&                    fragmentShader);

};


/**
 * \brief Vulkan compute pipeline
 */
class GfxVulkanComputePipeline : public GfxComputePipelineIface {

public:

  GfxVulkanComputePipeline(
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanPipelineLayout&      layout,
    const GfxComputePipelineDesc&       desc);

  ~GfxVulkanComputePipeline();

  /**
   * \brief Retrieves pipeline handle
   *
   * Compiles the pipeline on demand if necessary.
   * \returns Pipeline handle
   */
  VkPipeline getHandle() {
    VkPipeline handle = m_pipeline.load();

    if (unlikely(!handle))
      handle = createPipeline();

    return handle;
  }

  /**
   * \brief Retrieves pipeline layout
   * \returns Pipeline layout
   */
  const GfxVulkanPipelineLayout* getPipelineLayout() const {
    return &m_layout;
  }

  /**
   * \brief Queries actual workgroup size
   * \returns Workgroup size
   */
  Extent3D getWorkgroupSize() const override;

  /**
   * \brief Checks whether the pipeline is available
   * \returns \c true if the pipeline is available
   */
  bool isAvailable() const override;

  /**
   * \brief Compiles pipeline
   * \returns Pipeline handle
   */
  VkPipeline createPipeline();

private:

  GfxVulkanPipelineManager&       m_mgr;
  const GfxVulkanPipelineLayout&  m_layout;

  GfxComputePipelineDesc    m_desc;
  GfxVulkanSpecConstantData m_specConstants;

  Extent3D                  m_workgroupSize = Extent3D(0, 0, 0);

  std::mutex                m_mutex;
  std::atomic<VkPipeline>   m_pipeline = { VK_NULL_HANDLE };

  VkPipeline createPipelineLocked();

};


/**
 * \brief Vulkan pipeline manager
 *
 * Manages all pipeline-related objects, including descriptor set
 * layouts, pipeline layouts and pipeline objects themselves, and
 * implements background compilation for pipelines.
 */
class GfxVulkanPipelineManager {

public:

  GfxVulkanPipelineManager(
          GfxVulkanDevice&              device);

  ~GfxVulkanPipelineManager();

  /**
   * \brief Queries Vulkan device
   * \returns Vulkan device
   */
  GfxVulkanDevice& device() const {
    return m_device;
  }

  /**
   * \brief Queries specialization constant data
   *
   * Some specialization constants may need to be adjusted
   * afterwards depending on the pipeline properties.
   * \returns Default spec constant data
   */
  GfxVulkanSpecConstantData getDefaultSpecConstants() const;

  /**
   * \brief Initializes a shader stage struct
   *
   * Creates a shader module as necessary, which \e must be
   * freed by the caller after creating the pipeline.
   * \param [in] shader Shader object
   * \param [in] specInfo Specialization info
   * \param [out] stageInfo Vulkan shader stage info
   * \param [out] extraInfo Additional stage parameters
   * \returns \c true if the code in the returned
   *    shader module create info must be freed
   */
  bool initShaderStage(
    const GfxShader&                    shader,
    const VkSpecializationInfo*         specInfo,
          VkPipelineShaderStageCreateInfo& stageInfo,
          GfxVulkanShaderStageExtraInfo& extraInfo,
    const GfxVulkanSpecConstantData*    specData) const;

  /**
   * \brief Initializes spec constant data
   *
   * Fills the property struct in question and populates
   * the specialization constant data array itself.
   * \param [out] specData Specialization data
   * \param [out] specInfo Specialization info
   */
  void initSpecializationInfo(
    const GfxVulkanSpecConstantData*    specData,
          VkSpecializationInfo&         specInfo) const;

  /**
   * \brief Creates descriptor array layout
   *
   * \param [in] type Descriptor type
   * \returns Descriptor layout
   */
  const GfxVulkanDescriptorLayout* getDescriptorArrayLayout(
          GfxShaderBindingType          type);

  /**
   * \brief Creates compute pipeline
   *
   * \param [in] desc Pipeline description
   * \returns Compute pipeline
   */
  GfxVulkanComputePipeline& createComputePipeline(
    const GfxComputePipelineDesc&       desc);

  /**
   * \brief Creates graphics pipeline
   *
   * \param [in] desc Pipeline description
   * \returns Graphics pipeline
   */
  GfxVulkanGraphicsPipeline& createGraphicsPipeline(
    const GfxGraphicsPipelineDesc&      desc);

  /**
   * \brief Creates graphics pipeline
   *
   * \param [in] desc Pipeline description
   * \returns Graphics pipeline
   */
  GfxVulkanGraphicsPipeline& createGraphicsPipeline(
    const GfxMeshPipelineDesc&          desc);

  /**
   * \brief Creates a vertex input pipeline
   *
   * Note that this will generally try to reduce the number
   * of redundant pipelines by normalizing render state.
   * \param [in] renderState Render state object. Must
   *    contain valid vertex and input assembly state.
   * \returns Vertex input pipeline for the given
   *    render state object.
   */
  GfxVulkanVertexInputPipeline& createVertexInputPipeline(
    const GfxVulkanRenderState&         renderState);

  /**
   * \brief Creates a fragment output pipeline
   *
   * Note that this will generally try to reduce the number
   * of redundant pipelines by normalizing render state.
   * \param [in] targetState Render target state.
   * \param [in] renderState Render state object. Must
   *    contain valid blend and multisample states.
   * \param [in] sampleShading Whether to enable sample shading.
   * \param [in] shaderIoMasks Shader input and output masks
   * \returns Vertex input pipeline for the given
   *    render state object.
   */
  GfxVulkanFragmentOutputPipeline& createFragmentOutputPipeline(
    const GfxVulkanRenderTargetState&   targetState,
    const GfxVulkanRenderState&         renderState,
          VkBool32                      sampleShading,
    const GfxShaderIoMask&              shaderIoMasks);

  /**
   * \brief Creates render state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanRenderState& createRenderState(
    const GfxRenderStateData&           desc) {
    return createStateObject(m_renderStates, desc);
  }

  /**
   * \brief Creates render target state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanRenderTargetState& createRenderTargetState(
    const GfxRenderTargetStateDesc&     desc) {
    return createStateObject(m_renderTargetStates, desc);
  }

  /**
   * \brief Asynchronously compiles pipeline variant
   *
   * \param [in] pipeline Graphics pipeline
   * \param [in] key Variant key
   */
  void deferCreateGraphicsPipelineVariant(
          GfxVulkanGraphicsPipeline&    pipeline,
    const GfxVulkanGraphicsPipelineVariantKey& key);

private:

  enum class WorkItemType : uint32_t {
    eComputePipeline,
    eGraphicsPipeline,
    eGraphicsVariant,
  };

  struct WorkItem {
    WorkItem(GfxVulkanComputePipeline& p)
    : type            (WorkItemType::eComputePipeline)
    , computePipeline (&p) { }

    WorkItem(GfxVulkanGraphicsPipeline& p)
    : type            (WorkItemType::eGraphicsPipeline)
    , graphicsPipeline(&p) { }

    WorkItem(GfxVulkanGraphicsPipeline& p,
      const GfxVulkanGraphicsPipelineVariantKey& key)
    : type            (WorkItemType::eGraphicsVariant)
    , graphicsPipeline(&p)
    , graphicsState   (key) { }

    WorkItemType type;

    union {
      GfxVulkanGraphicsPipeline* graphicsPipeline;
      GfxVulkanComputePipeline* computePipeline;
    };

    GfxVulkanGraphicsPipelineVariantKey graphicsState;
  };

  GfxVulkanDevice&        m_device;

  std::mutex              m_mutex;

  std::unordered_map<
    GfxVulkanDescriptorLayoutKey,
    GfxVulkanDescriptorLayout,
    HashMemberProc>       m_descriptorSetLayouts;

  std::unordered_map<
    GfxVulkanPipelineLayoutKey,
    GfxVulkanPipelineLayout,
    HashMemberProc>       m_pipelineLayouts;

  std::unordered_map<
    GfxRenderStateData,
    GfxVulkanRenderState,
    HashMemberProc>       m_renderStates;

  std::unordered_map<
    GfxVulkanVertexInputKey,
    GfxVulkanVertexInputPipeline,
    HashMemberProc>       m_vertexInputPipelines;

  std::unordered_map<
    GfxVulkanFragmentOutputKey,
    GfxVulkanFragmentOutputPipeline,
    HashMemberProc>       m_fragmentOutputPipelines;

  std::unordered_map<
    GfxRenderTargetStateDesc,
    GfxVulkanRenderTargetState,
    HashMemberProc>       m_renderTargetStates;

  std::unordered_map<
    UniqueHash,
    GfxVulkanComputePipeline,
    HashMemberProc>       m_computePipelines;

  std::unordered_map<
    GfxVulkanGraphicsPipelineKey,
    GfxVulkanGraphicsPipeline,
    HashMemberProc>       m_graphicsPipelines;

  std::mutex              m_compilerMutex;
  bool                    m_compilerStopped = false;
  std::condition_variable m_compilerCond;
  std::queue<WorkItem>    m_compilerQueue;
  std::vector<std::thread> m_compilerThreads;

  template<typename Map>
  Map::mapped_type& createStateObject(Map& map, const Map::key_type& key) {
    std::lock_guard lock(m_mutex);
    return createStateObjectLocked<Map>(map, key);
  }

  template<typename Map>
  Map::mapped_type& createStateObjectLocked(Map& map, const Map::key_type& key) {
    auto entry = map.find(key);

    if (entry != map.end())
      return entry->second;

    auto insert = map.emplace(std::piecewise_construct,
      std::forward_as_tuple(key),
      std::forward_as_tuple(*this, key));
    return insert.first->second;
  }

  const GfxVulkanDescriptorLayout* getDescriptorLayoutLocked(
    const GfxVulkanDescriptorLayoutKey& key);

  const GfxVulkanDescriptorLayout* getDescriptorArrayLayoutLocked(
          GfxShaderBindingType          type);

  const GfxVulkanPipelineLayout* getPipelineLayoutLocked(
    const GfxVulkanPipelineLayoutKey&   key);

  const GfxVulkanPipelineLayout* getPipelineLayoutForShadersLocked(
          uint32_t                      shaderCount,
    const GfxShader*                    shaders);

  const GfxVulkanPipelineLayout* getGraphicsPipelineLayoutLocked(
    const GfxGraphicsPipelineDesc&      desc);

  const GfxVulkanPipelineLayout* getGraphicsPipelineLayoutLocked(
    const GfxMeshPipelineDesc&          desc);

  template<typename T>
  GfxVulkanGraphicsPipeline& createGraphicsPipelineTyped(
    const T&                            desc);

  uint32_t getMaxDescriptorCountForType(
          VkDescriptorType              type) const;

  void runWorker();

};

}
