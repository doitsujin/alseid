#include <filesystem>
#include <list>
#include <vector>

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

  if (argc < 3) {
    Log::err("Usage: ", argv[0], " out.asa shader.spv [shader2.spv [...]]");
    return 1;
  }

  std::filesystem::path outPath = argv[1];

  IoArchiveDesc desc;
  std::list<std::vector<char>> data;

  for (int i = 2; i < argc; i++) {
    std::filesystem::path inPath = argv[i];

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
    auto& shaderDescData = data.emplace_back(std::move(shaderDescStream).getData());
    auto& shaderBinaryData = data.emplace_back(std::move(shaderBinaryStream).getData());

    auto& file = desc.files.emplace_back();
    file.name = inPath.stem();
    file.inlineDataSource.memory = shaderDescData.data();
    file.inlineDataSource.size = shaderDescData.size();

    auto& subFile = file.subFiles.emplace_back();
    subFile.dataSource.memory = shaderBinaryData.data();
    subFile.dataSource.size = shaderBinaryData.size();
    subFile.identifier = FourCC('S', 'P', 'I', 'R');
    subFile.compression = IoArchiveCompression::eHuffLzss;
  }

  IoArchiveBuilder builder(io, desc);

  if (builder.build(outPath) != IoStatus::eSuccess) {
    Log::err("Failed to write ", outPath);
    return 1;
  }

  return 0;
}
