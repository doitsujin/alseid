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
 * \brief Input stream helper
 *
 * Provides convenience methods for typed reads
 * on top of an arbitrary stream class.
 * \tparam Base Base class. Must provide a
 *    \c read and a \c skip method.
 */
template<typename Base>
class RdStream {

public:

  RdStream(Base& base)
  : m_base(base) { }

  /**
   * \brief Reads raw data
   *
   * This call is forwarded directly to the base class.
   * \param [in] dst Location to copy read data to
   * \param [in] size Number of bytes to read
   * \returns Whether the given number of bytes could be read
   */
  bool read(void* dst, size_t size) {
    return m_base.read(dst, size);
  }

  /**
   * \brief Skips given amount of data
   *
   * This call is forwarded directly to the base class.
   * \param [in] size Number of bytes to skip
   * \returns \c true if all bytes could be skipped
   */
  bool skip(size_t size) {
    return m_base.skip(size);
  }

  /**
   * \brief Reads trivial data type
   *
   * \param [out] value Data read from the stream
   * \returns \c true on success, \c false if
   *    the end of the stream was reached.
   */
  template<typename T, std::enable_if_t<
    std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool read(T& value) {
    return read(&value, sizeof(value));
  }

  /**
   * \brief Reads trivial data type with conversion
   *
   * \tparam T Type of the given data within the stream
   * \tparam T_ Type to convert read data to
   * \param [out] value Data read from the stream
   * \returns \c true on success, \c false if
   *    the end of the stream was reached.
   */
  template<typename T, typename T_, std::enable_if_t<
    std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool readAs(T_& value) {
    T tmp = { };
    if (!read(tmp))
      return false;

    value = T_(tmp);
    return true;
  }

  /**
   * \brief Reads array data
   *
   * \param [in] value Array to read from the stream
   * \returns \c true on success, \c false if not
   *    all data could be written to the stream.
   */
  template<typename T, std::enable_if_t<
    std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool read(std::vector<T>& value) {
    return read(value.data(), value.size() * sizeof(T));
  }

  template<typename T, size_t N, std::enable_if_t<
    std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool read(std::array<T, N>& value) {
    return read(value.data(), value.size() * sizeof(T));
  }

  /**
   * \brief Accesses base object
   * \returns Pointer to base object
   */
  Base* operator -> () const {
    return &m_base;
  }

private:

  Base& m_base;

};


/**
 * \brief Output stream helper
 *
 * Provides convenience methods for typed writes
 * on top of an arbitrary stream class.
 * \tparam Base Base class. Must provide a \c write method.
 */
template<typename Base>
class WrStream {

public:

  WrStream(Base& base)
  : m_base(base) { }

  /**
   * \brief Writes raw data
   *
   * \param [in] src Location of source data
   * \param [in] size Number of bytes to write
   * \returns Whether the given number of bytes could be written
   */
  bool write(const void* src, size_t size) {
    return m_base.write(src, size);
  }

  /**
   * \brief Writes trivial data type
   *
   * \param [in] value Data to write to the stream
   * \returns \c true on success, \c false if not
   *    all data could be written to the stream.
   */
  template<typename T, std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool write(const T& value) {
    return write(&value, sizeof(value));
  }

  /**
   * \brief Writes array data
   *
   * \param [in] value Data to write to the stream
   * \returns \c true on success, \c false if not
   *    all data could be written to the stream.
   */
  template<typename T, std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool write(const std::vector<T>& value) {
    return write(value.data(), value.size() * sizeof(T));
  }

  template<typename T, size_t N, std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivial_v<T>, bool> = true>
  bool write(const std::array<T, N>& value) {
    return write(value.data(), value.size() * sizeof(T));
  }

  /**
   * \brief Flushes pending writes asnecessary
   *
   * Calls \c flush method of the underlying stream.
   * \returns \c true on success, or \c false if an error occured.
   */
  bool flush() {
    return m_base.flush();
  }

  /**
   * \brief Accesses base object
   * \returns Pointer to base object
   */
  Base* operator -> () const {
    return &m_base;
  }

private:

  Base& m_base;

};


/**
 * \brief Read-only memory view
 */
class RdMemoryView {

public:

  RdMemoryView() { }

  RdMemoryView(const void* data, size_t size)
  : m_data(size ? reinterpret_cast<const char*>(data) : nullptr), m_size(size) { }

  template<typename T>
  RdMemoryView(const std::vector<T>& vector)
  : RdMemoryView(vector.data(), vector.size() * sizeof(T)) { }

  template<typename T, size_t N>
  RdMemoryView(const std::array<T, N>& array)
  : RdMemoryView(array.data(), array.size() * sizeof(T)) { }

  /**
   * \brief Queries data pointer
   * \returns Pointer to the beginning of the view
   */
  const void* getData() const {
    return m_data;
  }

  /**
   * \brief Queries data pointer at given offset
   *
   * \param [in] offset Offset from start of the view
   * \returns Pointer to the requested memory location
   */
  const void* getData(size_t offset) const {
    return reinterpret_cast<const char*>(m_data) + offset;
  }

  /**
   * \brief Queries size of the view
   * \returns Total size, in bytes
   */
  size_t getSize() const {
    return m_size;
  }

  /**
   * \brief Queries current write offset
   * \returns Current write offset
   */
  size_t getOffset() const {
    return m_offset;
  }

  /**
   * \brief Tries to read raw data
   *
   * \param [in] dst Location to copy read data to
   * \param [in] size Number of bytes to read
   * \returns Number of bytes actually read
   */
  size_t load(void* dst, size_t size) {
    if (unlikely(m_offset + size > m_size)) {
      size = std::min(size, m_size - m_offset);
      std::memcpy(dst, m_data + m_offset, size);
    } else {
      // Doing this explicitly may help compiler
      // optimization figure out fixed-size loads
      std::memcpy(dst, m_data + m_offset, size);
    }

    m_offset += size;
    return size;
  }

