#include "gfx_scene_material.h"
#include <cstdint>

namespace as {

GfxSceneMaterial::GfxSceneMaterial(
  const GfxDevice&                    device,
  const GfxSceneMaterialDesc&         desc)
: m_name(desc.debugName ? desc.debugName : "Unnamed material") {
  GfxRenderState renderState = createRenderState(device, desc);

  for (uint32_t i = 0; i < desc.shaderCount; i++) {
    GfxMeshPipelineDesc pipelineDesc = { };
    pipelineDesc.debugName = desc.debugName;
    pipelineDesc.task = desc.shaders[i].task;
    pipelineDesc.mesh = desc.shaders[i].mesh;
    pipelineDesc.fragment = desc.shaders[i].fragment;

    GfxGraphicsPipeline pipeline = device->createGraphicsPipeline(pipelineDesc);

    for (auto passType : desc.shaders[i].passTypes) {
      uint32_t passIndex = tzcnt(uint32_t(passType));
      
      auto& entry = m_pipelines.at(passIndex);
      entry.pipeline = pipeline;
      entry.renderState = renderState;
    }
  }
}


GfxSceneMaterial::~GfxSceneMaterial() {

}


bool GfxSceneMaterial::begin(
  const GfxContext&                   context,
        GfxScenePassType              passType,
        uint32_t                      setIndex) const {
  // Look up graphics pipeline and fail if it is null
  uint32_t passIndex = tzcnt(uint32_t(passType));
  auto& pipeline = m_pipelines.at(passIndex);

  if (!pipeline.pipeline)
    return false;

  // Bind pipeline and render state
  context->beginDebugLabel(m_name.c_str(), 0xfff6d9a4);
  context->bindPipeline(pipeline.pipeline);
  context->setRenderState(pipeline.renderState);

  // TODO bind assets

  return true;
}


void GfxSceneMaterial::end(
  const GfxContext&                   context) const {
  context->endDebugLabel();
}


GfxRenderState GfxSceneMaterial::createRenderState(
  const GfxDevice&                    device,
  const GfxSceneMaterialDesc&         desc) {
  GfxCullMode cullMode = (desc.flags & GfxSceneMaterialFlag::eTwoSided)
    ? GfxCullMode::eNone
    : GfxCullMode::eBack;

  GfxRenderStateDesc renderState = { };
  renderState.cullMode = &cullMode;

  return device->createRenderState(renderState);
}



GfxSceneMaterialManager::GfxSceneMaterialManager(
        GfxDevice                     device,
  const GfxSceneMaterialManagerDesc&  desc)
: m_device      (std::move(device))
, m_desc        (desc) {

}


GfxSceneMaterialManager::~GfxSceneMaterialManager() {

}


uint32_t GfxSceneMaterialManager::createMaterial(
  const GfxSceneMaterialDesc&         desc) {
  uint32_t index = m_materialAllocator.allocate();

  m_materials.emplace(index, m_device, desc);
  return index;
}


void GfxSceneMaterialManager::addInstanceDraws(
  const GfxSceneInstanceManager&      instanceManager,
        GfxSceneNodeRef               instanceRef) {
  adjustInstanceDraws(instanceManager, instanceRef, 1);
}


void GfxSceneMaterialManager::removeInstanceDraws(
  const GfxSceneInstanceManager&      instanceManager,
        GfxSceneNodeRef               instanceRef) {
  adjustInstanceDraws(instanceManager, instanceRef, -1);
}


void GfxSceneMaterialManager::updateDrawBuffer(
  const GfxContext&                   context,
        GfxSceneDrawBuffer&           drawBuffer) {
  uint32_t groupCount = m_materialAllocator.getCount();

  // Update local draw count array. Values will be passed to indirect
  // draws, and materials with no draws will be skipped entirely.
  m_drawCounts.resize(groupCount);

  for (uint32_t i = 0; i < groupCount; i++) {
    m_drawCounts[i] = m_materials.hasObjectAt(i)
      ? m_materials[i].getDrawCount()
      : 0u;
  }

  // Resize draw buffer for the current frame
  GfxSceneDrawBufferDesc drawBufferDesc;
  drawBufferDesc.drawGroupCount = groupCount;
  drawBufferDesc.drawCounts = m_drawCounts.data();

  drawBuffer.updateLayout(context, drawBufferDesc);
}


void GfxSceneMaterialManager::dispatchDraws(
  const GfxContext&                   context,
  const GfxScenePassManager&          passManager,
  const GfxSceneInstanceManager&      instanceManager,
  const GfxSceneNodeManager&          nodeManager,
  const GfxScenePassGroupBuffer&      passGroup,
        uint32_t                      drawBufferCount,
  const GfxSceneDrawBuffer**          drawBuffers,
        GfxScenePassType              passType,
        uint32_t                      frameId) const {
  GfxSceneMaterialDrawArgs args = { };
  args.passInfoVa = passManager.getGpuAddress();
  args.passGroupVa = passGroup.getGpuAddress();
  args.instanceVa = instanceManager.getGpuAddress();
  args.sceneVa = nodeManager.getGpuAddress();
  args.frameId = frameId;

  for (uint32_t i = 0; i < uint32_t(m_drawCounts.size()); i++) {
    args.drawGroup = i;

    if (!m_drawCounts[i])
      continue;

    if (!m_materials[i].begin(context, passType,
        m_desc.materialAssetDescriptorSet))
      continue;

    for (uint32_t j = 0; j < drawBufferCount; j++) {
      args.drawListVa = drawBuffers[j]->getGpuAddress();

      context->setShaderConstants(0, args);

      context->drawMeshIndirect(
        drawBuffers[j]->getDrawParameterDescriptor(i),
        drawBuffers[j]->getDrawCountDescriptor(i),
        m_drawCounts[i]);

    }

    m_materials[i].end(context);
  }
}


void GfxSceneMaterialManager::adjustInstanceDraws(
  const GfxSceneInstanceManager&      instanceManager,
        GfxSceneNodeRef               instanceRef,
        int32_t                       adjustment) {
  const auto& instanceData = instanceManager.getInstanceData(instanceRef);

  auto header = instanceData.getHeader();
  auto draws = instanceData.getDraws();

  for (uint32_t i = 0; i < header->drawCount; i++) {
    uint32_t material = draws[i].materialIndex;

    if (m_materials.hasObjectAt(material))
      m_materials[material].adjustDrawCount(adjustment);
  }
}

}
