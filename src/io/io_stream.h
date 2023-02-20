#pragma once

#include "../util/util_stream.h"

#include "io_file.h"

namespace as {

/**
 * \brief Input file stream
 *
 * Implements a stream object on top of
 * synchronous file operations.
 */
class RdFileStream : public RdBufferedStream {

public:

  RdFileStream();

  /**
   * \brief Initializes file writer
   *
   * Reading will always start at offset 0.
   * \param [in] file File to read from
   */
  RdFileStream(IoFile file);

  ~RdFileStream();

  /**
   * \brief Queries current offset in file
   * \returns Current offset in file
   */
  uint64_t getOffset() const {
    return m_offset;
  }

  /**
   * \brief Queries file size
   * \returns File size, in bytes
   */
  uint64_t getSize() const {
    return m_size;
  }

  /**
   * \brief Checks whether the file is valid
   * \returns \c true if the file is valid
   */
  operator bool () const {
    return bool(m_file);
  }

private:

  IoFile        m_file;
  uint64_t      m_size    = 0;
  uint64_t      m_offset  = 0;

protected:

  size_t readFromSource(
          void*                         data,
          size_t                        size) override;

};


/**
 * \brief Output file stream
 *
 * Implements a stream object on top of
 * synchronous file operations.
 */
class WrFileStream : public WrBufferedStream {

public:

  WrFileStream();

  /**
   * \brief Initializes file writer
   *
   * Any write operations to this stream
   * will append data to the file.
   * \param [in] file File to write to
   */
  WrFileStream(IoFile file);

  ~WrFileStream();

  /**
   * \brief Queries file size
   * \returns File size, in bytes
   */
  uint64_t getSize() {
    flush();
    return m_size;
  }

  /**
   * \brief Checks whether the file is valid
   * \returns \c true if the file is valid
   */
  operator bool () const {
    return bool(m_file);
  }

private:

  IoFile        m_file;
  uint64_t      m_size  = 0;

protected:

  std::pair<size_t, size_t> writeToContainer(
    const void*                         data,
          size_t                        size) override;

};

}
