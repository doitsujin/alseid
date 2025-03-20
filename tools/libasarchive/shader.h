#pragma once

#include "../../src/gfx/gfx_shader.h"
#include "../../src/gfx/gfx_spirv.h"

#include "archive.h"

namespace as::archive {

struct ShaderDesc {

};

class ShaderBuildJob : public BuildJob {

public:

  ShaderBuildJob(
          Environment                   env,
    const ShaderDesc&                   desc,
          std::filesystem::path         input);

  ~ShaderBuildJob();

  std::pair<BuildResult, ArchiveFile> build() override;

private:

  Environment           m_env;
  ShaderDesc            m_desc;
  std::filesystem::path m_input;

};

}
