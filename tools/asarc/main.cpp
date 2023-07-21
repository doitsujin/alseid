#include <fstream>

#include <nlohmann/json.hpp>

#include "../../src/gfx/gfx_format.h"

#include "../libasarchive/archive.h"
#include "../libasarchive/merge.h"
#include "../libasarchive/shader.h"
#include "../libasarchive/texture.h"

using namespace as;
using namespace as::archive;
using nlohmann::json;

Environment g_env;


struct TextureArgs {
  TextureDesc desc;
  std::vector<std::string> inputs;
};

void from_json(const json& j, TextureArgs& args) {
  std::string format;

  if (j.count("name"))
    j.at("name").get_to(args.desc.name);

  if (j.count("format"))
    j.at("format").get_to(format);

  if (j.count("mips"))
    j.at("mips").get_to(args.desc.enableMips);

  if (j.count("cube"))
    j.at("cube").get_to(args.desc.enableCube);

  if (j.count("array"))
    j.at("array").get_to(args.desc.enableLayers);

  if (j.count("allowCompression"))
    j.at("allowCompression").get_to(args.desc.allowCompression);

  if (j.count("allowBc7"))
    j.at("allowBc7").get_to(args.desc.allowBc7);

  if (j.count("inputs"))
    j.at("inputs").get_to(args.inputs);

  if (args.desc.name.empty() && !args.inputs.empty())
    args.desc.name = std::filesystem::path(args.inputs.at(0)).stem();

  args.desc.enableLayers |= args.desc.enableCube;
  args.desc.format = textureFormatFromString(format);
}


struct ShaderArgs {
  ShaderDesc desc;
  std::vector<std::string> inputs;
};

void from_json(const json& j, ShaderArgs& args) {
  if (j.count("inputs"))
    j.at("inputs").get_to(args.inputs);
}


struct ArchiveArgs {
  std::vector<TextureArgs> textures;
  std::vector<ShaderArgs> shaders;
};

void from_json(const json& j, ArchiveArgs& args) {
  if (j.count("textures"))
    j.at("textures").get_to(args.textures);

  if (j.count("shaders"))
    j.at("shaders").get_to(args.shaders);
}


class ConsoleArgs {

public:

  ConsoleArgs() { }
  ConsoleArgs(int argc, char** argv)
  : m_argc(argc), m_argv(argv) { }

  std::string next() {
    if (m_next < m_argc)
      return m_argv[m_next++];

    return "";
  }

  std::string peek() {
    if (m_next < m_argc)
      return m_argv[m_next];

    return "";
  }

  bool has(int count) const {
    return m_next + count <= m_argc;
  }

private:

  int m_argc = 0;
  int m_next = 1;

  char** m_argv = nullptr;

};


std::vector<std::filesystem::path> getInputList(ConsoleArgs& args) {
  std::vector<std::filesystem::path> result;

  while (args.has(1)) {
    std::string arg = args.peek();

    if (arg.size() == 0 || arg[0] == '-')
      return result;

    result.push_back(args.next());
  }

  return result;
}


bool buildMerge(ArchiveBuilder& builder, const std::filesystem::path& path) {
  auto archive = std::make_shared<IoArchive>(g_env.io->open(path, IoOpenMode::eRead));

  if (!(*archive)) {
    Log::err("Failed to open archive ", path);
    return false;
  }

  // Dispatch one merge job per file
  for (uint32_t i = 0; i < archive->getFileCount(); i++)
    builder.addBuildJob(std::make_shared<MergeBuildJob>(g_env, archive, i));

  return true;
}


bool buildMerges(ConsoleArgs& args, ArchiveBuilder& builder) {
  std::vector<std::filesystem::path> paths = getInputList(args);

  for (const auto& path : paths) {
    if (!buildMerge(builder, path))
      return false;
  }

  return true;
}


void buildShader(ArchiveBuilder& builder, const ShaderDesc& desc, const std::filesystem::path& path) {
  builder.addBuildJob(std::make_shared<ShaderBuildJob>(g_env, desc, path));
}


bool buildShaders(ConsoleArgs& args, ArchiveBuilder& builder, const ShaderDesc& desc) {
  std::vector<std::filesystem::path> paths = getInputList(args);

  for (const auto& path : paths)
    buildShader(builder, desc, path);

  return true;
}


