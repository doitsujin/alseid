#include <filesystem>

#include "../../src/gfx/gfx_shader.h"
#include "../../src/gfx/gfx_spirv.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"
#include "../../src/io/io_stream.h"

#include "../../src/util/util_log.h"

using namespace as;

int main(int argc, char** argv) {
  Log::setLogLevel(LogSeverity::eError);

  Io io(IoBackend::eDefault, 1);

  if (argc != 3) {
    Log::err("Usage: ", argv[0], " in.spv out.av");
    return 1;
  }

  std::filesystem::path inPath = argv[1];
  std::filesystem::path outPath = argv[2];

  // Open and read SPIR-V file
  IoFile inFile = io->open(inPath, IoOpenMode::eRead);

  if (!inFile) {
    Log::err("Failed to open ", inPath);
    return 1;
  }

  std::vector<char> spv(inFile->getSize());

  if (!InFileStream(std::move(inFile)).read(spv)) {
    Log::err("Failed to read ", inPath);
    return 1;
  }

  // Generate and serialize shader description
  auto shaderDesc = reflectSpirvBinary(spv.size(), spv.data());

  if (!shaderDesc) {
    Log::err("Failed to reflect SPIR-V binary");
    return 1;
  }

  OutVectorStream shaderDescStream;

  if (!shaderDesc->serialize(shaderDescStream)) {
    Log::err("Failed to serialize shader description");
    return 1;
  }

  // Encode actual shader binary
  OutVectorStream shaderBinaryStream;
  InMemoryStream shaderMemoryStream(spv);

  if (!encodeSpirvBinary(shaderBinaryStream, shaderMemoryStream, spv.size())) {
    Log::err("Failed to encode SPIR-V binary");
    return 1;
  }

  // Create archive file
  auto shaderDescData = std::move(shaderDescStream).getData();
  auto shaderBinaryData = std::move(shaderBinaryStream).getData();

  IoArchiveDesc desc;

  auto& file = desc.files.emplace_back();
  file.name = outPath.stem();
  file.inlineDataSource.memory = shaderDescData.data();
  file.inlineDataSource.size = shaderDescData.size();

  auto& subFile = file.subFiles.emplace_back();
  subFile.dataSource.memory = shaderBinaryData.data();
  subFile.dataSource.size = shaderBinaryData.size();
  subFile.identifier = FourCC('S', 'P', 'I', 'R');
  subFile.decodingTable = 0;

  IoArchiveBuilder builder(io, desc);

  if (builder.build(outPath) != IoStatus::eSuccess) {
    Log::err("Failed to write ", outPath);
    return 1;
  }

  return 0;
}
