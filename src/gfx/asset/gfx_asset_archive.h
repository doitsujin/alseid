#pragma once

#include "gfx_asset.h"
#include "gfx_asset_manager.h"

#include "../gfx_transfer.h"

#include "../../io/io_archive.h"

namespace as {

/**
 * \brief Archive-backed geometry asset
 */
class GfxAssetGeometryFromArchive : public GfxAssetGeometryIface {

public:

  GfxAssetGeometryFromArchive(
          GfxTransferManager            transferManager,
          std::shared_ptr<IoArchive>    archive,
    const IoArchiveFile*                file);

  ~GfxAssetGeometryFromArchive();

  /**
   * \brief Queries current asset properties
   * \returns Asset properties
   */
  GfxAssetProperties getAssetInfo() const override;

  /**
   * \brief Begins stream request for the asset
   *
   * Creates the GPU buffer and forwards the stream
   * request to the transfer manager.
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID on the timeline
   * \returns \c true if the asset can be made resident immediately.
   */
  bool requestStream(
          GfxAssetManagerIface          assetManager,
          uint32_t                      frameId) override;

  /**
   * \brief Begins eviction request for the asset
   *
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID on the timeline
   */
  void requestEviction(
          GfxAssetManagerIface          assetManager,
          uint32_t                      frameId) override;

  /**
   * \brief Marks the asset as resident
   *
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID on the timeline
   */
  void makeResident(
          GfxAssetManagerIface          assetManager) override;

  /**
   * \brief Evicts asset
   * \param [in] assetManager Asset manager
   */
  void evict(
          GfxAssetManagerIface          assetManager) override;

  /**
   * \brief Retrieves geometry pointer
   * \returns Geometry pointer
   */
  const GfxGeometry* getGeometry() const override;

private:

  GfxTransferManager          m_transferManager;

  std::shared_ptr<IoArchive>  m_archive;
  const IoArchiveFile*        m_archiveFile;

  GfxAssetStatus              m_status = GfxAssetStatus::eNonResident;

  GfxGeometry                 m_geometry;
  GfxBuffer                   m_buffer;

  uint64_t                    m_streamBatchId = 0u;

};

}
