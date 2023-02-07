#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../util/util_bytestream.h"
#include "../../util/util_hash.h"
#include "../../util/util_lock_free.h"
#include "../../util/util_small_vector.h"

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
  eFlagEnum               = 0
};

using GfxVulkanDynamicStates = Flags<GfxVulkanDynamicState>;


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
 * \brief Vulkan vertex input state
 *
 * Manages a vertex input pipeline library.
 */
class GfxVulkanVertexInputState : public GfxVertexInputStateIface {

public:

  GfxVulkanVertexInputState(
          GfxVulkanPipelineManager&     mgr,
    const GfxVertexInputStateDesc&      desc);

  ~GfxVulkanVertexInputState();

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
   * \brief Retrieves dynamic state flags
   * \returns Dynamic state flags
   */
  GfxVulkanDynamicStates getDynamicStateFlags() const {
    return m_dynamic;
  }

  /**
   * \brief Appends dynamic states to a given vector
   * \param [out] container Vector containing dynamic states
   */
  template<typename T>
  void getDynamicStates(T& container) {
    for (uint32_t i = 0; i < m_dyState.dynamicStateCount; i++)
      container.push_back(m_dyState.pDynamicStates[i]);
  }

private:

  GfxVulkanPipelineManager&               m_mgr;
  GfxVulkanDynamicStates                  m_dynamic = 0;

  std::array<VkVertexInputAttributeDescription, GfxMaxVertexAttributes> m_viAttributes = { };
  std::array<VkVertexInputBindingDescription,   GfxMaxVertexBindings>   m_viBindings   = { };
  std::array<VkDynamicState,                    1>                      m_dyList       = { };

  VkPipelineVertexInputStateCreateInfo    m_viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  VkPipelineInputAssemblyStateCreateInfo  m_iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  VkPipelineTessellationStateCreateInfo   m_tsState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
  VkPipelineDynamicStateCreateInfo        m_dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

  VkPipeline                              m_pipeline  = VK_NULL_HANDLE;

  void createLibrary();

};


/**
 * \brief Vulkan rasterization state
 */
class GfxVulkanRasterizerState : public GfxRasterizerStateIface {

public:

  GfxVulkanRasterizerState(
          GfxVulkanPipelineManager&     mgr,
    const GfxRasterizerStateDesc&       desc);

  ~GfxVulkanRasterizerState();

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

private:

  VkPipelineRasterizationConservativeStateCreateInfoEXT m_rsConservative = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT };
  VkPipelineRasterizationStateCreateInfo                m_rsState        = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

};


/**
 * \brief Vulkan depth-stencil state
 */
class GfxVulkanDepthStencilState : public GfxDepthStencilStateIface {

public:

  GfxVulkanDepthStencilState(
          GfxVulkanPipelineManager&     mgr,
    const GfxDepthStencilStateDesc&     desc);

  ~GfxVulkanDepthStencilState();

  /**
   * \brief Retrieves Vulkan depth-stencil info
   * \returns Vulkan depth-stencil info
   */
  VkPipelineDepthStencilStateCreateInfo getDsState() const {
    return m_dsState;
  }

  /**
   * \brief Appends dynamic states to a given vector
   * \param [out] container Vector containing dynamic states
   */
  template<typename T>
  void getDynamicStates(T& container) {
    for (uint32_t i = 0; i < m_dyState.dynamicStateCount; i++)
      container.push_back(m_dyState.pDynamicStates[i]);
  }

private:

  std::array<VkDynamicState, 2> m_dyList = { };

  VkPipelineDepthStencilStateCreateInfo m_dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
  VkPipelineDynamicStateCreateInfo      m_dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

  static VkStencilOpState getVkStencilState(
    const GfxStencilDesc&               desc);

};


/**
 * \brief Vulkan blend state
 */
class GfxVulkanColorBlendState : public GfxColorBlendStateIface {

public:

