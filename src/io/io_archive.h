#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../job/job.h"

#include "../util/util_types.h"

#include "io.h"
#include "io_file.h"
#include "io_request.h"
#include "io_stream.h"

namespace as {

/**
 * \brief Archive header
 */
struct IoArchiveHeader {
  /** 'ASFILE' */
  char magic[6];
  /** File version, currently 0 */
  uint16_t version;
  /** Number of files */
  uint32_t fileCount;
  /** Offset to file data section */
  uint32_t fileOffset;
  /** Compressed metadata size */
  uint32_t compressedMetadataSize;
  /** Size of uncompressed metadata */
  uint32_t rawMetadataSize;
};


/**
 * \brief File metadata
 */
struct IoArchiveFileMetadata {
  /** Length of the name in bytes, including
   *  the terminating null character. */
  uint16_t nameLength;
  /** Number of sub-files of this file */
  uint16_t subFileCount;
  /** Size of inline data. Inline data is a block of
   *  arbitrary data stored in the metadata block, so
   *  if will always be available for reading. */
  uint32_t inlineDataSize;
};


/**
 * \brief Compression type
 */
enum class IoArchiveCompression : uint16_t {
  /** Data is uncompressed. */
  eNone         = 0,
  /** Entire file is encoded using DEFLATE. */
  eDeflate      = 1,
};


/**
 * \brief Sub-file metadata
 */
struct IoArchiveSubFileMetadata {
  /** Sub-file identifier within the file. This is optional,
   *  as sub-files can be accessed by their index as well. */
  FourCC identifier;
  /** Compression type. Can be one of the values specified
   *  in the \ref IoArchiveCompression enum, or custom. */
  IoArchiveCompression compression;
  /** Currently unused field, always 0 */
  uint16_t reserved;
  /** Offset of this sub-file within the archive, in bytes,
   *  counted from the start of the file data section. */
  uint64_t offset;
  /** Size of the compressed sub-file. This is the number of
   *  bytes that the sub-file takes in the archive. */
  uint32_t compressedSize;
  /** Size of the file after decompression, in bytes. */
  uint32_t rawSize;
};



/**
 * \brief Archive sub-file object
 */
class IoArchiveSubFile {

public:

  explicit IoArchiveSubFile(
    const IoArchiveSubFileMetadata&     metadata,
          uint64_t                      extraOffset)
  : m_metadata(metadata) {
    m_metadata.offset += extraOffset;
  }

  /**
   * \brief Retrieves sub-file identifier
   * \returns Identifier
   */
  FourCC getIdentifier() const {
    return m_metadata.identifier;
  }

  /**
   * \brief Retrieves compression type
   * \returns Compression type
   */
  IoArchiveCompression getCompressionType() const {
    return m_metadata.compression;
  }

  /**
   * \brief Retrieves file offset in archive
   * \returns File offset in archive
   */
  uint64_t getOffsetInArchive() const {
    return m_metadata.offset;
  }

  /**
   * \brief Retrieves compressed data size
   * \returns Compressed data size
   */
  uint32_t getCompressedSize() const {
    return m_metadata.compressedSize;
  }

  /**
   * \brief Retrieves decoded data size
   * \returns Decompressed data size
   */
  uint32_t getSize() const {
    return m_metadata.rawSize;
  }

  /** 
   * \brief Checks whether any compression is used
   * \returns \c true if the sub file is compressed
   */
  bool isCompressed() const {
    return m_metadata.compression != IoArchiveCompression::eNone;
  }

private:

  IoArchiveSubFileMetadata m_metadata;

};


/**
 * \brief Archive file info
 */
class IoArchiveFile {

public:

  explicit IoArchiveFile(
    const IoArchiveFileMetadata&        metadata,
    const char*                         name,
    const IoArchiveSubFile*             subFiles,
    const void*                         inlineData)
  : m_name          (metadata.nameLength ? name : nullptr)
  , m_subFileCount  (metadata.subFileCount)
  , m_subFiles      (metadata.subFileCount ? subFiles : nullptr)
  , m_inlineSize    (metadata.inlineDataSize)
  , m_inlineData    (metadata.inlineDataSize ? inlineData : nullptr) { }

  /**
   * \brief Retrieves file name
   * \returns File name
   */
  const char* getName() const {
    return m_name;
  }

  /**
   * \brief Counts number of sub-files
   * \returns Sub file count
   */
  uint32_t getSubFileCount() const {
    return m_subFileCount;
  }

  /**
   * \brief Queries sub file by index
   *
   * \param [in] index Sub file index
   * \returns Sub file object, or \c nullptr
   *    if the index is out of bounds
   */
  const IoArchiveSubFile* getSubFile(uint32_t index) const {
    return index < m_subFileCount
      ? &m_subFiles[index]
      : nullptr;
  }

