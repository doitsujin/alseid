#pragma once

#include "../../util/util_object_map.h"

#include "../gfx.h"

#include "gfx_scene_common.h"
#include <cstdint>

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
  uint32_t reserved;
};

static_assert(sizeof(GfxSceneInstanceAnimateArgs) == 24);


/**
 * \brief Instance node update arguments
 */
struct GfxSceneInstanceUpdateNodeArgs {
  uint64_t dstInstanceVa;
  uint64_t srcInstanceVa;
  uint64_t updateListVa;
  uint32_t updateCount;
  uint32_t frameId;
};

static_assert(sizeof(GfxSceneInstanceUpdateNodeArgs) == 32);


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
  uint64_t groupBufferVa;
};

static_assert(sizeof(GfxSceneInstanceUpdateExecuteArgs) == 16);


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
 * \brief Arguments for data upload shader
 */
struct GfxSceneUploadArgs {
  uint64_t scratchVa;
  uint64_t metadataVa;
  uint32_t chunkIndex;
  uint32_t chunkCount;
};


/**
 * \brief Data upload chunk info on the GPU
 *
 * Stores parameters for a single upload from
 * a scratch buffer.
 */
struct GfxSceneUploadInfo {
  /** Scratch buffer offset, in bytes */
  uint32_t srcOffset;
  /** Data size, in bytes */
  uint32_t srcSize;
  /** Destination address */
  uint64_t dstVa;
};


/**
 * \brief Chunk description for data upload
 */
struct GfxSceneUploadChunk {
  /** Pointer to source data */
  const void* srcData;
  /** Data size, in bytes */
  uint32_t size;
  /** Destination address */
  uint64_t dstVa;
};


/**
 * \brief Occlusion test arguments
 */
struct GfxSceneOcclusionTestArgs {
  uint64_t passInfoVa;
  uint64_t passGroupVa;
  uint64_t sceneVa;
  uint32_t passIndex;
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
   * \brief Initializes pass group buffer for BVH traversal
   *
   * Must be used to initialize a pass group buffer prior the first pass
   * of BVH traversal using the given pass group buffer in a frame.
   * Animations for nodes that have a BVH child node attached to them
   * must be updated prior to this.
   * \param [in] context Context object
   * \param [in] args Push constants, including buffer addresses
   * \param [in] rootNodes Root BVH node references
   */
  void initBvhTraversal(
    const GfxContext&                   context,
    const GfxScenePassInitArgs&         args,
    const GfxSceneNodeRef*              rootNodes) const;

  /**
   * \brief Prepares pass group buffer for further BVH traversal
   *
   * Must be used after appending BVH nodes to the traversal lists,
   * prior to performing additional traversal passes.
   * \param [in] context Context object
   * \param [in] passGroupVa Pass group buffer address
   */
  void prepareBvhTraversal(
    const GfxContext&                   context,
          uint64_t                      passGroupVa) const;

