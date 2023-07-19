#pragma once

#include "../../src/gfx/gfx_shader.h"
#include "../../src/gfx/gfx_spirv.h"

#include "archive.h"

namespace asarchive {

struct ShaderDesc {

};

class ShaderBuildJob : public BuildJob {

public:

  ShaderBuildJob(
          Environment                   env,
    const ShaderDesc&                   desc,
          std::filesystem::path         input);

  ~ShaderBuildJob();

  std::pair<BuildResult, BuildProgress> getProgress();

  std::pair<BuildResult, ArchiveFile> getFileInfo();

  void dispatchJobs();

  void abort();

private:

  Environment           m_env;
  ShaderDesc            m_desc;
  std::filesystem::path m_input;
  Job                   m_job;

  ArchiveData           m_shaderDesc;
  ArchiveData           m_shaderData;
  size_t                m_rawSize = 0;

  std::atomic<BuildResult> m_result = { BuildResult::eSuccess };

  BuildResult processShader();

};

}
