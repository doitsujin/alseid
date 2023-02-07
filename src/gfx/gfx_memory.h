#pragma once

#include "../util/util_flags.h"

#include "gfx_types.h"
#include "gfx_utils.h"

namespace as {

/**
 * \brief Memory type
 */
enum class GfxMemoryType : uint32_t {
  /** Video memory. Preferred for all resources that are not mapped
   *  into CPU address space and are frequently used by the GPU. */
  eVideoMemory  = (1u << 0),
  /** System memory. Preferred for resources that are mapped into CPU
   *  address space and are only used for copies, but also used as a
   *  fallback when exceeding the allocator's video memory budget. */
  eSystemMemory = (1u << 1),
  /** BAR memory. Preferred for resources that are mapped into the
   *  CPU address space and are used as shader resources. Resources
   *  that are not CPU-mapped cannot be allocated on this type. */
  eBarMemory    = (1u << 2),
  /** Convenience value to allow all memory types compatible with
   *  the resource's CPU usage flags. */
  eAny          = eVideoMemory | eSystemMemory | eBarMemory,

  eFlagEnum     = 0
};

using GfxMemoryTypes = Flags<GfxMemoryType>;


/**
 * \brief Memory information for a given resource
 */
struct GfxMemoryInfo {
  /** Memory type that the resource is allocated on. Can be used to
   *  determine whether to relocate the resource based on its usage,
   *  e.g. if a frequently used image was allocated in system memory. */
  GfxMemoryType type = GfxMemoryType(0);
  /** Allocation size of the resource, in bytes. This includes padding
   *  for alignment purposes, so even for buffers this may be larger
   *  than the specified buffer size. */
  uint64_t size = 0;
};

}
