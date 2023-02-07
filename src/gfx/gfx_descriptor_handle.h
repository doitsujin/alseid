#pragma once

#include <cstddef>
#include <cstdint>

namespace as {

constexpr size_t GfxDescriptorSize = 32;

/**
 * \brief Descriptor handle
 *
 * Contains information to populate a descriptor.
 * The data inside this struct should be treated
 * as opaque, and should not be accessed directly.
 *
 * A default-initialized descriptor is valid for
 * all binding types except samplers, and will be
 * treated as if no resource is bound to the given
 * binding.
 *
 * Sampler descriptors \e must be created from a
 * valid sampler object.
 */
struct alignas(16) GfxDescriptor {
  char data[GfxDescriptorSize];
};

}
