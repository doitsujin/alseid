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
class InFileStream : public InStream {

public:

  InFileStream();

  /**
   * \brief Initializes file writer
   *
   * Reading will always start at offset 0.
   * \param [in] file File to read from
   */
  InFileStream(IoFile file);

  ~InFileStream();

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
class OutFileStream : public OutStream {

public:

  OutFileStream();

  /**
   * \brief Initializes file writer
   *
   * Any write operations to this stream
   * will append data to the file.
   * \param [in] file File to write to
   */
  OutFileStream(IoFile file);

  ~OutFileStream();

  /**
   * \brief Queries file size
   * \returns File size, in bytes
   */
  uint64_t getSize() {
    flush();
    return m_size;
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
