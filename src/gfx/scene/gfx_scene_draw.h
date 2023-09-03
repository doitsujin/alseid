#pragma once

#include "../../util/util_types.h"

namespace as {

/**
 * \brief Draw list header
 *
 * The draw list provides one of these structures for each material,
 * which enables compute shaders that emit draw parameters to index
 * the draw list using the real material index.
 */
struct GfxSceneDrawListHeader {
  /** Index into the draw parameter for the first draw within this
   *  draw group. Must be pre-computed in such a way that each draw
   *  group can accomodate the maximum possible draw count. */
  uint32_t drawIndex;
  /** Number of draws within the draw group. Must be initialized to
   *  zero so that the draw count can be used as a linear allocator. */
  uint32_t drawCount;
};

static_assert(sizeof(GfxSceneDrawListHeader) == 8);


/**
 * \brief Draw info
 *
 * Stores additional parameters for a single draw which
 * the task shader can then index via the draw ID.
 */
struct GfxSceneDrawInfo {
  /** Instance node index. Can be used to obtain geometry information
   *  and the final transform, as well as visibility information. */
  uint24_t instanceIndex;
  /** Mesh LOD to use for rendering. */
  uint8_t lodIndex;
  /** Mesh index within the geometry asset. */
  uint16_t meshIndex;
  /** First mesh instance index for this draw. */
  uint16_t meshInstance;
};

static_assert(sizeof(GfxSceneDrawInfo) == 8);

}
