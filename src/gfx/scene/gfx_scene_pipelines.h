#pragma once

#include "../../util/util_object_map.h"

#include "../gfx.h"

#include "gfx_scene_common.h"

namespace as {

/**
 * \brief Pass init pipeline arguments
 */
struct GfxScenePassInitArgs {
  /** Scene buffer address. Will be accesed
   *  as a read-only shader resource. */
  uint64_t sceneBufferVa;
  /** Pass group buffer address. Will be accesed
   *  as a shader storage resource. */
  uint64_t groupBufferVa;
  /** Number of root BVH nodes */
  uint32_t nodeCount;
  /** Current frame number */
  uint32_t frameId;
};


/**
 * \brief Node update pipeline arguments
 *
 * Used for all typed of node updates. The actual data
 * type depends on the parameter being passed in.
 */
struct GfxSceneUpdateArgs {
  /** Node array address within the scene buffer. Must have the
   *  correct offset already applied since the node header is not
   *  read by the copy shader. */
  uint64_t dstNodeDataVa;
  /** Source node data address. */
  uint64_t srcNodeDataVa;
  /** Node index data array. */
  uint64_t srcNodeIndexVa;
  /** Number of nodes to update. */
  uint32_t nodeCount;
  /** Node data size, in bytes. Must be a multiple of 16. */
  uint32_t nodeSize;
};

static_assert(sizeof(GfxSceneUpdateArgs) == 32);


/**
 * \brief BVH traversal arguments
 */
struct GfxSceneTraverseBvhArgs {
  uint64_t passBufferVa;
  uint64_t sceneBufferVa;
  uint64_t groupBufferVa;
  uint32_t frameId;
  uint16_t bvhLayer;
  uint16_t distanceCullingPass;
};

static_assert(sizeof(GfxSceneTraverseBvhArgs) == 32);


/**
 * \brief BVH traversal reset arguments
 */
struct GfxSceneTraverseResetArgs {
  uint64_t groupBufferVa;
  uint32_t bvhLayer;
  uint32_t frameId;
};

static_assert(sizeof(GfxSceneTraverseResetArgs) == 16);


/**
 * \brief Instance animation arguments
 */
struct GfxSceneInstanceAnimateArgs {
  uint64_t instanceNodeBufferVa;
  uint64_t groupBufferVa;
  uint32_t frameId;
};


/**
 * \brief Instance update arguments
 */
struct GfxSceneInstanceUpdatePrepareArgs {
  uint64_t instanceBufferVa;
  uint64_t sceneBufferVa;
  uint64_t groupBufferVa;
  uint32_t frameId;
  uint32_t reserved;
};

static_assert(sizeof(GfxSceneInstanceUpdatePrepareArgs) == 32);


/**
 * \brief Instance update arguments
 */
struct GfxSceneInstanceUpdateExecuteArgs {
  uint64_t instanceBufferVa;
  uint64_t sceneBufferVa;
  uint64_t groupBufferVa;
};

static_assert(sizeof(GfxSceneInstanceUpdateExecuteArgs) == 24);


/**
 * \brief Draw list initialization arguments
 */
struct GfxSceneDrawListInitArgs {
  uint64_t drawListVa;
  uint32_t drawGroupCount;
};


/**
 * \brief Draw list initialization arguments
 */
struct GfxSceneDrawListGenerateArgs {
  uint64_t drawListVa;
  uint64_t instanceBufferVa;
  uint64_t sceneBufferVa;
  uint64_t passInfoVa;
  uint64_t passGroupVa;
  uint32_t frameId;
  uint32_t passMask;
  uint32_t lodSelectionPass;
};


/**
 * \brief Render pass host copy args
 */
struct GfxPassInfoUpdateCopyArgs {
  uint64_t dstPassInfoVa;
  uint64_t srcPassIndexVa;
  uint64_t srcPassInfoVa;
  uint32_t frameId;
  uint32_t passUpdateCount;
};


/**
 * \brief Render pass update preparation args
 */
struct GfxPassInfoUpdatePrepareArgs {
  uint64_t passInfoVa;
  uint64_t passListVa;
  uint32_t frameId;
  uint32_t passCount;
};


/**
 * \brief Render pass update execution args
 */
struct GfxPassInfoUpdateExecuteArgs {
  uint64_t passInfoVa;
  uint64_t passListVa;
  uint64_t sceneVa;
  uint32_t frameId;
};


/**
 * \brief Pipelines for scene rendering
 *
 * Creates compute and graphics pipelines for built-in shaders
 * that are used for scene updates, traversal, and rendering, as
 * well as methods to invoke those pipelines.
 *
 * All shaders are provided as plain SPIR-V and must be translated
 * to a compatible representation by the active graphics backend.
 */
class GfxScenePipelines {

public:

