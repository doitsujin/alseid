#include "../util/util_assert.h"
#include "../util/util_error.h"
#include "../util/util_huffman.h"
#include "../util/util_log.h"
#include "../util/util_lzss.h"

#include "io_archive.h"

namespace as {

static const std::array<char, 6> IoArchiveMagic = { 'A', 'S', 'F', 'I', 'L', 'E' };


IoArchive::IoArchive(IoFile file)
: m_file(std::move(file)) {
  if (!parseMetadata()) {
    // Reset everything if parsing failed
    m_file = nullptr;
    m_fileNames.clear();
    m_inlineData.clear();
    m_subFiles.clear();
    m_files.clear();
    m_lookupTable.clear();
  }
}


IoArchive::~IoArchive() { 

}


const IoArchiveFile* IoArchive::findFile(const std::string& name) const {
  auto entry = m_lookupTable.find(name);

  if (entry == m_lookupTable.end())
    return nullptr;

  return &m_files[entry->second];
}


IoStatus IoArchive::read(
  const IoArchiveSubFile*             subFile,
        void*                         dst) const {
  if (!subFile->isCompressed())
    return readCompressed(subFile, dst);

  std::vector<char> compressed(subFile->getCompressedSize());
  IoStatus status = readCompressed(subFile, compressed.data());

  if (status != IoStatus::eSuccess)
    return status;

  return decompress(subFile, dst, compressed.data())
    ? IoStatus::eSuccess
    : IoStatus::eError;
}


bool IoArchive::decompress(
  const IoArchiveSubFile*             subFile,
        void*                         dstData,
  const void*                         srcData) const {
  if (!subFile->isCompressed()) {
    if (subFile->getSize() != subFile->getCompressedSize())
      return false;

    // Callers should avoid this whenever possible
    std::memcpy(dstData, srcData, subFile->getSize());
    return true;
  }

  // Currently, no compression types are defined
  if (subFile->getCompressionType() != IoArchiveCompression::eNone)
    return false;

  return false;
}


bool IoArchive::parseMetadata() {
  if (!m_file) {
    Log::err("Archive: File failed to open");
    return false;
  }

  InFileStream stream(m_file);
  IoArchiveHeader fileHeader;

  if (!stream.read(fileHeader)) {
    Log::err("Archive: Failed to read header");
    return false;
  }

  // Check if the file is even something we can parse
  if (std::memcmp(fileHeader.magic, IoArchiveMagic.data(), IoArchiveMagic.size())) {
    Log::err("Archive: Invalid file header");
    return false;
  }

  // Version number is currently always 0
  if (fileHeader.version) {
    Log::err("Archive: Unsupported version ", fileHeader.version);
    return false;
  }

  // Read all file metadata in one go. We cannot actually
  // create the file objects at this point yet, however.
  std::vector<IoArchiveFileMetadata> files(fileHeader.fileCount);

  if (!stream.read(files)) {
    Log::err("Archive: Failed to read header");
    return false;
  }

  size_t totalFileNameSize = 0;
  size_t totalSubFileCount = 0;
  size_t totalInlineDataSize = 0;

  for (const auto& f : files) {
    totalFileNameSize += f.nameLength;
    totalSubFileCount += f.subFileCount;
    totalInlineDataSize += f.inlineDataSize;
  }

  // Read file names into the array. Later we'll validate
  // whether they are all properly null terminated.
  m_fileNames.resize(totalFileNameSize);

  if (!stream.read(m_fileNames)) {
    Log::err("Archive: Failed to read file names (", totalFileNameSize, " bytes)");
    return false;
  }

  // Read all subfile metadata in one go. We'll validate
  // whether the offsets and sizes are in bounds later.
  std::vector<IoArchiveSubFileMetadata> subFiles(totalSubFileCount);

  if (!stream.read(subFiles)) {
    Log::err("Archive: Failed to read sub file metadata");
    return false;
  }

  // Read all inline data directly into the array
  m_inlineData.resize(totalInlineDataSize);

  if (!stream.read(m_inlineData)) {
    Log::err("Archive: Failed to read inline data (", totalInlineDataSize, " bytes)");
    return false;
  }

  // Initialize and validate sub-file objects
  m_subFiles.reserve(totalSubFileCount);

  for (size_t i = 0; i < totalSubFileCount; i++) {
    auto& subFile = m_subFiles.emplace_back(subFiles[i]);

    if (subFile.getOffsetInArchive() + subFile.getCompressedSize() > stream.getSize()) {
      Log::err("Archive: Sub-file out of bounds:"
        "\n  Sub file Offset: ", subFile.getOffsetInArchive(),
        "\n  Sub file size:   ", subFile.getCompressedSize(),
        "\n  Archive size:    ", stream.getSize());
      return false;
    }
  }

  // Finally, create and validate all the file objects
  // and set up the lookup table.
  size_t currSubFileIndex = 0;
  size_t currFileNameOffset = 0;
  size_t currInlineDataOffset = 0;

  for (size_t i = 0; i < fileHeader.fileCount; i++) {
    const IoArchiveSubFile* subFiles = files[i].subFileCount
      ? &m_subFiles[currSubFileIndex]
      : nullptr;

    const char* name = files[i].nameLength
      ? &m_fileNames[currFileNameOffset]
      : nullptr;

    const void* inlineData = files[i].inlineDataSize
      ? &m_inlineData[currInlineDataOffset]
      : nullptr;

    if (name && name[files[i].nameLength - 1]) {
      Log::err("Archive: File name not null terminated");
      return false;
    }

    auto index = m_files.size();
    m_files.emplace_back(files[i], name, subFiles, inlineData);

    if (name && !m_lookupTable.insert({ name, index }).second) {
      Log::err("Archive: Duplicate file name: ", name);
      return false;
    }

    currSubFileIndex += files[i].subFileCount;
    currFileNameOffset += files[i].nameLength;
    currInlineDataOffset += files[i].inlineDataSize;
  }

  return true;
}




IoArchiveBuilder::IoArchiveBuilder(
        Io                            io,
        IoArchiveDesc                 desc)
: m_io    (std::move(io))
, m_desc  (std::move(desc)) {

}


IoArchiveBuilder::~IoArchiveBuilder() {

}


IoStatus IoArchiveBuilder::build(
        std::filesystem::path         path) {
  IoFile file = m_io->open(path, IoOpenMode::eCreate);

  if (!file)
    return IoStatus::eError;

  OutFileStream stream(std::move(file));

  // Prepare and write out the header
  IoArchiveHeader header = { };
  std::memcpy(header.magic, IoArchiveMagic.data(), IoArchiveMagic.size());

  // Process basic file metadata
  uint32_t totalInlineDataSize = 0;
  uint32_t totalSubFileCount = 0;

  std::vector<char> fileNames;
  std::vector<IoArchiveFileMetadata> files;
  files.reserve(m_desc.files.size());

  for (const auto& f : m_desc.files) {
    IoArchiveFileMetadata& info = files.emplace_back();
    info.nameLength = f.name.size() + 1;
    info.subFileCount = f.subFiles.size();
    info.inlineDataSize = f.inlineDataSource.size;

    size_t nameOffset = fileNames.size();
    fileNames.resize(nameOffset + info.nameLength);
    std::strncpy(&fileNames[nameOffset], f.name.c_str(), info.nameLength);

    totalInlineDataSize += info.inlineDataSize;
    totalSubFileCount += info.subFileCount;
  }

  // Now we can compute the size of the header, which is
  // important for being able to know sub-file offsets
  header.fileCount = files.size();
 
  uint64_t metadataSize = sizeof(header) + totalInlineDataSize +
    (sizeof(IoArchiveFileMetadata) * header.fileCount) +
    (sizeof(char) * fileNames.size()) +
    (sizeof(IoArchiveSubFileMetadata) * totalSubFileCount);

  // Process sub-file metadata
  std::vector<IoArchiveSubFileMetadata> subFiles;
  subFiles.reserve(totalSubFileCount);

  uint64_t subfileOffset = metadataSize;
  uint32_t subfileIndex = 0;

  for (const auto& f : m_desc.files) {
    for (const auto& s : f.subFiles) {
      uint32_t index = subfileIndex++;
      uint64_t compressedSize = s.dataSource.size;

      auto& info = subFiles.emplace_back();
      info.identifier = s.identifier;
      info.compression = s.compression;
      info.reserved = 0;
      info.offset = subfileOffset;
      info.compressedSize = compressedSize;
      info.rawSize = s.dataSource.size;

      subfileOffset += compressedSize;
    }
  }

  // Write file header
  if (!stream.write(header)
   || !stream.write(files)
   || !stream.write(fileNames)
   || !stream.write(subFiles))
    return IoStatus::eError;

  // Write inline data
  for (const auto& f : m_desc.files) {
    if (!f.inlineDataSource.size)
      continue;

    IoStatus status = processDataSource(f.inlineDataSource,
      [&stream] (const IoArchiveDataSource& source, const void* data) {
        return stream.write(data, source.size);
      });

    if (status != IoStatus::eSuccess)
      return status;
  }

  // Write subfile data
  IoStatus status = processFiles([this, &stream] (const IoArchiveSubFileDesc& subFile, const void* data, uint32_t index) {
    return writeCompressedSubfile(stream, subFile, data);
  });

  if (status != IoStatus::eSuccess)
    return status;

  return stream.flush()
    ? IoStatus::eSuccess
    : IoStatus::eError;
}


bool IoArchiveBuilder::writeCompressedSubfile(
        OutStream&                    stream,
  const IoArchiveSubFileDesc&         subfile,
  const void*                         data) {
  return stream.write(data, subfile.dataSource.size);
}


template<typename Proc>
IoStatus IoArchiveBuilder::processDataSource(
  const IoArchiveDataSource&          source,
  const Proc&                         proc) {
  return proc(source, source.memory)
    ? IoStatus::eSuccess
    : IoStatus::eError;
}


template<typename Proc>
IoStatus IoArchiveBuilder::processFiles(
  const Proc&                         proc) {
  uint32_t subfileIndex = 0;

  for (const auto& f : m_desc.files) {
    for (const auto& s : f.subFiles) {
      uint32_t index = subfileIndex++;

      IoStatus status = processDataSource(s.dataSource,
        [&proc, &s, index] (const IoArchiveDataSource& source, const void* data) {
          return proc(s, data, index);
        });

      if (status != IoStatus::eSuccess)
        return status;
    }
  }

  return IoStatus::eSuccess;
}

}
