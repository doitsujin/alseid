#include "gfx_scene_material.h"
#include "gfx_scene_draw.h"

namespace as {

GfxSceneMaterial::GfxSceneMaterial(
  const GfxDevice&                    device,
  const GfxSceneMaterialDesc&         desc)
: m_device(device)
, m_renderState(createRenderState(desc))
, m_name(desc.debugName ? desc.debugName : "Unnamed material") {

}


GfxSceneMaterial::~GfxSceneMaterial() {

}


void GfxSceneMaterial::setShaders(
        uint32_t                      shaderCount,
  const GfxSceneMaterialShaders*      shaders) {
  uint32_t passMask = m_passMask.load(std::memory_order_acquire);

  for (uint32_t i = 0; i < shaderCount; i++) {
    GfxMeshPipelineDesc pipelineDesc = { };
    pipelineDesc.debugName = m_name.c_str();
    pipelineDesc.task = shaders[i].task;
    pipelineDesc.mesh = shaders[i].mesh;
    pipelineDesc.fragment = shaders[i].fragment;

    GfxGraphicsPipeline pipeline;

    for (auto passType : shaders[i].passTypes) {
      uint32_t passIndex = tzcnt(uint32_t(passType));

      if (passMask & uint32_t(passType))
        continue;

      if (!pipeline) {
        pipeline = m_device->createGraphicsPipeline(pipelineDesc);
        // Workgroup sizes must be consistent across pipelines
        m_workgroupSize = pipeline->getWorkgroupSize().at<0>();
      }

      m_pipelines.at(passIndex) = pipeline;
      passMask |= uint32_t(passType);
    }
  }

  m_passMask.store(passMask, std::memory_order_release);
}


bool GfxSceneMaterial::begin(
  const GfxContext&                   context,
        GfxScenePassType              passType,
        uint32_t                      setIndex) const {
  // Look up graphics pipeline and fail if it is null
  uint32_t passIndex = tzcnt(uint32_t(passType));
  uint32_t passMask = m_passMask.load(std::memory_order_acquire);

  if (!(passMask & uint32_t(passType)))
    return false;

  // Bind pipeline and render state
  auto& pipeline = m_pipelines.at(passIndex);
  context->beginDebugLabel(m_name.c_str(), 0xfff6d9a4);
  context->bindPipeline(pipeline);
  context->setRenderState(m_renderState);

  // TODO bind assets

  return true;
}


void GfxSceneMaterial::end(
  const GfxContext&                   context) const {
  context->endDebugLabel();
}


GfxRenderState GfxSceneMaterial::createRenderState(
  const GfxSceneMaterialDesc&         desc) {
  GfxRenderStateDesc renderState = { };
  renderState.flags = GfxRenderStateFlag::eCullMode;
  renderState.cullMode = (desc.flags & GfxSceneMaterialFlag::eTwoSided)
    ? GfxCullMode::eNone
    : GfxCullMode::eBack;

  return m_device->createRenderState(renderState);
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


void GfxSceneMaterialManager::updateMaterialShaders(
        uint32_t                      material,
        uint32_t                      shaderCount,
  const GfxSceneMaterialShaders*      shaders) {
  auto& mat = m_materials[material];
  mat.setShaders(shaderCount, shaders);
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
  m_drawGroups.resize(groupCount);

  for (uint32_t i = 0; i < groupCount; i++) {
    m_drawGroups[i] = m_materials.hasObjectAt(i)
      ? m_materials[i].getDrawGroupInfo()
      : GfxSceneDrawGroupDesc();
  }

  // Resize draw buffer for the current frame
  GfxSceneDrawBufferDesc drawBufferDesc;
  drawBufferDesc.drawGroupCount = groupCount;
  drawBufferDesc.drawGroups = m_drawGroups.data();

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

  for (uint32_t i = 0; i < uint32_t(m_drawGroups.size()); i++) {
    args.drawGroup = i;

    if (!m_drawGroups[i].drawCount)
      continue;

    if (!m_materials[i].begin(context, passType,
        m_desc.materialAssetDescriptorSet))
      continue;

    for (uint32_t j = 0; j < drawBufferCount; j++) {
      args.drawListVa = drawBuffers[j]->getGpuAddress();

      context->setShaderConstants(0, args);

      context->drawMeshIndirect(
        drawBuffers[j]->getDrawParameterDescriptor(i),
        GfxDescriptor(),
        drawBuffers[j]->getDrawCount(i));
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

    if (m_materials.hasObjectAt(material)) {
      int32_t meshletCount = int32_t(draws[i].meshInstanceCount * draws[i].maxMeshletCount);
      m_materials[material].adjustDrawCount(adjustment, adjustment * meshletCount);
    }
  }
}

}
