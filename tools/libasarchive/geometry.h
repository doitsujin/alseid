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
  std::string name;
  std::shared_ptr<GltfPackedVertexLayoutMap> layoutMap;
};


class GeometryBuildJob : public BuildJob {

public:

  GeometryBuildJob(
          Environment                   env,
    const GeometryDesc&                 desc,
          std::filesystem::path         input);

  ~GeometryBuildJob();

  std::pair<BuildResult, ArchiveFile> build() override;

private:

  Environment           m_env;
  GeometryDesc          m_desc;
  std::filesystem::path m_input;

};

}
