#include <filesystem>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "../../src/util/util_log.h"

#include "../../src/job/job.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

using namespace as;

enum class ArgMode : uint32_t {
  eInput,
  eOutput,
};

using InputDataList = std::list<std::vector<char>>;

void printHelp(const char* name) {
  std::cout << "Usage: " << name << " merge|extract|print [...]" << std::endl << std::endl
            << "Use --help with any of the subcommands for details." << std::endl;
}


void printMergeHelp(const char* name) {
  std::cout << "Usage: " << name << " merge -o outfile [[input1, input2, ...]]" << std::endl
            << "  -o  outfile   : Specifies output file" << std::endl
            << "  --help        : Shows this message." << std::endl;
}


void printExtractHelp(const char* name) {
  std::cout << "Usage: " << name << " extract archive file [-n index | -s fourcc | -i] [-c] [-o output]" << std::endl
            << "  -o  outfile   : Specifies output file" << std::endl
            << "  -n  index     : Extracts sub-file at the given index" << std::endl
            << "  -s  name      : Extracts sub-file with the given name" << std::endl
            << "  -i            : Extracts inline data" << std::endl
            << "  -c            : Extracts raw (compressed) data" << std::endl;
}


void printPrintHelp(const char* name) {
  std::cout << "Usage: " << name << " print archive" << std::endl;
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


bool processInput(
  const Io&                             io,
        IoArchiveDesc&                  outputDesc,
        InputDataList&                  inputs,
        std::filesystem::path           path) {
  IoArchive archive(io->open(path, IoOpenMode::eRead));

  if (!archive) {
    Log::err("Failed to open archive ", path);
    return false;
  }

  // Accumulate basic information about this file
  size_t dataSize = 0;

  for (uint32_t i = 0; i < archive.getFileCount(); i++) {
    auto file = archive.getFile(i);
    dataSize += file->getInlineData().getSize();

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

    auto inlineData = file->getInlineData();

    if (inlineData) {
      std::memcpy(&data[dataOffset], inlineData.getData(), inlineData.getSize());

      info.inlineDataSource.memory = &data[dataOffset];
      info.inlineDataSource.size = inlineData.getSize();

      dataOffset += inlineData.getSize();
    }

    for (uint32_t j = 0; j < file->getSubFileCount(); j++) {
      auto* sub = file->getSubFile(j);
      archive.read(rq, sub, &data[dataOffset]);

      auto& subInfo = info.subFiles.emplace_back();
      subInfo.dataSource.memory = &data[dataOffset];
      subInfo.dataSource.size = sub->getSize();
      subInfo.identifier = sub->getIdentifier();
      subInfo.compression = sub->getCompressionType();

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


int merge(const Io& io, const Jobs& jobs, int argc, char** argv) {
  // Source data arrays, one vector per source file.
  InputDataList inputs;

  // Output archive description
  IoArchiveDesc outputDesc;

  // File path for output archive
  std::optional<std::filesystem::path> outputPath;

  // Argument processing mode
  ArgMode argMode = ArgMode::eInput;

  for (int i = 2; i < argc; i++) {
    std::string arg = argv[i];

    switch (argMode) {
      case ArgMode::eInput: {
        if (arg == "-o") {
          argMode = ArgMode::eOutput;
        } else if (arg == "-h" || arg == "--help") {
          printHelp(argv[0]);
          return 0;
        } else {
          if (!processInput(io, outputDesc, inputs, arg))
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
    }
  }

  if (!outputPath) {
    Log::err("No output specified");
    return 1;
  }

  IoArchiveBuilder builder(io, jobs, outputDesc);

  if (builder.build(*outputPath) != IoStatus::eSuccess) {
    Log::err("Failed to write output file ", *outputPath);
    return 1;
  }

  return 0;
}


enum class ExtractMode : uint32_t {
  eNone           = 0,
  eInlineData     = 1,
  eSubfileIndex   = 2,
  eSubfileFourCC  = 3,
};


int extract(const Io& io, int argc, char** argv) {
  if (argc < 3) {
    Log::err("No input file specified");
    return 1;
  }

  if (argc < 4) {
    Log::err("No file within the archive specified");
    return 1;
  }

  // Parse remaining arguments
  std::optional<std::filesystem::path> outputPath;

  bool compressed = false;
  ExtractMode mode = ExtractMode::eNone;

  FourCC subfileIdentifier = { };
  uint32_t subfileIndex = 0;

  for (int i = 4; i < argc; i++) {
    std::string arg = argv[i];

    ExtractMode newMode = ExtractMode::eNone;

    if (arg == "-c") {
      compressed = true;
    } else if (arg == "-i") {
      newMode = ExtractMode::eInlineData;
    } else if (arg == "-o") {
      if (i + 1 == argc) {
        Log::err("Missing argument");
        return 1;
      }

      if (outputPath) {
        Log::err("Output already specified");
        return 1;
      }

      outputPath = argv[++i];
    } else if (arg == "-n") {
      newMode = ExtractMode::eSubfileIndex;

      if (i + 1 == argc) {
        Log::err("Missing argument");
        return 1;
      }

      arg = argv[++i];

      try {
        subfileIndex = std::stoi(arg);
      } catch (const std::exception& e) {
        Log::err("Invalid subfile index");
        return 1;
      }
    } else if (arg == "-s") {
      newMode = ExtractMode::eSubfileFourCC;

      if (i + 1 == argc) {
        Log::err("Missing argument");
        return 1;
      }

      arg = argv[++i];

      if (arg.size() != 4) {
        Log::err("Invalid subfile identifier");
        return 1;
      }

      subfileIdentifier = FourCC(arg[0], arg[1], arg[2], arg[3]);
    }

    if (newMode != ExtractMode::eNone) {
      if (mode != ExtractMode::eNone) {
        Log::err("Extract mode already specified");
        return 1;
      }

      mode = newMode;
    }
  }

  if (mode == ExtractMode::eNone) {
    Log::err("No sub-file specified");
    return 1;
  }

  if (!outputPath) {
    Log::err("No output specified");
    return 1;
  }

  // Actually load and process the file
  std::filesystem::path path = argv[2];
  IoArchive archive(io->open(path, IoOpenMode::eRead));

  if (!archive) {
    Log::err("Failed to open archive ", path);
    return 1;
  }

  auto file = archive.findFile(argv[3]);

  if (!file) {
    Log::err("Given file not found in archive");
    return 1;
  }

  std::vector<char> data;

  if (mode == ExtractMode::eInlineData) {
    auto inlineData = file->getInlineData();

    data.resize(inlineData.getSize());
    std::memcpy(data.data(), inlineData.getData(), inlineData.getSize());
  } else {
    const IoArchiveSubFile* subFile = nullptr;

    if (mode == ExtractMode::eSubfileIndex)
      subFile = file->getSubFile(subfileIndex);
    else if (mode == ExtractMode::eSubfileFourCC)
      subFile = file->findSubFile(subfileIdentifier);

    if (!subFile) {
      Log::err("Given sub-file not found in file");
      return 1;
    }

    if (compressed) {
      data.resize(subFile->getCompressedSize());
      archive.readCompressed(subFile, data.data());
    } else {
      data.resize(subFile->getSize());
      archive.read(subFile, data.data());
    }
  }

  WrFileStream outfile(io->open(*outputPath, IoOpenMode::eCreate));

  if (!outfile) {
    Log::err("Failed to open output file ", *outputPath);
    return 1;
  }

  if (!WrStream(outfile).write(data)) {
    Log::err("Failed to write output file");
    return 1;
  }

  return 0;
}


int print(const Io& io, int argc, char** argv) {
  if (argc < 3) {
    Log::err("No input file specified");
    return 1;
  }

  std::filesystem::path path = argv[2];
  IoArchive archive(io->open(path, IoOpenMode::eRead));

  if (!archive) {
    Log::err("Failed to open archive ", path);
    return 1;
  }

  std::cout << "Files: " << archive.getFileCount() << std::endl;

  for (uint32_t i = 0; i < archive.getFileCount(); i++) {
    auto file = archive.getFile(i);
    std::cout << "    " << file->getName() << ":" << std::endl;

    auto inlineData = file->getInlineData();

    if (inlineData)
      std::cout << "        Inline data: " << inlineData.getSize() << " bytes " << std::endl;

    std::cout << "        Sub files: " << file->getSubFileCount() << std::endl;

    for (uint32_t j = 0; j < file->getSubFileCount(); j++) {
      auto subFile = file->getSubFile(j);
      std::cout << "            '" << subFile->getIdentifier().toString() << "' (" << j << ") : "
                << subFile->getSize() << " bytes";

      if (subFile->isCompressed())
        std::cout << " (" << subFile->getCompressedSize() << " compressed)";

      std::cout << ", offset: " << subFile->getOffsetInArchive() << std::endl;

      std::string compression;

      switch (subFile->getCompressionType()) {
        case IoArchiveCompression::eNone:
          compression = "None";
          break;

        case IoArchiveCompression::eDeflate:
          compression = "Deflate";
          break;

        default:
          compression = strcat("IoArchiveCompression(", uint32_t(subFile->getCompressionType()), ")");
          break;
      }

      std::cout << "                Compression: " << compression << std::endl;
    }
  }

  return 0;
}


int main(int argc, char** argv) {
  Log::setLogLevel(LogSeverity::eError);

  // Initialize I/O system
  Io io(IoBackend::eDefault, std::thread::hardware_concurrency());
  Jobs jobs(std::thread::hardware_concurrency());

  if (argc >= 2) {
    std::string op = argv[1];

    if (op == "merge")
      return merge(io, jobs, argc, argv);

    if (op == "extract")
      return extract(io, argc, argv);

    if (op == "print")
      return print(io, argc, argv);
  }

  printHelp(argv[0]);
  return 1;
}
