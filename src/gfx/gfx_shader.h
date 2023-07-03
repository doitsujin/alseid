#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "../util/util_flags.h"
#include "../util/util_hash.h"
#include "../util/util_iface.h"
#include "../util/util_likely.h"
#include "../util/util_stream.h"
#include "../util/util_types.h"

#include "gfx_types.h"

namespace as {

/**
 * \brief Shader binary format
 */
enum class GfxShaderFormat : uint16_t {
  eUnknown                = 0,
  eVulkanSpirv            = 1,
  eVulkanSpirvCompressed  = 2,
  eCount
};


/**
 * \brief Shader format info
 *
 * Specifies the shader format to
 * use for a given graphics device.
 */
struct GfxShaderFormatInfo {
  /** Format of the shader binary */
  GfxShaderFormat format = GfxShaderFormat::eUnknown;
  /** FourCC of this format in archive files */
  FourCC identifier = FourCC();
};


/**
 * \brief Shader binding type
 */
enum class GfxShaderBindingType : uint32_t {
  eUnknown                = 0,
  eSampler                = 1,
  eConstantBuffer         = 2,
  eResourceBuffer         = 3,
  eResourceBufferView     = 4,
  eResourceImageView      = 5,
  eStorageBuffer          = 6,
  eStorageBufferView      = 7,
  eStorageImageView       = 8,
  eBvh                    = 9,
};


/**
 * \brief Shader binding structure
 */
struct GfxShaderBinding {
  /** Descriptor type. This determines what kind
   *  of resource can be bound to this binding. */
  GfxShaderBindingType type = GfxShaderBindingType::eUnknown;
  /** Descriptor set index. */
  uint32_t descriptorSet = 0;
  /** Descriptor index within the set. */
  uint32_t descriptorIndex = 0;
  /** Number of descriptors within the binding. If this is
   *  \c 0, this binding references a global descriptor array,
   *  and \e must be the only binding within the set. */
  uint32_t descriptorCount = 0;
  /** Binding name for local bindings. Global descriptor
   *  array bindings may have this set to \c nullptr. */
  GfxSemanticName name = "";
};


/**
 * \brief Shader flags
 */
enum class GfxShaderFlag : uint32_t {
  /** Fragment shader runs at sample rate */
  eSampleRate             = (1u << 0),

  eFlagEnum               = 0
};

using GfxShaderFlags = Flags<GfxShaderFlag>;


/**
 * \brief Shader description
 *
 * Stores metadata about the shader, including
 * info about all resource bindings.
 */
struct GfxShaderDesc {
  /** Shader debug name. */
  const char* debugName = nullptr;
  /** Shader stage. This must be a stage
   *  flag consisting of exactly one bit. */
  GfxShaderStage stage = GfxShaderStage::eFlagEnum;
  /** Shader property flags */
  GfxShaderFlags flags = 0;
  /** Workgroup size. Only defined for compute, mesh
   *  and task shaders. If any component is 0, a
   *  specialization constant is used instead. */
  Extent3D workgroupSize = Extent3D(0, 0, 0);
  /** Workgroup size specialization constant IDs. */
  Extent3D workgroupSpecIds = Extent3D(0, 0, 0);
  /** Maximum number of vertices written by mesh shader */
  uint32_t maxOutputVertices = 0;
  /** Maximum number of primitives written by mesh shader */
  uint32_t maxOutputPrimitives = 0;
  /** Number of bytes for shader constants. */
  uint32_t constantSize = 0;
  /** Binding descriptions. Bindings \e must be ordered
   *  by set index, and then by descriptor index, in
   *  ascending order. */
  std::vector<GfxShaderBinding> bindings;

  /**
   * \brief Writes shader description to stream
   *
   * Note that the debug name will \e not be included,
   * since this can be set to the file name itself.
   * \param [in] output Stream to write to
   * \returns \c true on success
   */
  bool serialize(
          WrBufferedStream&             output) const;

  /**
   * \brief Reads shader description from stream
   *
   * \param [in] input Stream to read from
   * \returns \c true on success
   */
  bool deserialize(
          RdMemoryView                  input);

};


/**
 * \brief Mesh shader output info
 */
struct GfxShaderMeshOutputInfo {
  /** Maximum number of vertices that the shader can emit */
  uint32_t maxVertexCount = 0;
  /** Maximum number of primitives that the shader can emit */
  uint32_t maxPrimitiveCount = 0;
};


/**
 * \brief Shader binary description
 *
 * Stores metadata about a shader binary,
 * as well as the binary itself.
 */
struct GfxShaderBinaryDesc {
  /** Format of the shader binary. When compiling pipelines
   *  with this shader, the format \e must be compatible
   *  with the selected graphics backend. */
  GfxShaderFormat format = GfxShaderFormat::eUnknown;
  /** Raw shader binary in the given format. */
  std::vector<char> data;
};


/**
 * \brief Shader binary info
 */
struct GfxShaderBinary {
  /** Format of the shader binary stored in the shader object */
  GfxShaderFormat format = GfxShaderFormat::eUnknown;
  /** Size of the binary in bytes */
  uint32_t size = 0;
  /** Pointer to the shader binary */
  const void* data = nullptr;
  /** Unique shader binary hash */
  UniqueHash hash;
};


/**
 * \brief Shader interface
 *
 * Stores a shader binary as well as metadata that can be
 * used by both the respective graphics backend as well as
 * the application for reflection purposes.
 */
class GfxShaderIface {

public:

  GfxShaderIface(
          GfxShaderDesc&&               desc,
          GfxShaderBinaryDesc&&         binary);

  ~GfxShaderIface();

  /**
   * \brief Queries shader stage
   * \returns Shader stage
   */
  GfxShaderStage getShaderStage() const {
    return m_desc.stage;
  }

  /**
   * \brief Queries debug name
   *
   * Always returns a valid name even if none
   * was specified during object creation.
   * \returns Debug name
   */
  const char* getDebugName() const {
    return m_debugName.c_str();
  } 

  /**
   * \brief Queries size of shader constant block
   * \returns Shader constant size, in bytes
   */
  uint32_t getConstantSize() const {
    return m_desc.constantSize;
  }

  /**
   * \brief Queries number of resource bindings
   * \returns Total number of bindings
   */
  uint32_t getBindingCount() const {
    return uint32_t(m_desc.bindings.size());
  }

  /**
   * \brief Retrieves info about a given binding
   *
   * Bindings are ordered by set number, then
   * by descriptor index, in ascending order.
   * \param [in] index Binding index
   */
  GfxShaderBinding getBinding(uint32_t index) const {
    if (likely(index < getBindingCount()))
      return m_desc.bindings[index];

    return GfxShaderBinding();
  }

  /**
   * \brief Queries shader binary info
   *
   * Used to check whether the binary is at all
   * compatible with the given graphics backend.
   * \returns Shader binary format
   */
  GfxShaderBinary getShaderBinary() const {
    GfxShaderBinary result;
    result.format = m_binary.format;
    result.size = m_binary.data.size();
    result.data = m_binary.data.data();
    result.hash = m_hash;
    return result;
  }

  /**
   * \brief Queries shader property flags
   * \returns Shader flags
   */
  GfxShaderFlags getFlags() const {
    return m_desc.flags;
  }

  /**
   * \brief Queries workgroup size
   * \returns Workgroup size
   */
  Extent3D getWorkgroupSize() const {
    return m_desc.workgroupSize;
  }

  /**
   * \brief Queries workgroup size spec IDs
   *
   * For any component where the reported workgroup
   * size is 0, this will return the specialization
   * constant ID that determines the workgroup size.
   * \returns Workgroup size spec constant IDs
   */
  Extent3D getWorkgroupSizeSpecIds() const {
    return m_desc.workgroupSpecIds;
  }

  /**
   * \brief Queries maximum vertex and primitive count
   *
   * Only relevant for mesh shaders, and used internally
   * to compute the workgroup size for mesh shaders.
   * \returns Maximum vertex and primitive count
   */
  GfxShaderMeshOutputInfo getMeshOutputInfo() const {
    GfxShaderMeshOutputInfo result;
    result.maxVertexCount = m_desc.maxOutputVertices;
    result.maxPrimitiveCount = m_desc.maxOutputPrimitives;
    return result;
  }

  /**
   * \brief Finds binding by name
   *
   * This can be used to find the binding and set number
   * for a given binding based on its variable name. Note
   * that global descriptor array bindings have no name.
   * \note This function is \e not expected to be fast.
   *    Cache the returned binding info, and prefer using
   *    fixed binding numbers for resources where possible.
   * \param [in] name Binding name
   * \returns The corresponding binding, or \c nullopt if
   *    no binding with the given name exists in the set.
   */
  std::optional<GfxShaderBinding> findBinding(
    const char*                         name) const;

private:

  GfxShaderDesc       m_desc;
  GfxShaderBinaryDesc m_binary;

  UniqueHash          m_hash;
  std::string         m_debugName;

};


/**
 * \brief Shader object
 *
 * See GfxShaderIface.
 */
class GfxShader : public IfaceRef<GfxShaderIface> {

public:

  GfxShader() { }
  GfxShader(std::nullptr_t) { }

  /**
   * \brief Initializes shader object
   *
   * \param [in] desc Shader description
   * \param [in] binary Shader binary description
   */
  explicit GfxShader(
          GfxShaderDesc&&               desc,
          GfxShaderBinaryDesc&&         binary);

};


}