  /**
   * \brief Finds a sub file by identifier
   *
   * \param [in] identifier Identifier
   * \returns Sub file object, or \c nullptr if no
   *    sub-file with the given identifier exists
   */
  const IoArchiveSubFile* findSubFile(FourCC identifier) const {
    for (uint32_t i = 0; i < m_subFileCount; i++) {
      if (m_subFiles[i].getIdentifier() == identifier)
        return &m_subFiles[i];
    }

    return nullptr;
  }

  /**
   * \brief Retrieves inline data view
   *
   * Note that this pointer does not necessarily
   * meet any specific alignment requirements.
   * \returns Pointer to inline data
   */
  RdMemoryView getInlineData() const {
    return RdMemoryView(m_inlineData, m_inlineSize);
  }

private:

  const char*             m_name = nullptr;

  uint32_t                m_subFileCount  = 0;
  const IoArchiveSubFile* m_subFiles      = nullptr;

  uint32_t                m_inlineSize    = 0;
  const void*             m_inlineData    = nullptr;

};


/**
 * \brief Archive file
 *
 * Archives can essentially pack multiple files of any type within
 * one file system file. The layout of these files enables both
 * efficient compression and fast access to stored data.
 *
 * On the top level, there a named files (see \ref IoArchiveFile)
 * which can contain multiple sub-files as well as inline data.
 * For example, a texture could be stored entirely as one file,
 * with inline data being used to describe texture metadata, and
 * each subresource being stored in an indexed sub-file.
 *
 * Another example is shaders. Different graphics backends will
 * need shaders in different formats, so inline data can again
 * be used to store shader metadata, and named sub-files can be
 * used to store the actual binaries. The FourCC code of each
 * sub file can be used to identify the correct binary format.
 */
class IoArchive {

public:

  /**
   * \brief Loads an archive from a file
   *
   * Loads and parses all file metadata and inline data.
   * \param [in] file The file
   */
  IoArchive(IoFile file);

  ~IoArchive();

  IoArchive             (const IoArchive&) = delete;
  IoArchive& operator = (const IoArchive&) = delete;

  /**
   * \brief Counts number of files in the archive
   * \returns Number of files in the archive
   */
  uint32_t getFileCount() const {
    return uint32_t(m_files.size());
  }

  /**
   * \brief Retrieves file by index
   *
   * \param [in] index File index
   * \returns Pointer to file object, or \c nullptr
   *    if the given index is out of bounds.
   */
  const IoArchiveFile* getFile(uint32_t index) const {
    return index < getFileCount() ? &m_files[index] : nullptr;
  }

  /**
   * \brief Looks up file by name
   *
   * \param [in] name File name
   * \returns Pointer to file object, or \c nullptr
   *    if no file with the given name could be found.
   */
  const IoArchiveFile* findFile(const std::string& name) const;

  /**
   * \brief Synchronously reads sub file
   *
   * Reads and, if necessary, decompresses sub-file
   * contents into pre-allocated memory as necessary.
   * \param [in] subFile Sub-file to read
   * \param [in] dst Destination buffer
   * \returns Result of the I/O operation
   */
  IoStatus read(
    const IoArchiveSubFile*             subFile,
          void*                         dst) const;

  /**
   * \brief Synchronously reads compresseed sub file
   *
   * Reads and, if necessary, decompresses sub-file
   * contents into pre-allocated memory as necessary.
   * \param [in] subFile Sub-file to read
   * \param [in] dst Destination buffer
   * \returns Result of the I/O operation
   */
  IoStatus readCompressed(
    const IoArchiveSubFile*             subFile,
          void*                         dst) const {
    return m_file->read(
      subFile->getOffsetInArchive(),
      subFile->getCompressedSize(),
      dst);
  }

  /**
   * \brief Reads sub file
   *
   * Reads and, if necessary, decompresses sub-file
   * contents into pre-allocated memory as necessary.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] dst Destination buffer
   */
  void read(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          void*                         dst) const {
    if (!subFile->isCompressed())
      readCompressed(request, subFile, dst);

    streamCompressed(request, subFile,
      [this, subFile, dst] (const void* src, size_t size) {
        return decompress(subFile, dst, src)
          ? IoStatus::eSuccess
          : IoStatus::eError;
      });
  }

  /**
   * \brief Reads sub file with callback
   *
   * Reads and decompresses sub-file contents and executes
   * a callback on completion.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] dst Destination buffer
   * \param [in] callback Callback to process data
   *    Takes a pointer to the decompressed data as
   *    well as the decompressed size, in bytes.
   */
  template<typename Cb>
  void read(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          void*                         dst,
          Cb&&                          callback) const {
    if (!subFile->isCompressed())
      readCompressed(request, subFile, dst, std::move(callback));

    streamCompressed(request, subFile,
      [this, subFile, dst, cb = std::move(callback)] (const void* src, size_t size) {
        if (!decompress(subFile, dst, src))
          return IoStatus::eError;

        return cb(dst, subFile->getSize());
      });
  }

