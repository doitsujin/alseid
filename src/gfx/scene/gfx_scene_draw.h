#pragma once

#include <vector>

#include "../../util/util_types.h"

#include "../gfx_device.h"

#include "gfx_scene_instance.h"
#include "gfx_scene_node.h"
#include "gfx_scene_pass.h"
#include "gfx_scene_pipelines.h"

namespace as {

/**
 * \brief Draw list header
 *
 * Stores information about a draw list buffer. In the GPU buffer, this is
 * immediately followed by an array of \c GfxSceneDrawListEntry structures.
 */
struct GfxSceneDrawListHeader {
  /** Number of draw groups in the draw group buffer. */
  uint32_t drawGroupCount;
  /** Offset of indirect draw parameters, in bytes, relative to the
   *  start of the buffer. This stores a packed array of task shader
   *  workgroup counts for each possible draw. */
  uint32_t drawParameterOffset;
  /** Offset of draw infos, in bytes, relative to the start of the
   *  buffer. This stores a  */
  uint32_t drawInfoOffset;
  /** Reserved for future use. */
  uint32_t reserved;
};

static_assert(sizeof(GfxSceneDrawListHeader) == 16);


/**
 * \brief Draw list entry
 *
 * The draw list provides one of these structures for each material,
 * which enables compute shaders that emit draw parameters to index
 * the draw list using the real material index.
 */
struct GfxSceneDrawListEntry {
  /** Index into the draw parameter for the first draw within this
   *  draw group. Must be pre-computed in such a way that each draw
   *  group can accomodate the maximum possible draw count. */
  uint32_t drawIndex;
  /** Number of draws within the draw group. Must be initialized to
   *  zero so that the draw count can be used as a linear allocator. */
  uint32_t drawCount;
};

static_assert(sizeof(GfxSceneDrawListEntry) == 8);


/**
 * \brief Draw info
 *
 * Stores additional parameters for a single draw which
 * the task shader can then index via the draw ID.
 */
struct GfxSceneDrawInstanceInfo {
  /** Instance node index. Can be used to obtain geometry information
   *  and the final transform, as well as visibility information. */
  uint24_t instanceIndex;
  /** Mesh LOD to use for rendering. */
  uint8_t lodIndex;
  /** Mesh index within the geometry asset. */
  uint16_t meshIndex;
  /** First mesh instance index for this draw. */
  uint16_t meshInstance;
  /** Offet to shading parameters within the instance data buffer, in bytes.
   *  Taken directly from the draw parameters, since this cannot be otherwise
   *  inferred at draw time. */
  uint32_t shadingDataOffset;
  /** Mask of passes where this instance is visible. This is useful when
   *  rendering multiple passes at once, e.g. for shadow maps. Task shaders
   *  will have to work out the pass index based on the workgroup ID. */
  uint32_t passMask;
};

static_assert(sizeof(GfxSceneDrawInstanceInfo) == 16);


/**
 * \brief Draw buffer description
 */
struct GfxSceneDrawBufferDesc {
  /** Number of draw groups. */
  uint32_t drawGroupCount;
  /** Maximum number of draws in each draw group. */
  const uint32_t* drawCounts;
};


/**
 * \brief Draw buffer
 *
 * Allocates a dynamically laid out GPU buffer with the purpose for storing a
 * single draw list. Draw lists should be generated on demand in order to reuse
 * this memory as much as possible, multiple times per frame.
 */
class GfxSceneDrawBuffer {

public:

  /**
   * \brief Creates draw buffer
   */
  explicit GfxSceneDrawBuffer(
          GfxDevice                     device);

  ~GfxSceneDrawBuffer();

  /**
   * \brief Queries GPU address of the buffer
   * \returns GPU address of the buffer
   */
  uint64_t getGpuAddress() const {
    return m_buffer ? m_buffer->getGpuAddress() : 0ull;
  }

  /**
   * \brief Queries descriptor for the indirect draw count
   *
   * \param [in] drawGroup Draw group index
   * \returns Descriptor for indirect draw count
   */
  GfxDescriptor getDrawCountDescriptor(
          uint32_t                      drawGroup) const;

  /**
   * \brief Queries descriptor for indirect draw parameters
   *
   * \param [in] drawGroup Draw group index
   * \returns Descriptor for indirect draw parameters
   */
  GfxDescriptor getDrawParameterDescriptor(
          uint32_t                      drawGroup) const;

  /**
   * \brief Updates buffer layout
   *
   * Allocates storage for the draw buffer as necessary and unconditionally
   * initializes it with the new buffer layout. This must be called any
   * time the draw group layouts change.
   * \param [in] context Context object
   * \param [in] desc Draw buffer description
   * \returns Old buffer, or \c nullptr if no the buffer was not actually
   *    replaced. This must be kept alive until the current frame finishes
   *    processing on the GPU.
   */
  GfxBuffer updateLayout(
    const GfxContext&                   context,
    const GfxSceneDrawBufferDesc&       desc);

  /**
   * \brief Generates draw lists
   */
  void generateDraws(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxDescriptor&                passInfos,
    const GfxSceneNodeManager&          nodeManager,
    const GfxSceneInstanceManager&      instanceManager,
    const GfxScenePassGroupBuffer&      groupBuffer,
          uint32_t                      frameId,
          uint32_t                      passMask,
          uint32_t                      lodSelectionPass);

private:

  GfxDevice                           m_device;
  GfxBuffer                           m_buffer;

  GfxSceneDrawListHeader              m_header = { };
  std::vector<GfxSceneDrawListEntry>  m_entries;

  uint32_t allocateStorage(
          uint64_t&                     allocator,
          size_t                        size);

};

}
