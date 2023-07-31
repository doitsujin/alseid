#include <sstream>

#include "gfx_pipeline.h"

namespace as {

GfxVertexInputStateIface::GfxVertexInputStateIface(
  const GfxVertexInputStateDesc&      desc)
: m_desc(desc) {
  for (uint32_t i = 0; i < GfxMaxVertexAttributes; i++) {
    if (desc.attributes[i].format != GfxFormat::eUnknown)
      m_vertexBufferMask |= 1u << i;
  }
}


size_t GfxVertexInputAttribute::hash() const {
  HashState result;
  result.add(binding);
  result.add(uint32_t(format));
  result.add(offset);
  result.add(uint32_t(inputRate));
  return result;
}


size_t GfxVertexInputStateDesc::hash() const {
  HashState result;
  result.add(uint32_t(primitiveTopology));
  result.add(patchVertexCount);

  for (size_t i = 0; i < GfxMaxVertexAttributes; i++)
    result.add(attributes[i].hash());

  return result;
}


bool GfxRasterizerStateDesc::isDepthBiasEnabled() const {
  return depthBias != 0.0f || depthBiasSlope != 0.0f;
}


size_t GfxRasterizerStateDesc::hash() const {
  HashState result;
  result.add(uint32_t(frontFace));
  result.add(uint32_t(cullMode));
  result.add(uint32_t(conservativeRasterization));
  result.add(hashFloat(depthBias));
  result.add(hashFloat(depthBiasClamp));
  result.add(hashFloat(depthBiasSlope));
  return result;
}


bool GfxStencilDesc::isStencilTestEnabled(
        bool                          depthTestCanFail) const {
  bool stencilTestCanFail = compareOp != GfxCompareOp::eAlways;

  return stencilTestCanFail || isStencilWriteEnabled(depthTestCanFail);
}


bool GfxStencilDesc::isStencilWriteEnabled(
        bool                          depthTestCanFail) const {
  if (!writeMask)
    return false;

  bool stencilTestCanFail = compareOp != GfxCompareOp::eAlways;
  bool stencilTestCanPass = compareOp != GfxCompareOp::eNever;

  return (stencilTestCanFail && failOp != GfxStencilOp::eKeep)
      || (stencilTestCanPass && passOp != GfxStencilOp::eKeep)
      || (depthTestCanFail && depthFailOp != GfxStencilOp::eKeep);
}


size_t GfxStencilDesc::hash() const {
  HashState result;
  result.add(uint32_t(failOp));
  result.add(uint32_t(passOp));
  result.add(uint32_t(depthFailOp));
  result.add(uint32_t(compareOp));
  result.add(compareMask);
  result.add(writeMask);
  return result;
}


bool GfxDepthStencilStateDesc::isDepthTestEnabled() const {
  bool depthTestCanFail = depthCompareOp != GfxCompareOp::eAlways;

  return depthTestCanFail || enableDepthWrite;
}


bool GfxDepthStencilStateDesc::isStencilTestEnabled() const {
  bool depthTestCanFail = depthCompareOp != GfxCompareOp::eAlways;

  return front.isStencilTestEnabled(depthTestCanFail)
      || back.isStencilTestEnabled(depthTestCanFail);
}


bool GfxDepthStencilStateDesc::isStencilWriteEnabled() const {
  bool depthTestCanFail = depthCompareOp != GfxCompareOp::eAlways;

  return front.isStencilWriteEnabled(depthTestCanFail)
      || back.isStencilWriteEnabled(depthTestCanFail);
}


size_t GfxDepthStencilStateDesc::hash() const {
  HashState result;
  result.add(uint32_t(enableDepthWrite));
  result.add(uint32_t(enableDepthBoundsTest));
  result.add(uint32_t(depthCompareOp));
  result.add(front.hash());
  result.add(back.hash());
  return result;
}


bool GfxRenderTargetBlend::isBlendingEnabled() const {
  if (!writeMask)
    return false;

  return srcColor != GfxBlendFactor::eOne
      || dstColor != GfxBlendFactor::eZero
      || colorOp != GfxBlendOp::eAdd
      || srcAlpha != GfxBlendFactor::eOne
      || dstAlpha != GfxBlendFactor::eZero
      || alphaOp != GfxBlendOp::eAdd;
}


static inline bool gfxBlendFactorUsesDualSource(GfxBlendFactor factor) {
  return factor == GfxBlendFactor::eSrc1Color
      || factor == GfxBlendFactor::eOneMinusSrc1Color
      || factor == GfxBlendFactor::eSrc1Alpha
      || factor == GfxBlendFactor::eOneMinusSrc1Alpha;
}


static inline bool gfxBlendFactorUsesBlendConstants(GfxBlendFactor factor) {
  return factor == GfxBlendFactor::eConstantColor
      || factor == GfxBlendFactor::eOneMinusConstantAlpha
      || factor == GfxBlendFactor::eConstantAlpha
      || factor == GfxBlendFactor::eOneMinusConstantAlpha;
}


bool GfxRenderTargetBlend::usesBlendConstants() const {
  if (!writeMask)
    return false;

  return gfxBlendFactorUsesDualSource(srcColor)
      || gfxBlendFactorUsesDualSource(dstColor)
      || gfxBlendFactorUsesDualSource(srcAlpha)
      || gfxBlendFactorUsesDualSource(dstAlpha);
}


bool GfxRenderTargetBlend::usesDualSource() const {
  if (!writeMask)
    return false;

  return gfxBlendFactorUsesBlendConstants(srcColor)
      || gfxBlendFactorUsesBlendConstants(dstColor)
      || gfxBlendFactorUsesBlendConstants(srcAlpha)
      || gfxBlendFactorUsesBlendConstants(dstAlpha);
}


size_t GfxRenderTargetBlend::hash() const {
  HashState result;
  result.add(uint32_t(srcColor));
  result.add(uint32_t(dstColor));
  result.add(uint32_t(colorOp));
  result.add(uint32_t(srcAlpha));
  result.add(uint32_t(dstAlpha));
  result.add(uint32_t(alphaOp));
  result.add(uint32_t(writeMask));
  return result;
}


bool GfxColorBlendStateDesc::isLogicOpEnabled() const {
  return logicOp != GfxLogicOp::eSrc;
}


size_t GfxColorBlendStateDesc::hash() const {
  HashState result;
  result.add(uint32_t(logicOp));

  for (size_t i = 0; i < GfxMaxColorAttachments; i++)
    result.add(renderTargets[i].hash());

  return result;
}


size_t GfxMultisampleStateDesc::hash() const {
  HashState result;
  result.add(sampleCount);
  result.add(sampleMask);
  result.add(uint32_t(enableAlphaToCoverage));
  return result;
}


size_t GfxRenderTargetStateDesc::hash() const {
  HashState result;

  for (size_t i = 0; i < GfxMaxColorAttachments; i++)
    result.add(uint32_t(colorFormats[i]));

  result.add(uint32_t(depthStencilFormat));
  result.add(sampleCount);
  return result;
}


size_t GfxGraphicsStateDesc::hash() const {
  HashState result;
  result.add(vertexInputState.hash());
  result.add(rasterizerState.hash());
  result.add(depthStencilState.hash());
  result.add(colorBlendState.hash());
  result.add(multisampleState.hash());
  result.add(renderTargetState.hash());
  return result;
}


size_t GfxPrimitiveTopology::hash() const {
  HashState result;
  result.add(uint32_t(primitiveType));
  result.add(uint32_t(patchVertexCount));
  return result;
}


size_t GfxVertexLayout::hash() const {
  HashState result;

  for (const auto& attribute : attributes)
    result.add(attribute.hash());

  return result;
}


size_t GfxDepthBias::hash() const {
  HashState result;
  result.add(hashFloat(depthBias));
  result.add(hashFloat(depthBiasSlope));
  result.add(hashFloat(depthBiasClamp));
  return result;
}


size_t GfxShadingRate::hash() const {
  HashState result;
  result.add(uint32_t(shadingRateOp));
  result.add(shadingRate.at<0>());
  result.add(shadingRate.at<1>());
  return result;
}


size_t GfxDepthTest::hash() const {
  HashState result;
  result.add(uint32_t(enableDepthWrite));
  result.add(uint32_t(enableDepthBoundsTest));
  result.add(uint32_t(depthCompareOp));
  return result;
}


size_t GfxStencilTest::hash() const {
  HashState result;
  result.add(front.hash());
  result.add(back.hash());
  return result;
}


size_t GfxMultisampling::hash() const {
  HashState result;
  result.add(sampleCount);
  result.add(sampleMask);
  result.add(uint32_t(enableAlphaToCoverage));
  return result;
}


size_t GfxBlending::hash() const {
  HashState result;
  result.add(uint32_t(logicOp));

  for (const auto& renderTarget : renderTargets)
    result.add(renderTarget.hash());

  return result;
}


GfxRenderStateData::GfxRenderStateData(
  const GfxRenderStateDesc&           desc) {
  if (desc.primitiveTopology) {
    flags |= GfxRenderStateFlag::ePrimitiveTopology;
    primitiveTopology = *desc.primitiveTopology;
  }

  if (desc.vertexLayout) {
    flags |= GfxRenderStateFlag::eVertexLayout;
    vertexLayout = *desc.vertexLayout;
  }

  if (desc.frontFace) {
    flags |= GfxRenderStateFlag::eFrontFace;
    frontFace = *desc.frontFace;
  }

  if (desc.cullMode) {
    flags |= GfxRenderStateFlag::eCullMode;
    cullMode = *desc.cullMode;
  }

  if (desc.conservativeRaster) {
    flags |= GfxRenderStateFlag::eConservativeRaster;
    conservativeRaster = *desc.conservativeRaster;
  }

  if (desc.depthBias) {
    flags |= GfxRenderStateFlag::eDepthBias;
    depthBias = *desc.depthBias;
  }

  if (desc.shadingRate) {
    flags |= GfxRenderStateFlag::eShadingRate;
    shadingRate = *desc.shadingRate;
  }

  if (desc.depthTest) {
    flags |= GfxRenderStateFlag::eDepthTest;
    depthTest = *desc.depthTest;
  }

  if (desc.stencilTest) {
    flags |= GfxRenderStateFlag::eStencilTest;
    stencilTest = *desc.stencilTest;
  }

  if (desc.multisampling) {
    flags |= GfxRenderStateFlag::eMultisampling;
    multisampling = *desc.multisampling;
  }

  if (desc.blending) {
    flags |= GfxRenderStateFlag::eBlending;
    blending = *desc.blending;
  }
}


size_t GfxRenderStateData::hash() const {
  HashState result;
  result.add(uint32_t(flags));
  result.add(primitiveTopology.hash());
  result.add(vertexLayout.hash());
  result.add(uint32_t(frontFace));
  result.add(uint32_t(cullMode));
  result.add(uint32_t(conservativeRaster));
  result.add(depthBias.hash());
  result.add(shadingRate.hash());
  result.add(depthTest.hash());
  result.add(stencilTest.hash());
  result.add(multisampling.hash());
  result.add(blending.hash());
  return result;
}


GfxRenderStateIface::GfxRenderStateIface(
  const GfxRenderStateData&           desc)
: m_data(desc) {
  if (desc.flags & GfxRenderStateFlag::ePrimitiveTopology)
    m_desc.primitiveTopology = &m_data.primitiveTopology;
  if (desc.flags & GfxRenderStateFlag::eVertexLayout)
    m_desc.vertexLayout = &m_data.vertexLayout;
  if (desc.flags & GfxRenderStateFlag::eFrontFace)
    m_desc.frontFace = &m_data.frontFace;
  if (desc.flags & GfxRenderStateFlag::eCullMode)
    m_desc.cullMode = &m_data.cullMode;
  if (desc.flags & GfxRenderStateFlag::eConservativeRaster)
    m_desc.conservativeRaster = &m_data.conservativeRaster;
  if (desc.flags & GfxRenderStateFlag::eDepthBias)
    m_desc.depthBias = &m_data.depthBias;
  if (desc.flags & GfxRenderStateFlag::eShadingRate)
    m_desc.shadingRate = &m_data.shadingRate;
  if (desc.flags & GfxRenderStateFlag::eStencilTest)
    m_desc.stencilTest = &m_data.stencilTest;
  if (desc.flags & GfxRenderStateFlag::eMultisampling)
    m_desc.multisampling = &m_data.multisampling;
  if (desc.flags & GfxRenderStateFlag::eBlending)
    m_desc.blending = &m_data.blending;
}


GfxRenderStateIface::~GfxRenderStateIface() {

}


size_t GfxGraphicsPipelineDesc::hash() const {
  HashState result;
  result.add(vertex.hash());
  result.add(tessControl.hash());
  result.add(tessEval.hash());
  result.add(geometry.hash());
  result.add(fragment.hash());
  return result;
}


size_t GfxMeshPipelineDesc::hash() const {
  HashState result;
  result.add(task.hash());
  result.add(mesh.hash());
  result.add(fragment.hash());
  return result;
}


GfxGraphicsPipelineIface::GfxGraphicsPipelineIface(
  const GfxGraphicsPipelineDesc&      desc) {
  if (desc.vertex)
    m_stages |= GfxShaderStage::eVertex;

  if (desc.tessControl)
    m_stages |= GfxShaderStage::eTessControl;

  if (desc.tessEval)
    m_stages |= GfxShaderStage::eTessEval;

  if (desc.geometry)
    m_stages |= GfxShaderStage::eGeometry;

  if (desc.fragment)
    m_stages |= GfxShaderStage::eFragment;

  if (desc.debugName) {
    m_debugName = desc.debugName;
  } else {
    std::stringstream builder;

    if (desc.vertex)
      builder << "v:" << desc.vertex->getDebugName();

    if (desc.tessControl)
      builder << ",c:" << desc.tessControl->getDebugName();

    if (desc.tessEval)
      builder << ",e:" << desc.tessEval->getDebugName();

    if (desc.geometry)
      builder << ",g:" << desc.geometry->getDebugName();

    if (desc.fragment)
      builder << ",f:" << desc.fragment->getDebugName();

    m_debugName = builder.str();
  }
}


GfxGraphicsPipelineIface::GfxGraphicsPipelineIface(
  const GfxMeshPipelineDesc&          desc) {
  if (desc.mesh)
    m_stages |= GfxShaderStage::eMesh;

  if (desc.task)
    m_stages |= GfxShaderStage::eTask;

  if (desc.debugName) {
    m_debugName = desc.debugName;
  } else {
    std::stringstream builder;

    if (desc.task)
      builder << "t:" << desc.task->getDebugName() << ",";

    if (desc.mesh)
      builder << "m:" << desc.task->getDebugName();

    if (desc.fragment)
      builder << ",f:" << desc.fragment->getDebugName();

    m_debugName = builder.str();
  }
}


GfxComputePipelineIface::GfxComputePipelineIface(
  const GfxComputePipelineDesc&       desc) {
  m_debugName = desc.debugName
    ? std::string(desc.debugName)
    : desc.compute->getDebugName();
}

}