  explicit GfxScenePipelines(
          GfxDevice                     device);

  ~GfxScenePipelines();

  GfxScenePipelines             (const GfxScenePipelines&) = delete;
  GfxScenePipelines& operator = (const GfxScenePipelines&) = delete;

  /**
   * \brief Initializes pass group buffer
   *
   * Must be used to initialize a pass group buffer prior to BVH
   * traversal. Animations for nodes that have a BVH child node
   * attached to them must be updated prior to this.
   * \param [in] context Context object
   * \param [in] args Push constants, including buffer addresses
   * \param [in] rootNodes Root BVH node references
   */
  void initPassGroupBuffer(
    const GfxContext&                   context,
    const GfxScenePassInitArgs&         args,
    const GfxSceneNodeRef*              rootNodes) const;

  /**
   * \brief Updates node buffer
   *
   * Copies node data from a host array to the GPU, using a basic compute
   * shader to unpack the node array written to the scratch buffer.
   * \param [in] context Context object
   * \param [in] nodeDataVa Node array address within the scene buffer
   * \param [in] nodeCount Number of node objects to update
   * \param [in] nodeIndices Array of indices of nodes to update
   * \param [in] srcNodes Pointer to flat node data array. This will
   *    be indexed using the indices stored in the node index array.
   */
  template<typename T, std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  void updateSceneBuffer(
    const GfxContext&                   context,
          uint64_t                      nodeDataVa,
          uint32_t                      nodeCount,
    const uint32_t*                     nodeIndices,
    const ObjectMap<T>&                 srcNodes) const {
    static_assert(!(sizeof(T) % 16));

    auto indexBuffer = context->writeScratch(GfxUsage::eShaderResource,
      sizeof(*nodeIndices) * nodeCount, nodeIndices);

    auto dataBuffer = context->allocScratch(
      GfxUsage::eCpuWrite | GfxUsage::eShaderResource,
      sizeof(T) * nodeCount);

    // Pack node data into the linear scratch buffer, unpacking
    // will happen in the shader based on the index buffer
    auto dstNodes = reinterpret_cast<T*>(dataBuffer.map(GfxUsage::eCpuWrite, 0));

    for (uint32_t i = 0; i < nodeCount; i++)
      dstNodes[i] = srcNodes[nodeIndices[i]];

    // Dispatch the update shader
    GfxSceneUpdateArgs args = { };
    args.dstNodeDataVa = nodeDataVa;
    args.srcNodeDataVa = dataBuffer.getGpuAddress();
    args.srcNodeIndexVa = indexBuffer.getGpuAddress();
    args.nodeCount = nodeCount;
    args.nodeSize = sizeof(T);

    context->bindPipeline(m_csSceneUpdate);
    context->setShaderConstants(0, args);
    context->dispatch(m_csSceneUpdate->computeWorkgroupCount(Extent3D(nodeCount, 1u, 1u)));
  }

  /**
   * \brief Traverses scene BVH
   *
   * Processes a single layer of the scene BVH for a given pass group. Callers
   * must insert a barrier between layers to ensure the buffers can be used for
   * both \c GfxUsage::eShaderResource and \c GfxUsage::eShaderStorage.
   * \param [in] context Context object
   * \param [in] dispatchTraverse Indirect dispatch descriptor for traversal
   * \param [in] dispatchReset Indirect dispatch descriptor for reset
   * \param [in] args Push constants, including buffer addresses
   */
  void processBvhLayer(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatchTraverse,
    const GfxDescriptor&                dispatchReset,
    const GfxSceneTraverseBvhArgs&      args) const;

  /**
   * \brief Prepares instance animations
   *
   * Generates a dispatch argument buffer in order to process animations.
   * Must be run after BVH traversal, but \e before preparing the instance
   * updates, since that will compute the absolute transforms.
   * \param [in] context Context object
   * \param [in] dispatch Indirect dispatch descriptor for preprocessing
   * \param [in] args Arguments to pass to the shader
   */
  void prepareInstanceAnimations(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatch,
    const GfxSceneInstanceAnimateArgs&  args) const;

  /**
   * \brief Processes instance animations
   *
   * Computes relative transforms and morph target weights for all
   * visible animated instances. Must be run after the preparation
   * step, but before performing instance updates.
   * \param [in] context Context object
   * \param [in] dispatch Indirect dispatch descriptor for preprocessing
   * \param [in] args Arguments to pass to the animation shader
   */
  void processInstanceAnimations(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatch,
    const GfxSceneInstanceAnimateArgs&  args) const;

