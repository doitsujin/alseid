#pragma once

#include "../../src/gfx/gfx_shader.h"
#include "../../src/gfx/gfx_spirv.h"

#include "archive.h"

namespace as::archive {

struct FileDesc {
  std::string name;
  FourCC type;
  ArchiveData inlineData;
  std::vector<ArchiveSubFile> subFiles;
};

class BasicBuildJob : public BuildJob {

public:

  BasicBuildJob(
          Environment                   env,
          FileDesc&&                    desc);

  ~BasicBuildJob();

  std::pair<BuildResult, ArchiveFile> build() override;

private:

  Environment           m_env;
  FileDesc              m_desc;

};

}
