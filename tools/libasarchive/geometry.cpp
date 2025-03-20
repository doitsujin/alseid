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

}


std::pair<BuildResult, ArchiveFile> GeometryBuildJob::build() {
  std::pair<BuildResult, ArchiveFile> result;
  result.first = BuildResult::eSuccess;

  // Load and convert GLTF input
  std::shared_ptr<Gltf> gltf;

  try {
    gltf = std::make_shared<Gltf>(m_env.io, m_input);
  } catch (const Error& e) {
    result.first = BuildResult::eIoError;
    return result;
  }

  // Create and dispatch mesh converter
  GltfConverter converter(m_env.jobs, std::move(gltf), m_desc.layoutMap);
  converter.convert();

  // Compress the geometry buffer
  auto srcBuffer = converter.getBuffer(0u);

  ArchiveData buffer;

  if (!(gdeflateEncode(Lwrap<WrVectorStream>(buffer), srcBuffer))) {
    result.first = BuildResult::eIoError;
    return result;
  }

  ArchiveData metadata;

  if (!converter.getGeometry()->serialize(Lwrap<WrVectorStream>(metadata)))
    return std::make_pair(BuildResult::eIoError, ArchiveFile());

  result.second = ArchiveFile(FourCC('G', 'E', 'O', 'M'), m_desc.name);
  result.second.setInlineData(std::move(metadata));
  result.second.addSubFile(FourCC('M', 'E', 'T', 'A'),
    IoArchiveCompression::eGDeflate, srcBuffer.getSize(), std::move(buffer));
  return result;
}

}
