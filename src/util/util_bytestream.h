#pragma once

#include <cstring>
#include <istream>
#include <ostream>
#include <type_traits>
#include <vector>

#include "util_likely.h"

namespace as {

/**
 * \brief Bytestream reader
 *
 * Provides convenient ways to read typed data from a raw
 * byte array. Can be subclassed to support complex data
 * types.
 */
class BytestreamReader {

public:

  BytestreamReader() { }

  BytestreamReader(
          size_t                        size,
    const void*                         data)
  : m_data    (size ? reinterpret_cast<const char*>(data) : nullptr)
  , m_size    (size)
  , m_offset  (0) { }

  BytestreamReader(
          std::pair<size_t, const void*> memoryRange)
  : BytestreamReader(memoryRange.first, memoryRange.second) { }

  /**
   * \brief Queries internal read offset
   * \returns Internal read offset
   */
  size_t getOffset() const {
    return m_offset;
  }

  /**
   * \brief Queries pointer at given offset
   *
   * \param [in] offset Offset, in bytes
   * \returns Pointer to data
   */
  const void* getData(size_t offset) const {
    return static_cast<const void*>(m_data + offset);
  }

  /**
   * \brief Retrieves memory range at a given offset
   *
   * Will return a null range if the desired range would
   * be out of bounds. The resulting object can be used
   * to construct another bytestream reader.
   * \param [in] offset Offset, in bytes
   * \param [in] size Size, in bytes
   * \returns Memory range
   */
  std::pair<size_t, const void*> getMemory(size_t offset, size_t size) const {
    if (offset > m_size || size > m_size || offset + size > m_size)
      return std::make_pair(0, nullptr);

    return std::make_pair(size, getData(offset));
  }

  /**
   * \brief Skips given number of bytes
   *
   * Only advances internal read offset without reading data.
   * \returns \c true on success, or \c false if \c size
   *    was larger than the amount of memory remaining.
   */
  bool skip(size_t size) {
    if (unlikely(m_offset + size > m_size))
      return false;

    m_offset += size;
    return true;
  }

  /**
   * \brief Reads given number of bytes
   *
   * \returns \c true on success, or \c false
   *    if not enough bytes could be extracted.
   */
  bool read(size_t size, void* dst) {
    if (unlikely(m_offset + size > m_size))
      return false;

    std::memcpy(dst, m_data + m_offset, size);
    m_offset += size;
    return true;
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
    return read(sizeof(value), &value);
  }

  /**
   * \brief Checks whether the reader has backing storage
   * \returns \c false if the underlying array is null.
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
 * \brief Bytestream writer
 *
 * Provides convenient ways to write typed data to
 * a raw byte array. Can be subclassed to support
 * complex data types.
 */
class BytestreamWriter {

public:

  BytestreamWriter() { }

  /**
   * \brief Creates bytestream from existing buffer
   *
   * Takes ownership of the given data buffer.
   * Write calls will append data to the buffer.
   */
  explicit BytestreamWriter(
          std::vector<char>&&           data)
  : m_data(std::move(data)) { }

  /**
   * \brief Queries current size
   * \returns Current size
   */
  size_t getSize() const {
    return m_data.size();
  }

  /**
   * \brief Writes given number of bytes
   *
   * \param [in] size Number of bytes to write
   * \param [in] data Data to write
   */
  void write(size_t size, const void* data) {
    size_t oldSize = m_data.size();
    m_data.resize(oldSize + size);
    std::memcpy(&m_data[oldSize], data, size);
  }

  /**
   * \brief Writes trivial data type
   * \param [in] value Data to write to the stream
   */
  template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, bool> = true>
  void write(const T& value) {
    return write(sizeof(value), &value);
  }

  /**
   * \brief Allocates memory
   *
   * Resizes the internal array and returns a pointer to the start
   * of the reserved area. The returned pointer will be invalidated
   * by any subsequent writes or resize operations.
   * \param [in] size Number of bytes to allocate
   * \returns Allocated data
   */
  void* alloc(size_t size) {
    size_t oldSize = m_data.size();
    m_data.resize(oldSize + size);
    return &m_data[oldSize];
  }

  /**
   * \brief Reserves memory
   *
   * Can be used to avoid frequent allocations.
   * \param [in] capacity Number of bytes to reserve
   *    in addition to the current size of the writer.
   */
  void reserve(size_t capacity) {
    m_data.reserve(m_data.size() + capacity);
  }

  /**
   * \brief Retrieves data
   *
   * In order to avoid a copy, this will extract
   * the internal array and reset the object to
   * a default-initialized state.
   * \returns Byte array
   */
  std::vector<char> getData() && {
    return std::move(m_data);
  }

private:

  std::vector<char> m_data;

};

}
