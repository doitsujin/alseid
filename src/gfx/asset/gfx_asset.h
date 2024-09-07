#pragma once

#include <atomic>
#include <memory>

#include "../gfx_device.h"
#include "../gfx_geometry.h"

#include "../../io/io_archive.h"

#include "../../util/util_handle.h"
#include "../../util/util_small_vector.h"

namespace as {

class GfxAssetManager;
class GfxAssetManagerIface;

/**
 * \brief Asset type
 */
enum class GfxAssetType : uint8_t {
  /** Undefined asset type. */
  eUndefined  = 0u,
  /** Plain data buffer. The asset will be accessed using its GPU
   *  address directly and must be fully resident to be valid. */
  eBuffer     = 1u,
  /** Geometry buffer asset. The asset will be accessed using the
   *  GPU address of the metadata buffer, and additional buffers
   *  may be provided for higher-detailed LODs. */
  eGeometry   = 2u,
  /** Texture. The asset will be accessed using a 32-bit descriptor
   *  index that points to a view referencing all subresources. */
  eTexture    = 3u,
  /** Sampler. The asset will be accessed using a 32-bit descriptor
   *  index into a dedicated sampler descriptor array, is always
   *  resident, and does not use any backing storage. */
  eSampler    = 4u,
};


/**
 * \brief Asset residency flags
 */
enum class GfxAssetStatus : uint8_t {
  /* Asset is not resident and cannot be used for rendering. */
  eNonResident      = 0u,
  /** Indicates that the resource is fully resident and can be
   *  used for rendering. */
  eResident         = 1u,
  /** Indicates that a request to make this resource resident is
   *  currently queued up, but has not yet completed. */
  eStreamRequest    = 2u,
  /** Indicates that a request to evict the resource from memory
   *  has been set. Any resource with an evict request \e must
   *  be marked as non-resident, even if GPU resources are still
   *  alive, in order to avoid potential use-after-free issues.
   *  Any resource in this state can be immediately made resident
   *  again if desired, or if memory is needed, GPU resources can
   *  be destroyed if the GPU is done with the asset. */
  eEvictRequest     = 3u,
};


/**
 * \brief Asset properties
 */
struct GfxAssetProperties {
  /** Asset type. The asset reference type can be
   *  deduced from the asset type itself. */
  GfxAssetType type = GfxAssetType::eUndefined;
  /** Current residency status */
  GfxAssetStatus status = GfxAssetStatus::eNonResident;
  /** Descriptor index, if applicable. */
  uint32_t descriptorIndex = 0u;
  /** Buffer GPU address, if applicable. */
  uint64_t gpuAddress = 0ull;
  /** GPU memory allocation size of the asset. For certain asset
   *  types, this will always be zero. Used for eviction heuristics. */
  uint64_t gpuSize = 0ull;
};


/**
 * \brief Asset interface
 *
 * Provides methods to load and evict an asset. While explicit locking
 * is not necessary, all methods may be called from worker threads, and
 * app-provided assets must ensure to properly align resource usage with
 * the frame timeline.
 */
class GfxAssetIface {

public:

  virtual ~GfxAssetIface();

  /**
   * \brief Queries current asset properties
   *
   * This includes the residency status and asset references.
   * Used by the asset manager to update asset lists.
   * \returns Asset properties
   */
  virtual GfxAssetProperties getAssetInfo() const = 0;

  /**
   * \brief Begins stream request for the asset
   *
   * Creates GPU resources and populates them with data as necessary.
   * This process should ideally happen asynchronously in order to
   * avoid stalls. Will only be called if the asset is non-resident.
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID on the timeline
   * \returns \c true if the asset can be made resident immediately.
   */
  virtual bool requestStream(
          GfxAssetManagerIface          assetManager,
          uint32_t                      frameId) = 0;

  /**
   * \brief Begins eviction request for the asset
   *
   * The asset \e must not be accessed by the GPU from the next
   * frame onwards, and full eviction \e may happen once the
   * current frame completes. Will only be called if the asset
   * is currently resident.
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID on the timeline
   */
  virtual void requestEviction(
          GfxAssetManagerIface          assetManager,
          uint32_t                      frameId) = 0;

  /**
   * \brief Marks the asset as resident
   *
   * Note that this may stall the calling thread until the
   * asset becomes available for use. Will only be called if
   * the asset has a completed stream request, or if it has
   * a pending eviction request but has not been evicted.
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID on the timeline
   */
  virtual void makeResident(
          GfxAssetManagerIface          assetManager) = 0;

  /**
   * \brief Evicts asset
   *
   * Destroys GPU resources and frees any descriptors allocated
   * from the asset manager. Will only be called if the asset is
   * either resident or an eviction request has been sent, and
   * if the GPU is no longer actively using the asset.
   * \param [in] assetManager Asset manager
   */
  virtual void evict(
          GfxAssetManagerIface          assetManager) = 0;

};


/**
 * \brief Geometry asset
 *
 * Base class for all assets representing a geometry.
 * Provides read access to the \c GfxGeometry object.
 */
class GfxAssetGeometryIface : public GfxAssetIface {

public:

  /**
   * \brief Retrieves geometry object pointer
   * \returns Pointer to geometry object
   */
  virtual const GfxGeometry* getGeometry() const = 0;

};


/**
 * \brief Asset info
 *
 * Stores the asset interface instance and some information
 * required for memory management heuristics.
 */
struct GfxAssetInfo {
  /** Asset interface instance that implements asset loading and
   *  residency methods. Callers \e must take the residency lock
   *  of the asset manager prior to calling any of its methods. */
  std::unique_ptr<GfxAssetIface> iface;
  /** Number of actively used asset groups containing this asset.
   *  This counter is only accessed by the worker when processing
   *  feedback for GPU-managed asset groups. */
  uint32_t activeGroupCount = 0u;
  /** Frame ID of when the asset has been marked as unused. This
   *  information is only useful if \c numActiveGroups is zero,
   *  and is used to implement an LRU scheme for asset eviction. */
  uint32_t activeFrameId = 0u;
};

using GfxAsset = Handle<GfxAssetInfo>;

}
