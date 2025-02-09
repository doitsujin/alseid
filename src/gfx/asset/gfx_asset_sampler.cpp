#include "gfx_asset_sampler.h"

namespace as {

bool GfxAssetSamplerDesc::serialize(
        WrBufferedStream&             output) {
  bool success = true;
  WrStream writer(output);

  success &= writer.write(uint8_t(0))
          && writer.write(uint8_t(type))
          && writer.write(uint8_t(magFilter))
          && writer.write(uint8_t(minFilter))
          && writer.write(uint8_t(mipFilter))
          && writer.write(uint8_t(addressModeU))
          && writer.write(uint8_t(addressModeV))
          && writer.write(uint8_t(addressModeW))
          && writer.write(uint8_t(borderColor))
          && writer.write(uint8_t(allowAnisotropy))
          && writer.write(uint8_t(allowLodBias))
          && writer.write(uint8_t(compareOp))
          && writer.write(int16_t(lodBias * 256.0f));

  return success;
}


bool GfxAssetSamplerDesc::deserialize(
        RdMemoryView                  input) {
  RdStream reader(input);
  uint8_t version = 0;

  if (!reader.read(version) || version > 0)
    return false;

  int16_t rawLodBias = 0;

  if (!reader.readAs<uint8_t>(type)
   || !reader.readAs<uint8_t>(magFilter)
   || !reader.readAs<uint8_t>(minFilter)
   || !reader.readAs<uint8_t>(mipFilter)
   || !reader.readAs<uint8_t>(addressModeU)
   || !reader.readAs<uint8_t>(addressModeV)
   || !reader.readAs<uint8_t>(addressModeW)
   || !reader.readAs<uint8_t>(borderColor)
   || !reader.readAs<uint8_t>(allowAnisotropy)
   || !reader.readAs<uint8_t>(allowLodBias)
   || !reader.readAs<uint8_t>(compareOp)
   || !reader.readAs<int16_t>(rawLodBias))
    return false;

  lodBias = float(rawLodBias) / 256.0f;
  return true;
}


void GfxAssetSamplerDesc::fillSamplerDesc(
          GfxSamplerDesc&               desc) {
  desc.type = type;
  desc.magFilter = magFilter;
  desc.minFilter = minFilter;
  desc.mipFilter = mipFilter;
  desc.addressModeU = addressModeU;
  desc.addressModeV = addressModeV;
  desc.addressModeW = addressModeW;
  desc.borderColor = borderColor;
  desc.compareOp = compareOp;

  if (!allowAnisotropy || minFilter != GfxFilter::eLinear)
    desc.anisotropy = 0u;

  if (!allowLodBias)
    desc.lodBias = 0.0f;

  desc.lodBias += lodBias;
}

}