  /**
   * \brief Prepares instance updates
   *
   * \param [in] context Context object
   * \param [in] dispatch Indirect dispatch descriptor for preprocessing
   * \param [in] args Arguments to pass to the instance processing shader
   */
  void prepareInstanceUpdates(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatch,
    const GfxSceneInstanceUpdatePrepareArgs& args) const;

  /**
   * \brief Executes instance updates
   *
   * \param [in] context Context object
   * \param [in] dispatch Indirect dispatch descriptor for preprocessing
   * \param [in] args Arguments to pass to the instance processing shader
   */
  void executeInstanceUpdates(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatch,
    const GfxSceneInstanceUpdateExecuteArgs& args) const;

  /**
   * \brief Initializes draw list buffer
   *
   * Copies draw group properties from a host buffer to the GPU, but resets
   * the active draw count to zero so that draws can be added dynamically.
   * Draw lists consist of a \c GfxDrawListHeader structure, followed by an
   * array of \c GfxSceneDrawListEntry structures.
   * \param [in] context Context object
   * \param [in] args Arguments to pass to the initialization shader
   */
  void initDrawList(
    const GfxContext&                   context,
    const GfxSceneDrawListInitArgs&     args) const;

  /**
   * \brief Generates draw list
   *
   * \param [in] context Context object
   * \param [in] dispatch Indirect dispatch descriptor
   * \param [in] args Arguments to pass to the initialization shader
   */
  void generateDrawList(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatch,
    const GfxSceneDrawListGenerateArgs& args) const;

  /**
   * \brief Resets update lists of a group buffer
   *
   * \param [in] context Context object
   * \param [in] groupBufferVa Group buffer address
   */
  void resetUpdateLists(
    const GfxContext&                   context,
          uint64_t                      groupBufferVa) const;

  /**
   * \brief Initializes render pass update list
   *
   * Should ideally run with other shaders in parallel.
   * \param [in] context Context object
   * \param [in] passListVa Pass list buffer address
   */
  void initRenderPassUpdateList(
    const GfxContext&                   context,
          uint64_t                      passListVa) const;

  /**
   * \brief Copies render pass infos from a host buffer
   *
   * \param [in] context Context object
   * \param [in] args Arguments to pass to the shader
   */
  void copyRenderPassInfos(
    const GfxContext&                   context,
    const GfxPassInfoUpdateCopyArgs&    args) const;

  /**
   * \brief Prepares render pass updates
   *
   * Scans render passes for passes that require an update. This includes
   * any pass that has not been updated by the host but is attached to a
   * node, or has been otherwise updated by the GPU.
   * \param [in] context Context object
   * \param [in] args Arguments to pass to the shader
   */
  void prepareRenderPassUpdates(
    const GfxContext&                   context,
    const GfxPassInfoUpdatePrepareArgs& args) const;

  /**
   * \brief Executes render pass updates
   *
   * Must be performed prior to BVH traversal.
   * \param [in] context Context object
   * \param [in] dispatch Indirect dispatch descriptor
   * \param [in] args Arguments to pass to the shader
   */
  void executeRenderPassUpdates(
    const GfxContext&                   context,
    const GfxDescriptor&                dispatch,
    const GfxPassInfoUpdateExecuteArgs& args) const;

private:

  GfxDevice           m_device;

  GfxComputePipeline  m_csDrawListInit;
  GfxComputePipeline  m_csDrawListGenerate;

  GfxComputePipeline  m_csInstanceAnimate;
  GfxComputePipeline  m_csInstanceAnimatePrepare;
  GfxComputePipeline  m_csInstanceUpdateExecute;
  GfxComputePipeline  m_csInstanceUpdatePrepare;

  GfxComputePipeline  m_csPassInfoUpdateCopy;
  GfxComputePipeline  m_csPassInfoUpdateExecute;
  GfxComputePipeline  m_csPassInfoUpdateInit;
  GfxComputePipeline  m_csPassInfoUpdatePrepare;

  GfxComputePipeline  m_csPassInit;
  GfxComputePipeline  m_csPassResetUpdate;
  GfxComputePipeline  m_csPassTraverseBvh;
  GfxComputePipeline  m_csPassTraverseReset;

  GfxComputePipeline  m_csSceneUpdate;

  template<size_t N>
  GfxComputePipeline createComputePipeline(
    const char*                         name,
    const uint32_t                      (&cs)[N]) const {
    GfxComputePipelineDesc pipelineDesc = { };
    pipelineDesc.debugName = name;
    pipelineDesc.compute = GfxShader::createBuiltIn(
      GfxShaderFormat::eVulkanSpirv, N * sizeof(uint32_t), cs);

    return m_device->createComputePipeline(pipelineDesc);
  }

};

}
