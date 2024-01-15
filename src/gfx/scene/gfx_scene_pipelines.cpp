#include "gfx_scene_pipelines.h"

#include "../gfx_spirv.h"

#include "../../util/util_ptr.h"

#include <cs_draw_list_generate.h>
#include <cs_draw_list_init.h>

#include <cs_animation_prepare.h>
#include <cs_animation_process.h>

#include <cs_group_init.h>
#include <cs_group_reset_update.h>
#include <cs_group_traverse_bvh.h>
#include <cs_group_traverse_reset.h>

#include <cs_instance_update_execute.h>
#include <cs_instance_update_node.h>
#include <cs_instance_update_prepare.h>

#include <cs_renderpass_update_execute.h>
#include <cs_renderpass_update_init.h>
#include <cs_renderpass_update_prepare.h>
#include <cs_renderpass_upload.h>

#include <cs_scene_upload.h>

#include <fs_occlusion_test.h>
#include <ms_occlusion_test.h>
#include <ts_occlusion_test.h>

namespace as {

GfxScenePipelines::GfxScenePipelines(
        GfxDevice                     device)
: m_device                  (std::move(device))
, m_csAnimationPrepare      (createComputePipeline("cs_animation_prepare", cs_animation_prepare))
, m_csAnimationProcess      (createComputePipeline("cs_animation_process", cs_animation_process))
, m_csDrawListInit          (createComputePipeline("cs_draw_list_init", cs_draw_list_init))
, m_csDrawListGenerate      (createComputePipeline("cs_draw_list_generate", cs_draw_list_generate))
, m_csGroupInit             (createComputePipeline("cs_group_init", cs_group_init))
, m_csGroupResetUpdate      (createComputePipeline("cs_group_reset_update", cs_group_reset_update))
, m_csGroupTraverseBvh      (createComputePipeline("cs_group_traverse_bvh", cs_group_traverse_bvh))
, m_csGroupTraverseReset    (createComputePipeline("cs_group_traverse_reset", cs_group_traverse_reset))
, m_csInstanceUpdateExecute (createComputePipeline("cs_instance_update_execute", cs_instance_update_execute))
, m_csInstanceUpdateNode    (createComputePipeline("cs_instance_update_node", cs_instance_update_node))
, m_csInstanceUpdatePrepare (createComputePipeline("cs_instance_update_prepare", cs_instance_update_prepare))
, m_csRenderPassUpdateExecute(createComputePipeline("cs_renderpass_update_execute", cs_renderpass_update_execute))
, m_csRenderPassUpdateInit  (createComputePipeline("cs_renderpass_update_init", cs_renderpass_update_init))
, m_csRenderPassUpdatePrepare(createComputePipeline("cs_renderpass_update_prepare", cs_renderpass_update_prepare))
, m_csRenderPassUpload      (createComputePipeline("cs_renderpass_upload", cs_renderpass_upload))
, m_csSceneUpload           (createComputePipeline("cs_scene_upload", cs_scene_upload))
, m_occlusionTestPipeline   (createTaskMeshPipeline("occlusion_test", ts_occlusion_test, ms_occlusion_test, fs_occlusion_test))
, m_occlusionTestState      (createOcclusionTestRenderState()) {

}


GfxScenePipelines::~GfxScenePipelines() {

}


void GfxScenePipelines::initPassGroupBuffer(
  const GfxContext&                   context,
  const GfxScenePassInitArgs&         args,
  const GfxSceneNodeRef*              rootNodes) const {
  auto scratch = context->writeScratch(GfxUsage::eShaderResource,
    sizeof(*rootNodes) * args.nodeCount, rootNodes);

  context->bindPipeline(m_csGroupInit);
  context->bindDescriptor(0, 0, scratch.getDescriptor(GfxUsage::eShaderResource));
  context->setShaderConstants(0, args);
  context->dispatch(m_csGroupInit->computeWorkgroupCount(Extent3D(args.nodeCount, 1u, 1u)));
}


void GfxScenePipelines::processBvhLayer(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatchTraverse,
  const GfxDescriptor&                dispatchReset,
  const GfxSceneTraverseBvhArgs&      args) const {
  // Dispatch the shader to process relevant child nodes.
  context->bindPipeline(m_csGroupTraverseBvh);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatchTraverse);