  /**
   * \brief Finalizes pass group buffer
   *
   * Should be run immediately after BVH traversal.
   * \param [in] context Context object
   * \param [in] passGroupVa Address of pass group buffer
   */
  void finalizeBvhTraversal(
    const GfxContext&                   context,
          uint64_t                      passGroupVa) const;

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
   * \brief Updates instance node data
   *
   * \param [in] context Context object
   * \param [in] args Arguments to pass to the shader
   */
  void updateInstanceNodes(
    const GfxContext&                   context,
    const GfxSceneInstanceUpdateNodeArgs& args) const;

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
  void uploadRenderPassInfos(
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

  /**
   * \brief Uploads data to a buffer
   *
   * Allocates scratch buffers for both the payload and the metadata
   * buffer, and dispatches a compute shader to scatter the data to
   * the appropriate locations.
   * Using this over regular buffer copy functions is preferred when
   * individual uploads are small. The shader operates with a chunk
   * size of 16 bytes per thread.
   * \param [in] context Context object
   * \param [in] chunkCount Number of chunks to upload
   * \param [in] chunks Chunk description
   */
  void uploadChunks(
    const GfxContext&                   context,
          uint32_t                      chunkCount,
    const GfxSceneUploadChunk*          chunks) const;

  /**
   * \brief Performs initial coarse BVH occlusion testing
   *
   * Culls or accepts BVH nodes based on the hierarchical depth buffer,
   * and generates a list of nodes that need to be rendered in order to
   * perform more fine-grained occlusion testing.
   * \param [in] context Context object
   * \param [in] hizView Hi-Z image view
   * \param [in] dispatch Task shader dispatch descriptor
   * \param [in] args Pipeline arguments
   */
  void precullBvhOcclusion(
    const GfxContext&                   context,
    const GfxImageView&                 hizView,
    const GfxDescriptor&                dispatch,
    const GfxSceneOcclusionTestArgs&    args) const;

  /**
   * \brief Performs occlusion testing for a given render pass
   *
   * Runs a mesh shader pipeline on he list of BVHs generated by the
   * pre-cull shader, and culls based on a lower resolution mip level
   * of the Hi-Z buffer.
   *
   * Note that this must be called outside any rendering commands.
   * \param [in] context Context object
   * \param [in] hizView Hi-Z image view
   * \param [in] dispatch Task shader dispatch descriptor
   * \param [in] args Pipeline arguments
   */
  void testBvhOcclusion(
    const GfxContext&                   context,
    const GfxImageView&                 hizView,
    const GfxDescriptor&                dispatch,
    const GfxSceneOcclusionTestArgs&    args) const;

private:

  GfxDevice           m_device;

  GfxComputePipeline  m_csAnimationPrepare;
  GfxComputePipeline  m_csAnimationProcess;

  GfxComputePipeline  m_csDrawListInit;
  GfxComputePipeline  m_csDrawListGenerate;

  GfxComputePipeline  m_csGroupFinalize;
  GfxComputePipeline  m_csGroupResetUpdate;
  GfxComputePipeline  m_csGroupTraverseBvh;
  GfxComputePipeline  m_csGroupTraverseInit;
  GfxComputePipeline  m_csGroupTraversePrepare;
  GfxComputePipeline  m_csGroupTraverseReset;

  GfxComputePipeline  m_csInstanceUpdateExecute;
  GfxComputePipeline  m_csInstanceUpdateNode;
  GfxComputePipeline  m_csInstanceUpdatePrepare;

  GfxComputePipeline  m_csOcclusionPrecull;

  GfxComputePipeline  m_csRenderPassUpdateExecute;
  GfxComputePipeline  m_csRenderPassUpdateInit;
  GfxComputePipeline  m_csRenderPassUpdatePrepare;
  GfxComputePipeline  m_csRenderPassUpload;

  GfxComputePipeline  m_csSceneUpload;

  GfxGraphicsPipeline m_occlusionTestPipeline;
  GfxRenderState      m_occlusionTestState;

  template<size_t CsSize>
  GfxComputePipeline createComputePipeline(
    const char*                         name,
    const uint32_t                      (&cs)[CsSize]) const {
    GfxComputePipelineDesc pipelineDesc = { };
    pipelineDesc.debugName = name;
    pipelineDesc.compute = GfxShader::createBuiltIn(
      GfxShaderFormat::eVulkanSpirv, CsSize * sizeof(uint32_t), cs);

    return m_device->createComputePipeline(pipelineDesc);
  }


  template<size_t MsSize, size_t FsSize>
  GfxGraphicsPipeline createMeshPipeline(
    const char*                         name,
    const uint32_t                      (&ms)[MsSize],
    const uint32_t                      (&fs)[FsSize]) const {
    GfxMeshPipelineDesc pipelineDesc = { };
    pipelineDesc.debugName = name;
    pipelineDesc.mesh = GfxShader::createBuiltIn(
      GfxShaderFormat::eVulkanSpirv, MsSize * sizeof(uint32_t), ms);
    pipelineDesc.fragment = GfxShader::createBuiltIn(
      GfxShaderFormat::eVulkanSpirv, FsSize * sizeof(uint32_t), fs);

    return m_device->createGraphicsPipeline(pipelineDesc);
  }


  GfxRenderState createOcclusionTestRenderState() const;

};

}
