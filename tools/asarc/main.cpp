#include <fstream>

#include "../../src/third_party/nlohmann/json.hpp"

#include "../../src/gfx/gfx_format.h"

#include "../libasarchive/archive.h"
#include "../libasarchive/geometry.h"
#include "../libasarchive/merge.h"
#include "../libasarchive/shader.h"
#include "../libasarchive/texture.h"

#include "common.h"
#include "geometry.h"
#include "sampler.h"
#include "texture.h"

Environment g_env;

std::filesystem::path g_basedir;


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
  auto archive = IoArchive::fromFile(g_env.io->open(path, IoOpenMode::eRead));

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


void buildGeometry(ArchiveBuilder& builder, const GeometryDesc& desc, const std::filesystem::path& path) {
  builder.addBuildJob(std::make_shared<GeometryBuildJob>(g_env, desc, path));
}


bool buildGeometries(ConsoleArgs& args, ArchiveBuilder& builder, const GeometryDesc& desc) {
  std::vector<std::filesystem::path> paths = getInputList(args);

  for (const auto& path : paths)
    buildGeometry(builder, desc, path);

  return true;
}


bool buildJson(ConsoleArgs& args, ArchiveBuilder& builder) {
  std::vector<std::filesystem::path> paths = getInputList(args);

  for (const auto& path : paths) {
    std::ifstream file(path);

    auto j = json::parse(file);
    processSamplers(builder, j);
    processTextures(builder, j);
    processGeometries(builder, j);
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
  GeometryDesc geometryDesc = { };
  geometryDesc.layoutMap = std::make_shared<GltfPackedVertexLayoutMap>();

  while (args.has(1)) {
    std::string arg = args.next();
    bool status = true;

    if (arg == "-j") {
      status = buildJson(args, builder);
    } else if (arg == "-I") {
      arg = args.next();
      g_basedir = arg;
    } else if (arg == "-a") {
      status = buildMerges(args, builder);
    } else if (arg == "-s") {
      status = buildShaders(args, builder, shaderDesc);
    } else if (arg == "-t") {
      status = buildTextures(args, builder, textureDesc);
    } else if (arg == "-g") {
      status = buildGeometries(args, builder, geometryDesc);
      textureDesc.name = std::string();
    } else if (arg == "-g-layout") {
      arg = args.next();
      geometryDesc.layoutMap->emplace(json::parse(arg));
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
