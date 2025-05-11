#include "../util/util_assert.h"
#include "../util/util_deflate.h"
#include "../util/util_error.h"
#include "../util/util_log.h"

#include "io_archive.h"

namespace as {

static const std::array<char, 6> IoArchiveMagic = { 'A', 'S', 'F', 'I', 'L', 'E' };


IoArchiveSubFileRef IoArchiveFile::getSubFile(uint32_t index) const {
  return index < m_subFileCount
    ? IoArchiveSubFileRef(m_subFiles[index], m_archive.getPtr())
    : IoArchiveSubFileRef();
}


IoArchiveSubFileRef IoArchiveFile::findSubFile(FourCC identifier) const {
  for (uint32_t i = 0; i < m_subFileCount; i++) {
    if (m_subFiles[i].getIdentifier() == identifier)
      return IoArchiveSubFileRef(m_subFiles[i], m_archive.getPtr());
  }

  return IoArchiveSubFileRef();
}



IoArchive::IoArchive(Private, IoFile file)
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


IoArchiveFileRef IoArchive::findFile(const std::string& name) const {
  auto entry = m_lookupTable.find(name);

  if (entry == m_lookupTable.end())
    return IoArchiveFileRef();

  return IoArchiveFileRef(m_files[entry->second], shared_from_this());
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
  return decompress(
    WrMemoryView(dstData, subFile->getSize()),
    RdMemoryView(srcData, subFile->getCompressedSize()),
    subFile->getCompressionType());
}


bool IoArchive::decompress(
        WrMemoryView                  output,
        RdMemoryView                  input,
        IoArchiveCompression          compression) {
  switch (compression) {
    case IoArchiveCompression::eNone:
      if (output.getSize() != input.getSize())
        return false;

      return input.read(output.getData(), output.getSize());

    case IoArchiveCompression::eDeflate:
      return deflateDecode(output, input);

    case IoArchiveCompression::eGDeflate:
      return gdeflateDecode(output, input);
  }

  return false;
}


bool IoArchive::parseMetadata() {
  if (!m_file) {
    Log::err("Archive: File failed to open");
    return false;
  }

  RdFileStream file(m_file);
  RdStream fileStream(file);

  IoArchiveHeader fileHeader;

  if (!fileStream.read(fileHeader)) {
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

  // Metadata is compressed, so we need to decode it.
  std::vector<char> compressedMetadata(fileHeader.compressedMetadataSize);
  std::vector<char> metadataBlob(fileHeader.rawMetadataSize);

  if (!fileStream.read(compressedMetadata)) {
    Log::err("Archive: Failed to read compressed metadata");
    return false;
  }

  if (!deflateDecode(metadataBlob, compressedMetadata)) {
    Log::err("Archive: Failed to decompress metadata");
    return false;
  }

  compressedMetadata.clear();
  compressedMetadata.shrink_to_fit();

  RdMemoryView metadataView(metadataBlob);
  RdStream stream(metadataView);

  // Read all file metadata in one go. We cannot actually
  // create the file objects at this point yet, however.
  std::vector<IoArchiveFileMetadata> files(fileHeader.fileCount);

  if (!stream.read(files)) {
    Log::err("Archive: Failed to read file properties");
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
    auto& subFile = m_subFiles.emplace_back(subFiles[i], fileHeader.fileOffset);

    if (subFile.getOffsetInArchive() + subFile.getCompressedSize() > fileStream->getSize()) {
      Log::err("Archive: Sub-file out of bounds:"
        "\n  Sub file Offset: ", subFile.getOffsetInArchive(),
        "\n  Sub file size:   ", subFile.getCompressedSize(),
        "\n  Archive size:    ", fileStream->getSize());
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
    m_files.emplace_back(*this, files[i], name, subFiles, inlineData);

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




IoArchiveCollection::IoArchiveCollection(Io io)
: m_io(std::move(io)) {

}


IoArchiveCollection::~IoArchiveCollection() {

}


void IoArchiveCollection::addHandler(FourCC type, IoArchiveFileHandler&& handler) {
  std::unique_lock lock(m_mutex);
  m_handlers.insert_or_assign(type, std::move(handler));
}


IoRequest IoArchiveCollection::loadArchive(IoFile file) {
  auto archive = IoArchive::fromFile(std::move(file));

  if (!(*archive))
    return nullptr;

  std::vector<IoArchiveFileRef> files;
  files.reserve(archive->getFileCount());

  { std::unique_lock lock(m_mutex);
    for (uint32_t i = 0; i < archive->getFileCount(); i++) {
      auto file = archive->getFile(i);
      auto result = m_files.insert(std::make_pair(std::string(file->getName()), file));

      if (result.second)
        files.push_back(std::move(file));
      else
        Log::warn("Archive: File name not unique: ", file->getName());
    }
  }

  IoRequest request = m_io->createRequest();

  { std::shared_lock lock(m_mutex);

    for (auto f : files) {
      auto handler = m_handlers.find(f->getType());

      if (handler != m_handlers.end())
        handler->second(request, f);
    }
  }

  if (!m_io->submit(request))
    return nullptr;

  return request;
}


IoArchiveFileRef IoArchiveCollection::findFile(const char* name) {
  std::shared_lock lock(m_mutex);
  auto entry = m_files.find(name);

  if (entry == m_files.end())
    return IoArchiveFileRef();

  return entry->second;
}

}
