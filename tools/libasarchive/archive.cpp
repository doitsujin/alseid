#include "../../src/util/util_deflate.h"

#include "archive.h"

namespace as::archive {

ArchiveFile::ArchiveFile() {

}


ArchiveFile::ArchiveFile(
        FourCC                        type,
        std::string                   name)
: m_type  (type)
, m_name  (std::move(name)) {

}


bool ArchiveFile::setInlineData(
          ArchiveData&&                 data) {
  if (!m_inlineData.empty())
    return false;

  m_inlineData = std::move(data);
  return true;
}


bool ArchiveFile::addSubFile(
          FourCC                        identifier,
          IoArchiveCompression          compression,
          size_t                        rawSize,
          ArchiveData&&                 compressedData) {
  if (compression == IoArchiveCompression::eNone && rawSize != compressedData.size())
    return false;

  auto& item = m_subFiles.emplace_back();
  item.identifier = identifier;
  item.compression = compression;
  item.rawSize = rawSize;
  item.compressedData = std::move(compressedData);

  return true;
}


const char* ArchiveFile::getFileName() const {
  return m_name.c_str();
}


void ArchiveFile::getFileMetadata(
        IoArchiveFileMetadata*        metadata,
  const void**                        inlineData) const {
  if (metadata) {
    metadata->type = m_type;
    metadata->nameLength = m_name.size() + 1;
    metadata->subFileCount = m_subFiles.size();
    metadata->inlineDataSize = m_inlineData.size();
  }

  if (inlineData)
    *inlineData = m_inlineData.data();
}


void ArchiveFile::getSubFileMetadata(
        uint64_t&                     dataOffset,
        size_t                        entryCount,
        IoArchiveSubFileMetadata*     metadata,
  const void**                        subFileData) const {
  size_t entry = 0;

  for (const auto& subFile : m_subFiles) {
    if (entry >= entryCount)
      return;

    if (metadata) {
      auto subFileInfo = &metadata[entry];
      subFileInfo->identifier = subFile.identifier;
      subFileInfo->compression = subFile.compression;
      subFileInfo->reserved = 0;
      subFileInfo->offset = dataOffset;
      subFileInfo->compressedSize = subFile.compressedData.size();
      subFileInfo->rawSize = subFile.rawSize;

      dataOffset += subFileInfo->compressedSize;
    }

    if (subFileData)
      subFileData[entry] = subFile.compressedData.data();

    entry += 1;
  }
}




BuildJob::~BuildJob() {

}




ArchiveStreams::ArchiveStreams(
        Environment                   environment)
: m_environment(std::move(environment)) {

}


ArchiveStreams::~ArchiveStreams() {

}


void ArchiveStreams::addFile(
    const ArchiveFile&                  file) {
  auto& fileMetadata = m_fileMetadata.emplace_back();
  const void* fileInlineData = nullptr;

  file.getFileMetadata(&fileMetadata, &fileInlineData);
  m_fileInlineData.push_back(fileInlineData);

  size_t fileNameOffset = m_fileNames.size();
  m_fileNames.resize(fileNameOffset + fileMetadata.nameLength);
  std::memcpy(&m_fileNames[fileNameOffset], file.getFileName(), fileMetadata.nameLength - 1);

  size_t subFileIndex = m_subFileMetadata.size();
  m_subFileMetadata.resize(subFileIndex + fileMetadata.subFileCount);
  m_subFileData.resize(subFileIndex + fileMetadata.subFileCount);

  if (fileMetadata.subFileCount) {
    file.getSubFileMetadata(m_subFileDataOffset, fileMetadata.subFileCount,
      &m_subFileMetadata[subFileIndex], &m_subFileData[subFileIndex]);
  }
}


BuildResult ArchiveStreams::write(
        std::filesystem::path         path) const {
  WrFileStream file(m_environment.io->open(path, IoOpenMode::eCreate));

  if (!file)
    return BuildResult::eIoError;

  WrStream stream(file);

  // Accumulate metadata, including inline file
  // data, in a single uncompressed blob
  std::vector<char> rawMetadata;

  if (!getMetadataBlob(Lwrap<WrVectorStream>(rawMetadata)))
    return BuildResult::eIoError;

  // Compress the metadata blob
  std::vector<char> compressedMetadata;

  if (!deflateEncode(Lwrap<WrVectorStream>(compressedMetadata), rawMetadata))
    return BuildResult::eIoError;

  // Write file header
  IoArchiveHeader header = { };
  std::memcpy(header.magic, "ASFILE", sizeof(header.magic));
  header.fileCount = m_fileMetadata.size();
  header.fileOffset = sizeof(header) + compressedMetadata.size();
  header.compressedMetadataSize = compressedMetadata.size();
  header.rawMetadataSize = rawMetadata.size();

  if (!stream.write(header)
   || !stream.write(compressedMetadata))
    return BuildResult::eIoError;

  // Append sub-file data in order
  for (size_t i = 0; i < m_subFileMetadata.size(); i++) {
    if (!m_subFileMetadata[i].compressedSize)
      continue;

    if (!stream.write(m_subFileData[i], m_subFileMetadata[i].compressedSize))
      return BuildResult::eIoError;
  }

  if (!stream.flush())
    return BuildResult::eIoError;

  return BuildResult::eSuccess;
}


bool ArchiveStreams::getMetadataBlob(
        WrVectorStream&               stream) const {
  WrStream metadataWriter(stream);

  // Write basic file metadata
  if (!metadataWriter.write(m_fileMetadata)
   || !metadataWriter.write(m_fileNames)
   || !metadataWriter.write(m_subFileMetadata))
    return false;

  // Write inline data
  for (size_t i = 0; i < m_fileMetadata.size(); i++) {
    if (!m_fileMetadata[i].inlineDataSize)
      continue;

    if (!metadataWriter.write(m_fileInlineData[i], m_fileMetadata[i].inlineDataSize))
      return false;
  }

  return metadataWriter.flush();
}




ArchiveBuilder::ArchiveBuilder(
        Environment                   environment)
: m_environment(std::move(environment)) {

}


ArchiveBuilder::~ArchiveBuilder() {
  std::unique_lock lock(m_mutex);

  while (!m_buildJobs.empty()) {
    m_environment.jobs->wait(m_buildJobs.front().job);
    m_buildJobs.pop();
  }
}


void ArchiveBuilder::addBuildJob(
        std::shared_ptr<BuildJob>     job) {
  std::unique_lock lock(m_mutex);

  auto& item = m_buildJobs.emplace();
  item.job = m_environment.jobs->dispatch(m_environment.jobs->create<SimpleJob>([this,
    cJob  = std::move(job),
    cItem = &item
  ] {
    if (m_status.load() != BuildResult::eSuccess) {
      cItem->status.first = BuildResult::eAborted;
      return;
    }

    cItem->status = cJob->build();

    if (cItem->status.first != BuildResult::eSuccess) {
      auto expected = BuildResult::eSuccess;
      m_status.compare_exchange_strong(expected, cItem->status.first, std::memory_order_release, std::memory_order_relaxed);
    }
  }));
}


BuildResult ArchiveBuilder::build(
        std::filesystem::path         path) {
  ArchiveStreams streams(m_environment);

  std::unique_lock lock(m_mutex);

  // The file objects contain the actual data blobs, so we
  // must keep them alive at constant memory locations
  std::list<ArchiveFile> files;

  while (!m_buildJobs.empty()) {
    auto& job = m_buildJobs.front();
    m_environment.jobs->wait(job.job);

    if (job.status.first != BuildResult::eSuccess) {
      Log::err("error");
      return job.status.first;
    }

    streams.addFile(files.emplace_back(std::move(job.status.second)));
    m_buildJobs.pop();
  }

  return streams.write(path);
}

}
