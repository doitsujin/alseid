#include <filesystem>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "../../src/util/util_log.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

using namespace as;

enum class ArgMode : uint32_t {
  eInput,
  eOutput,
  eDecodingTableMap,
};

// Source data arrays, one vector per source file.
std::list<std::vector<char>> inputs;

// Current decoding table map. This can
// change as we process inputs.
std::unordered_map<uint16_t, uint16_t> decodingTableMap;

// Output archive description
IoArchiveDesc outputDesc;


void printHelp(const char* name) {
  std::cout << "Usage: " << name << " -o outfile [[-m map] [input1, input2, ...]]" << std::endl
            << "  -o  outfile   : Set output file to outfile" << std::endl
            << "  -m  a:b[,c:d] : Maps the decoding table a in subsequent inputs to decoding" << std::endl
            << "                  table b in the output. Can perform multiple mappings." << std::endl
            << "  --help        : Shows this message." << std::endl;
}


template<typename Cb>
bool parseDecodingMap(const std::string& str, const Cb& cb) {
  int32_t k = 0;
  int32_t v = 0;

  int32_t factor = 1;

  bool hasKey = false;
  bool hasVal = false;

  bool isKey = true;

  for (uint32_t i = 0; i < str.size(); i++) {
    if (str[i] == ',') {
      if (!hasVal)
        return false;

      if (!cb(k, v))
        return false;

      k = 0;
      v = 0;

      hasKey = false;
      hasVal = false;

      factor = 1;
      isKey = true;
    } else if (str[i] == ':') {
      if (!hasKey || !isKey)
        return false;

      factor = 1;
      isKey = false;
    } else if (str[i] >= '0' && str[i] <= '9') {
      int32_t digit = int32_t(str[i]) - int32_t('0');

      int32_t& target = isKey ? k : v;
      target = 10 * target + factor * digit;

      (isKey ? hasKey : hasVal) = true;

      if (target < -1 || target > 0xFFFE)
        return false;
    } else if (str[i] == '-') {
      if (factor != 1)
        return false;

      if (isKey ? hasKey : hasVal)
        return false;

      factor = -1;
    } else {
      return false;
    }
  }

  if (!hasVal)
    return false;

  return cb(k, v);
}


bool processInput(const Io& io, std::filesystem::path path) {
  IoArchive archive(io->open(std::move(path), IoOpenMode::eRead));

  if (!archive) {
    Log::err("Failed to open archive ", path);
    return false;
  }

  // Accumulate basic information about this file
  size_t dataSize = 0;

  for (uint32_t i = 0; i < archive.getFileCount(); i++) {
    auto file = archive.getFile(i);
    dataSize += file->getInlineDataSize();

    for (uint32_t j = 0; j < file->getSubFileCount(); j++)
      dataSize += file->getSubFile(j)->getSize();
  }

  // Allocate memory for all files including inline data
  auto& data = inputs.emplace_back(dataSize);
  size_t dataOffset = 0;

  IoRequest rq = io->createRequest();

  for (uint32_t i = 0; i < archive.getFileCount(); i++) {
    auto* file = archive.getFile(i);
    auto& info = outputDesc.files.emplace_back();
    info.name = file->getName();

    if (file->getInlineDataSize()) {
      std::memcpy(&data[dataOffset], file->getInlineData(), file->getInlineDataSize());

      info.inlineDataSource.memory = &data[dataOffset];
      info.inlineDataSource.size = file->getInlineDataSize();

      dataOffset += file->getInlineDataSize();
    }

    for (uint32_t j = 0; j < file->getSubFileCount(); j++) {
      auto* sub = file->getSubFile(j);
      archive.read(rq, sub, &data[dataOffset]);

      auto& subInfo = info.subFiles.emplace_back();
      subInfo.dataSource.memory = &data[dataOffset];
      subInfo.dataSource.size = sub->getSize();
      subInfo.identifier = sub->getIdentifier();
      subInfo.compression = sub->getCompressionType();
      subInfo.decodingTable = sub->getDecodingTableIndex();

      auto entry = decodingTableMap.find(subInfo.decodingTable);

      if (entry != decodingTableMap.end())
        subInfo.decodingTable = entry->second;

      dataOffset += sub->getSize();
    }
  }

  io->submit(rq);

  if (rq->wait() != IoStatus::eSuccess) {
    Log::err("Failed to read archive ", path);
    return false;
  }

  return true;
}


int main(int argc, char** argv) {
  Log::setLogLevel(LogSeverity::eError);

  // Initialize I/O system
  Io io(IoBackend::eDefault, std::thread::hardware_concurrency());

  // File path for output archive
  std::optional<std::filesystem::path> outputPath;

  // Argument processing mode
  ArgMode argMode = ArgMode::eInput;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    switch (argMode) {
      case ArgMode::eInput: {
        if (arg == "-o") {
          argMode = ArgMode::eOutput;
        } else if (arg == "-m") {
          argMode = ArgMode::eDecodingTableMap;
        } else if (arg == "-h" || arg == "--help") {
          printHelp(argv[0]);
          return 0;
        } else {
          if (!processInput(io, arg))
            return 1;
        }
      } break;

      case ArgMode::eOutput: {
        if (outputPath) {
          Log::err("Output already specified");
          return 1;
        }

        outputPath = arg;
        argMode = ArgMode::eInput;
      } break;

      case ArgMode::eDecodingTableMap: {
        decodingTableMap.clear();

        auto cb = [map = &decodingTableMap] (uint16_t k, uint16_t v) {
          return map->insert({ k, v }).second;
        };

        if (!parseDecodingMap(arg, cb)) {
          Log::err("Invalid map: ", arg);
          return 1;
        }

        argMode = ArgMode::eInput;
      } break;
    }
  }

  if (!outputPath) {
    Log::err("No output specified");
    return 1;
  }

  IoArchiveBuilder builder(io, outputDesc);

  if (builder.build(*outputPath) != IoStatus::eSuccess) {
    Log::err("Failed to write output file ", *outputPath);
    return 1;
  }

  return 0;
}
