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

}


std::pair<BuildResult, ArchiveFile> ShaderBuildJob::build() {
  std::pair<BuildResult, ArchiveFile> result;
  result.first = BuildResult::eSuccess;

  RdFileStream inFile(m_env.io->open(m_input, IoOpenMode::eRead));

  if (!inFile) {
    Log::err("Failed to open ", m_input);

    result.first = BuildResult::eIoError;
    return result;
  }

  std::vector<char> spv(inFile.getSize());

  if (!RdStream(inFile).read(spv)) {
    Log::err("Failed to read ", m_input);

    result.first = BuildResult::eIoError;
    return result;
  }

  // Reflect shader and generate metadata blob
  auto shaderDesc = spirvReflectBinary(spv.size(), spv.data());

  if (!shaderDesc) {
    Log::err("Failed to reflect SPIR-V binary");

    result.first = BuildResult::eInvalidInput;
    return result;
  }

  ArchiveData shaderMetadata;

  if (!shaderDesc->serialize(Lwrap<WrVectorStream>(shaderMetadata))) {
    Log::err("Failed to serialize shader description");

    result.first = BuildResult::eInvalidInput;
    return result;
  }

  // Encode SPIR-V binary
  ArchiveData shaderBinaryData;

  if (!spirvEncodeBinary(Lwrap<WrVectorStream>(shaderBinaryData), spv)) {
    Log::err("Failed to encode SPIR-V binary");

    result.first = BuildResult::eInvalidInput;
    return result;
  }

  // Compress binary further with deflate
  ArchiveData shaderData;

  if (!deflateEncode(Lwrap<WrVectorStream>(shaderData), shaderBinaryData)) {
    Log::err("Failed to compress SPIR-V binary");

    result.first = BuildResult::eInvalidInput;
    return result;
  }

  result.second = ArchiveFile(FourCC('S', 'H', 'D', 'R'), m_input.stem());
  result.second.setInlineData(std::move(shaderMetadata));
  result.second.addSubFile(FourCC('S', 'P', 'I', 'R'),
    IoArchiveCompression::eDeflate,
    shaderBinaryData.size(), std::move(shaderData));

  return result;
}

}
