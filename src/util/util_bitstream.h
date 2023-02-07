#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "util_bytestream.h"
#include "util_likely.h"

namespace as {

/**
 * \brief Bitstream reader
 *
 * Helper class that allows reading individual bits
 * from an array in a reasonably performant manner.
 */
class BitstreamReader {

public:

  BitstreamReader() { }

  /**
   * \brief Initializes bitstream reader
   *
   * \param [in] size Size of the source array, in bytes
   * \param [in] data Source data
   */
  BitstreamReader(
          size_t                        size,
    const void*                         data)
  : m_data    (reinterpret_cast<const char*>(data))
  , m_size    (size)
  , m_curr    (readQword())
  , m_next    (readQword()) { }

  /**
   * \brief Initializes bitstream reader from memory range
   *
   * Used when creating a bitstream from a \ref BytestreamReader.
   * \param [in] memoryRange Memory range
   */
  BitstreamReader(
          std::pair<size_t, const void*> memoryRange)
  : BitstreamReader(memoryRange.first, memoryRange.second) { }

  /**
   * \brief Reads bits from source without advancing pointer
   *
   * The number of bits to read \e must be between 1 and 64.
   * \param [in] bits Number of bits to read
   * \returns Bits read from the source array
   */
  uint64_t peek(uint32_t bits) {
    if (likely(m_bit + bits < 64))
      return (m_curr >> m_bit) & ~(~1ull << (bits - 1));

    uint32_t read = (64 - m_bit);
    uint64_t data = m_curr >> m_bit;

    if (bits - read)
      data |= (m_next & ~(~1ull << (bits - read - 1))) << read;

    return data;
  }

  /**
   * \brief Reads bits from the source array
   *
   * The number of bits to read \e must be between 1 and 64.
   * \param [in] bits Number of bits to read
   * \returns Bits read from the source array
   */
  uint64_t read(uint32_t bits) {
    uint64_t data = peek(bits);
    m_bit += bits;

    if (likely(m_bit < 64))
      return data;

    m_bit -= 64;

    m_curr = m_next;
    m_next = readQword();
    return data;
  }

private:

  const char* m_data = nullptr;

  size_t    m_size    = 0;
  size_t    m_offset  = 0;

  uint32_t  m_bit     = 0;
  uint64_t  m_curr    = 0;
  uint64_t  m_next    = 0;

  uint64_t readQword() {
    uint64_t result = 0;

    if (likely(m_offset + sizeof(result) <= m_size)) {
      // Fast path, always read 8 bytes at once if we can
      std::memcpy(&result, &m_data[m_offset], sizeof(result));
      m_offset += sizeof(result);
      return result;
    } else if (m_offset < m_size) {
      // Read as many bytes as there are left in the stream
      size_t size = m_size - m_offset;
      std::memcpy(&result, &m_data[m_offset], size);
      m_offset += size;
      return result;
    }

    return result;
  }

};


/**
 * \brief Bitstream writer
 *
 * Helper class that allows writing individual bits
 * to an array in a reasonably performant manner.
 */
class BitstreamWriter {

public:

  BitstreamWriter() { }

  BitstreamWriter(
          BytestreamWriter&             byteStream)
  : m_byteStream(&byteStream) { }

  ~BitstreamWriter() {
    if (m_bit)
      flush();
  }

  /**
   * \brief Writes data to the bit stream
   *
   * The number of bits to write \e must be between 1 and 64.
   * \param [in] data Data to write
   * \param [in] bits Number of bits to write from data
   */
  void write(uint64_t data, uint32_t bits) {
    const uint64_t qword = uint64_t(data) & ~(~1ull << (bits - 1));

    m_buffer |= qword << m_bit;
    m_bit += bits;

    if (unlikely(m_bit >= 64)) {
      m_byteStream->write(m_buffer);

      m_bit -= 64;

      // Handle the special case where the number of remaining
      // bits is zero since we might otherwise shift by 64
      m_buffer = m_bit ? qword >> (bits - m_bit) : 0ull;
    }
  }

  /**
   * \brief Flushes pending writes
   *
   * Must be called after writing the final bits to the
   * stream. This can be called explicitly in order to
   * separate bit streams as it will end the current byte.
   */
  void flush() {
    uint32_t byteCount = (m_bit  + 7) / 8;
    m_byteStream->write(byteCount, &m_buffer);

    m_buffer = 0;
    m_bit = 0;
  }

private:

  BytestreamWriter* m_byteStream = nullptr;

  uint32_t  m_bit     = 0;
  uint64_t  m_buffer  = 0;

};

}
