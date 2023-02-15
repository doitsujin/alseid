#include <cstring>

#include "../util/util_string.h"

#include "gfx_shader.h"

namespace as {

GfxShaderIface::GfxShaderIface(
        GfxShaderDesc&&               desc,
        GfxShaderBinaryDesc&&         binary)
: m_desc    (std::move(desc))
, m_binary  (std::move(binary))
, m_hash    (UniqueHash::compute(m_binary.data.size(), m_binary.data.data())) {
  m_debugName = m_desc.debugName
    ? strcat(m_desc.debugName)
    : m_hash.toString();

  m_desc.debugName = m_debugName.c_str();
}


GfxShaderIface::~GfxShaderIface() {

}


std::optional<GfxShaderBinding> GfxShaderIface::findBinding(
  const char*                         name) const {
  for (auto& binding : m_desc.bindings) {
    if (binding.name == name)
      return binding;
  }

  return std::nullopt;
}




GfxShader::GfxShader(
        GfxShaderDesc&&               desc,
        GfxShaderBinaryDesc&&         binary)
: IfaceRef<GfxShaderIface>(std::make_shared<GfxShaderIface>(
    std::move(desc), std::move(binary))) {
  
}

}
