#include "merge.h"

namespace as::archive {

MergeBuildJob::MergeBuildJob(
        Environment                   env,
        std::shared_ptr<IoArchive>    archive,
        uint32_t                      fileId)
: m_env         (std::move(env))
, m_archive     (std::move(archive))
, m_archiveFile (m_archive->getFile(fileId)) {

}


MergeBuildJob::~MergeBuildJob() {

}


std::pair<BuildResult, ArchiveFile> MergeBuildJob::build() {
  std::pair<BuildResult, ArchiveFile> result = { };
  result.first = BuildResult::eSuccess;
  result.second = ArchiveFile(
    m_archiveFile->getType(),
    m_archiveFile->getName());

  // Copy inline data directly
  RdMemoryView srcInlineData = m_archiveFile->getInlineData();
  ArchiveData inlineData(srcInlineData.getSize());
  std::memcpy(inlineData.data(), srcInlineData.getData(), inlineData.size());

  result.second.setInlineData(std::move(inlineData));

  // Process sub-files one by one
  for (uint32_t i = 0; i < m_archiveFile->getSubFileCount(); i++) {
    auto subFile = m_archiveFile->getSubFile(i);

    ArchiveData compressedData(subFile->getCompressedSize());

    // Use an IO request in order to circumvent thread
    // safety issues with synchronous file I/O.
    IoRequest ioRequest = m_env.io->createRequest();
    m_archive->readCompressed(ioRequest, subFile.get(), compressedData.data());

    if (!m_env.io->submit(ioRequest)) {
      result.first = BuildResult::eIoError;
      return result;
    }

    if (ioRequest->wait() != IoStatus::eSuccess) {
      result.first = BuildResult::eIoError;
      return result;
    }

    result.second.addSubFile(
      subFile->getIdentifier(),
      subFile->getCompressionType(),
      subFile->getSize(),
      std::move(compressedData));
  }

  return result;
}


}
