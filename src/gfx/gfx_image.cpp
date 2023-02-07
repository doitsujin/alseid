#include "gfx.h"
#include "gfx_image.h"
#include "gfx_utils.h"

namespace as {

GfxImageViewIface::GfxImageViewIface(
  const GfxImageIface&                image,
  const GfxImageViewDesc&             desc)
: m_desc        (desc)
, m_imageExtent (image.getDesc().extent)
, m_imageSamples(image.getDesc().samples) {

}


GfxImageIface::GfxImageIface(
  const GfxImageDesc&                 desc)
: m_desc(desc) {
  if (m_desc.debugName) {
    m_debugName = m_desc.debugName;
    m_desc.debugName = m_debugName.c_str();
  }
}


GfxImageSubresource GfxImageIface::getAvailableSubresources() const {
  auto desc = getDesc();
  auto& formatInfo = Gfx::getFormatInfo(desc.format);

  return GfxImageSubresource(formatInfo.aspects,
    0, desc.mips, 0, desc.layers);
}

}