  /**
   * \brief Reads raw data
   *
   * \param [in] dst Location to copy read data to
   * \param [in] size Number of bytes to read
   * \returns Whether the given number of bytes could be read
   */
  bool read(void* dst, size_t size) {
    if (unlikely(m_offset + size > m_size))
      return false;

    std::memcpy(dst, m_data + m_offset, size);
    m_offset += size;
    return true;
  }

  /**
   * \brief Skips given amount of data
   *
   * Only advances internal offset.
   * \param [in] size Number of bytes to skip
   * \returns \c true if all bytes could be skipped
   */
  bool skip(size_t size) {
    if (unlikely(m_offset + size > m_size))
      return false;

    m_offset += size;
    return true;
  }

  /**
   * \brief Changes offset to given location
   *
   * \param [in] offset Desired offset
   * \returns \c true if the offset was changed
   */
  bool seek(size_t offset) {
    if (unlikely(offset > m_size))
      return false;

    m_size = offset;
    return true;
  }

  /**
   * \brief Checks whether the view is valid
   * \returns \c true if the view points to valid memory
   */
  operator bool () const {
    return m_data != nullptr;
  }

private:

  const char* m_data    = nullptr;
  size_t      m_size    = 0;
  size_t      m_offset  = 0;

};


/**
 * \brief Writable memory view
 *
 * Much like \c RdMemoryView, escept
 * this one supports write operations.
 */
class WrMemoryView {

public:

  WrMemoryView() { }

  WrMemoryView(void* data, size_t size)
  : m_data(size ? reinterpret_cast<char*>(data) : nullptr), m_size(size) { }

  template<typename T>
  WrMemoryView(std::vector<T>& vector)
  : WrMemoryView(vector.data(), vector.size() * sizeof(T)) { }

  template<typename T, size_t N>
  WrMemoryView(std::array<T, N>& array)
  : WrMemoryView(array.data(), array.size() * sizeof(T)) { }

  /**
   * \brief Queries data pointer
   * \returns Pointer to the beginning of the view
   */
  void* getData() const {
    return m_data;
  }

  /**
   * \brief Queries data pointer at given offset
   *
   * \param [in] offset Offset from start of the view
   * \returns Pointer to the requested memory location
   */
  void* getData(size_t offset) const {
    return reinterpret_cast<char*>(m_data) + offset;
  }

  /**
   * \brief Queries size of the view
   * \returns Total size, in bytes
   */
  size_t getSize() const {
    return m_size;
  }

  /**
   * \brief Queries current write offset
   * \returns Current write offset
   */
  size_t getOffset() const {
    return m_offset;
  }

  /**
   * \brief Writes raw data
   *
   * \param [in] src Location of source data
   * \param [in] size Number of bytes to write
   * \returns Whether the given number of bytes could be written
   */
  bool write(const void* src, size_t size) {
    if (unlikely(m_offset + size > m_size))
      return false;

    std::memcpy(m_data + m_offset, src, size);
    m_offset += size;
    return true;
  }

  /**
   * \brief Flush method
   *
   * No-op for this type of stream.
   * \returns \c true
   */
  bool flush() {
    return true;
  }

  /**
   * \brief Checks whether the view is valid
   * \returns \c true if the view points to valid memory
   */
  operator bool () const {
    return m_data != nullptr;
  }

private:

  char*   m_data    = nullptr;
  size_t  m_size    = 0;
  size_t  m_offset  = 0;

};


/**
 * \brief Buffered input stream base class
 *
 * Allows buffered sequential reading of a given input,
 * with convenience methods to support basic data types.
 */
class RdBufferedStream {
  constexpr static size_t BufferSize = 1024;
public:

  virtual ~RdBufferedStream();

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

private:

  size_t                        m_bufferSize    = 0;
  size_t                        m_bufferOffset  = 0;
  std::array<char, BufferSize>  m_buffer        = { };

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
 * \brief Buffered output stream base class
 *
 * Allows buffered sequential writing to a given output,
 * with convenience methods to support basic data types.
 */
class WrBufferedStream {
  constexpr static size_t BufferSize = 1024;
public:

  WrBufferedStream             (const WrBufferedStream&) = delete;
  WrBufferedStream& operator = (const WrBufferedStream&) = delete;

  virtual ~WrBufferedStream();

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
   * \brief Flushes internal buffer
   *
   * Writes all buffered data to the memory stream.
   * \returns \c true on success, or \c false if an error occured.
   */
  bool flush();

private:

  size_t                        m_bufferSize    = 0;
  size_t                        m_bufferOffset  = 0;
  std::array<char, BufferSize>  m_buffer        = { };

  bool writeComplex(const void* src, size_t size);

protected:

  WrBufferedStream() { }

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
 * \brief Buffered vector stream
 *
 * Allows appending data to an existing \c std::vector.
 */
class WrVectorStream : public WrBufferedStream {

public:

  WrVectorStream(
          std::vector<char>&            vector);

  ~WrVectorStream();

  /**
   * \brief Queries size of the underlying vector
   *
   * Implicitly flushes to keep the result up to date.
   * \returns Size of the vector
   */
  size_t getSize() {
    flush();
    return m_vector.size();
  }

  /**
   * \brief Retrieves a reference to the vector
   *
   * Allows code to manipulate the vector directly.
   * \returns Vector reference
   */
  std::vector<char>& getVector() {
    flush();
    return m_vector;
  }

private:

  std::vector<char>& m_vector;

protected:

  std::pair<size_t, size_t> writeToContainer(
    const void*                         data,
          size_t                        size) override;

};

}
