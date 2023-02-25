#pragma once

#include <array>
#include <string>

#include "../util/util_flags.h"
#include "../util/util_hash.h"
#include "../util/util_iface.h"
#include "../util/util_likely.h"
#include "../util/util_math.h"
#include "../util/util_types.h"

#include "gfx_descriptor_handle.h"
#include "gfx_format.h"
#include "gfx_memory.h"
#include "gfx_types.h"
#include "gfx_utils.h"

namespace as {

class GfxBufferIface;

/**
 * \brief Buffer view description
 *
 * The view description is also used to look up
 * views internally and therefore has comparison
 * and hash functions.
 */
struct GfxBufferViewDesc {
  /** View format. Must be a supported format for buffer views. */
  GfxFormat format = GfxFormat::eUnknown;
  /** View usage. Must be either \c GfxUsage::eShaderResource
   *  or \c GfxUsage::eShaderStorage, and the usage flag must
   *  also be included in the buffer's \c usage flag. */
  GfxUsage usage = GfxUsage::eFlagEnum;
  /** Offset of the view within the buffer, in bytes. */
  uint64_t offset = 0;
  /** Size of the buffer view, in bytes. */
  uint64_t size = 0;

  /**
   * \brief Computes hash
   * \returns Hash
   */
  size_t hash() const {
    HashState hash;
    hash.add(uint32_t(format));
    hash.add(uint32_t(usage));
    hash.add(offset);
    hash.add(size);
    return hash;
  }

  bool operator == (const GfxBufferViewDesc&) const = default;
  bool operator != (const GfxBufferViewDesc&) const = default;
};


/**
 * \brief Buffer flags
 */
enum class GfxBufferFlag : uint32_t {
  /** Forces a dedicated allocation. This should be used sparingly. */
  eDedicatedAllocation  = (1u << 0),
  /** Enables sparse residency for this resource. If specified, no
   *  memory will be allocated at buffer creation time, instead, the
   *  app can dynamically bind memory at runtime. */
  eSparseResidency      = (1u << 1),
  eFlagEnum             = 0
};

using GfxBufferFlags = Flags<GfxBufferFlag>;


/**
 * \brief Buffer description
 */
struct GfxBufferDesc {
  /** Buffer debug name */
  const char* debugName;
  /** Buffer usage. Specifies which kind of operations
   *  the buffer can be used with.*/
  GfxUsageFlags usage = 0u;
  /** Buffer size, in bytes */
  uint64_t size = 0;
  /** Buffer flags */
  GfxBufferFlags flags = 0;
};


/**
 * \brief Buffer view interface
 */
class GfxBufferViewIface {

public:

  GfxBufferViewIface(
    const GfxBufferViewDesc&            desc)
  : m_desc(desc) { }

  virtual ~GfxBufferViewIface() { }

  /**
   * \brief Retrieves buffer view descriptor
   *
   * The resulting descriptor can be used to bind the view to a
   * shader pipeline. Descriptors may be cached as long as they
   * are not used after the view object gets destroyed.
   * \returns View descriptor
   */
  virtual GfxDescriptor getDescriptor() const = 0;

  /**
   * \brief Queries buffer view description
   * \returns Buffer view description
   */
  GfxBufferViewDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxBufferViewDesc m_desc;

};

using GfxBufferView = PtrRef<GfxBufferViewIface>;


/**
 * \brief Buffer resource interface
 */
class GfxBufferIface {

public:

  GfxBufferIface(
    const GfxBufferDesc&                desc,
          uint64_t                      va,
          void*                         mapPtr)
  : m_desc(desc), m_va(va), m_mapPtr(mapPtr) {
    if (m_desc.debugName) {
      m_debugName = m_desc.debugName;
      m_desc.debugName = m_debugName.c_str();
    }
  }

  virtual ~GfxBufferIface() { }

  /**
   * \brief Retrieves view with the given properties
   *
   * If a view with the given properties has already been created,
   * this will return the existing view object, so calls to this
   * function are expected to be relatively fast. View objects have
   * the same lifetime as the buffer, so they should not be cached
   * if doing so risks accessing stale views.
   * \param [in] desc View description
   * \returns View object
   */
  virtual GfxBufferView createView(
    const GfxBufferViewDesc&            desc) = 0;

  /**
   * \brief Retrieves buffer descriptor
   *
   * Retrieves an unformatted buffer descriptor that can be used
   * as a constant buffer, a shader resource or storage buffer
   * in a shader, depending on the specified \c usage.
   * \param [in] usage Buffer descriptor usage
   * \param [in] offset Buffer usage
   * \param [in] size Buffer size
   * \returns Buffer descriptor
   */
  virtual GfxDescriptor getDescriptor(
          GfxUsage                      usage,
          uint64_t                      offset,
          uint64_t                      size) const = 0;

  /**
   * \brief Queries memory info for the resource
   * \returns Buffer memory allocation info
   */
  virtual GfxMemoryInfo getMemoryInfo() const = 0;

  /**
   * \brief Returns GPU address
   *
   * May be 0 if the graphics backend does not support returning
   * native GPU addresses, or if the buffer cannot be used inside
   * shaders.
   * \returns GPU address of the buffer
   */
  uint64_t getGpuAddress() const {
    return m_va;
  }

  /**
   * \brief Returns pointer to mapped memory region
   *
   * If the buffer has been created with \c GfxUsage::eCpuRead,
   * the returned pointer should \e not be cached, and \c map
   * \e must be called every time the data is accessed. The
   * reason is that not all devices support coherent readbacks,
   * and calling \c map invalidates CPU caches as necessary.
   *
   * Otherwise, if only \c GfxUsage::eCpuWrite was set, the buffer
   * is guaranteed to be allocated on a coherent memory type, and
   * this method will immediately return the mapped pointer without
   * any further action.
   * \param [in] access CPU access flags. \e Must contain at least
   *    one of \c GfxUsage::eCpuRead or \c GfxUsage::eCpuWrite.
   * \param [in] offset Offset into the mapped resource
   * \returns Pointer to mapped buffer, or \c nullptr if
   *    the buffer is not mapped into CPU address space.
   */
  void* map(GfxUsageFlags access, size_t offset);

  /**
   * \brief Flushes mapped memory region
   *
   * If the buffer has been created with \c GfxUsage::eCpuRead,
   * this \e must be called after the last access to the mapped
   * buffer region in order to ensure that writes are visible to
   * the GPU.
   *
   * Otherwise, if only \c GfxUsage::eCpuWrite was set, calling
   * this method has no effect and doing so is entirely optional.
   * \param [in] access CPU access flags. \e Must be identical to
   *    the access flags used in a prior call to \c map.
   */
  void unmap(GfxUsageFlags access);

  /**
   * \brief Queries buffer description
   * \returns Buffer description
   */
  GfxBufferDesc getDesc() const {
    return m_desc;
  }

protected:

  GfxBufferDesc m_desc;
  std::string   m_debugName;

  uint64_t      m_va = 0;
  void*         m_mapPtr = nullptr;
  GfxUsageFlags m_incoherentUsage = 0;

  virtual void invalidateMappedRegion() = 0;

  virtual void flushMappedRegion() = 0;

};

/** See GfxBufferIface. */
using GfxBuffer = IfaceRef<GfxBufferIface>;

}
