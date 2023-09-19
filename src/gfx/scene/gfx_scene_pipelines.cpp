#include "gfx_scene_pipelines.h"

#include "../gfx_spirv.h"

#include <cs_pass_init.h>
#include <cs_pass_traverse_bvh.h>
#include <cs_pass_traverse_reset.h>

#include <cs_scene_update.h>

namespace as {

GfxScenePipelines::GfxScenePipelines(
        GfxDevice                     device)
: m_device              (std::move(device))
, m_csPassInit          (createComputePipeline("cs_pass_init", cs_pass_init))
, m_csPassTraverseBvh   (createComputePipeline("cs_pass_traverse_bvh", cs_pass_traverse_bvh))
, m_csPassTraverseReset (createComputePipeline("cs_pass_traverse_reset", cs_pass_traverse_reset))
, m_csSceneUpdate       (createComputePipeline("cs_scene_update", cs_scene_update)) {

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

}
