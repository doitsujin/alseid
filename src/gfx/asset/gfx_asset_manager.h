#pragma once

#include <queue>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "../../util/util_object_map.h"

#include "../gfx_buffer_pool.h"
#include "../gfx_device.h"

#include "gfx_asset.h"
#include "gfx_asset_descriptor.h"
#include "gfx_asset_group.h"
#include "gfx_asset_pipelines.h"

namespace as {

/**
 * \brief Asset request type
 */
enum class GfxAssetRequestType : uint32_t {
  /* Request to stop worker thread. */
  eStopWorker         = 0u,
  /* Explicit request to stream in an app-managed asset group. */
  eRequestStream      = 1u,
  /* Explicit request to mark an app-managed asset group for
   * eviction. Assets will be marked as unused by that group. */
  eRequestEvict       = 2u,
  /** Marks inactive assets for eviction in order to meet memory
   *  budget constraints. */
  eEvictUnused        = 3u,
};


/**
 * \brief Asset request
 */
struct GfxAssetRequest {
  /** Request type. */
  GfxAssetRequestType type = GfxAssetRequestType::eStopWorker;
  /** Asset group for which the request was made, if any. */
  GfxAssetGroup assetGroup;
};


/**
 * \brief Typed asset storage
 *
 * Pairs an object map with an allocator to store
 * asset data of a certain type.
 */
template<typename T>
struct GfxAssetStorage {
  /** Object allocator */
  ObjectAllocator allocator;
  /** Object map */
  ObjectMap<T, 14, 8> map;
};


/**
 * \brief Unused asset set
 *
 * Stores the asset handle and the frame ID of when the asset
 * has last been accessed. Useful to quickly manipulate or
 * iterate over the set.
 */
struct GfxAssetUnusedEntry {
  /** Frame ID of last use */
  uint32_t frameId = 0u;
  /** Asset handle */
  GfxAsset asset;

  auto operator <=> (const GfxAssetUnusedEntry& other) const {
    std::strong_ordering ordering = frameId <=> other.frameId;

    if (ordering == std::strong_ordering::equal)
      ordering = uint32_t(asset) <=> uint32_t(other.asset);

    return ordering;
  }
};


/**
 * \brief Asset manager
 *
 * Stores asset properties and provides methods to dynamically manage
 * residency at runtime, and provides descriptor pools for texture
 * and sampler assets which allow accessing the resources in shaders.
 *
 * In order to allow applications to either provide assets at runtime
 * or load them from existing files, this class does not tie asset
 * management to asset loading or streaming; that functionality is
 * instead provided by dedicated classes.
 */
class GfxAssetManager {
  constexpr static uint32_t TextureDescriptorCount = 256u << 10u;
  constexpr static uint32_t SamplerDescriptorCount =   1u << 10u;

  friend class GfxAssetManagerIface;
public:

  GfxAssetManager(
        GfxDevice                       device);

  ~GfxAssetManager();

  /**
   * \brief Feedback buffer address
   *
   * Only valid after a call to \c commitUpdates.
   * \returns Feedback buffer address
   */
  uint64_t getFeedbackBufferGpuAddress() const {
    return m_feedbackBuffer
      ? m_feedbackBuffer->getGpuAddress()
      : 0ull;
  }

  /**
   * \brief Asset group buffer address
   *
   * Asset groups are assigned to instances and other
   * objects via the GPU address of the asset list buffer.
   * \param [in] assetGroup Asset group to query
   * \returns GPU address of asset group buffer
   */
  uint64_t getAssetGroupGpuAddress(GfxAssetGroup assetGroup) const {
    return assetGroup
      ? m_groups.map[uint32_t(assetGroup)].bufferSlice.getGpuAddress()
      : 0ull;
  }

  /**
   * \brief Binds descriptor arrays to a context
   *
   * \param [in] context Context object
   * \param [in] samplerIndex Sampler set index
   * \param [in] textureIndex Texture set index
   */
  void bindDescriptorArrays(
    const GfxContext&                   context,
          uint32_t                      samplerIndex,
          uint32_t                      textureIndex) const;

  /**
   * \brief Creates asset
   *
   * \tparam T Non-virtual asset class
   * \param [in] name Unique asset name
   * \param [in] args Constructor args
   * \returns Asset handle
   */
  template<typename T, typename... Args>
  GfxAsset createAsset(
    const GfxSemanticName&              name,
          Args&&...                     args) {
    std::unique_ptr<GfxAssetIface> iface =
      std::make_unique<T>(std::forward<Args>(args)...);
    return createAssetWithIface(name, std::move(iface));
  }

