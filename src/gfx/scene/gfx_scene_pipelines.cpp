#include "gfx_scene_pipelines.h"

#include "../gfx_spirv.h"

#include <cs_draw_list_generate.h>
#include <cs_draw_list_init.h>

#include <cs_animation_prepare.h>
#include <cs_animation_process.h>

#include <cs_instance_update_execute.h>
#include <cs_instance_update_node.h>
#include <cs_instance_update_prepare.h>

#include <cs_pass_info_update_copy.h>
#include <cs_pass_info_update_execute.h>
#include <cs_pass_info_update_init.h>
#include <cs_pass_info_update_prepare.h>

#include <cs_pass_init.h>
#include <cs_pass_reset_update.h>
#include <cs_pass_traverse_bvh.h>
#include <cs_pass_traverse_reset.h>

#include <cs_scene_update.h>

namespace as {

GfxScenePipelines::GfxScenePipelines(
        GfxDevice                     device)
: m_device                  (std::move(device))
, m_csAnimationPrepare      (createComputePipeline("cs_animation_prepare", cs_animation_prepare))
, m_csAnimationProcess      (createComputePipeline("cs_animation_process", cs_animation_process))
, m_csDrawListInit          (createComputePipeline("cs_draw_list_init", cs_draw_list_init))
, m_csDrawListGenerate      (createComputePipeline("cs_draw_list_generate", cs_draw_list_generate))
, m_csInstanceUpdateExecute (createComputePipeline("cs_instance_update_execute", cs_instance_update_execute))
, m_csInstanceUpdateNode    (createComputePipeline("cs_instance_update_node", cs_instance_update_node))
, m_csInstanceUpdatePrepare (createComputePipeline("cs_instance_update_prepare", cs_instance_update_prepare))
, m_csPassInfoUpdateCopy    (createComputePipeline("cs_pass_info_update_copy", cs_pass_info_update_copy))
, m_csPassInfoUpdateExecute (createComputePipeline("cs_pass_info_update_execute", cs_pass_info_update_execute))
, m_csPassInfoUpdateInit    (createComputePipeline("cs_pass_info_update_init", cs_pass_info_update_init))
, m_csPassInfoUpdatePrepare (createComputePipeline("cs_pass_info_update_prepare", cs_pass_info_update_prepare))
, m_csPassInit              (createComputePipeline("cs_pass_init", cs_pass_init))
, m_csPassResetUpdate       (createComputePipeline("cs_pass_reset_update", cs_pass_reset_update))
, m_csPassTraverseBvh       (createComputePipeline("cs_pass_traverse_bvh", cs_pass_traverse_bvh))
, m_csPassTraverseReset     (createComputePipeline("cs_pass_traverse_reset", cs_pass_traverse_reset))
, m_csSceneUpdate           (createComputePipeline("cs_scene_update", cs_scene_update)) {

}


GfxScenePipelines::~GfxScenePipelines() {

}


void GfxScenePipelines::initPassGroupBuffer(
  const GfxContext&                   context,
  const GfxScenePassInitArgs&         args,
  const GfxSceneNodeRef*              rootNodes) const {
  auto scratch = context->writeScratch(GfxUsage::eShaderResource,
    sizeof(*rootNodes) * args.nodeCount, rootNodes);

  context->bindPipeline(m_csPassInit);
  context->bindDescriptor(0, 0, scratch.getDescriptor(GfxUsage::eShaderResource));
  context->setShaderConstants(0, args);
  context->dispatch(m_csPassInit->computeWorkgroupCount(Extent3D(args.nodeCount, 1u, 1u)));
}


void GfxScenePipelines::processBvhLayer(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatchTraverse,
  const GfxDescriptor&                dispatchReset,
  const GfxSceneTraverseBvhArgs&      args) const {
  // Dispatch the shader to process relevant child nodes.
  context->bindPipeline(m_csPassTraverseBvh);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatchTraverse);

  // No barrier needed since execution of these shaders is mutually
  // exclusive, in that the dispatch args for one will always be 0.
  GfxSceneTraverseResetArgs resetArgs = { };
  resetArgs.groupBufferVa = args.groupBufferVa;
  resetArgs.bvhLayer = args.bvhLayer;
  resetArgs.frameId = args.frameId;

  context->bindPipeline(m_csPassTraverseReset);
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
  context->bindPipeline(m_csPassResetUpdate);
  context->setShaderConstants(0, groupBufferVa);
  context->dispatch(m_csPassResetUpdate->computeWorkgroupCount(
    Extent3D(uint32_t(GfxSceneNodeType::eCount) - uint32_t(GfxSceneNodeType::eBuiltInCount), 1u, 1u)));
}


void GfxScenePipelines::initRenderPassUpdateList(
  const GfxContext&                   context,
        uint64_t                      passListVa) const {
  context->bindPipeline(m_csPassInfoUpdateInit);
  context->setShaderConstants(0, passListVa);
  context->dispatch(Extent3D(1, 1, 1));
}


void GfxScenePipelines::copyRenderPassInfos(
  const GfxContext&                   context,
  const GfxPassInfoUpdateCopyArgs&    args) const {
  context->bindPipeline(m_csPassInfoUpdateCopy);
  context->setShaderConstants(0, args);
  context->dispatch(m_csPassInfoUpdateCopy->computeWorkgroupCount(
    Extent3D(args.passUpdateCount, 1, 1)));
}


void GfxScenePipelines::prepareRenderPassUpdates(
  const GfxContext&                   context,
  const GfxPassInfoUpdatePrepareArgs& args) const {
  context->bindPipeline(m_csPassInfoUpdatePrepare);
  context->setShaderConstants(0, args);
  context->dispatch(m_csPassInfoUpdatePrepare->computeWorkgroupCount(
    Extent3D(args.passCount, 1, 1)));
}


void GfxScenePipelines::executeRenderPassUpdates(
  const GfxContext&                   context,
  const GfxDescriptor&                dispatch,
  const GfxPassInfoUpdateExecuteArgs& args) const {
  context->bindPipeline(m_csPassInfoUpdateExecute);
  context->setShaderConstants(0, args);
  context->dispatchIndirect(dispatch);
}

}
