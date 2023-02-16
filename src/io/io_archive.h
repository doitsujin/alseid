#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../util/util_huffman.h"
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
  /** Number of Huffman decoding tables */
  uint16_t decodingTableCount;
  /** Reserved value that's always 0 for now */
  uint16_t reserved;
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
 *
 * Note that all data can use Huffman compression with
 * one of the global decoding tables regardless of the
 * compression method specified here.
 */
enum class IoArchiveCompression : uint16_t {
  /** Data is uncompressed other than
   *  basic Huffman encoding. */
  eDefault        = 0,
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
  /** Index to Huffman decoding table. If this is \c -1,
   *  this sub-file does not use Huffman encoding. */
  uint16_t decodingTable;
  /** Offset of this sub-file within the archive, in bytes,
   *  counted from the start of the archive file itself. */
  uint64_t offset;
  /** Size of the compressed sub-file. This is the number of
   *  bytes that the sub-file takes in the archive. */
  uint32_t compressedSize;
  /** Size of the file after decompression, in bytes. */
  uint32_t rawSize;
};



/**
 * \brief Huffman decoding table metadata
 */
struct IoArchiveDecodingTableMetadata {
  /** Number of bytes required to store the decoding table. */
  uint32_t tableSize;
};


/**
 * \brief Archive sub-file object
 */
class IoArchiveSubFile {

public:

  explicit IoArchiveSubFile(
    const IoArchiveSubFileMetadata&     metadata)
  : m_metadata(metadata) { }

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
   * \brief Retrieves decoding table index
   * \returns Decoding table index
   */
  uint16_t getDecodingTableIndex() const {
    return m_metadata.decodingTable;
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
   * \brief Checks whether Huffman compression is used
   * \returns \c true if the decoding table index is valid
   */
  bool isHuffmanCompressed() const {
    return m_metadata.decodingTable != 0xFFFF;
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
   * \brief Retrieves inline data stream
   *
   * This is returned as a stream since the data
   * may not meet alignment requirements in memory.
   * \returns Memory stream for inline data
   */
  InMemoryStream getInlineData() const {
    return InMemoryStream(m_inlineData, m_inlineSize);
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
   * Loads and parses all file metadata, inline
   * data, as well as Huffman decoding tables.
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
   * \brief Retrieves Huffman decoding table
   *
   * \param [in] index Decoding table index
   * \returns Pointer to Huffman decoder, or \c nullptr
   *    if the given index is out of bounds.
   */
  const HuffmanDecoder* getDecoder(uint32_t index) const {
    return index < m_decoders.size() ? &m_decoders[index] : nullptr;
  }

  /**
   * \brief Synchronously reads a sub-file
   *
   * Reads and decodes the given sub-file into the given stream.
   * \param [in] subFile Sub-file to read
   * \param [in] data Data pointer. \e Must be large enough
   *    to hold the raw (uncompressed) sub file.
   * \returns Status of the operation
   */
  IoStatus read(
    const IoArchiveSubFile*             subFile,
          void*                         data);

  /**
   * \brief Synchronously reads compressed sub file
   *
   * Copies the compressed sub-file directly to the given
   * memory location without decoding its contents.
   * \param [in] subFile Sub-file to read
   * \param [in] data Data pointer. \e Must be large
   *    enough to hold the compressed sub file.
   * \returns Status of the operation
   */
  IoStatus readCompressed(
    const IoArchiveSubFile*             subFile,
          void*                         data);

  /**
   * \brief Adds asynchronous read request
   *
   * Asynchronous version of \c read. Note that the file
   * archive object \e must not be destroyed while any
   * asynchronous requests are pending.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] data Data pointer
   */
  void requestRead(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          void*                         data);

  /**
   * \brief Adds raw asynchronous read request
   *
   * Asynchronous version of \c readRaw.
   * \param [in] request I/O request object
   * \param [in] subFile Sub-file to read
   * \param [in] data Data pointer
   */
  void requestReadCompressed(
    const IoRequest&                    request,
    const IoArchiveSubFile*             subFile,
          void*                         data);

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
  std::vector<HuffmanDecoder>   m_decoders;

  std::vector<IoArchiveSubFile> m_subFiles;
  std::vector<IoArchiveFile>    m_files;

  std::unordered_map<std::string, size_t> m_lookupTable;

  bool parseMetadata();

  bool decompress(
    const IoArchiveSubFile*             subFile,
          void*                         dstData,
          size_t                        dstSize,
    const void*                         srcData,
          size_t                        srcSize) const;

};


/**
 * \brief Archive file data source
 */
struct IoArchiveDataSource {
  /** Memory location of source data.
   *  May be \c nullptr to use a file. */
  const void* memory = nullptr;
  /** Path to the source file, if any.
   *  Ignored if memory is non-null. */
  std::filesystem::path file;
  /** Offset of sub-file data within
   *  the source file or stream */
  uint64_t offset = 0;
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
  IoArchiveCompression compression = IoArchiveCompression::eDefault;
  /** Huffman decoding table. If this is 0xFFFF,
   *  this sub-file will not be compressed. */
  uint16_t decodingTable = 0xFFF;
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

  struct HuffmanObjects {
    HuffmanCounter counter;
    HuffmanEncoder encoder;
    HuffmanDecoder decoder;
  };

  Io            m_io;
  IoArchiveDesc m_desc;

  std::vector<HuffmanObjects> m_huffmanObjects;
  std::vector<HuffmanCounter> m_huffmanCounters;

  IoStatus buildHuffmanObjects();

  uint64_t computeCompressedSize(
    const IoArchiveSubFileDesc&         subfile,
          uint32_t                      subfileIndex) const;

  bool writeCompressedSubfile(
          OutStream&                    stream,
    const IoArchiveSubFileDesc&         subfile,
    const void*                         data);

  template<typename Proc>
  IoStatus processDataSource(
    const IoArchiveDataSource&          source,
    const Proc&                         proc);

  template<typename Pred, typename Proc>
  IoStatus processFilesIf(
    const Pred&                         pred,
    const Proc&                         proc);

  template<typename Proc>
  IoStatus processFiles(
    const Proc&                         proc);

};

}
