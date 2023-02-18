#include "../util/util_assert.h"
#include "../util/util_math.h"

#include "gfx_device.h"
#include "gfx_scratch.h"

namespace as {

GfxScratchBufferPage::GfxScratchBufferPage(
        std::shared_ptr<GfxScratchAllocator> buffer,
        uint32_t                      pageIndex,
        uint32_t                      pageCount,
        GfxMemoryType                 memoryType)
: m_parent    (std::move(buffer))
, m_pageIndex (pageIndex)
, m_pageCount (pageCount)
, m_memoryType(memoryType)
, m_allocator (pageCount * GfxScratchPageSize) {

}


GfxScratchBufferPage::GfxScratchBufferPage(GfxScratchBufferPage&& other)
: m_parent    (std::move(other.m_parent))
, m_pageIndex (std::exchange(other.m_pageIndex, 0u))
, m_pageCount (std::exchange(other.m_pageCount, 0u))
, m_memoryType(std::exchange(other.m_memoryType, GfxMemoryType::eFlagEnum))
, m_allocator (std::exchange(other.m_allocator, LinearAllocator<uint64_t>())) {

}


GfxScratchBufferPage& GfxScratchBufferPage::operator = (GfxScratchBufferPage&& other) {
  m_parent = std::move(other.m_parent);
  m_pageIndex = std::exchange(other.m_pageIndex, 0u);
  m_pageCount = std::exchange(other.m_pageCount, 0u);
  m_memoryType = std::exchange(other.m_memoryType, GfxMemoryType::eFlagEnum);
  m_allocator = std::exchange(other.m_allocator, LinearAllocator<uint64_t>());
  return *this;
}


GfxScratchBufferPage::~GfxScratchBufferPage() {
  if (m_parent)
    m_parent->freePages(m_pageIndex, m_pageCount);
}


std::optional<GfxScratchBuffer> GfxScratchBufferPage::alloc(
        uint64_t                      size,
        uint64_t                      alignment) {
  auto offset = m_allocator.alloc(size, alignment);

  if (!offset)
    return std::nullopt;

  GfxScratchBuffer result;
  result.buffer = m_parent->getBuffer();
  result.offset = GfxScratchPageSize * m_pageIndex + *offset;
  result.size = align(size, alignment);
  return std::make_optional(std::move(result));
}




GfxScratchAllocator::GfxScratchAllocator(
        GfxDeviceIface&               device,
        GfxMemoryType                 memoryType)
: m_memoryType  (memoryType)
, m_allocator   (GfxScratchPageCount) {
  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = "Scratch buffer";
  bufferDesc.usage = GfxUsage::eTransferSrc
                   | GfxUsage::eParameterBuffer
                   | GfxUsage::eIndexBuffer
                   | GfxUsage::eVertexBuffer
                   | GfxUsage::eConstantBuffer
                   | GfxUsage::eShaderResource;
  bufferDesc.size = GfxScratchBufferSize;

  if (memoryType != GfxMemoryType::eVideoMemory)
    bufferDesc.usage |= GfxUsage::eCpuWrite;

  if (memoryType != GfxMemoryType::eBarMemory) {
    bufferDesc.usage |= GfxUsage::eTransferDst
                     |  GfxUsage::eShaderStorage;
  }

  if (memoryType == GfxMemoryType::eSystemMemory)
    bufferDesc.usage |= GfxUsage::eCpuRead;

  m_buffer = device.createBuffer(bufferDesc, memoryType | GfxMemoryType::eSystemMemory);
}


GfxScratchAllocator::~GfxScratchAllocator() {

}


std::optional<GfxScratchBufferPage> GfxScratchAllocator::allocPages(
        uint32_t                      pageCount) {
  auto pageIndex = m_allocator.alloc(pageCount);

  if (!pageIndex)
    return std::nullopt;

  return std::make_optional<GfxScratchBufferPage>(
    shared_from_this(), *pageIndex, pageCount, m_memoryType);
}


void GfxScratchAllocator::freePages(
        uint32_t                      pageIndex,
        uint32_t                      pageCount) {
  m_allocator.free(pageIndex, pageCount);
}




GfxScratchBufferPool::GfxScratchBufferPool(
        GfxDeviceIface&               device)
: m_device(device) {

}


GfxScratchBufferPool::~GfxScratchBufferPool() {

}


GfxScratchBufferPage GfxScratchBufferPool::allocPages(
        GfxMemoryType                 memoryType,
        uint32_t                      pageCount) {
  // Allocating pages from a buffer is thread-safe, no need to lock
  auto page = tryAllocPages(memoryType, pageCount);

  if (likely(page))
    return std::move(*page);

  // Try again with a locked context. We lock here since we don't
  // want multiple threads to concurrently create new buffers if
  // only a small number of pages is needed.
  std::lock_guard lock(m_mutex);
  page = tryAllocPages(memoryType, pageCount);

  if (page)
    return std::move(*page);

  // If we still could not find a page, create and append a buffer.
  auto& buffer = *m_buffers.insert(std::make_shared<GfxScratchAllocator>(m_device, memoryType));
  return std::move(*buffer->allocPages(pageCount));
}


std::optional<GfxScratchBufferPage> GfxScratchBufferPool::tryAllocPages(
        GfxMemoryType                 memoryType,
        uint32_t                      pageCount) {
  for (const auto& buffer : m_buffers) {
    if (buffer->getMemoryType() != memoryType)
      continue;

    auto page = buffer->allocPages(pageCount);

    if (page)
      return page;
  }

  return std::nullopt;
}

}
