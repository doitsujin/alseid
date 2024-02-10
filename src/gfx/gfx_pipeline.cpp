#include <sstream>

#include "gfx_pipeline.h"

namespace as {

size_t GfxVertexInputAttribute::hash() const {
  HashState result;
  result.add(binding);
  result.add(uint32_t(format));
  result.add(offset);
  result.add(uint32_t(inputRate));
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


size_t GfxRenderTargetStateDesc::hash() const {
  HashState result;

  for (size_t i = 0; i < GfxMaxColorAttachments; i++)
    result.add(uint32_t(colorFormats[i]));

  result.add(uint32_t(depthStencilFormat));
  result.add(sampleCount);
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


bool GfxRenderStateDesc::operator == (const GfxRenderStateDesc& other) const {
  bool eq = flags == other.flags;

  if (eq && (flags & GfxRenderStateFlag::ePrimitiveTopology))
    eq = primitiveTopology == other.primitiveTopology;

  if (eq && (flags & GfxRenderStateFlag::eVertexLayout))
    eq = vertexLayout == other.vertexLayout;

  if (eq && (flags & GfxRenderStateFlag::eFrontFace))
    eq = frontFace == other.frontFace;

  if (eq && (flags & GfxRenderStateFlag::eCullMode))
    eq = cullMode == other.cullMode;

  if (eq && (flags & GfxRenderStateFlag::eConservativeRaster))
    eq = conservativeRaster == other.conservativeRaster;

  if (eq && (flags & GfxRenderStateFlag::eDepthBias))
    eq = depthBias == other.depthBias;

  if (eq && (flags & GfxRenderStateFlag::eShadingRate))
    eq = shadingRate == other.shadingRate;

  if (eq && (flags & GfxRenderStateFlag::eDepthTest))
    eq = depthTest == other.depthTest;

  if (eq && (flags & GfxRenderStateFlag::eStencilTest))
    eq = stencilTest == other.stencilTest;

  if (eq && (flags & GfxRenderStateFlag::eMultisampling))
    eq = multisampling == other.multisampling;

  if (eq && (flags & GfxRenderStateFlag::eBlending))
    eq = blending == other.blending;

  return eq;
}


bool GfxRenderStateDesc::operator != (const GfxRenderStateDesc& other) const {
  return !operator == (other);
}


size_t GfxRenderStateDesc::hash() const {
  HashState result;
  result.add(uint32_t(flags));

  if (flags & GfxRenderStateFlag::ePrimitiveTopology)
    result.add(primitiveTopology.hash());

  if (flags & GfxRenderStateFlag::eVertexLayout)
    result.add(vertexLayout.hash());

  if (flags & GfxRenderStateFlag::eFrontFace)
    result.add(uint32_t(frontFace));

  if (flags & GfxRenderStateFlag::eCullMode)
    result.add(uint32_t(cullMode));

  if (flags & GfxRenderStateFlag::eConservativeRaster)
    result.add(uint32_t(conservativeRaster));

  if (flags & GfxRenderStateFlag::eDepthBias)
    result.add(depthBias.hash());

  if (flags & GfxRenderStateFlag::eShadingRate)
    result.add(shadingRate.hash());

  if (flags & GfxRenderStateFlag::eDepthTest)
    result.add(depthTest.hash());

  if (flags & GfxRenderStateFlag::eStencilTest)
    result.add(stencilTest.hash());

  if (flags & GfxRenderStateFlag::eMultisampling)
    result.add(multisampling.hash());

  if (flags & GfxRenderStateFlag::eBlending)
    result.add(blending.hash());

  return result;
}


GfxRenderStateIface::GfxRenderStateIface(
  const GfxRenderStateDesc&           desc)
: m_desc(desc) {

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
