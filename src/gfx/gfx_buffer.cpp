#include "gfx_buffer.h"

namespace as {

void* GfxBufferIface::map(GfxUsageFlags access, size_t offset) {
  if (unlikely(!m_mapPtr))
    return nullptr;

  if (unlikely(m_incoherentUsage & access))
    invalidateMappedRegion();

  return reinterpret_cast<char*>(m_mapPtr) + offset;
}


void GfxBufferIface::unmap(GfxUsageFlags access) {
  if (unlikely(m_incoherentUsage & access))
    flushMappedRegion();
}

}
