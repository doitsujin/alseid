#include "gfx_asset_archive.h"

namespace as {

GfxAssetGeometryFromArchive::GfxAssetGeometryFromArchive(
        GfxTransferManager            transferManager,
        std::shared_ptr<IoArchive>    archive,
  const IoArchiveFile*                file)
: m_transferManager (std::move(transferManager))
, m_archive         (std::move(archive))
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

  m_buffer = assetManager.getDevice()->createBuffer(bufferDesc, GfxMemoryType::eAny),

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

}
