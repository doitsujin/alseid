#pragma once

#include <array>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

#include "util_common.h"
#include "util_likely.h"

namespace as {

/**
 * \brief Input stream base class
 *
 * Allows buffered sequential reading of a given input,
 * with convenience methods to support basic data types.
 */
class InStream {
  constexpr static size_t BufferSize = 1024;
public:

  /**
   * \brief Tries to read raw data
   *
   * \param [in] dst Location to copy read data to
   * \param [in] size Number of bytes to read
   * \returns Number of bytes actually read
   */
  size_t load(void* dst, size_t size) {
    if (likely(m_bufferOffset + size <= m_bufferSize)) {
      std::memcpy(dst, &m_buffer[m_bufferOffset], size);
      m_bufferOffset += size;
      return size;
    }

    return readComplex(dst, size);
  }
  

  /**
   * \brief Reads raw data
   *
   * \param [in] dst Location to copy read data to
   * \param [in] size Number of bytes to read
   * \returns Whether the given number of bytes could be read
   */
  bool read(void* dst, size_t size) {
    if (likely(m_bufferOffset + size <= m_bufferSize)) {
      std::memcpy(dst, &m_buffer[m_bufferOffset], size);
      m_bufferOffset += size;
      return true;
    }

    size_t read = readComplex(dst, size);
    return read == size;
  }

  /**
   * \brief Skips given number of bytes
   *
   * \param [in] size Number of bytes to skip
   * \returns Whether the given number of bytes could be skipped
   */
  bool skip(size_t size) {
    if (likely(m_bufferOffset + size <= m_bufferSize)) {
      m_bufferOffset += size;
      return true;
    }

    size_t skipped = skipComplex(size);
    return skipped == size;
  }

  /**
   * \brief Reads trivial data type
   *
   * \param [out] value Data read from the stream
   * \returns \c true on success, \c false if
   *    the end of the stream was reached.
   */
  template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, bool> = true>
  bool read(T& value) {
    return read(&value, sizeof(value));
  }

private:

  size_t                        m_bufferSize    = 0;
  size_t                        m_bufferOffset  = 0;
  std::array<char, BufferSize>  m_buffer;

  size_t readComplex(void* dst, size_t size);

  size_t skipComplex(size_t size);

protected:

  /**
   * \brief Reads data from the underlying data source
   *
   * \param [in] data Pointer to internal buffer. May
   *    be \c nullptr for skip operations.
   * \param [in] size Maximum number of bytes to copy
   * \returns Actual number of bytes read or skipped
   */
  virtual size_t readFromSource(
          void*                         data,
          size_t                        size) = 0;

};


/**
 * \brief Output stream base class
 *
 * Allows buffered sequential writing to a given output,
 * with convenience methods to support basic data types.
 */
class OutStream {
  constexpr static size_t BufferSize = 1024;
public:

  /**
   * \brief Writes raw data
   *
   * \param [in] src Location of source data
   * \param [in] size Number of bytes to write
   * \returns Whether the given number of bytes could be written
   */
  bool write(const void* src, size_t size) {
    if (likely(m_bufferOffset + size <= m_bufferSize)) {
      std::memcpy(&m_buffer[m_bufferOffset], src, size);
      m_bufferOffset += size;
      return true;
    }

    return writeComplex(src, size);
  }

  /**
   * \brief Writes trivial data type
   *
   * \param [in] value Data to write to the stream
   * \returns \c true on success, \c false if not
   *    all data could be written to the stream.
   */
  template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, bool> = true>
  bool write(const T& value) {
    return write(&value, sizeof(value));
  }

  /**
   * \brief Flushes internal buffer
   *
   * Writes all buffered data to the memory stream.
   * \returns \c true on success, or \c false if an error occured.
   */
  bool flush();

private:

  size_t                        m_bufferSize    = 0;
  size_t                        m_bufferOffset  = 0;
  std::array<char, BufferSize>  m_buffer;

  bool writeComplex(const void* src, size_t size);

protected:

