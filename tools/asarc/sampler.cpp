#include "../../src/gfx/asset/gfx_asset_sampler.h"

#include "sampler.h"

struct SamplerDesc {
  std::string name;
  std::string type;
  std::string minFilter;
  std::string magFilter;
  std::string mipFilter;
  std::string addressModeU;
  std::string addressModeV;
  std::string addressModeW;
  std::string borderColor;
  std::string compareOp;
  float lodBias = 0.0f;
  bool allowAnisotropy = true;
  bool allowLodBias = true;
};


void from_json(const json& j, SamplerDesc& args) {
  if (j.count("name"))
    j.at("name").get_to(args.name);

  if (j.count("type"))
    j.at("type").get_to(args.type);

  if (j.count("minFilter"))
    j.at("minFilter").get_to(args.minFilter);

  if (j.count("magFilter"))
    j.at("magFilter").get_to(args.magFilter);

  if (j.count("mipFilter"))
    j.at("mipFilter").get_to(args.mipFilter);

  if (j.count("addrModeU"))
    j.at("addrModeU").get_to(args.addressModeU);

  if (j.count("addrModeV"))
    j.at("addrModeV").get_to(args.addressModeV);

  if (j.count("addrModeW"))
    j.at("addrModeW").get_to(args.addressModeW);

  if (j.count("borderColor"))
    j.at("borderColor").get_to(args.borderColor);

  if (j.count("compareOp"))
    j.at("compareOp").get_to(args.compareOp);

  if (j.count("lodBias"))
    j.at("lodBias").get_to(args.lodBias);

  if (j.count("allowAnisotropy"))
    j.at("allowAnisotropy").get_to(args.allowAnisotropy);

  if (j.count("allowLodBias"))
    j.at("allowLodBias").get_to(args.allowLodBias);
}


GfxSamplerType parseSamplerType(const std::string& type) {
  if (type.empty())
    return GfxSamplerType::eDefault;

  if (type == "depth-compare")
    return GfxSamplerType::eDepthCompare;

  if (type != "default")
    Log::err("Unknown sampler type ", type);

  return GfxSamplerType::eDefault;
}


GfxFilter parseFilter(const std::string& filter) {
  if (filter.empty())
    return GfxFilter::eLinear;

  if (filter == "nearest")
    return GfxFilter::eNearest;

  if (filter != "linear")
    Log::err("Unknown filter ", filter);

  return GfxFilter::eLinear;
}


GfxMipFilter parseMipFilter(const std::string& filter) {
  if (filter.empty())
    return GfxMipFilter::eLinear;

  if (filter == "nearest")
    return GfxMipFilter::eNearest;

  if (filter != "linear")
    Log::err("Unknown filter ", filter);

  return GfxMipFilter::eLinear;
}


GfxAddressMode parseAddressMode(const std::string& mode) {
  static const std::array<std::pair<const char*, GfxAddressMode>, 5> s_modes = {{
    { "repeat",         GfxAddressMode::eRepeat             },
    { "mirror",         GfxAddressMode::eMirror             },
    { "clamp",          GfxAddressMode::eClampToEdge        },
    { "clamp-border",   GfxAddressMode::eClampToBorder      },
    { "mirror-clamp",   GfxAddressMode::eMirrorClampToEdge  },
  }};

  if (mode.empty())
    return GfxAddressMode::eRepeat;

  for (const auto& p : s_modes) {
    if (mode == p.first)
      return p.second;
  }

  Log::err("Unknown address mode ", mode);
  return GfxAddressMode::eRepeat;
}


GfxBorderColor parseBorderColor(const std::string& color) {
  static const std::array<std::pair<const char*, GfxBorderColor>, 6> s_modes = {{
    { "transparent",        GfxBorderColor::eFloatTransparent },
    { "black",              GfxBorderColor::eFloatBlack       },
    { "white",              GfxBorderColor::eFloatWhite       },
    { "int-transparent",    GfxBorderColor::eIntTransparent   },
    { "int-black",          GfxBorderColor::eIntBlack         },
    { "int-white",          GfxBorderColor::eIntWhite         },
  }};

  if (color.empty())
    return GfxBorderColor::eFloatTransparent;

  for (const auto& p : s_modes) {
    if (color == p.first)
      return p.second;
  }

  Log::err("Unknown border color ", color);
  return GfxBorderColor::eFloatTransparent;
}


GfxCompareOp parseCompareOp(const std::string& op) {
  static const std::array<std::pair<const char*, GfxCompareOp>, 8> s_ops = {{
    { "never",          GfxCompareOp::eNever        },
    { "less",           GfxCompareOp::eLess         },
    { "equal",          GfxCompareOp::eEqual        },
    { "less-equal",     GfxCompareOp::eLessEqual    },
    { "greater",        GfxCompareOp::eGreater      },
    { "not-equal",      GfxCompareOp::eNotEqual     },
    { "greater-equal",  GfxCompareOp::eGreaterEqual },
    { "always",         GfxCompareOp::eAlways       },
  }};

  if (op.empty())
    return GfxCompareOp::eAlways;

  for (const auto& p : s_ops) {
    if (op == p.first)
      return p.second;
  }

  Log::err("Unknown compare op ", op);
  return GfxCompareOp::eAlways;
}


GfxAssetSamplerDesc parseDesc(const SamplerDesc& desc) {
  GfxAssetSamplerDesc result = { };
  result.type = parseSamplerType(desc.type);
  result.minFilter = parseFilter(desc.minFilter);
  result.magFilter = parseFilter(desc.magFilter);
  result.mipFilter = parseMipFilter(desc.mipFilter);
  result.addressModeU = parseAddressMode(desc.addressModeU);
  result.addressModeV = parseAddressMode(desc.addressModeV);
  result.addressModeW = parseAddressMode(desc.addressModeW);
  result.borderColor = parseBorderColor(desc.borderColor);
  result.lodBias = desc.lodBias;
  result.allowAnisotropy = desc.allowAnisotropy;
  result.allowLodBias = desc.allowLodBias;
  result.compareOp = parseCompareOp(desc.compareOp);
  return result;
}


void processSamplers(ArchiveBuilder& builder, const json& j) {
  std::vector<SamplerDesc> samplers;

  if (j.count("samplers"))
    j.at("samplers").get_to(samplers);

  for (const auto& sampler : samplers) {
    FileDesc desc;
    desc.name = sampler.name;
    desc.type = "SMP"_4cc;

    if (!parseDesc(sampler).serialize(Lwrap<WrVectorStream>(desc.inlineData))) {
      Log::err("Failed to serialize sampler ", sampler.name);
      continue;
    }

    builder.addBuildJob(std::make_shared<BasicBuildJob>(g_env, std::move(desc)));
  }
}