  // No barrier needed since execution of these shaders is mutually
  // exclusive, in that the dispatch args for one will always be 0.
  GfxSceneTraverseResetArgs resetArgs = { };
  resetArgs.groupBufferVa = args.groupBufferVa;
  resetArgs.bvhLayer = args.bvhLayer;
  resetArgs.frameId = args.frameId;

  context->bindPipeline(m_csGroupTraverseReset);
  context->setShaderConstants(0, resetArgs);
  context->dispatchIndirect(dispatchReset);
}


void GfxScenePipelines::prepareInstanceAnimations(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxSceneInstanceAnimateArgs&  args) const {
  context->bindPipeline(m_csAnimationPrepare);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}


void GfxScenePipelines::processInstanceAnimations(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxSceneInstanceAnimateArgs&  args) const {
  context->bindPipeline(m_csAnimationProcess);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}


void GfxScenePipelines::updateInstanceNodes(
  const GfxContext&                   context,
  const GfxSceneInstanceUpdateNodeArgs& args) const {
  context->bindPipeline(m_csInstanceUpdateNode);
  context->setShaderConstants(0, args);
  context->dispatch(m_csInstanceUpdateNode->computeWorkgroupCount(
    Extent3D(args.updateCount, 1, 1)));
}


void GfxScenePipelines::prepareInstanceUpdates(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxSceneInstanceUpdatePrepareArgs& args) const {
  context->bindPipeline(m_csInstanceUpdatePrepare);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}


void GfxScenePipelines::executeInstanceUpdates(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxSceneInstanceUpdateExecuteArgs& args) const {
  context->bindPipeline(m_csInstanceUpdateExecute);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}


void GfxScenePipelines::initDrawList(
  const GfxContext&                   context,
  const GfxSceneDrawListInitArgs&     args) const {
  context->bindPipeline(m_csDrawListInit);
  context->setShaderConstants(0, args);
  context->dispatch(m_csDrawListInit->computeWorkgroupCount(
    Extent3D(args.drawGroupCount, 1, 1)));
}


void GfxScenePipelines::generateDrawList(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxSceneDrawListGenerateArgs& args) const {
  context->bindPipeline(m_csDrawListGenerate);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}


void GfxScenePipelines::resetUpdateLists(
  const GfxContext&                   context,
        uint64_t                      groupBufferVa) const {
  context->bindPipeline(m_csGroupResetUpdate);
  context->setShaderConstants(0, groupBufferVa);
  context->dispatch(m_csGroupResetUpdate->computeWorkgroupCount(
    Extent3D(uint32_t(GfxSceneNodeType::eCount) - uint32_t(GfxSceneNodeType::eBuiltInCount), 1u, 1u)));
}


void GfxScenePipelines::initRenderPassUpdateList(
  const GfxContext&                   context,
        uint64_t                      passListVa) const {
  context->bindPipeline(m_csRenderPassUpdateInit);
  context->setShaderConstants(0, passListVa);
  context->dispatch(Extent3D(1, 1, 1));
}


void GfxScenePipelines::uploadRenderPassInfos(
  const GfxContext&                   context,
  const GfxPassInfoUpdateCopyArgs&    args) const {
  context->bindPipeline(m_csRenderPassUpload);
  context->setShaderConstants(0, args);
  context->dispatch(m_csRenderPassUpload->computeWorkgroupCount(
    Extent3D(args.passUpdateCount, 1, 1)));
}


void GfxScenePipelines::prepareRenderPassUpdates(
  const GfxContext&                   context,
  const GfxPassInfoUpdatePrepareArgs& args) const {
  context->bindPipeline(m_csRenderPassUpdatePrepare);
  context->setShaderConstants(0, args);
  context->dispatch(m_csRenderPassUpdatePrepare->computeWorkgroupCount(
    Extent3D(args.passCount, 1, 1)));
}


void GfxScenePipelines::executeRenderPassUpdates(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxPassInfoUpdateExecuteArgs& args) const {
  context->bindPipeline(m_csRenderPassUpdateExecute);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}


