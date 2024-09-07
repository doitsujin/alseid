#pragma once

#include <atomic>
#include <shared_mutex>

#include "../gfx_buffer_pool.h"
#include "../gfx_device.h"

#include "gfx_asset.h"

namespace as {

/**
 * \brief Asset list header
 *
 * Stores GPU-side metadata for an asset list. This is immediately
 * followed by a dword array that stores descriptor indices and
 * bufer addresses.
 */
struct GfxAssetListHeader {
  /** Asset group handle. Used for the feedback buffer. */
  uint32_t handle;
  /** Reserved for future use. */
  uint32_t reserved;
  /** Frame ID of when the asset list has last been updated. Used
   *  to determine whether per-instances asset data needs updating. */
  uint32_t lastUpdateFrameId;
  /** Frame ID of when the asset list has last been accessed for
   *  rendering. Used to implement residency heuristics. */
  uint32_t lastAccessFrameId;
};

static_assert(sizeof(GfxAssetListHeader) == 16u);


/**
 * \brief Asset reference type
 */
enum class GfxAssetRefType : uint8_t {
  /** Descriptor index. May index into an arbitrary descriptor array
   *  that the application must bind before performing draws. Indices
   *  are represented as signed 32-bit integers, with a value of zero
   *  indicating that the resource is not valid or not resident. */
  eDescriptorIndex  = 0,
  /** Buffer address. Points directly to a buffer of an arbitrary type
   *  that the shader can interpret. Addresses are 64-bit unsigned
   *  integers, and must be aligned to 16 bytes. */
  eBufferAddress    = 1,
};


/**
 * \brief Queries asset reference type for asset type
 *
 * \param [in] type Asset type
 * \returns Asset reference type
 */
inline GfxAssetRefType gfxGetAssetRefType(GfxAssetType type) {
  return (type == GfxAssetType::eBuffer || type == GfxAssetType::eGeometry)
    ? GfxAssetRefType::eBufferAddress
    : GfxAssetRefType::eDescriptorIndex;
}


/**
 * \brief Queries asset reference size
 *
 * \param [in] type Asset reference type
 * \returns Size, in bytes, of asset reference
 */
inline uint32_t gfxGetAssetRefSize(GfxAssetRefType type) {
  return type == GfxAssetRefType::eDescriptorIndex
    ? sizeof(uint32_t)
    : sizeof(uint64_t);
}


/**
 * \brief Asset group type
 */
enum class GfxAssetGroupType : uint32_t {
  /** Residency of this asset group is entirely managed by the
   *  application, and GPU usage tracking is ignored. This is
   *  useful for basic functionality such as UI resources. */
  eAppManaged = 0u,
  /** Residency of this asset group is managed by the asset manager
   *  itself, using GPU feedback to track when the assets in the
   *  group are used for rendering. This is most useful for assets
   *  used in the actual scene. */
  eGpuManaged = 1u,
};


/**
 * \brief Asset group entry
 *
 * Stores the asset handle, asset type, and location where the
 * descriptor index or buffer address is stored in the buffer.
 */
struct GfxAssetGroupEntry {
  /** Asset handle. */
  GfxAsset asset;
  /** Asset reference type. Determines the entry size. */
  GfxAssetRefType type = GfxAssetRefType::eDescriptorIndex;
  /** Entry offet, in dwords, relative to the start
   *  of the asset list within the buffer slice. */
  uint24_t index = uint24_t(0u);
};


/**
 * \brief Asset group status flags
 *
 * Used to track the current residency status.
 */
enum class GfxAssetGroupStatus : uint32_t {
  /** All assets in the asset list are resident */
  eResident       = (1u << 0),
  /** The asset list is active and has ownership of all
   *  assets, so that assets will not be evicted. */
  eActive         = (1u << 1),

  eFlagEnum = 0u
};

using GfxAssetGroupStatusFlags = Flags<GfxAssetGroupStatus>;


/**
 * \brief Asset group
 *
 * Represents a group of assets that are expected to have a similar
 * lifetime, e.g. all assets used by the instances within a single
 * scene BVH node.
 *
 * Asset groups are introduced as a concept in order to reduce the
 * amount of usage tracking primarily on the GPU compared to other
 * approaches that would track per-asset usage and LODs. The tradeoff
 * is higher memory usage and less control over individual asset LODs.
 */
struct GfxAssetGroupInfo {  
  /** Asset group type. */
  GfxAssetGroupType type = GfxAssetGroupType::eAppManaged;
  /** Asset list status. */
  GfxAssetGroupStatusFlags status;
  /** List of asset handles in the group. */
  std::vector<GfxAssetGroupEntry> assets;
  /** Buffer slice that stores asset group metadata. */
  GfxBufferSlice bufferSlice;
  /** Total number of dwords in the asset list. */
  uint32_t dwordCount = 0u;
  /** Frame ID of when the buffer has last been updated. Used to
   *  update the GPU buffer as necessary. */
  uint32_t lastUpdateFrameId = 0u;
  /** Frame ID of when updates have last been committed.  */
  uint32_t lastCommitFrameId = 0u;
  /** Frame ID of when the asset group has last been actively used
   *  for rendering. Note that this relies on GPU feedback and will
   *  be several frames out of date; making any changes to asset
   *  residency \e must consider that the asset group may have been
   *  accessed in the current frame regardless. This is still useful
   *  for eviction heuristics. */
  uint32_t lastUseFrameId = 0u;
};

using GfxAssetGroup = Handle<GfxAssetGroupInfo>;

}