  GfxVulkanColorBlendState(
          GfxVulkanPipelineManager&     mgr,
    const GfxColorBlendStateDesc&       desc);

  ~GfxVulkanColorBlendState();

  /**
   * \brief Retrieves Vulkan color blend info
   *
   * \param [in] rtCount Number of render targets
   * \returns Vulkan color blend info
   */
  VkPipelineColorBlendStateCreateInfo getCbState(uint32_t rtCount) const {
    VkPipelineColorBlendStateCreateInfo result = m_cbState;
    result.attachmentCount = rtCount;
    result.pAttachments = rtCount ? m_cbAttachments.data() : nullptr;
    return result;
  }

  /**
   * \brief Appends dynamic states to a given vector
   * \param [out] container Vector containing dynamic states
   */
  template<typename T>
  void getDynamicStates(T& container) {
    for (uint32_t i = 0; i < m_dyState.dynamicStateCount; i++)
      container.push_back(m_dyState.pDynamicStates[i]);
  }

private:

  std::array<VkPipelineColorBlendAttachmentState, GfxMaxColorAttachments> m_cbAttachments = { };
  std::array<VkDynamicState,                      1>                      m_dyList        = { };

  VkPipelineColorBlendStateCreateInfo m_cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
  VkPipelineDynamicStateCreateInfo    m_dyState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };


};


/**
 * \brief Vulkan multisample state
 */
class GfxVulkanMultisampleState : public GfxMultisampleStateIface {

public:

  GfxVulkanMultisampleState(
          GfxVulkanPipelineManager&     mgr,
    const GfxMultisampleStateDesc&      desc);

  ~GfxVulkanMultisampleState();

  /**
   * \brief Queries Vulkan sample count
   * \returns Vulkan sample count
   */
  VkSampleCountFlagBits getSampleCount() const {
    return VkSampleCountFlagBits(m_desc.sampleCount);
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
          bool                          sampleShading) const;

private:

  VkSampleMask                         m_msMask  = 0;
  VkPipelineMultisampleStateCreateInfo m_msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

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
 * \brief Vulkan fragment output pipeline key
 *
 * Only consists of render target formats and blend state.
 */
struct GfxVulkanFragmentOutputStateKey {
  GfxColorBlendState colorBlendState;
  GfxMultisampleState multisampleState;
  GfxRenderTargetState renderTargetState;
  VkBool32 sampleRateShading = VK_FALSE;

  bool operator == (const GfxVulkanFragmentOutputStateKey& other) const = default;
  bool operator != (const GfxVulkanFragmentOutputStateKey& other) const = default;

  size_t hash() const;
};


/**
 * \brief Vulkan fragment output pipeline
 */
class GfxVulkanFragmentOutputState {

public:

  GfxVulkanFragmentOutputState(
          GfxVulkanPipelineManager&     mgr,
    const GfxVulkanFragmentOutputStateKey& key);

  ~GfxVulkanFragmentOutputState();

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

  GfxVulkanPipelineManager&       m_mgr;
  GfxVulkanDynamicStates          m_dynamic   = 0;

  VkPipeline                      m_pipeline  = VK_NULL_HANDLE;

};


/**
 * \brief Shader binary metadata
 */
struct GfxVulkanShaderBinary {
  GfxShaderStage stage;
  uint32_t offset;
  uint32_t size;
};


/**
 * \brief Decompressed shader binaries
 */
struct GfxVulkanShaderStageInfo {
  std::vector<char> data;
  small_vector<VkShaderModuleCreateInfo, 5> moduleInfo;
  small_vector<VkPipelineShaderStageCreateInfo, 5> stageInfo;
};


/**
 * \brief Compressed shader binaries
 */
class GfxVulkanCompressedShaderBinaries {

public:

  GfxVulkanCompressedShaderBinaries(
    const GfxGraphicsPipelineDesc&      desc);

  GfxVulkanCompressedShaderBinaries(
    const GfxMeshPipelineDesc&          desc);