void GfxScenePipelines::uploadChunks(
  const GfxContext&                   context,
        uint32_t                      chunkCount,
  const GfxSceneUploadChunk*          chunks) const {
  const uint32_t maxChunksPerIteration = 16384u * m_csSceneUpload->getWorkgroupSize().at<0>();
  const uint32_t maxBytesPerIteration = 1u << 20;

  context->bindPipeline(m_csSceneUpload);

  uint32_t chunkIndex = 0;

  while (chunkIndex < chunkCount) {
    uint32_t totalSize = 0;
    uint32_t localCount = 0;

    for (uint32_t i = chunkIndex; i < std::min(chunkCount, chunkIndex + maxChunksPerIteration); i++) {
      uint32_t chunkSize = align(chunks[i].size, 16u);

      if (totalSize && totalSize + chunkSize > maxBytesPerIteration)
        break;

      totalSize += chunkSize;
      localCount += 1u;
    }

    // Allocate scratch buffer and metadata buffer
    GfxScratchBuffer chunkBuffer = context->allocScratch(
      GfxUsage::eCpuWrite | GfxUsage::eShaderResource, totalSize);
    GfxScratchBuffer metadataBuffer = context->allocScratch(
      GfxUsage::eCpuWrite | GfxUsage::eShaderResource,
      localCount * sizeof(GfxSceneUploadInfo));

    auto chunkData = chunkBuffer.map(GfxUsage::eCpuWrite, 0);
    auto metadata = reinterpret_cast<GfxSceneUploadInfo*>(
      metadataBuffer.map(GfxUsage::eCpuWrite, 0));

    // Copy data and metadata to GPU buffers
    totalSize = 0;

    for (uint32_t i = 0; i < localCount; i++) {
      const auto& chunk = chunks[chunkIndex + i];

      std::memcpy(ptroffset(chunkData, totalSize),
        chunk.srcData, chunk.size);

      uint32_t chunkSize = align(chunk.size, 16u);

      metadata[i].srcOffset = totalSize;
      metadata[i].srcSize = chunkSize;
      metadata[i].dstVa = chunk.dstVa;

      totalSize += chunkSize;
    }

    GfxSceneUploadArgs args = { };
    args.scratchVa = chunkBuffer.getGpuAddress();
    args.metadataVa = metadataBuffer.getGpuAddress();
    args.chunkIndex = 0;
    args.chunkCount = localCount;

    context->setShaderConstants(0, args);
    context->dispatch(m_csSceneUpload->computeWorkgroupCount(
      Extent3D(localCount, 1u, 1u)));

    // Prepare next iteration
    chunkIndex += localCount;
  }
}


void GfxScenePipelines::testBvhOcclusion(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxSceneOcclusionTestArgs&    args) const {
  context->bindPipeline(m_occlusionTestPipeline);
  context->setRenderState(m_occlusionTestState);
  context->setShaderConstants(0, args);
  context->drawMeshIndirect(dispatch, GfxDescriptor(), 1u);
}


GfxRenderState GfxScenePipelines::createOcclusionTestRenderState() const {
  GfxDeviceFeatures features = m_device->getFeatures();

  GfxFrontFace frontFace = GfxFrontFace::eCw;
  GfxCullMode cullMode = GfxCullMode::eBack;
  bool conservativeRaster = features.conservativeRasterization;

  GfxDepthBias depthBias = { };
  GfxShadingRate shadingRate = { };

  GfxDepthTest depthTest = { };
  depthTest.depthCompareOp = GfxCompareOp::eGreater;

  GfxStencilTest stencilTest = { };
  GfxMultisampling multisampling = { };
  GfxBlending blending = { };

  GfxRenderStateDesc stateDesc = { };
  stateDesc.frontFace = &frontFace;
  stateDesc.cullMode = &cullMode;
  stateDesc.conservativeRaster = &conservativeRaster;
  stateDesc.depthBias = &depthBias;
  stateDesc.shadingRate = &shadingRate;
  stateDesc.depthTest = &depthTest;
  stateDesc.stencilTest = &stencilTest;
  stateDesc.multisampling = &multisampling;
  stateDesc.blending = &blending;

  return m_device->createRenderState(stateDesc);
}

}
