#include <array>
#include <unordered_map>

#include "util_lzss.h"
#include "util_math.h"

namespace as {

class LzssEncoder {
  // Maximum length of uncompressed data between control words.
  // The length is encoded using either 6 or 14 bits, and for
  // the 14-bit encoding we'll add 64 to the encoded number.
  constexpr static size_t MaxSequenceLength = 16448;

  // Maximum sliding window size based on relative offset encoding.
  constexpr static size_t MaxSlidingWindow = 65536;

  // Maximum length of patterns we'll encode. The length is
  // encoded in 6 bits, with the minimum being 4 bytes.
  constexpr static size_t MaxPatternLength = 67;
public:

  explicit LzssEncoder(
          size_t                        window)
  : m_window    (std::min(window, MaxSlidingWindow))
  , m_freeCount (m_window) {
    for (size_t i = 0; i < m_freeCount; i++)
      m_free[i] = m_freeCount - i - 1;
  }


  bool encode(
          WrBufferedStream&             output,
    const void*                         data,
          size_t                        size) {
    WrStream writer(output);

    // pattern -> offset lookup table for prior pattern occurences.
    // We use dwords to encode the pattern here since that's the
    // smallest unit of data that we'll compress.
    std::unordered_multimap<uint32_t, size_t> lut;
    auto src = reinterpret_cast<const uint8_t*>(data);

    // Length of current uncompressed sequence
    size_t sequenceLength = 0;
    size_t skipLength = 0;

    // Encode actual data
    bool success = true;

    for (size_t i = 0; i < size; i++) {
      size_t matchLength = 0;
      size_t matchOffset = 0;

      if (i + sizeof(uint32_t) <= size) {
        uint32_t dw = 0;
        std::memcpy(&dw, &src[i], sizeof(dw));

        if (!skipLength) {
          // Find longest match closest to the current source pointer.
          // This way we may end up with smaller offset encodings.
          int32_t nodeId = findLut(dw);

          while (nodeId >= 0) {
            size_t offset = m_nodes[nodeId].offset;
            nodeId = m_nodes[nodeId].next;

            size_t maxLength = std::min(MaxPatternLength,
              std::min(size - i, i - offset));

            if (maxLength < 4)
              continue;

            size_t m = match(&src[offset], &src[i], maxLength);

            if (m >= 4 && m > matchLength) {
              matchOffset = offset;
              matchLength = m;
            }
          }
        }

        if (i >= m_window) {
          uint32_t pattern = 0;
          std::memcpy(&pattern, &src[i - m_window], sizeof(pattern));
          removeLut(pattern);
        }

        insertLut(dw, i);
      }

      if (!skipLength) {
        if (matchLength) {
          if (sequenceLength) {
            success &= emitSequence(output, sequenceLength, &src[i - sequenceLength]);
            sequenceLength = 0;
          }

          success &= emitRepetition(output, i - matchOffset, matchLength);
          skipLength = matchLength - 1;
        } else {
          sequenceLength += 1;

          if (sequenceLength == MaxSequenceLength || i + 1 == size) {
            success &= emitSequence(output, sequenceLength, &src[i + 1 - sequenceLength]);
            sequenceLength = 0;
          }
        }
      } else {
        skipLength -= 1;
      }
    }

    return success;
  }

private:

  struct List {
    int32_t head = -1;
    int32_t tail = -1;
  };

  struct Node {
    size_t offset;
    int32_t next;
    int32_t prev;
  };

  size_t m_window;

  std::unordered_map<uint32_t, List>    m_lut;
  std::array<Node,    MaxSlidingWindow> m_nodes;
  std::array<int32_t, MaxSlidingWindow> m_free;

  size_t m_freeCount;


  size_t match(
    const uint8_t*                      a,
    const uint8_t*                      b,
          size_t                        maxLength) {
    size_t qwordCount = maxLength / sizeof(uint64_t);

    for (size_t i = 0; i < qwordCount; i++) {
      uint64_t qa, qb;

      std::memcpy(&qa, &a[sizeof(qa) * i], sizeof(qa));
      std::memcpy(&qb, &b[sizeof(qb) * i], sizeof(qb));

      uint64_t delta = qa ^ qb;

      if (delta) {
        uint64_t mismatchByte = tzcnt(delta) / 8;
        return i * sizeof(uint64_t) + mismatchByte;
      }
    }

    for (size_t i = qwordCount * sizeof(uint64_t); i < maxLength; i++) {
      if (a[i] != b[i])
        return i;
    }

    return maxLength;
  }


