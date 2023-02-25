#pragma once

#include <string>

#include "../util/util_iface.h"

#include "gfx_descriptor_handle.h"
#include "gfx_shader.h"

namespace as {

/**
 * \brief Descriptor array properties
 */
struct GfxDescriptorArrayDesc {
  /** Debug name for the descriptor array */
  const char* debugName = nullptr;
  /** Binding type to create the descriptor array for.
   *  \e Must be one of the following:
   *  - \c GfxShaderBindingType::eSampler
   *  - \c GfxShaderBindingType::eResourceBuffer
   *  - \c GfxShaderBindingType::eResourceImageView
   *  - \c GfxShaderBindingType::eStorageBuffer
   *  - \c GfxShaderBindingType::eStorageImageView
   */
  GfxShaderBindingType bindingType = GfxShaderBindingType(0);
  /** Number of descriptors in the descriptor array */
  uint32_t descriptorCount = 0;
};


/**
 * \brief Descriptor array
 *
 * A descriptor array is essentially a block of descriptors
 * that shaders can access dynamically.
 */
class GfxDescriptorArrayIface {

public:

  GfxDescriptorArrayIface(
    const GfxDescriptorArrayDesc&       desc)
  : m_desc(desc) {
    if (desc.debugName) {
      m_debugName = desc.debugName;
      m_desc.debugName = m_debugName.c_str();
    }
  }

  virtual ~GfxDescriptorArrayIface() { }

  /**
   * \brief Writes descriptors
   *
   * \note Writing the same descriptors from different
   *    threads results in undefined behaviour.
   * \param [in] index First descriptor to write 
   * \param [in] count Number of descriptors to write 
   * \param [in] descriptors Descriptor array 
   */
  virtual void setDescriptors(
          uint32_t                      index,
          uint32_t                      count,
    const GfxDescriptor*                descriptors) = 0;

  /**
   * \brief Writes a single descriptor
   *
   * Convenience method that only updates one descriptor.
   * Prefer batched updates whenever possible.
   * \param [in] index Index of the descriptor to write
   * \param [in] descriptor Descriptor data
   */
  void setDescriptor(
          uint32_t                      index,
    const GfxDescriptor&                descriptor) {
    setDescriptors(index, 1, &descriptor);
  }

  /**
   * \brief Queries descriptor array properties
   * \returns Descriptor array properties
   */
  GfxDescriptorArrayDesc getDesc() const {
    return m_desc;
  }

private:

  GfxDescriptorArrayDesc  m_desc;
  std::string             m_debugName;

};

/** See GfxDescriptorArrayIface. */
using GfxDescriptorArray = IfaceRef<GfxDescriptorArrayIface>;

}