  /**
   * \brief Reads compressed sub file
   *
   * Reads the raw sub-file into pre-allocated
   * memory without performing any decompression.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] dst Destination buffer
   */
  void readCompressed(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          void*                         dst) const {
    request->read(m_file,
      subFile->getOffsetInArchive(),
      subFile->getCompressedSize(),
      dst);
  }

  /**
   * \brief Reads compressed sub file with callback
   *
   * Reads the raw sub-file into pre-allocated memory
   * and executes a callback on completion.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] dst Destination buffer
   * \param [in] callback Callback to process data.
   *    Takes a pointer to the compressed data as
   *    well as the compressed data size.
   */
  template<typename Cb>
  void readCompressed(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          void*                         dst,
          Cb&&                          callback) const {
    request->read(m_file,
      subFile->getOffsetInArchive(),
      subFile->getCompressedSize(),
      dst, std::move(callback));
  }

  /**
   * \brief Streams compressed sub file
   *
   * Reads the raw sub-file into a temporary buffer provided
   * by the back-end and executes a callback, which can then
   * process the data further.
   *
   * No decompressing version of this method is provided since
   * decompression would require on-the-fly memory allocation.
   * The main purpose of this method is to allow applications
   * to process the compressed data directly or decompress it
   * into a memory region that may not have been pre-allocated.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] callback Callback to process data.
   *    Takes a pointer to and the size of the temporary
   *    buffer, which contains the compressed sub-file.
   */
  template<typename Cb>
  void streamCompressed(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          Cb&&                          callback) const {
    request->stream(m_file,
      subFile->getOffsetInArchive(),
      subFile->getCompressedSize(),
      std::move(callback));
  }

  /**
   * \brief Decompresses sub-file in memory
   *
   * Most useful in combination with stream requests, 
   * \param [in] subFile Sub-file metadata
   * \param [in] dstData Buffer to write decompressed data to.
   *    \e Must be large enough to hold the decompressed sub file.
   * \param [in] srcData Buffer containing compressed sub-file
   *    \e Must be large enough to hold the compressed sub file.
   * \returns \c true on success
   */
  bool decompress(
    const IoArchiveSubFile*             subFile,
          void*                         dstData,
    const void*                         srcData) const;

  /**
   * \brief Checks whether the archive file is valid
   * \returns \c true if the archive file was read successfully
   */
  operator bool() const {
    return m_file != nullptr;
  }

private:

  IoFile                        m_file;

  std::vector<char>             m_fileNames;
  std::vector<char>             m_inlineData;

  std::vector<IoArchiveSubFile> m_subFiles;
  std::vector<IoArchiveFile>    m_files;

  std::unordered_map<std::string, size_t> m_lookupTable;

  static bool decompress(
          WrMemoryView                  output,
          RdMemoryView                  input,
          IoArchiveCompression          compression);

  bool parseMetadata();

};


/**
 * \brief Archive file data source
 */
struct IoArchiveDataSource {
  /** Memory location of source data.
   *  May be \c nullptr to use a file. */
  const void* memory = nullptr;
  /** Size of sub-file data within the
   *  source file or stream */
  uint64_t size = 0;
};


/**
 * \brief Archive sub-file description
 */
struct IoArchiveSubFileDesc {
  /** Data source for the sub-file */
  IoArchiveDataSource dataSource;
  /** Identifier */
  FourCC identifier = FourCC();
  /** Compression method */
  IoArchiveCompression compression = IoArchiveCompression::eNone;
};


/**
 * \brief Archive file description
 */
struct IoArchiveFileDesc {
  /** File name. Must be a non-empty string. */
  std::string name;
  /** Data source for inline data. If the size of this
   *  is 0, the file will not have inline data. */
  IoArchiveDataSource inlineDataSource;
  /** Sub-file descriptions */
  std::vector<IoArchiveSubFileDesc> subFiles;
};


/**
 * \brief Archive description
 *
 * Stores information about all files
 * to include in a archive.
 */
struct IoArchiveDesc {
  std::vector<IoArchiveFileDesc> files;
};


/**
 * \brief Archive builder
 *
 * Constructs an archive file from scratch
 * using the provided source data.
 */
class IoArchiveBuilder {

public:

  explicit IoArchiveBuilder(
          Io                            io,
          Jobs                          jobs,
          IoArchiveDesc                 desc);

  ~IoArchiveBuilder();

  /**
   * \brief Builds the archive file
   *
   * \param [in] path Path to the archive file
   * \returns Status of the I/O operations
   */
  IoStatus build(
          std::filesystem::path         path);

private:

  struct SubfileData {
    Job               job;
    std::vector<char> data;
    bool              status = false;
  };

  Io            m_io;
  Jobs          m_jobs;
  IoArchiveDesc m_desc;

  static bool compress(
          WrVectorStream&               output,
          RdMemoryView                  input,
          IoArchiveCompression          compression);

};

}
