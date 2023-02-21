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


const GfxFormatInfo& GfxImageViewIface::getFormatInfo() const {
  return Gfx::getFormatInfo(m_desc.format);
}


GfxImageIface::GfxImageIface(
  const GfxImageDesc&                 desc)
: m_desc(desc) {
  if (m_desc.debugName) {
    m_debugName = m_desc.debugName;
    m_desc.debugName = m_debugName.c_str();
  }
}


const GfxFormatInfo& GfxImageIface::getFormatInfo() const {
  return Gfx::getFormatInfo(m_desc.format);
}


uint32_t GfxImageIface::computeSubresourceIndex(
  const GfxImageSubresource&          subresource) const {
  uint32_t plane = 0;

  if (subresource.aspects) {
    auto formatInfo = Gfx::getFormatInfo(m_desc.format);
    plane = formatInfo.computePlaneIndex(subresource.aspects.first());
  }

  return m_desc.mips * (m_desc.layers * plane +
    subresource.layerIndex) + subresource.mipIndex;
}


GfxImageSubresource GfxImageIface::getAvailableSubresources() const {
  auto desc = getDesc();
  auto& formatInfo = Gfx::getFormatInfo(desc.format);

  return GfxImageSubresource(formatInfo.aspects,
    0, desc.mips, 0, desc.layers);
}




bool GfxTextureDesc::serialize(
        WrBufferedStream&             output) {
  bool success = true;
  WrStream writer(output);

  // Version number, image type etc
  success &= writer.write(uint8_t(0))
          && writer.write(uint8_t(type))
          && writer.write(uint16_t(format));

  // Write required size components only
  for (uint32_t i = 0; i < gfxGetImageDimensions(type); i++)
    success &= writer.write(uint16_t(extent[i]));

  success &= writer.write(uint16_t(mips))
          && writer.write(uint16_t(layers))
          && writer.write(uint32_t(flags));

  return success;
}


bool GfxTextureDesc::deserialize(
        RdMemoryView                  input) {
  RdStream reader(input);
  uint8_t version = 0;

  if (!reader.read(version) || version > 0)
    return false;

  if (!reader.readAs<uint8_t>(type)
   || !reader.readAs<uint16_t>(format))
    return false;

  extent = Extent3D(1, 1, 1);

  for (uint32_t i = 0; i < gfxGetImageDimensions(type); i++) {
    if (!reader.readAs<uint16_t>(extent[i]))
      return false;
  }
  if (!reader.readAs<uint16_t>(mips)
   || !reader.readAs<uint16_t>(layers)
   || !reader.readAs<uint32_t>(flags))
    return false;

  return true;
}


void GfxTextureDesc::fillImageDesc(
        GfxImageDesc&                 desc) {
  desc.type = type;
  desc.format = format;
  desc.extent = extent;
  desc.mips = mips;
  desc.layers = layers;

  if (flags & GfxTextureFlag::eCubeMap)
    desc.flags |= GfxImageFlag::eCubeViews;
}

}
