#include "../../src/util/util_deflate.h"

#include "basic.h"

namespace as::archive {

BasicBuildJob::BasicBuildJob(
          Environment                   env,
          FileDesc&&                    desc)
: m_env(std::move(env))
, m_desc(std::move(desc)) {

}


BasicBuildJob::~BasicBuildJob() {
  m_env.jobs->wait(m_job);
}


std::pair<BuildResult, BuildProgress> BasicBuildJob::getProgress() {
  BuildResult status = m_result.load(std::memory_order_acquire);

  BuildProgress prog = { };
  prog.addJob(m_job);

  if (status == BuildResult::eSuccess && prog.itemsCompleted < m_desc.subFiles.size())
    status = BuildResult::eInProgress;

  return std::make_pair(status, prog);
}


std::pair<BuildResult, ArchiveFile> BasicBuildJob::getFileInfo() {
  m_env.jobs->wait(m_job);

  BuildResult status = m_result.load(std::memory_order_acquire);

  if (int32_t(status) < 0)
    return std::make_pair(status, ArchiveFile());

  std::pair<BuildResult, ArchiveFile> result = { };
  result.first = BuildResult::eSuccess;
  result.second = ArchiveFile(m_desc.type, m_desc.name);
  result.second.setInlineData(std::move(m_desc.inlineData));

  for (auto& subFile : m_desc.subFiles) {
    result.second.addSubFile(subFile.identifier,
      subFile.compression, subFile.rawSize, std::move(subFile.compressedData));
  }

  return result;
}


void BasicBuildJob::dispatchJobs() {
  if (m_desc.subFiles.empty())
    return;

  m_job = m_env.jobs->create<BatchJob>([this] (uint32_t index) {
    ArchiveSubFile& subFile = m_desc.subFiles[index];
    subFile.rawSize = subFile.compressedData.size();

    ArchiveData rawData = std::move(subFile.compressedData);

    switch (subFile.compression) {
      case IoArchiveCompression::eNone: {
        subFile.compressedData = std::move(rawData);
      } break;

      case IoArchiveCompression::eDeflate: {
        if (!deflateEncode(Lwrap<WrVectorStream>(subFile.compressedData), rawData)) {
          Log::err("Failed to compress binary");
          m_result.store(BuildResult::eInvalidInput, std::memory_order_acquire);
        }
      } break;

      case IoArchiveCompression::eGDeflate: {
        if (!gdeflateEncode(Lwrap<WrVectorStream>(subFile.compressedData), rawData)) {
          Log::err("Failed to compress binary");
          m_result.store(BuildResult::eInvalidInput, std::memory_order_acquire);
        }
      } break;
    }
  }, uint32_t(m_desc.subFiles.size()), 1u);

  m_env.jobs->dispatch(m_job);
}


void BasicBuildJob::abort() {
  BuildResult expected = BuildResult::eSuccess;

  m_result.compare_exchange_strong(expected,
    BuildResult::eAborted, std::memory_order_release);
}

}
