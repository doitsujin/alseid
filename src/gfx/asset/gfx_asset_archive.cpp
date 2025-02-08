#include "gfx_asset_archive.h"

#include "../gfx.h"

namespace as {

GfxAssetGeometryFromArchive::GfxAssetGeometryFromArchive(
        GfxTransferManager            transferManager,
  const IoArchiveFile*                file)
: m_transferManager (std::move(transferManager))
, m_archiveFile     (file) {
  if (!m_geometry.deserialize(file->getInlineData()))
    throw Error("Failed to deserialize geometry data");
}


GfxAssetGeometryFromArchive::~GfxAssetGeometryFromArchive() {

}


GfxAssetProperties GfxAssetGeometryFromArchive::getAssetInfo() const {
  GfxAssetProperties result = { };
  result.type = GfxAssetType::eGeometry;
  result.status = m_status;

  if (m_buffer) {
    result.gpuAddress = m_buffer->getGpuAddress();
    result.gpuSize = m_buffer->getMemoryInfo().size;
  }

  return result;
}


bool GfxAssetGeometryFromArchive::requestStream(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eStreamRequest;

  auto subFile = m_archiveFile->getSubFile(0);

  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = m_archiveFile->getName();
  bufferDesc.size = subFile->getSize();
  bufferDesc.usage = GfxUsage::eShaderResource |
    GfxUsage::eDecompressionDst |
    GfxUsage::eTransferDst;

  m_buffer = assetManager.getDevice()->createBuffer(bufferDesc, GfxMemoryType::eAny);

  m_streamBatchId = m_transferManager->uploadBuffer(subFile, m_buffer, 0);
  return false;
}


void GfxAssetGeometryFromArchive::requestEviction(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eEvictRequest;
}


void GfxAssetGeometryFromArchive::makeResident(
        GfxAssetManagerIface          assetManager) {
  // Ensure that all buffer data is valid at this point
  m_transferManager->waitForCompletion(m_streamBatchId);

  m_status = GfxAssetStatus::eResident;
}


void GfxAssetGeometryFromArchive::evict(
        GfxAssetManagerIface          assetManager) {
  m_status = GfxAssetStatus::eNonResident;
  m_buffer = GfxBuffer();
}


const GfxGeometry* GfxAssetGeometryFromArchive::getGeometry() const {
  return &m_geometry;
}




GfxAssetTextureFromArchive::GfxAssetTextureFromArchive(
        GfxTransferManager            transferManager,
  const IoArchiveFile*                file)
: m_transferManager (std::move(transferManager))
, m_archiveFile     (file) {
  if (!m_desc.deserialize(file->getInlineData()))
    throw Error("Failed to deserialize texture metadata");
}


GfxAssetTextureFromArchive::~GfxAssetTextureFromArchive() {

}


GfxAssetProperties GfxAssetTextureFromArchive::getAssetInfo() const {
  GfxAssetProperties result = { };
  result.type = GfxAssetType::eTexture;
  result.status = m_status;

  if (m_image) {
    result.descriptorIndex = m_descriptor;
    result.gpuSize = m_image->getMemoryInfo().size;
  }

  return result;
}


bool GfxAssetTextureFromArchive::requestStream(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  // TODO support some sort of size cap
  m_status = GfxAssetStatus::eStreamRequest;

  GfxImageDesc imageDesc = { };
  m_desc.fillImageDesc(imageDesc, 0);

  imageDesc.debugName = m_archiveFile->getName();
  imageDesc.usage |= GfxUsage::eShaderResource |
    GfxUsage::eDecompressionDst |
    GfxUsage::eTransferDst;

  m_image = assetManager.getDevice()->createImage(imageDesc, GfxMemoryType::eAny);

  GfxFormatInfo formatInfo = Gfx::getFormatInfo(m_desc.format);

  GfxImageViewDesc viewDesc = { };
  viewDesc.type = m_desc.type;
  viewDesc.format = m_desc.format;
  viewDesc.usage = GfxUsage::eShaderResource;
  viewDesc.subresource.aspects = formatInfo.aspects;
  viewDesc.subresource.mipCount = m_desc.mips;
  viewDesc.subresource.layerCount = m_desc.layers;

  GfxImageView view = m_image->createView(viewDesc);
  m_descriptor = assetManager.createDescriptor(GfxAssetType::eTexture, view->getDescriptor());

  for (uint32_t l = 0; l < m_desc.layers; l++) {
    for (uint32_t m = 0; m < std::min(m_desc.mipTailStart + 1u, m_desc.mips); m++) {
      auto subFile = getSubFile(l, m);

      if (!subFile) {
        Log::err(m_archiveFile->getName(), ": No sub file found for layer ", l, ", mip ", m);
        continue;
      }

      GfxImageSubresource subresource = { };
      subresource.aspects = formatInfo.aspects;
      subresource.mipIndex = m;
      subresource.mipCount = (m >= m_desc.mipTailStart) ? m_desc.mips - m : 1u;
      subresource.layerIndex = l;
      subresource.layerCount = 1u;

      m_streamBatchId = m_transferManager->uploadImage(subFile, m_image, subresource);
    }
  }

  return false;
}


void GfxAssetTextureFromArchive::requestEviction(
        GfxAssetManagerIface          assetManager,
        uint32_t                      frameId) {
  m_status = GfxAssetStatus::eEvictRequest;
}


void GfxAssetTextureFromArchive::makeResident(
        GfxAssetManagerIface          assetManager) {
  // Ensure that all streamed data is valid at this point
  m_transferManager->waitForCompletion(m_streamBatchId);

  m_status = GfxAssetStatus::eResident;
}


void GfxAssetTextureFromArchive::evict(
        GfxAssetManagerIface          assetManager) {
  m_status = GfxAssetStatus::eNonResident;
  m_image = GfxImage();

  if (m_descriptor) {
    assetManager.freeDescriptor(GfxAssetType::eTexture, m_descriptor);
    m_descriptor = 0u;
  }
}


FourCC GfxAssetTextureFromArchive::getSubFileIdentifier(
        uint32_t                      layer,
        int32_t                       mip) {
  static const std::array<char, 16> lut = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
  };

  char mipPart = mip < 0 ? 'T' : lut[mip & 0xf];

  return FourCC(
    lut[bextract(layer, 8, 4)],
    lut[bextract(layer, 4, 4)],
    lut[bextract(layer, 0, 4)],
    mipPart);
}


const IoArchiveSubFile* GfxAssetTextureFromArchive::getSubFile(
        uint32_t                      layer,
        uint32_t                      mip) const {
  FourCC identifier = getSubFileIdentifier(layer,
    mip >= m_desc.mipTailStart ? -1 : int32_t(mip));

  return m_archiveFile->findSubFile(identifier);
}

}
