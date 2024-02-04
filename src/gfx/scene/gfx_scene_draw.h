#pragma once

#include <cstdint>
#include <vector>

#include "../../util/util_types.h"

#include "../gfx_device.h"

#include "gfx_scene_instance.h"
#include "gfx_scene_node.h"
#include "gfx_scene_pass.h"
#include "gfx_scene_pipelines.h"

namespace as {

/** Maximum depth of the search tree. */
constexpr uint32_t GfxSceneDrawSearchTreeDepth = 6u;
/** Maximum number of task shader workgroups per indirect dispatch.
 *  Used to split extremely large draws that would otherwise exceed
 *  device limits. */
constexpr uint32_t GfxSceneDrawMaxTsWorkgroupsPerDispatch = 32768u;

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
  /** Index of the first draw info for this draw group within the
   *  draw info array. Note that this does not directly correspond
   *  to a task shader dispatch. */
  uint32_t drawIndex;
  /** Number of draws within the draw group. When generating draw
   *  lists, this must be initialized to zero so that the draw count
   *  can be used as a linear allocator. */
  uint32_t drawCount;
  /** Index of the first task shader dispatch argument. */
  uint32_t dispatchIndex;
  /** Maximum task shader dispatch count. */
  uint32_t dispatchCount;
  /** Number of valid layers in the search tree. Higher layers must not
   *  be accessed. This is static, based on the maximum draw count. */
  uint32_t searchTreeDepth;
  /** First workgroup counter in the counter buffer to use when computing the
   *  search tree for this draw group. Counters must be zero-initialized. */
  uint32_t searchTreeCounterIndex;
  /** Dispatch parameters for generating the search tree */
  GfxDispatchArgs searchTreeDispatch;
  /** Offsets of the individual search tree layers within the buffer,
   *  starting with the lowest layer that stores per-draw counts. */
  std::array<uint32_t, GfxSceneDrawSearchTreeDepth> searchTreeLayerOffsets;
  /** Total number of task shader threads for the draw group. */
  uint32_t taskShaderThreadCount;
};

static_assert(sizeof(GfxSceneDrawListEntry) == 64);


/**
 * \brief Draw info
 *
 * Stores additional parameters for a single draw which
 * the task shader can then index via the draw ID.
 */
struct GfxSceneDrawInstanceInfo {
  /** GPU address of the meshlet buffer for this draw. Taken from the mesh
   *  metadata structure to reduce the number of dependent memory loads in
   *  the mesh shader. */
  uint64_t meshletBufferVa;
  /** Instance node index. Can be used to obtain geometry information
   *  and the final transform, as well as visibility information. */
  uint24_t instanceIndex;
  /** Mesh LOD to use for rendering. */
  uint8_t lodIndex;
  /** Local draw index of the instance. Used to pull in data such
   *  as material parameters and resources for shading. */
  uint16_t instanceDrawIndex;
  /** Number of mesh instances to draw. Can be derived from the draw as
   *  well, but this is needed to compute the task shader thread count. */
  uint16_t meshInstanceCount;
  /** Mesh index to draw. Used to reduce the number of indirections in
   *  the mesh shader. */
  uint32_t meshIndex;
  /** Index of the first meshlet of the selected LOD. Used to reduce
   *  the number of indirections in the task shader. */
  uint32_t meshletIndex;
  /** Total number of meshlets in the selected LOD. Contributes to
   *  the task shader thread count as well. */
  uint32_t meshletCount;
  /** Mask of passes where this instance is visible. This is useful when
   *  rendering multiple passes at once, e.g. for shadow maps. Task shaders
   *  will have to work out the pass index based on the workgroup ID. */
  uint32_t passMask;
};

static_assert(sizeof(GfxSceneDrawInstanceInfo) == 32);


/**
 * \brief Draw group info for the draw buffer
 */
struct GfxSceneDrawGroupDesc {
  /** Number of draws in the draw group */
  uint32_t drawCount = 0;
  /** Maximum number of meshlets in the draw group */
  uint32_t meshletCount = 0;
  /** Number of meshlets emitted per task shader workgroup */
  uint32_t meshletCountPerWorkgroup = 0;
};


/**
 * \brief Draw buffer description
 */
struct GfxSceneDrawBufferDesc {
  /** Number of draw groups. */
  uint32_t drawGroupCount = 0;
  /** Maximum number of draws in each draw group. */
  const GfxSceneDrawGroupDesc* drawGroups = nullptr;
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
  uint32_t getDrawCount(
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
   */
  void updateLayout(
    const GfxContext&                   context,
    const GfxSceneDrawBufferDesc&       desc);

  /**
   * \brief Generates draw lists
   *
   * \param [in] context Context object
   * \param [in] pipelines Scene pipeline object
   * \param [in] passInfoVa GPU address of pass info buffer
   * \param [in] nodeManager Node manager object
   * \param [in] instanceManager Instance manager object
   * \param [in] passGroupBuffer Pass group object to
   *    use for visibility tests
   * \param [in] frameId Current frame ID
   * \param [in] passMask Passes to include in the resulting
   *    draw list, relative to the pass group
   * \param [in] lodSelectionPass Absolute index of pass to
   *    use for LOD testing. Does not have to be part of the
   *    pass group.
   */
  void generateDraws(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint64_t                      passInfoVa,
    const GfxSceneNodeManager&          nodeManager,
    const GfxSceneInstanceManager&      instanceManager,
    const GfxScenePassGroupBuffer&      groupBuffer,
          uint32_t                      frameId,
          uint32_t                      passMask,
          uint32_t                      lodSelectionPass);

private:

  GfxDevice                           m_device;
  GfxBuffer                           m_buffer;
  GfxBuffer                           m_counters;

  GfxSceneDrawListHeader              m_header = { };
  std::vector<GfxSceneDrawListEntry>  m_entries;

  void recreateDrawBuffer(
    const GfxContext&                   context,
          uint64_t                      size);

  void recreateCounterBuffer(
    const GfxContext&                   context,
          uint32_t                      counters);

  uint32_t allocateStorage(
          uint64_t&                     allocator,
          size_t                        size);

};

}