  /**
   * \brief Decodes compressed binaries initializes shader module info
   *
   * \param [in] manager Pipeline manager
   * \param [out] shaders Shader stage info
   */
  void getShaderStageInfo(
        GfxVulkanPipelineManager&       manager,
        GfxVulkanShaderStageInfo&       shaders) const;

private:

  std::vector<char> m_compressed;

  static void addBinary(
          BytestreamWriter&             bytestream,
    const GfxShader&                    shader);

  static std::vector<char> compress(
    const std::vector<char>&            data);

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
   * \param [in] state Render state
   * \returns Pipeline handle
   */
  GfxVulkanGraphicsPipelineVariant getVariant(
    const GfxGraphicsStateDesc&         state);

  /**
   * \brief Compiles pipeline variant with the given state
   *
   * \param [in] state Render state
   * \returns Pipeline handle
   */
  GfxVulkanGraphicsPipelineVariant createVariant(
    const GfxGraphicsStateDesc&         state);

  /**
   * \brief Compiles shader pipeline library
   * \returns Pipeline library handle
   */
  GfxVulkanGraphicsPipelineVariant createLibrary();

  /**
   * \brief Checks whether the pipeline is available
   * \returns \c true if the pipeline is available
   */
  bool isAvailable() const override;

  /**
   * \brief Compiles a pipeline variant with the given state
   *
   * No-op if fast linking is possible for the given pipeline.
   * \param [in] state Pipeline state vector
   */
  void compileVariant(
    const GfxGraphicsStateDesc&         state) override;

private:

  struct LinkedVariant {
    LinkedVariant(
      const GfxGraphicsStateDesc&             s,
      const GfxVulkanGraphicsPipelineVariant& v)
    : vertexInputState  (s.vertexInputState)
    , colorBlendState   (s.colorBlendState)
    , multisampleState  (s.multisampleState)
    , renderTargetState (s.renderTargetState)
    , variant           (v) { }

    GfxVertexInputState               vertexInputState;
    GfxColorBlendState                colorBlendState;
    GfxMultisampleState               multisampleState;
    GfxRenderTargetState              renderTargetState;
    GfxVulkanGraphicsPipelineVariant  variant;
  };

  struct OptimizedVariant {
    OptimizedVariant(
      const GfxGraphicsStateDesc&             s,
      const GfxVulkanGraphicsPipelineVariant& v)
    : state(s), dynamicStates(v.dynamicStates), pipeline(v.pipeline) { }

    GfxGraphicsStateDesc              state;
    uint32_t                          dynamicStates = 0;
    std::atomic<VkPipeline>           pipeline = { VK_NULL_HANDLE };

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

  GfxVulkanGraphicsPipelineVariant  m_library;
  VkBool32                          m_sampleRateShading = VK_FALSE;
  VkBool32                          m_canLink           = VK_FALSE;

  std::mutex                        m_linkedMutex;
  LockFreeList<LinkedVariant>       m_linkedVariants;

  std::mutex                        m_optimizedMutex;
  LockFreeList<OptimizedVariant>    m_optimizedVariants;

  GfxVulkanCompressedShaderBinaries m_binaries;
  std::atomic<bool>                 m_isAvailable = { false };

  GfxVulkanGraphicsPipelineVariant lookupLinked(
    const GfxGraphicsStateDesc&         state) const;

  LookupResult lookupOptimized(
    const GfxGraphicsStateDesc&         state) const;

  GfxVulkanGraphicsPipelineVariant createLibraryLocked();

  GfxVulkanGraphicsPipelineVariant createVariantLocked(
    const GfxGraphicsStateDesc&         state) const;

  GfxVulkanGraphicsPipelineVariant linkVariant(
    const GfxGraphicsStateDesc&         state);

  bool canLinkVariant(
    const GfxGraphicsStateDesc&         state) const;

  void deferCreateVariant(
    const GfxGraphicsStateDesc&         state);

  bool canFastLink();

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

