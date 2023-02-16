#include "../util/util_assert.h"
#include "../util/util_error.h"
#include "../util/util_log.h"

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
    m_decoders.clear();
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
        void*                         data) {
  if (subFile->getCompressionType() == IoArchiveCompression::eDefault && !subFile->isHuffmanCompressed())
    return readCompressed(subFile, data);

  std::vector<char> compressed(subFile->getCompressedSize());
  IoStatus status = readCompressed(subFile, compressed.data());

  if (status != IoStatus::eSuccess)
    return status;

  bool success = decompress(subFile, data, subFile->getSize(),
    compressed.data(), compressed.size());
  return success ? IoStatus::eSuccess : IoStatus::eError;
}


IoStatus IoArchive::readCompressed(
  const IoArchiveSubFile*             subFile,
        void*                         data) {
  return m_file->read(
    subFile->getOffsetInArchive(),
    subFile->getCompressedSize(),
    data);
}


void IoArchive::requestRead(
  const IoRequest&                    request,
  const IoArchiveSubFile*             subFile,
        void*                         data) {
  if (subFile->getCompressionType() == IoArchiveCompression::eDefault && !subFile->isHuffmanCompressed()) {
    requestReadCompressed(request, subFile, data);
    return;
  }

  // We need to allocate temporary memory that is shared between threads
  auto memory = std::make_shared<char[]>(subFile->getCompressedSize());

  auto callback = [this,
    cSubFile  = subFile,
    cMemory   = memory,
    cData     = data
  ] (const void* compressed, size_t compressedSize) {
    bool success = decompress(cSubFile, cData, cSubFile->getSize(),
      cMemory.get(), cSubFile->getCompressedSize());
    return success ? IoStatus::eSuccess : IoStatus::eError;
  };

  request->read(m_file,
    subFile->getOffsetInArchive(),
    subFile->getCompressedSize(),
    memory.get(), std::move(callback));
}


void IoArchive::requestReadCompressed(
  const IoRequest&                    request,
  const IoArchiveSubFile*             subFile,
        void*                         data) {
  request->read(m_file,
    subFile->getOffsetInArchive(),
    subFile->getCompressedSize(),
    data);
}


