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

}


std::pair<BuildResult, ArchiveFile> BasicBuildJob::build() {
  std::pair<BuildResult, ArchiveFile> result = { };
  result.first = BuildResult::eSuccess;

  if (!m_desc.subFiles.empty()) {
    m_env.jobs->wait(m_env.jobs->dispatch(m_env.jobs->create<BatchJob>([this, &result] (uint32_t index) {
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
            result.first = BuildResult::eInvalidInput;
          }
        } break;

        case IoArchiveCompression::eGDeflate: {
          if (!gdeflateEncode(Lwrap<WrVectorStream>(subFile.compressedData), rawData)) {
            Log::err("Failed to compress binary");
            result.first = BuildResult::eInvalidInput;
          }
        } break;
      }
    }, uint32_t(m_desc.subFiles.size()), 1u)));
  }

  result.second = ArchiveFile(m_desc.type, m_desc.name);
  result.second.setInlineData(std::move(m_desc.inlineData));

  for (auto& subFile : m_desc.subFiles) {
    result.second.addSubFile(subFile.identifier,
      subFile.compression, subFile.rawSize, std::move(subFile.compressedData));
  }

  return result;
}

}
