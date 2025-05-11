#pragma once

#include "archive.h"

namespace as::archive {

class MergeBuildJob : public BuildJob {

public:

  MergeBuildJob(
          Environment                   env,
          std::shared_ptr<IoArchive>    archive,
          uint32_t                      fileId);

  ~MergeBuildJob();

  std::pair<BuildResult, ArchiveFile> build() override;

private:

  Environment                 m_env;
  std::shared_ptr<IoArchive>  m_archive;
  IoArchiveFileRef            m_archiveFile;

};

}