  /**
   * \brief Writes data to underlying container
   *
   * \param [in] data Pointer to source data
   * \param [in] size Number of bytes to write
   * \returns Pair of actual number of bytes written, and
   *    maximum number of bytes that can be processed with
   *    the next write operation.
   */
  virtual std::pair<size_t, size_t> writeToContainer(
    const void*                         data,
          size_t                        size) = 0;

};


/**
 * \brief Memory input stream
 *
 * Can be used to sequentially read data from
 * a memory region.
 */
class InMemoryStream : public InStream {

public:

  InMemoryStream() { }

  /**
   * \brief Initializes memory reader
   *
   * \param [in] data Pointer to memory region
   * \param [in] size Size of the memory region
   */
  InMemoryStream(
    const void*                         data,
          size_t                        size)
  : m_data(data), m_offset(0), m_capacity(size) { }

  /**
   * \brief Initializes memory reader from vector
   * \param [in] vector Container to use as a data source
   */
  template<typename T>
  explicit InMemoryStream(
    const std::vector<T>&               vector)
  : InMemoryStream(vector.data(), vector.size() * sizeof(T)) { }

  /**
   * \brief Initializes memory reader from array
   * \param [in] array Array to use as a data source
   */
  template<typename T, size_t N>
  explicit InMemoryStream(
    const std::array<T, N>&             array)
  : InMemoryStream(array.data(), array.size() * sizeof(T)) { }

private:

  const void*   m_data      = nullptr;
  size_t        m_offset    = 0;
  size_t        m_capacity  = 0;

protected:

  size_t readFromSource(
          void*                         data,
          size_t                        size) override;

};


/**
 * \brief Output memory stream
 *
 * Can be used to write to a fixed-size memory region.
 */
class OutMemoryStream : public OutStream {

public:

  OutMemoryStream();

  ~OutMemoryStream();

  /**
   * \brief Initializes memory writer
   *
   * \param [in] data Pointer to memory region
   * \param [in] size Size of the memory region
   */
  OutMemoryStream(
          void*                         data,
          size_t                        size)
  : m_data(data), m_offset(0), m_capacity(size) { }

  /**
   * \brief Initializes memory writer from vector
   * \param [in] vector Container to use as a data source
   */
  template<typename T>
  explicit OutMemoryStream(
          std::vector<T>&               vector)
  : OutMemoryStream(vector.data(), vector.size() * sizeof(T)) { }

  /**
   * \brief Initializes memory reader from array
   * \param [in] array Array to use as a data source
   */
  template<typename T, size_t N>
  explicit OutMemoryStream(
          std::array<T, N>&             array)
  : OutMemoryStream(array.data(), array.size() * sizeof(T)) { }

private:

  void*         m_data      = nullptr;
  size_t        m_offset    = 0;
  size_t        m_capacity  = 0;

protected:

  std::pair<size_t, size_t> writeToContainer(
    const void*                         data,
          size_t                        size) override;

};


/**
 * \brief Output vector stream
 *
 * Can be used to accumulate sequentially written
 * data if the size is previously not known.
 */
class OutVectorStream : public OutStream {

public:

  /**
   * \brief Initializes empty vector
   */
  OutVectorStream();

  /**
   * \brief Takes ownership of existing vector
   * \param [in] vector Existing vector
   */
  explicit OutVectorStream(
          std::vector<char>&&           vector);

  ~OutVectorStream();

  /**
   * \brief Extracts vector
   * \returns Vector
   */
  std::vector<char> getData() && {
    flush();
    return std::move(m_vector);
  }

  /**
   * \brief Queries current size
   * \returns Current size of the vector
   */
  size_t getSize() {
    flush();
    return m_vector.size();
  }

private:

  std::vector<char> m_vector;

protected:

  std::pair<size_t, size_t> writeToContainer(
    const void*                         data,
          size_t                        size) override;

};


/**
 * \brief Null output stream
 *
 * Discards all written data, but accumulates written size.
 * This can be useful to determine the number of bytes needed
 * for a given set of data before actually writing it.
 */
class OutNullStream : public OutStream {

public:

  OutNullStream();

  ~OutNullStream();

  /**
   * \brief Queries number of bytes written
   * \returns Number of bytes written
   */
  uint64_t getWritten() {
    flush();
    return m_written;
  }

private:

  uint64_t m_written = 0;

protected:

  std::pair<size_t, size_t> writeToContainer(
    const void*                         data,
          size_t                        size) override;

};

}
