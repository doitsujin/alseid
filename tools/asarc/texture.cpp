#include "texture.h"

struct TextureLayout {
  std::string name;
  GfxFormat format = GfxFormat::eUnknown;
  bool enableMips = true;
  bool enableCube = false;
  bool enableLayers = false;
  bool allowCompression = true;
  bool allowBc7 = false;
};


void from_json(const json& j, TextureLayout& args) {
  std::string format;

  if (j.count("name"))
    j.at("name").get_to(args.name);

  if (j.count("format"))
    j.at("format").get_to(format);

  if (j.count("mips"))
    j.at("mips").get_to(args.enableMips);

  if (j.count("cube"))
    j.at("cube").get_to(args.enableCube);

  if (j.count("array"))
    j.at("array").get_to(args.enableLayers);

  if (j.count("compression"))
    j.at("compression").get_to(args.allowCompression);

  if (j.count("bc7"))
    j.at("bc7").get_to(args.allowBc7);

  args.enableLayers |= args.enableCube;
  args.format = textureFormatFromString(format);
}


struct TextureFile {
  std::string name;
  std::string layout;
  std::vector<std::string> input;
};


void from_json(const json& j, TextureFile& args) {
  if (j.count("name"))
    j.at("name").get_to(args.name);

  if (j.count("layout"))
    j.at("layout").get_to(args.layout);

  if (j.count("input"))
    j.at("input").get_to(args.input);
}


void processTextures(ArchiveBuilder& builder, const json& j) {
  std::unordered_map<std::string, TextureLayout> layouts;

  std::vector<TextureLayout> layoutList;
  std::vector<TextureFile> textureList;

  if (j.count("texture_layouts"))
    j.at("texture_layouts").get_to(layoutList);

  if (j.count("textures"))
    j.at("textures").get_to(textureList);

  for (const auto& layout : layoutList)
    layouts.insert_or_assign(layout.name, layout);

  for (const auto& texture : textureList) {
    auto layout = layouts.find(texture.layout);

    if (layout == layouts.end()) {
      Log::err("Unknown layout ", texture.layout, " for texture ", texture.name);
      continue;
    }

    std::vector<std::filesystem::path> inputs;

    for (const auto& input : texture.input)
      inputs.push_back(g_basedir / input);

    TextureDesc desc;
    desc.name = texture.name;
    desc.format = layout->second.format;
    desc.enableMips = layout->second.enableMips;
    desc.enableCube = layout->second.enableCube;
    desc.enableLayers = layout->second.enableLayers;
    desc.allowCompression = layout->second.allowCompression;
    desc.allowBc7 = layout->second.allowBc7;

    builder.addBuildJob(std::make_shared<TextureBuildJob>(g_env, desc, std::move(inputs)));
  }
}
