#pragma once

#include "../../util/util_types.h"

namespace as {

/**
 * \brief Node type
 */
enum class GfxSceneNodeType : uint8_t {
  /** Abstract node. The value of this must not change,
   *  since node references may be zero-initialized. */
  eNone             = 0,
  /** BVH node. The value of this must not be changed,
   *  since  */
  eBvh              = 1,

  /** Instance node. */
  eInstance         = 2,
  /** Light node. */
  eLight            = 3,
  /** Reflection probe. */
  eReflectionProbe  = 4,
  /** First custom node type. */
  eFirstCustom      = 16,

  /** Number of special node types. */
  eBuiltInCount     = 2,

  /** Maximum number of different node types. Used
   *  to determine the size of some lookup tables. */
  eCount            = 32
};


/**
 * \brief Node reference
 *
 * Defines the type of a node, as well as the type-specific index
 * of that node which defines where type-specific data for that
 * node is stored, including the original scene node index.
 */
struct GfxSceneNodeRef {
  GfxSceneNodeRef() = default;

  GfxSceneNodeRef(
          GfxSceneNodeType              type_,
          uint32_t                      index_)
  : type(type_), index(index_) { }

  GfxSceneNodeRef(
          GfxSceneNodeType              type_,
          uint24_t                      index_)
  : type(type_), index(index_) { }

  /** Node type. */
  GfxSceneNodeType type;
  /** Index into the typed node array. Not the scene node index. */
  uint24_t index;

  bool operator == (const GfxSceneNodeRef&) const = default;
  bool operator != (const GfxSceneNodeRef&) const = default;
};

static_assert(sizeof(GfxSceneNodeRef) == 4);

}