  /**
   * \brief Retrieves asset interface
   *
   * Useful to access certain properties of a typed asset.
   * \tparam Interface type
   * \param [in] asset Asset handle
   * \returns Pointer to typed interface, or \c nullptr
   *    if the asset does not support the given interface type.
   */
  template<typename T>
  const T* getAssetAs(
          GfxAsset                      asset) const {
    return dynamic_cast<const T*>(m_assets.map[uint32_t(asset)].iface.get());
  }

  /**
   * \brief Looks up asset by name
   *
   * \param [in] name Asset name
   * \returns Asset handle
   */
  GfxAsset findAsset(
    const GfxSemanticName&              name);

  /**
   * \brief Creates asset group
   *
   * \param [in] name Unique asset group name
   * \param [in] type Asset group type
   * \param [in] assetCount Number of assets
   * \param [in] assets Assets to add to the group
   * \returns Asset group handle
   */
  GfxAssetGroup createAssetGroup(
    const GfxSemanticName&              name,
          GfxAssetGroupType             type,
          uint32_t                      assetCount,
    const GfxAsset*                     assets);

  /**
   * \brief Looks up asset group by name
   *
   * \param [in] name Asset group name
   * \returns Asset group handle
   */
  GfxAssetGroup findAssetGroup(
    const GfxSemanticName&              name);

  /**
   * \brief Streams in an asset group
   *
   * Can be used both for app-managed and GPU-managed asset groups,
   * with the latter being useful to avoid pop-in when loading a new
   * scene. In that case, the last active frame ID of each asset will
   * be updated so that assets are not evicted again immediately.
   * \param [in] group Asset group to load
   */
  void streamAssetGroup(
          GfxAssetGroup                 group);

  /**
   * \brief Requests eviction of asset group
   *
   * Releases ownership of all assets in the group so that they
   * can be freed if the application is running low on GPU memory.
   * Calling this on a GPU-managed asset group may not have the
   * desired effect if it is still actively being used.
   * \param [in] group Asset group to evict
   */
  void evictAssetGroup(
          GfxAssetGroup                 group);

  /**
   * \brief Retrieves asset within an asset group
   *
   * \param [in] group Asset group
   * \param [in] index Asset index
   * \return Asset and dword index into the buffer
   */
  GfxAssetGroupEntry getAssetInGroup(
          GfxAssetGroup                 group,
          uint32_t                      index);

  /**
   * \brief Uploads dirty asset group buffers
   *
   * Ensures that all descriptor indices are valid and can
   * be safely accssed by the GPU in the current frame.
   * \param [in] context Context object
   * \param [in] currFrameId Current frame ID
   * \param [in] lastFrameId Last completed frame ID
   */
  void commitUpdates(
    const GfxContext&                   context,
          uint32_t                      currFrameId,
          uint32_t                      lastFrameId);

  /**
   * \brief Collects feedback for the current frame
   *
   * Allocates a scratch buffer slice from the given context.
   * The context must not be reset until the feedback has been
   * processed by the asset manager.
   * \param [in] context Feedback context
   * \returns Scratch buffer slice to map for reading
   */
  GfxScratchBuffer getFeedbackData(
    const GfxContext&                   context);

  /**
   * \brief Processes feedback
   *
   * Uses feedback from a completed frame to stream in or evict asset groups.
   * \param [in] feedback Buffer slice returned from \c getFeedbackData.
   * \param [in] frameId Frame ID of when the feedback was captured
   */
  void processFeedback(
    const GfxScratchBuffer&             feedback,
          uint32_t                      frameId);

private:

  GfxDevice                           m_device;
  GfxAssetPipelines                   m_pipelines;

  GfxAssetDescriptorPool              m_samplerPool;
  GfxAssetDescriptorPool              m_texturePool;

  GfxBufferPool                       m_groupBuffers;

  GfxBuffer                           m_feedbackBuffer;

  std::vector<GfxAssetGroup>          m_feedbackGroups;
  uint32_t                            m_feedbackFrameId = 0u;

  GfxAssetStorage<GfxAssetInfo>       m_assets;
  GfxAssetStorage<GfxAssetGroupInfo>  m_groups;

  alignas(CacheLineSize)
  std::mutex                          m_assetMutex;
  uint32_t                            m_currFrameId = 1u;
  uint32_t                            m_lastFrameId = 0u;

  uint64_t                            m_gpuMemoryBudget = 0ull;
  uint64_t                            m_gpuMemoryUsed = 0ull;

  std::unordered_multimap<GfxAsset,
    GfxAssetGroup, HashMemberProc>    m_groupList;

  std::vector<GfxAssetGroup>          m_dirtyGroups;