  bool emitSequence(
          WrBufferedStream&             output,
          size_t                        length,
    const void*                         data) {
    WrStream writer(output);

    if (!length || length > size_t(1u << 14) + 64)
      return !length;

    bool success = true;

    // Emit a control word. The format is as follows:
    // - For sequences up to 64 bytes, subtract 1 from the
    //   length and encode the number as 6 bits: 00xxxxxx.
    // - For longer sequences, subtract 65 and encode the
    //   number as 14 bits: 01xxxxxx yyyyyyyy.
    size_t encodedLength = length - 1;

    if (encodedLength < 64) {
      success &= writer.write(uint8_t(encodedLength));
    } else {
      encodedLength -= 64;
      success &= writer.write(uint8_t(0x40 | (encodedLength >> 8)));
      success &= writer.write(uint8_t(encodedLength));
    }

    success &= writer.write(data, length);
    return success;
  }


  bool emitRepetition(
          WrBufferedStream&             output,
          size_t                        offset,
          size_t                        length) {
    WrStream writer(output);

    // We don't try to compress any patterns smaller than 4 bytes
    length -= 4;

    if (length >= size_t(1u << 6))
      return false;

    bool success = true;

    // We're not doing anything special with the offset, just
    // subtract 1 and write it out as either 1 or 2 bytes
    offset -= 1;

    if (offset < size_t(1u << 8)) {
      success &= writer.write(uint8_t(0x80 | length));
      success &= writer.write(uint8_t(offset));
    } else if (offset < size_t(1u << 16)) {
      success &= writer.write(uint8_t(0xC0 | length));
      success &= writer.write(uint16_t(offset));
    } else {
      // Too long
      return false;
    }

    return success;
  }


  int32_t findLut(
          uint32_t                      pattern) const {
    auto entry = m_lut.find(pattern);

    if (entry == m_lut.end())
      return -1;

    return entry->second.head;
  }


  void insertLut(
          uint32_t                      pattern,
          size_t                        offset) {
    int32_t nodeId = m_free[--m_freeCount];
    auto insert = m_lut.emplace(std::piecewise_construct,
      std::tuple(pattern), std::tuple());

    List& list = insert.first->second;

    if (insert.second) {
      list.head = nodeId;
      list.tail = nodeId;

      Node& node = m_nodes[nodeId];
      node.offset = offset;
      node.next = -1;
      node.prev = -1;
    } else {
      Node& node = m_nodes[nodeId];
      node.offset = offset;
      node.next = list.head;
      node.prev = -1;

      m_nodes[node.next].prev = nodeId;
      list.head = nodeId;
    }
  }


  void removeLut(
          uint32_t                      pattern) {
    auto entry = m_lut.find(pattern);

    if (entry == m_lut.end())
      return;

    List& list = entry->second;
    m_free[m_freeCount++] = list.tail;

    list.tail = m_nodes[list.tail].prev;

    if (list.tail == -1)
      m_lut.erase(entry);
    else
      m_nodes[list.tail].next = -1;
  }

};


bool lzssEncode(
        WrBufferedStream&               output,
        RdMemoryView                    input,
        size_t                          window) {
  LzssEncoder encoder(window);

  return encoder.encode(output,
    input.getData(), input.getSize());
}


bool lzssDecode(
        WrMemoryView                    output,
        RdMemoryView                    input) {
  RdStream reader(input);

  auto dst = reinterpret_cast<uint8_t*>(output.getData());

  size_t size = output.getSize();
  size_t written = 0;

  while (written < size) {
    uint8_t control = 0;

    if (!reader.read(control))
      return false;

    if (control & 0x80) {
      size_t length = (control & 0x3F) + 4;
      size_t offset = 0;

      bool success = (control & 0x40)
        ? reader.readAs<uint16_t>(offset)
        : reader.readAs<uint8_t>(offset);

      offset += 1;

      if (!success || written + length > size || offset > written)
        return false;

      std::memcpy(&dst[written], &dst[written - offset], length);
      written += length;
    } else {
      size_t length = (control & 0x3F);

      if (control & 0x40) {
        uint8_t control2 = 0;

        if (!reader.read(control2))
          return false;

        length = (length << 8) + control2 + 64;
      }

      length += 1;

      if (length + written > size)
        return false;

      if (!reader.read(&dst[written], length))
        return false;

      written += length;
    }
  }

  return true;
}

}
