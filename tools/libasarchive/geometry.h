#pragma once

#include "../../src/gfx/gfx_geometry.h"

#include "../libgltfimport/gltf_asset.h"
#include "../libgltfimport/gltf_import.h"

#include "archive.h"

using namespace as::gltf;

namespace as::archive {

/**
 * \brief Geometry description
 */
struct GeometryDesc {
  std::shared_ptr<GltfPackedVertexLayoutMap> layoutMap;
};


class GeometryBuildJob : public BuildJob {

public:

  GeometryBuildJob(
          Environment                   env,
    const GeometryDesc&                 desc,
          std::filesystem::path         input);

  ~GeometryBuildJob();

  std::pair<BuildResult, BuildProgress> getProgress();

  std::pair<BuildResult, ArchiveFile> getFileInfo();

  void dispatchJobs();

  void abort();

private:

  Environment           m_env;
  GeometryDesc          m_desc;
  std::filesystem::path m_input;

  Job                   m_ioJob;
  Job                   m_convertJob;
  Job                   m_compressJob;

  std::atomic<BuildResult>  m_result = { BuildResult::eSuccess };

  std::vector<size_t>       m_rawSizes;
  std::vector<ArchiveData>  m_buffers;

  size_t                    m_animationSize = 0;
  ArchiveData               m_animationBuffer;

  std::shared_ptr<GltfConverter> m_converter;

  BuildResult runIoJob();

  void runCompressJob();

  void synchronizeJobs();

};

}
