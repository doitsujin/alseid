#include "../../util/util_assert.h"

#include "../gfx_spirv.h"

#include "gfx_vulkan_utils.h"

namespace as {

GfxShader createVkBuiltInShader(size_t size, const void* code) {
  GfxShaderBinaryDesc binary;
  binary.format = GfxShaderFormat::eVulkanSpirv;
  binary.data.resize(size);
  std::memcpy(binary.data.data(), code, size);

  auto desc = reflectSpirvBinary(size, code);
  dbg_assert(desc.has_value());
  return GfxShader(std::move(*desc), std::move(binary));
}

}
