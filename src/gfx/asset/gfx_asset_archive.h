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
  const IoArchiveFile*        m_archiveFile;

  GfxAssetStatus              m_status = GfxAssetStatus::eNonResident;

  GfxGeometry                 m_geometry;
  GfxBuffer                   m_buffer;

  uint64_t                    m_streamBatchId = 0u;

};


/**
 * \brief Archive-backed texture asset
 */
class GfxAssetTextureFromArchive : public GfxAssetIface {

public:

  GfxAssetTextureFromArchive(
          GfxTransferManager            transferManager,
    const IoArchiveFile*                file);

  ~GfxAssetTextureFromArchive();

  /**
   * \brief Queries current asset properties
   * \returns Asset properties
   */
  GfxAssetProperties getAssetInfo() const override;

  /**
   * \brief Begins stream request for the asset
   *
   * Creates the GPU image and streams the selected mip levels.
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
   * \brief Computes sub file idenfitier for a subresource
   *
   * \param [in] layer Array layer index
   * \param [in] mip Mip level index, or -1 for the mip tail
   * \returns Sub file identifier for the given subresource
   */
  static FourCC getSubFileIdentifier(
          uint32_t                      layer,
          int32_t                       mip);

private:

  GfxTransferManager          m_transferManager;
  const IoArchiveFile*        m_archiveFile;

  GfxAssetStatus              m_status = GfxAssetStatus::eNonResident;
  uint32_t                    m_descriptor = 0u;

  GfxTextureDesc              m_desc;
  GfxImage                    m_image;

  uint64_t                    m_streamBatchId = 0u;

  const IoArchiveSubFile* getSubFile(
          uint32_t                      layer,
          uint32_t                      mip) const;

};

}