  alignas(CacheLineSize)
  std::shared_mutex                   m_assetLutMutex;
  std::unordered_map<GfxSemanticName,
    GfxAsset, HashMemberProc>         m_assetLut;

  alignas(CacheLineSize)
  std::shared_mutex                   m_groupLutMutex;
  std::unordered_map<GfxSemanticName,
    GfxAssetGroup, HashMemberProc>    m_groupLut;

  alignas(CacheLineSize)
  std::mutex                          m_requestLock;
  std::condition_variable             m_requestCond;
  std::queue<GfxAssetRequest>         m_requestQueue;
  std::thread                         m_requestWorker;

  std::set<GfxAssetUnusedEntry>       m_unusedAssets;
  size_t                              m_unusedCleanupIndex = 0u;

  alignas(CacheLineSize)
  std::mutex                          m_residencyLock;
  std::condition_variable             m_residencyCond;
  std::queue<GfxAsset>                m_residencyQueue;
  std::thread                         m_residencyWorker;

  void adjustGpuMemory(
            int64_t                     amount) {
    m_gpuMemoryUsed += amount;
  }

  GfxAssetDescriptorPool* getDescriptorPool(
          GfxAssetType                  type);

  uint32_t createDescriptor(
          GfxAssetType                  type,
    const GfxDescriptor&                descriptor);

  void freeDescriptor(
          GfxAssetType                  type,
          uint32_t                      index);

  GfxAssetInfo& getAsset(
          GfxAsset                      asset);

  GfxAssetGroupInfo& getAssetGroup(
          GfxAssetGroup                 assetGroup);

  GfxAsset createAssetWithIface(
    const GfxSemanticName&              name,
          std::unique_ptr<GfxAssetIface>&& iface);

  void dirtyAssetGroups(
          GfxAsset                      asset,
          uint32_t                      currFrameId);

  GfxAsset registerNamedAsset(
    const GfxSemanticName&              name,
          uint32_t                      index);

  GfxAssetGroup registerNamedGroup(
    const GfxSemanticName&              name,
          uint32_t                      index);

  bool writeAssetData(
          uint32_t*                     dwords,
          GfxAssetRefType               type,
          GfxAsset                      asset);

  uint64_t computeFeedbackBufferSize() const;

  GfxBuffer createFeedbackBuffer(
          uint64_t                      size) const;

  void enqueueRequest(
    const GfxAssetRequest&              rq);

  void enqueueStreamAsset(
          GfxAsset                      asset);

  void executeStreamRequest(
          GfxAssetGroup                 assetGroup);

  void executeEvictRequest(
          GfxAssetGroup                 assetGroup);

  void executeEvictUnusedRequest();

  void runRequestWorker();

  void runResidencyWorker();

  void runEvictionWorker();

  static GfxBufferDesc getGroupBufferDesc();

};


/**
 * \brief Private asset manager interface
 *
 * Provides access to lower-level functionality for asset
 * interface implementations.
 */
class GfxAssetManagerIface {
  friend GfxAssetManager;
public:

  GfxAssetManagerIface() = default;

  /**
   * \brief Queries device
   * \returns Graphics device
   */
  GfxDevice getDevice() const {
    return m_assetManager->m_device;
  }

  /**
   * \brief Notifies GPU memory being allocated
   *
   * Increments the counter for memory used.
   * \param [in] size Number of bytes allocated
   */
  void notifyMemoryAlloc(
            uint64_t                    size) {
    m_assetManager->adjustGpuMemory(int64_t(size));
  }

  /**
   * \brief Notifies GPU memory being freed
   *
   * Decrements the counter for memory used.
   * \param [in] size Number of bytes freed
   */
  void notifyMemoryFree(
            uint64_t                    size) {
    m_assetManager->adjustGpuMemory(-int64_t(size));
  }

  /**
   * \brief Creates a descriptor for the given asset
   *
   * \param [in] type Asset type
   * \param [in] descriptor Descriptor data
   * \returns Descriptor index, or \c 0 on error
   */
  uint32_t createDescriptor(
          GfxAssetType                  type,
    const GfxDescriptor&                descriptor) {
    return m_assetManager->createDescriptor(type, descriptor);
  }

  /**
   * \brief Frees a descriptor
   *
   * \param [in] type Asset type
   * \param [in] index Descriptor index
   */
  void freeDescriptor(
          GfxAssetType                  type,
          uint32_t                      index) {
    return m_assetManager->freeDescriptor(type, index);
  }

private:

  explicit GfxAssetManagerIface(GfxAssetManager* manager)
  : m_assetManager(manager) { }

  GfxAssetManager* m_assetManager = nullptr;

};


}