bool IoArchive::decompress(
  const IoArchiveSubFile*             subFile,
        void*                         dstData,
        size_t                        dstSize,
  const void*                         srcData,
        size_t                        srcSize) const {
  if (!subFile->isHuffmanCompressed()) {
    if (srcSize != dstSize)
      return false;

    // Callers should avoid this whenever possible
    std::memcpy(dstData, srcData, dstSize);
    return true;
  }

  // Currently, no further compression types are defined
  if (subFile->getCompressionType() != IoArchiveCompression::eDefault)
    return false;

  // Decode huffman-compressed binary using the provided decoder
  auto decoder = getDecoder(subFile->getDecodingTableIndex());

  if (!decoder)
    return false;

  InMemoryStream reader(srcData, srcSize);
  BitstreamReader bitstream(reader);

  OutMemoryStream writer(dstData, dstSize);
  return decoder->decode(writer, bitstream, dstSize);
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

  // Read decoding table metadata and then all decoding tables in one go.
  std::vector<IoArchiveDecodingTableMetadata> decodingTables(fileHeader.decodingTableCount);

  if (!stream.read(decodingTables)) {
    Log::err("Archive: Failed to read decoding table metadata");
    return false;
  }

  size_t totalDecodingTableSize = 0;

  for (const auto& t : decodingTables)
    totalDecodingTableSize += t.tableSize;

  // Copy all decoding tables into memory to reduce the number of I/O operations.
  std::vector<char> decodingTableData(totalDecodingTableSize);

  if (!stream.read(decodingTableData)) {
    Log::err("Archive: Failed to read decoding tables (", totalDecodingTableSize, " bytes)");
    return false;
  }

  // Read all inline data directly into the array
  m_inlineData.resize(totalInlineDataSize);

  if (!stream.read(m_inlineData)) {
    Log::err("Archive: Failed to read inline data (", totalInlineDataSize, " bytes)");
    return false;
  }

  // Now that everything is loaded, create the Huffman
  // decoder objects first.
  m_decoders.resize(fileHeader.decodingTableCount);
  size_t decodingTableOffset = 0;

  for (uint32_t i = 0; i < fileHeader.decodingTableCount; i++) {
    InMemoryStream reader(&decodingTableData[decodingTableOffset], decodingTables[i].tableSize);
    BitstreamReader bitstream(reader);

    if (!m_decoders[i].read(bitstream)) {
      Log::err("Archive: Failed to parse decoding table ", i);
      return false;
    }

    decodingTableOffset += decodingTables[i].tableSize;
  }

  // Initialize and validate sub-file objects
  m_subFiles.reserve(totalSubFileCount);

  for (size_t i = 0; i < totalSubFileCount; i++) {
    auto& subFile = m_subFiles.emplace_back(subFiles[i]);

    if (subFile.isHuffmanCompressed()
     && subFile.getDecodingTableIndex() >= fileHeader.decodingTableCount) {
      Log::err("Archive: Invalid decoding table index: ", subFile.getDecodingTableIndex());
      return false;
    }

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

  // Create Huffman encoder/decoder objects
  IoStatus status = buildHuffmanObjects();

  if (status != IoStatus::eSuccess)
    return status;

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

  // Process decoding tables
  std::vector<IoArchiveDecodingTableMetadata> decodingTables;
  decodingTables.reserve(m_huffmanObjects.size());

  OutVectorStream decodingTableStream;

  for (const auto& o : m_huffmanObjects) {
    size_t tableSize = o.decoder.computeSize();

    IoArchiveDecodingTableMetadata& info = decodingTables.emplace_back();
    info.tableSize = tableSize;

    BitstreamWriter bitstream(decodingTableStream);
    o.decoder.write(bitstream);
    bitstream.flush();
  }

  std::vector<char> decodingTableData = std::move(decodingTableStream).getData();

  // Now we can compute the size of the header, which is
  // important for being able to know sub-file offsets
  header.fileCount = files.size();
  header.decodingTableCount = decodingTables.size();
  header.reserved = 0;
 
  uint64_t metadataSize = sizeof(header) + totalInlineDataSize +
    (sizeof(IoArchiveFileMetadata) * header.fileCount) +
    (sizeof(char) * fileNames.size()) +
    (sizeof(IoArchiveSubFileMetadata) * totalSubFileCount) +
    (sizeof(IoArchiveDecodingTableMetadata) * header.decodingTableCount) +
    (sizeof(char) * decodingTableData.size());

  // Process sub-file metadata
  std::vector<IoArchiveSubFileMetadata> subFiles;
  subFiles.reserve(totalSubFileCount);

  uint64_t subfileOffset = metadataSize;
  uint32_t subfileIndex = 0;

  for (const auto& f : m_desc.files) {
    for (const auto& s : f.subFiles) {
      uint32_t index = subfileIndex++;
      uint64_t compressedSize = computeCompressedSize(s, index);

      auto& info = subFiles.emplace_back();
      info.identifier = s.identifier;
      info.compression = s.compression;
      info.decodingTable = s.decodingTable;
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
   || !stream.write(subFiles)
   || !stream.write(decodingTables)
   || !stream.write(decodingTableData))
    return IoStatus::eError;

  // Write inline data
  for (const auto& f : m_desc.files) {
    if (!f.inlineDataSource.size)
      continue;

    status = processDataSource(f.inlineDataSource,
      [&stream] (const IoArchiveDataSource& source, const void* data) {
        return stream.write(data, source.size);
      });

    if (status != IoStatus::eSuccess)
      return status;
  }

  // Write subfile data
  status = processFiles([this, &stream] (const IoArchiveSubFileDesc& subFile, const void* data, uint32_t index) {
    return writeCompressedSubfile(stream, subFile, data);
  });

  if (status != IoStatus::eSuccess)
    return status;

  return stream.flush()
    ? IoStatus::eSuccess
    : IoStatus::eError;
}


IoStatus IoArchiveBuilder::buildHuffmanObjects() {
  size_t subfileCount = 0;

  for (const auto& f : m_desc.files)
    subfileCount += f.subFiles.size();

  m_huffmanCounters.resize(subfileCount);

  IoStatus status = processFilesIf(
    [this] (const IoArchiveSubFileDesc& subFile) {
      return subFile.decodingTable != 0xFFFF;
    }, [this] (const IoArchiveSubFileDesc& subFile, const void* data, uint32_t index) {
      uint16_t table = subFile.decodingTable;

      if (table >= m_huffmanObjects.size())
        m_huffmanObjects.resize(table + 1);

      auto& ctr = m_huffmanCounters.at(index);
      ctr.add(data, subFile.dataSource.size);

      m_huffmanObjects[table].counter.accumulate(ctr);
      return true;
    });

  if (status != IoStatus::eSuccess)
    return status;

  // Build decoders and encoders
  for (auto& o : m_huffmanObjects) {
    HuffmanTrie trie(o.counter);
    o.encoder = trie.createEncoder();
    o.decoder = trie.createDecoder();
  }

  return IoStatus::eSuccess;
}


uint64_t IoArchiveBuilder::computeCompressedSize(
  const IoArchiveSubFileDesc&         subfile,
        uint32_t                      subfileIndex) const {
  if (subfile.decodingTable == 0xFFFF)
    return subfile.dataSource.size;

  const auto& encoder = m_huffmanObjects.at(subfile.decodingTable).encoder;
  return encoder.computeEncodedSize(m_huffmanCounters.at(subfileIndex));
}


bool IoArchiveBuilder::writeCompressedSubfile(
        OutStream&                    stream,
  const IoArchiveSubFileDesc&         subfile,
  const void*                         data) {
  if (subfile.decodingTable == 0xFFFF)
    return stream.write(data, subfile.dataSource.size);

  BitstreamWriter bitstream(stream);

  const auto& encoder = m_huffmanObjects.at(subfile.decodingTable).encoder;
  return encoder.encode(bitstream, data, subfile.dataSource.size);
}


template<typename Proc>
IoStatus IoArchiveBuilder::processDataSource(
  const IoArchiveDataSource&          source,
  const Proc&                         proc) {
  if (source.memory) {
    return proc(source, source.memory)
      ? IoStatus::eSuccess
      : IoStatus::eError;
  } else {
    IoFile file = m_io->open(source.file, IoOpenMode::eRead);

    if (!file)
      return IoStatus::eError;

    std::vector<char> data(source.size);
    IoStatus status = file->read(source.offset, source.size, data.data());

    if (status != IoStatus::eSuccess)
      return status;

    return proc(source, data.data())
      ? IoStatus::eSuccess
      : IoStatus::eError;
  }
}


template<typename Pred, typename Proc>
IoStatus IoArchiveBuilder::processFilesIf(
  const Pred&                         pred,
  const Proc&                         proc) {
  uint32_t subfileIndex = 0;

  for (const auto& f : m_desc.files) {
    for (const auto& s : f.subFiles) {
      uint32_t index = subfileIndex++;

      if (!pred(s))
        continue;

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


template<typename Proc>
IoStatus IoArchiveBuilder::processFiles(
  const Proc&                         proc) {
  return processFilesIf([] (const IoArchiveSubFileDesc&) {
    return true;
  }, proc);
}

}
