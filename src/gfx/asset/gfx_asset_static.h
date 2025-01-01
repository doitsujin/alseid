#pragma once

#include "gfx_asset.h"
#include "gfx_asset_manager.h"

#include "../gfx_transfer.h"

#include "../../io/io_archive.h"

namespace as {

/**
 * \brief Static texture asset
 *
 * Uses an app-provided image. Contents may
 * change, but the image itself does not.
 */
class GfxAssetTextureStatic : public GfxAssetIface {

public:

  GfxAssetTextureStatic(
          GfxImage                      image,
          GfxImageViewType              type);

  ~GfxAssetTextureStatic();

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

private:

  GfxAssetStatus              m_status = GfxAssetStatus::eNonResident;
  uint32_t                    m_descriptor = 0u;

  GfxImageViewType            m_viewType = GfxImageViewType::e2D;
  GfxImage                    m_image;

};

/**
 * \brief Static texture asset
 *
 * Uses an app-provided image. Contents may
 * change, but the image itself does not.
 */
class GfxAssetSamplerStatic : public GfxAssetIface {

public:

  GfxAssetSamplerStatic(
          GfxSampler                    sampler);

  ~GfxAssetSamplerStatic();

  /**
   * \brief Queries current asset properties
   * \returns Asset properties
   */
  GfxAssetProperties getAssetInfo() const override;

  /**
   * \brief Begins stream request for the asset
   *
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

private:

  GfxAssetStatus              m_status = GfxAssetStatus::eNonResident;
  uint32_t                    m_descriptor = 0u;

  GfxSampler                  m_sampler;

};

}
