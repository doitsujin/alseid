#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "util_likely.h"
#include "util_stream.h"

namespace as {

/**
 * \brief Bitstream reader
 *
 * Helper class that allows reading individual bits from
 * a memory stream in a reasonably performant manner.
 */
class BitstreamReader {

public:

  BitstreamReader() { }

  /**
   * \brief Initializes bitstream reader
   * \param [in] input Stream to read from
   */
  explicit BitstreamReader(
          RdMemoryView&                 input)
  : m_stream  (&input)
  , m_curr    (readQword())
  , m_next    (readQword()) { }

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

  RdMemoryView* m_stream  = nullptr;

  uint32_t  m_bit     = 0;
  uint64_t  m_curr    = 0;
  uint64_t  m_next    = 0;

  uint64_t readQword() {
    uint64_t result = 0;
    m_stream->load(&result, sizeof(result));
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

  /**
   * \brief Initializes bitstream writer
   *
   * \param [in] stream Byte stream to write to
   */
  BitstreamWriter(
          WrBufferedStream&             stream)
  : m_stream(&stream) { }

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
   * \returns \c true on success, \c false otherwise
   */
  bool write(uint64_t data, uint32_t bits) {
    const uint64_t qword = uint64_t(data) & ~(~1ull << (bits - 1));

    m_buffer |= qword << m_bit;
    m_bit += bits;

    if (likely(m_bit < 64))
      return true;

    bool success = m_stream->write(&m_buffer, sizeof(m_buffer));
    m_bit -= 64;

    // Handle the special case where the number of remaining
    // bits is zero since we might otherwise shift by 64
    m_buffer = m_bit ? qword >> (bits - m_bit) : 0ull;
    return success;
  }

  /**
   * \brief Flushes pending writes
   *
   * Must be called after writing the final bits to the
   * stream. This can be called explicitly in order to
   * separate bit streams as it will end the current byte.
   */
  bool flush() {
    uint32_t byteCount = (m_bit  + 7) / 8;
    bool success = m_stream->write(&m_buffer, byteCount);

    m_buffer = 0;
    m_bit = 0;

    success &= m_stream->flush();
    return success;
  }

private:

  WrBufferedStream* m_stream = nullptr;

  uint32_t  m_bit     = 0;
  uint64_t  m_buffer  = 0;

};

}
