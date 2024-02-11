#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <vector>

#include "../gfx_device.h"

namespace as {

/**
 * \brief Descriptor allocator
 *
 * Simple helper class to deal with descriptor arrays. Descriptors
 * are lifetime-managed in such a way that assets cannot override
 * descriptors that may still be in use by the GPU.
 *
 * This structure is not lock-free, and is not expected to have
 * very high traffic in the first place.
 */
class GfxAssetDescriptorAllocator {

public:

  GfxAssetDescriptorAllocator(
          uint32_t                      capacity);

  GfxAssetDescriptorAllocator();

  ~GfxAssetDescriptorAllocator();

  /**
   * \brief Allocates a descriptor
   *
   * \param [in] lastFrameId Last completed frame ID
   * \returns Descriptor index if the allocator has enough
   *    space left, or \c nullopt in case it is full.
   */
  std::optional<uint32_t> alloc(
          uint32_t                      lastFrameId);

  /**
   * \brief Frees a descriptor
   *
   * The frame ID ensures that the descriptor will not be
   * recycled until the given frame has completed on the GPU.
   * \param [in] index Descriptor index
   * \param [in] currFrameId Current frame ID
   */
  void free(
          uint32_t                      index,
          uint32_t                      currFrameId);

private:

  struct FreeEntry {
    uint32_t index;
    uint32_t frameId;
  };

  std::mutex            m_mutex;
  std::queue<FreeEntry> m_freeList;
  uint32_t              m_next = 1u;
  uint32_t              m_capacity = 0u;

};


/**
 * \brief Asset descriptor pool
 *
 * Pairs a descriptor array object with a
 * descriptor allocator.
 */
struct GfxAssetDescriptorPool {
  GfxAssetDescriptorPool(
    const GfxDevice&                    device,
    const char*                         name,
          GfxShaderBindingType          type,
          uint32_t                      count);

  /** Descriptor array object */
  GfxDescriptorArray descriptorArray;
  /** Descriptor allocator for the array */
  GfxAssetDescriptorAllocator allocator;
};


}
