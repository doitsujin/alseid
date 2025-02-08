#include <unordered_map>

#include "../../src/gfx/gfx.h"

#include "../../src/util/util_deflate.h"

#include "geometry.h"

namespace as::archive {

GeometryBuildJob::GeometryBuildJob(
        Environment                   env,
  const GeometryDesc&                 desc,
        std::filesystem::path         input)
: m_env     (std::move(env))
, m_desc    (desc)
, m_input   (input) {

}


GeometryBuildJob::~GeometryBuildJob() {
  synchronizeJobs();
}


std::pair<BuildResult, BuildProgress> GeometryBuildJob::getProgress() {
  BuildResult status = m_result.load(std::memory_order_acquire);

  BuildProgress prog = { };
  prog.addJob(m_ioJob);

  if (m_ioJob && m_ioJob->isDone()) {
    prog.addJob(m_convertJob);

    if (m_convertJob && m_convertJob->isDone())
      prog.addJob(m_compressJob);
  }

  if (status == BuildResult::eSuccess && !prog.itemsCompleted)
    status = BuildResult::eInProgress;

  return std::make_pair(status, prog);
}


std::pair<BuildResult, ArchiveFile> GeometryBuildJob::getFileInfo() {
  synchronizeJobs();

  BuildResult status = m_result.load();

  if (status != BuildResult::eSuccess)
    return std::make_pair(status, ArchiveFile());

  ArchiveData metadata;

  if (!m_converter->getGeometry()->serialize(Lwrap<WrVectorStream>(metadata)))
    return std::make_pair(BuildResult::eIoError, ArchiveFile());

  std::pair<BuildResult, ArchiveFile> result;
  result.first = status;
  result.second = ArchiveFile(FourCC('G', 'E', 'O', 'M'), m_desc.name);
  result.second.setInlineData(std::move(metadata));

  for (size_t i = 0; i < m_buffers.size(); i++) {
    // Let the first sub file be the metadata buffer and first
    // data buffer, enumerate buffers in order afterwards. This
    // way, apps can access sub files by index.
    FourCC identifier('M', 'E', 'T', 'A');

    if (i)
      identifier = FourCC(strcat("DAT", std::hex, i));

    result.second.addSubFile(identifier, IoArchiveCompression::eGDeflate,
      m_rawSizes[i], std::move(m_buffers[i]));
  }

  return result;
}


void GeometryBuildJob::dispatchJobs() {
  m_ioJob = m_env.jobs->create<SimpleJob>([this] {
    BuildResult expected = m_result.load(std::memory_order_acquire);

    if (expected == BuildResult::eSuccess) {
      BuildResult result = runIoJob();

      m_result.compare_exchange_strong(expected,
        result, std::memory_order_release);
    }
  });

  m_env.jobs->dispatch(m_ioJob);
}


void GeometryBuildJob::abort() {
  BuildResult expected = BuildResult::eSuccess;

  m_result.compare_exchange_strong(expected,
    BuildResult::eAborted, std::memory_order_release);
}


BuildResult GeometryBuildJob::runIoJob() {
  std::shared_ptr<Gltf> gltf;

  try {
    gltf = std::make_shared<Gltf>(m_env.io, m_input);
  } catch (const Error& e) {
    return BuildResult::eIoError;
  }

  // Create and dispatch mesh converter
  m_converter = std::make_shared<GltfConverter>(
    m_env.jobs, std::move(gltf), m_desc.layoutMap);
  m_convertJob = m_converter->dispatchConvert();

  // Run compression. We're lazy and don't paralellize this
  // since we cannot know the buffer count in advance.
  m_compressJob = m_env.jobs->create<SimpleJob>(
    [this] { runCompressJob(); });

  m_env.jobs->dispatch(m_compressJob, m_convertJob);
  return BuildResult::eSuccess;
}


void GeometryBuildJob::runCompressJob() {
  if (m_result.load(std::memory_order_relaxed) != BuildResult::eSuccess)
    return;

  m_buffers.resize(m_converter->getGeometry()->info.bufferCount);
  m_rawSizes.resize(m_buffers.size());

  for (size_t i = 0; i < m_buffers.size(); i++) {
    auto srcBuffer = m_converter->getBuffer(i);
    m_rawSizes.at(i) = srcBuffer.getSize();

    if (!(gdeflateEncode(Lwrap<WrVectorStream>(m_buffers.at(i)), srcBuffer)))
      m_result.store(BuildResult::eIoError);
  }
}


void GeometryBuildJob::synchronizeJobs() {
  m_env.jobs->wait(m_ioJob);
  m_env.jobs->wait(m_convertJob);
  m_env.jobs->wait(m_compressJob);
}

}
