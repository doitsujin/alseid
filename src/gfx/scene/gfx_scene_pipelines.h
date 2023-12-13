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
  uint32_t bvhLayer;
  uint32_t frameId;
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

private:

  GfxDevice           m_device;

  GfxComputePipeline  m_csInstanceUpdateExecute;
  GfxComputePipeline  m_csInstanceUpdatePrepare;

  GfxComputePipeline  m_csPassInit;
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