  std::optional<GfxComputePipelineDesc> m_desc;

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
   * \brief Initializes a shader stage struct
   *
   * Creates a shader module as necessary, which \e must be
   * freed by the caller after creating the pipeline.
   * \param [in] stage Shader stage
   * \param [in] binary Shader binary
   * \param [in] stageInfo Vulkan shader stage info
   * \param [in] moduleInfo Vulkan shader module info
   */
  void initShaderStage(
          GfxShaderStage                stage,
          GfxShaderBinary               binary,
          VkPipelineShaderStageCreateInfo& stageInfo,
          VkShaderModuleCreateInfo&     moduleInfo);

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
   * \brief Creates vertex input state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanVertexInputState& createVertexInputState(
    const GfxVertexInputStateDesc&      desc) {
    return createStateObject(m_vertexInputStates, desc);
  }

  /**
   * \brief Creates rasterizer state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanRasterizerState& createRasterizerState(
    const GfxRasterizerStateDesc&       desc) {
    return createStateObject(m_rasterizerStates, desc);
  }

  /**
   * \brief Creates depth-stencil state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanDepthStencilState& createDepthStencilState(
    const GfxDepthStencilStateDesc&     desc) {
    return createStateObject(m_depthStencilStates, desc);
  }

  /**
   * \brief Creates color blend state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanColorBlendState& createColorBlendState(
    const GfxColorBlendStateDesc&       desc) {
    return createStateObject(m_colorBlendStates, desc);
  }

  /**
   * \brief Creates multisample state
   *
   * \param [in] desc State object description
   * \returns State object
   */
  GfxVulkanMultisampleState& createMultisampleState(
    const GfxMultisampleStateDesc&      desc) {
    return createStateObject(m_multisampleStates, desc);
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
   * \brief Looks up fragment output state
   * \returns Fragment output state
   */
  GfxVulkanFragmentOutputState& createFragmentOutputState(
    const GfxVulkanFragmentOutputStateKey& key) {
    return createStateObject(m_fragmentOutputStates, key);
  }

  /**
   * \brief Asynchronously compiles pipeline variant
   *
   * \param [in] pipeline Graphics pipeline
   * \param [in] state Render state
   */
  void deferCreateGraphicsPipelineVariant(
          GfxVulkanGraphicsPipeline&    pipeline,
    const GfxGraphicsStateDesc&         state);

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

    WorkItem(GfxVulkanGraphicsPipeline& p, const GfxGraphicsStateDesc& state)
    : type            (WorkItemType::eGraphicsVariant)
    , graphicsPipeline(&p)
    , graphicsState   (state) { }

    WorkItemType type;

    union {
      GfxVulkanGraphicsPipeline* graphicsPipeline;
      GfxVulkanComputePipeline* computePipeline;
    };

    GfxGraphicsStateDesc graphicsState;
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
    GfxVertexInputStateDesc,
    GfxVulkanVertexInputState,
    HashMemberProc>       m_vertexInputStates;

  std::unordered_map<
    GfxRasterizerStateDesc,
    GfxVulkanRasterizerState,
    HashMemberProc>       m_rasterizerStates;

  std::unordered_map<
    GfxDepthStencilStateDesc,
    GfxVulkanDepthStencilState,
    HashMemberProc>       m_depthStencilStates;

  std::unordered_map<
    GfxColorBlendStateDesc,
    GfxVulkanColorBlendState,
    HashMemberProc>       m_colorBlendStates;

  std::unordered_map<
    GfxMultisampleStateDesc,
    GfxVulkanMultisampleState,
    HashMemberProc>       m_multisampleStates;

  std::unordered_map<
    GfxRenderTargetStateDesc,
    GfxVulkanRenderTargetState,
    HashMemberProc>       m_renderTargetStates;

  std::unordered_map<
    GfxVulkanFragmentOutputStateKey,
    GfxVulkanFragmentOutputState,
    HashMemberProc>       m_fragmentOutputStates;

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
