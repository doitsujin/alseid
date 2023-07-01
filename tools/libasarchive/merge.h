#include "archive.h"

namespace asarchive {

class MergeBuildJob : public BuildJob {

public:

  MergeBuildJob(
          Environment                   env,
          std::shared_ptr<IoArchive>    archive,
          uint32_t                      fileId);

  ~MergeBuildJob();

  std::pair<BuildResult, BuildProgress> getProgress();

  std::pair<BuildResult, ArchiveFile> getFileInfo();

  void dispatchJobs();

  void abort();

private:

  Environment                 m_env;

  std::shared_ptr<IoArchive>  m_archive;
  const IoArchiveFile*        m_archiveFile;

  Job                         m_job;
  ArchiveFile                 m_fileInfo;

  std::atomic<BuildResult>    m_result = { BuildResult::eSuccess };

  BuildResult processFile();

};

}
