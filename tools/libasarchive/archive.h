#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "common.h"

namespace as::archive {

using ArchiveData = std::vector<char>;

/**
 * \brief Library environment
 */
struct Environment {
  Io   io;
  Jobs jobs;
};


/**
 * \brief Archive sub-file
 */
struct ArchiveSubFile {
  /** Optional sub-file identifier */
  FourCC identifier = FourCC();
  /** Compression type for this sub-file */
  IoArchiveCompression compression = IoArchiveCompression::eNone;
  /** Size of uncompressed data */
  size_t rawSize = 0;
  /** Compressed data buffer */
  ArchiveData compressedData;
};


/**
 * \brief Archive file
 *
 * Provides internal buffers to store archive file data,
 * as well as helper methods to generate metadata structs.
 */
class ArchiveFile {

public:

  ArchiveFile();

  /**
   * \brief Initializes archive file
   *
   * \param [in] type File type identifier
   * \param [in] name File name
   */
  ArchiveFile(
          FourCC                        type,
          std::string                   name);

  /**
   * \brief Sets inline data
   *
   * \param [in] data Inline data
   * \returns \c true on success
   */
  bool setInlineData(
          ArchiveData&&                 data);

  /**
   * \brief Adds sub-file to the file
   *
   * \param [in] identifier Sub-file identifier
   * \param [in] compression Compression type
   * \param [in] rawSize Uncompressed data size
   * \param [in] compressedData Compressed file data
   * \returns \c true on success
   */
  bool addSubFile(
          FourCC                        identifier,
          IoArchiveCompression          compression,
          size_t                        rawSize,
          ArchiveData&&                 compressedData);

  /**
   * \brief Queries file name
   *
   * The returned file name may never be \c nullptr.
   * \returns Pointer to null-terminated file name.
   */
  const char* getFileName() const;

  /**
   * \brief Queries file metadata
   *
   * Retrieves basic properties of the file.
   * \param [out] metadata Pointer where file metadata
   *    will be written.
   * \param [out] inlineData Pointer to inline data
   */
  void getFileMetadata(
          IoArchiveFileMetadata*        metadata,
    const void**                        inlineData) const;

  /**
   * \brief Queries sub-file metadata
   *
   * \param [in] dataOffset Initial data offset within the file
   * \param [in] entryCount Number of entries in the
   *    parameter arrays. Excess items will not be written.
   * \param [out] metadata Sub-file metadata array
   * \param [out] subFileData Sub-file data array
   */
  void getSubFileMetadata(
          uint64_t&                     dataOffset,
          size_t                        entryCount,
          IoArchiveSubFileMetadata*     metadata,
    const void**                        subFileData) const;

private:

  FourCC            m_type = FourCC();
  std::string       m_name;

  ArchiveData       m_inlineData;

  std::list<ArchiveSubFile> m_subFiles;

};


/**
 * \brief Build job result
 */
enum class BuildResult : int32_t {
  /** Operation completed successfully */
  eSuccess            = 0,
  /** Operation was aborted */
  eAborted            = -1,
  /** Input arguments are invalid or not
   *  applicable to the given input files */
  eInvalidArgument    = -2,
  /** Input files are invalid */
  eInvalidInput       = -3,
  /** Input file could not be opened */
  eIoError            = -4,
};


/**
 * \brief Build job
 *
 * Abstraction that generates a single file
 * within an archive from arbitrary inputs.
 */
class BuildJob {

public:

  virtual ~BuildJob();

  /**
   * \brief Builds archive file
   * \returns Status and archive file
   */
  virtual std::pair<BuildResult, ArchiveFile> build() = 0;

};


/**
 * \brief Archive file streams
 *
 * Provides a simple interface to 
 */
class ArchiveStreams {

public:

  ArchiveStreams(
          Environment                   environment);

  ~ArchiveStreams();

  /**
   * \brief Adds a file
   * \param [in] file File info
   */
  void addFile(
    const ArchiveFile&                  file);

  /**
   * \brief Writes archive file
   *
   * Creates an archive file containing all
   * the files that were added since.
   * \param [in] path File path
   * \returns Status of the operation
   */
  BuildResult write(
          std::filesystem::path         path) const;

private:

  Environment                             m_environment;

  std::vector<IoArchiveFileMetadata>      m_fileMetadata;
  std::vector<const void*>                m_fileInlineData;
  std::vector<char>                       m_fileNames;

  uint64_t                                m_subFileDataOffset = 0;
  std::vector<IoArchiveSubFileMetadata>   m_subFileMetadata;
  std::vector<const void*>                m_subFileData;

  bool getMetadataBlob(
          WrVectorStream&               stream) const;

};


/**
 * \brief Archive builder job info
 */
struct ArchiveBuilderJobInfo {
  std::pair<BuildResult, ArchiveFile> status;
  Job job;
};


/**
 * \brief Archive builder
 */
class ArchiveBuilder {

public:

  ArchiveBuilder(
          Environment                   environment);

  ~ArchiveBuilder();

  /**
   * \brief Adds a build job
   *
   * The job will be dispatched immediately.
   * \param [in] job Build job object
   */
  void addBuildJob(
          std::shared_ptr<BuildJob>     job);

  /**
   * \brief Builds the archive file
   *
   * Waits for all build jobs to complete in the order
   * they were added, and writes the output file. This
   * must only be called after all build jobs have been
   * added.
   * \param [in] path Output file path
   */
  BuildResult build(
          std::filesystem::path         path);

private:

  Environment                         m_environment;

  std::mutex                          m_mutex;
  std::atomic<BuildResult>            m_status = { BuildResult::eSuccess };
  std::queue<ArchiveBuilderJobInfo>   m_buildJobs;

};

}