void buildTexture(ArchiveBuilder& builder, const TextureDesc& desc, const std::vector<std::filesystem::path>& paths) {
  builder.addBuildJob(std::make_shared<TextureBuildJob>(g_env, desc, paths));
}


bool buildTextures(ConsoleArgs& args, ArchiveBuilder& builder, TextureDesc desc) {
  std::vector<std::filesystem::path> paths = getInputList(args);

  if (paths.empty())
    return false;

  if (desc.name.empty())
    desc.name = paths[0].stem();

  if (desc.enableLayers) {
    buildTexture(builder, desc, paths);
  } else {
    std::vector<std::filesystem::path> singlePath(1);

    for (const auto& path : paths) {
      singlePath[0] = path;
      buildTexture(builder, desc, singlePath);
    }
  }

  return true;
}


bool buildJson(ConsoleArgs& args, ArchiveBuilder& builder) {
  std::vector<std::filesystem::path> paths = getInputList(args);

  for (const auto& path : paths) {
    std::ifstream file(path);
    ArchiveArgs args = json::parse(file);

    for (const auto& tex : args.textures) {
      if (tex.desc.enableLayers) {
        std::vector<std::filesystem::path> inputPaths;

        for (const auto& input : tex.inputs)
          inputPaths.push_back(input);

        buildTexture(builder, tex.desc, inputPaths);
      } else {
        std::vector<std::filesystem::path> singlePath(1);

        for (const auto& input : tex.inputs) {
          singlePath[0] = input;
          buildTexture(builder, tex.desc, singlePath);
        }
      }
    }

    for (const auto& shader : args.shaders) {
      for (const auto& input : shader.inputs)
        buildShader(builder, shader.desc, input);
    }
  }

  return true;
}


int executeBuild(ConsoleArgs& args) {
  if (!args.has(1)) {
    std::cerr << "Output file not specified" << std::endl;
    return 1;
  }

  // Initialize builder
  ArchiveBuilder builder(g_env);
  std::filesystem::path outputPath(args.next());

  // Parse command line arguments. This is rather complex since parameters
  // and input files can be passed in manually, or via json files. The former
  // is required for build system integration.
  ShaderDesc shaderDesc = { };
  TextureDesc textureDesc = { };

  while (args.has(1)) {
    std::string arg = args.next();
    bool status = true;

    if (arg == "-j") {
      status = buildJson(args, builder);
    } else if (arg == "-a") {
      status = buildMerges(args, builder);
    } else if (arg == "-s") {
      status = buildShaders(args, builder, shaderDesc);
    } else if (arg == "-t") {
      status = buildTextures(args, builder, textureDesc);
      textureDesc.name = std::string();
    } else if (arg == "-t-allow-bc7") {
      arg = args.next();
      textureDesc.allowBc7 = arg == "on";
    } else if (arg == "-t-mips") {
      arg = args.next();
      textureDesc.enableMips = arg == "on";
    } else if (arg == "-t-cube") {
      arg = args.next();
      textureDesc.enableLayers = arg == "on";
      textureDesc.enableCube = arg == "on";
    } else if (arg == "-t-layers") {
      arg = args.next();
      textureDesc.enableLayers = arg == "on";
      textureDesc.enableCube = false;
    } else if (arg == "-t-format") {
      arg = args.next();
      textureDesc.format = textureFormatFromString(arg);
    } else if (arg == "-t-compression") {
      arg = args.next();
      textureDesc.allowCompression = arg == "on";
    } else {
      std::cerr << "Unknown argument: " << arg << std::endl;
      status = false;
    }

    if (!status)
      return 1;
  }

  // Wait for build process to complete
  BuildResult status = builder.build(outputPath);

  if (status != BuildResult::eSuccess) {
    std::cerr << "Failed to build archive" << std::endl;
    return 1;
  }

  return 0;
}


int printHelp() {
  return 1;
}


int main(int argc, char** argv) {
  Log::setLogLevel(LogSeverity::eError);

  g_env.io = Io(IoBackend::eDefault, std::thread::hardware_concurrency());
  g_env.jobs = Jobs(std::thread::hardware_concurrency());

  ConsoleArgs args(argc, argv);

  int status = 1;

  if (args.has(1)) {
    std::string mode = args.next();

    if (mode == "-h" || mode == "--help")
      status = printHelp();
    if (mode == "-o")
      status = executeBuild(args);
    else
      std::cerr << "Unknown mode: " << mode << std::endl;
  } else {
    status = printHelp();
  }

  return status;
}
