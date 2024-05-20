#include <tuple>

#include "util_stream.h"

namespace as {

RdBufferedStream::~RdBufferedStream() {

}


size_t RdBufferedStream::readComplex(void* dst, size_t size) {
  // Read remaining buffered data first, if any
  size_t remaining = m_bufferSize - m_bufferOffset;

  if (remaining) {
    std::memcpy(dst, m_buffer.data() + m_bufferOffset, remaining);
    dst = reinterpret_cast<char*>(dst) + remaining;
    size -= remaining;
  }

  if (size >= m_buffer.size()) {
    // If the remaining size to read exceeds the size of
    // the internal buffer, read from the source directly
    m_bufferSize = 0;
    m_bufferOffset = 0;

    size_t read = readFromSource(dst, size);
    return read + remaining;
  } else {
    // Otherwise, refill the internal buffer and read that
    m_bufferSize = readFromSource(m_buffer.data(), m_buffer.size());

    size_t read = std::min(size, m_bufferSize);
    std::memcpy(dst, m_buffer.data(), read);

    m_bufferOffset = read;
    return read + remaining;
  }
}


size_t RdBufferedStream::skipComplex(size_t size) {
  // Skip remaining buffered data first and flush buffer
  size -= m_bufferSize - m_bufferOffset;

  m_bufferSize = 0;
  m_bufferOffset = 0;

  return readFromSource(nullptr, size);
}




WrBufferedStream::~WrBufferedStream() {

}


bool WrBufferedStream::flush() {
  size_t written;
  std::tie(written, m_bufferSize) = writeToContainer(m_buffer.data(), m_bufferOffset);

  bool success = written == m_bufferOffset;

  m_bufferOffset = 0;
  m_bufferSize = std::min(m_bufferSize, m_buffer.size());
  return success;
}


bool WrBufferedStream::writeComplex(const void* src, size_t size) {
  // Write as much data to the buffer as we can before flushing
  size_t written = m_bufferSize - m_bufferOffset;

  if (written) {
    std::memcpy(m_buffer.data() + m_bufferOffset, src, written);
    src = reinterpret_cast<const char*>(src) + written;
    size -= written;

    m_bufferOffset += written;
  }

  if (!flush())
    return false;

  // Flush will set the buffer size to the actual amount of
  // data that we can write in one go, so check against that
  if (size >= m_bufferSize) {
    std::tie(written, m_bufferSize) = writeToContainer(src, size);
    m_bufferSize = std::min(m_bufferSize, m_buffer.size());
    return written == size;
  } else {
    std::memcpy(m_buffer.data(), src, size);
    m_bufferOffset = size;
    return true;
  }
}




WrVectorStream::WrVectorStream(
        std::vector<char>&            vector)
: m_vector(vector) {

}


WrVectorStream::~WrVectorStream() {
  flush();
}


std::pair<size_t, size_t> WrVectorStream::writeToContainer(
  const void*                         data,
        size_t                        size) {
  size_t oldSize = m_vector.size();
  size_t newSize = oldSize + size;

  m_vector.resize(newSize);

  if (likely(size))
    std::memcpy(&m_vector[oldSize], data, size);

  return std::make_pair(size, size_t(-1));
}

}