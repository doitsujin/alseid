#include "../../src/util/util_deflate.h"

#include "shader.h"

namespace as::archive {

ShaderBuildJob::ShaderBuildJob(
        Environment                   env,
  const ShaderDesc&                   desc,
        std::filesystem::path         input)
: m_env         (std::move(env))
, m_desc        (desc)
, m_input       (std::move(input)) {

}


ShaderBuildJob::~ShaderBuildJob() {
  m_env.jobs->wait(m_job);
}


std::pair<BuildResult, BuildProgress> ShaderBuildJob::getProgress() {
  BuildResult status = m_result.load(std::memory_order_acquire);

  BuildProgress prog = { };
  prog.addJob(m_job);

  if (status == BuildResult::eSuccess && !prog.itemsCompleted)
    status = BuildResult::eInProgress;

  return std::make_pair(status, prog);
}


std::pair<BuildResult, ArchiveFile> ShaderBuildJob::getFileInfo() {
  m_env.jobs->wait(m_job);

  BuildResult status = m_result.load(std::memory_order_acquire);

  if (int32_t(status) < 0)
    return std::make_pair(status, ArchiveFile());

  std::pair<BuildResult, ArchiveFile> result;
  result.first = BuildResult::eSuccess;
  result.second = ArchiveFile(FourCC('S', 'H', 'D', 'R'), m_input.stem());
  result.second.setInlineData(std::move(m_shaderDesc));
  result.second.addSubFile(FourCC('S', 'P', 'I', 'R'),
    IoArchiveCompression::eDeflate,
    m_rawSize, std::move(m_shaderData));

  return result;
}


void ShaderBuildJob::dispatchJobs() {
  m_job = m_env.jobs->create<SimpleJob>([this] {
    BuildResult expected = m_result.load(std::memory_order_acquire);

    if (expected == BuildResult::eSuccess) {
      BuildResult result = processShader();

      m_result.compare_exchange_strong(expected,
        result, std::memory_order_release);
    }
  });

  m_env.jobs->dispatch(m_job);
}


void ShaderBuildJob::abort() {
  BuildResult expected = BuildResult::eSuccess;

  m_result.compare_exchange_strong(expected,
    BuildResult::eAborted, std::memory_order_release);
}


BuildResult ShaderBuildJob::processShader() {
  RdFileStream inFile(m_env.io->open(m_input, IoOpenMode::eRead));

  if (!inFile) {
    Log::err("Failed to open ", m_input);
    return BuildResult::eIoError;
  }

  std::vector<char> spv(inFile.getSize());

  if (!RdStream(inFile).read(spv)) {
    Log::err("Failed to read ", m_input);
    return BuildResult::eIoError;
  }

  // Reflect shader and generate metadata blob
  auto shaderDesc = spirvReflectBinary(spv.size(), spv.data());

  if (!shaderDesc) {
    Log::err("Failed to reflect SPIR-V binary");
    return BuildResult::eInvalidInput;
  }

  if (!shaderDesc->serialize(Lwrap<WrVectorStream>(m_shaderDesc))) {
    Log::err("Failed to serialize shader description");
    return BuildResult::eInvalidInput;
  }

  // Encode SPIR-V binary
  ArchiveData shaderBinaryData;

  if (!spirvEncodeBinary(Lwrap<WrVectorStream>(shaderBinaryData), spv)) {
    Log::err("Failed to encode SPIR-V binary");
    return BuildResult::eInvalidInput;
  }

  // Compress binary further with deflate
  if (!deflateEncode(Lwrap<WrVectorStream>(m_shaderData), shaderBinaryData)) {
    Log::err("Failed to compress SPIR-V binary");
    return BuildResult::eInvalidInput;
  }

  m_rawSize = shaderBinaryData.size();
  return BuildResult::eSuccess;
}


}