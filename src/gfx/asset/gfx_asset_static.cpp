#include "gfx_asset_static.h"

#include "../gfx.h"

namespace as {

GfxAssetTextureStatic::GfxAssetTextureStatic(
        GfxImage                      image,
        GfxImageViewType              type)
: m_viewType  (type)
, m_image     (std::move(image)) {

}


GfxAssetTextureStatic::~GfxAssetTextureStatic() {

}


GfxAssetProperties GfxAssetTextureStatic::getAssetInfo() const {
  GfxAssetProperties result = { };
  result.type = GfxAssetType::eTexture;
  result.status = m_status;
  result.descriptorIndex = m_descriptor;
  result.gpuSize = m_image->getMemoryInfo().size;
  return result;
}


bool GfxAssetTextureStatic::requestStream(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eStreamRequest;

  GfxImageDesc desc = m_image->getDesc();

  GfxImageViewDesc viewDesc = { };
  viewDesc.type = m_viewType;
  viewDesc.format = desc.format;
  viewDesc.usage = GfxUsage::eShaderResource;
  viewDesc.subresource.aspects = Gfx::getFormatInfo(desc.format).aspects;
  viewDesc.subresource.mipCount = desc.mips;
  viewDesc.subresource.layerCount = desc.layers;

  GfxImageView view = m_image->createView(viewDesc);
  m_descriptor = assetManager.createDescriptor(GfxAssetType::eTexture, view->getDescriptor());
  return true;
}


void GfxAssetTextureStatic::requestEviction(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eEvictRequest;
}


void GfxAssetTextureStatic::makeResident(
        GfxAssetManagerIface          assetManager) {
  m_status = GfxAssetStatus::eResident;
}


void GfxAssetTextureStatic::evict(
        GfxAssetManagerIface          assetManager) {
  m_status = GfxAssetStatus::eNonResident;

  if (m_descriptor) {
    assetManager.freeDescriptor(GfxAssetType::eTexture, m_descriptor);
    m_descriptor = 0u;
  }
}




GfxAssetSamplerStatic::GfxAssetSamplerStatic(
        GfxSampler                    sampler)
: m_sampler(std::move(sampler)) {

}


GfxAssetSamplerStatic::~GfxAssetSamplerStatic() {

}


GfxAssetProperties GfxAssetSamplerStatic::getAssetInfo() const {
  GfxAssetProperties result = { };
  result.type = GfxAssetType::eSampler;
  result.status = m_status;
  result.descriptorIndex = m_descriptor;
  return result;
}


bool GfxAssetSamplerStatic::requestStream(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eStreamRequest;
  m_descriptor = assetManager.createDescriptor(GfxAssetType::eSampler, m_sampler->getDescriptor());
  return true;
}


void GfxAssetSamplerStatic::requestEviction(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eEvictRequest;
}


void GfxAssetSamplerStatic::makeResident(
        GfxAssetManagerIface          assetManager) {
  m_status = GfxAssetStatus::eResident;
}


void GfxAssetSamplerStatic::evict(
        GfxAssetManagerIface          assetManager) {
  m_status = GfxAssetStatus::eNonResident;

  if (m_descriptor) {
    assetManager.freeDescriptor(GfxAssetType::eSampler, m_descriptor);
    m_descriptor = 0u;
  }
}

}
