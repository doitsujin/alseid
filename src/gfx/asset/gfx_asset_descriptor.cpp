#include "gfx_asset_descriptor.h"

namespace as {

GfxAssetDescriptorAllocator::GfxAssetDescriptorAllocator(
        uint32_t                      capacity)
: m_capacity(capacity) {

}


GfxAssetDescriptorAllocator::~GfxAssetDescriptorAllocator() {

}


std::optional<uint32_t> GfxAssetDescriptorAllocator::alloc(
        uint32_t                      lastFrameId) {
  std::lock_guard lock(m_mutex);

  // The free list is implicitly ordered by frame ID,
  // so we only need to check the first entry.
  if (!m_freeList.empty()) {
    FreeEntry e = m_freeList.front();

    if (e.frameId <= lastFrameId) {
      m_freeList.pop();
      return e.index;
    }
  }

  // If we can't recycle, allocate a new descriptor
  if (m_next < m_capacity)
    return m_next++;

  // No space left
  return std::nullopt;
}


void GfxAssetDescriptorAllocator::free(
        uint32_t                      index,
        uint32_t                      currFrameId) {
  std::lock_guard lock(m_mutex);

  FreeEntry e = { };
  e.index = index;
  e.frameId = currFrameId;

  m_freeList.push(e);
}




GfxAssetDescriptorPool::GfxAssetDescriptorPool(
  const GfxDevice&                    device,
  const char*                         name,
        GfxShaderBindingType          type,
        uint32_t                      count)
: allocator(count) {
  GfxDescriptorArrayDesc desc = { };
  desc.debugName = name;
  desc.bindingType = type;
  desc.descriptorCount = count;

  descriptorArray = device->createDescriptorArray(desc);
}

}
