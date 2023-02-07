#pragma once

#include "../util/util_iface.h"

namespace as {

/**
 * \brief Command list interface
 *
 * Command lists are opaque objects retrieved from a
 * context, and their only purpose is to be submitted
 * to a device queue.
 */
class GfxCommandListIface {

public:

  virtual ~GfxCommandListIface() { }

};

using GfxCommandList = IfaceRef<GfxCommandListIface>;

}
