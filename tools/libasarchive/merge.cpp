#include "merge.h"

namespace asarchive {

MergeBuildJob::MergeBuildJob(
        Environment                   env,
        std::shared_ptr<IoArchive>    archive,
        uint32_t                      fileId)
: m_env         (std::move(env))
, m_archive     (std::move(archive))
, m_archiveFile (m_archive->getFile(fileId)) {

}


MergeBuildJob::~MergeBuildJob() {
  m_env.jobs->wait(m_job);
}


std::pair<BuildResult, BuildProgress> MergeBuildJob::getProgress() {
  BuildResult status = m_result.load(std::memory_order_acquire);

  BuildProgress prog;
  prog.itemsCompleted = (m_job && m_job->isDone()) ? 1 : 0;
  prog.itemsTotal = 1;

  if (status == BuildResult::eSuccess && !prog.itemsCompleted)
    status = BuildResult::eInProgress;

  return std::make_pair(status, prog);
}


std::pair<BuildResult, ArchiveFile> MergeBuildJob::getFileInfo() {
  m_env.jobs->wait(m_job);

  BuildResult status = m_result.load(std::memory_order_acquire);
  return std::make_pair(status, std::move(m_fileInfo));
}


void MergeBuildJob::dispatchJobs() {
  m_job = m_env.jobs->create<SimpleJob>([this] {
    BuildResult expected = m_result.load(std::memory_order_acquire);

    if (expected == BuildResult::eSuccess) {
      BuildResult result = processFile();

      m_result.compare_exchange_strong(expected,
        result, std::memory_order_release);
    }
  });

  m_env.jobs->dispatch(m_job);
}


void MergeBuildJob::abort() {
  BuildResult expected = BuildResult::eSuccess;

  m_result.compare_exchange_strong(expected,
    BuildResult::eAborted, std::memory_order_release);
}


BuildResult MergeBuildJob::processFile() {
  m_fileInfo = ArchiveFile(
    m_archiveFile->getType(),
    m_archiveFile->getName());

  // Copy inline data directly
  RdMemoryView srcInlineData = m_archiveFile->getInlineData();
  ArchiveData inlineData(srcInlineData.getSize());
  std::memcpy(inlineData.data(), srcInlineData.getData(), inlineData.size());

  m_fileInfo.setInlineData(std::move(inlineData));

  // Process sub-files one by one
  for (uint32_t i = 0; i < m_archiveFile->getSubFileCount(); i++) {
    const IoArchiveSubFile* subFile = m_archiveFile->getSubFile(i);

    ArchiveData compressedData(subFile->getCompressedSize());

    // Use an IO request in order to circumvent thread
    // safety issues with synchronous file I/O.
    IoRequest ioRequest = m_env.io->createRequest();
    m_archive->readCompressed(ioRequest, subFile, compressedData.data());

    if (!m_env.io->submit(ioRequest))
      return BuildResult::eIoError;

    if (ioRequest->wait() != IoStatus::eSuccess)
      return BuildResult::eIoError;

    m_fileInfo.addSubFile(
      subFile->getIdentifier(),
      subFile->getCompressionType(),
      subFile->getSize(),
      std::move(compressedData));
  }

  return BuildResult::eSuccess;
}


}
