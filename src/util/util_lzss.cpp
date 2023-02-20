#include <array>
#include <optional>
#include <unordered_map>

#include "util_log.h"
#include "util_lzss.h"
#include "util_math.h"

namespace as {

/**
 * \brief LZSS control word helper
 */
struct LzssControlWord {
  uint32_t patternLength = 0;
  uint32_t patternOffset = 0;
  size_t sequenceLength = 0;
};


class LzssEncoder {
  // Maximum sliding window size based on relative offset encoding.
  constexpr static size_t MaxSlidingWindow = 65536;

  // Maximum length of patterns we'll encode. The length is
  // encoded in up to 8 bits, with the minimum being 4 bytes.
  constexpr static size_t MinPatternLength = 4;
  constexpr static size_t MaxPatternLength = 259;
public:

  explicit LzssEncoder(
          size_t                        window)
  : m_window    (window ? std::min(window, MaxSlidingWindow) : MaxSlidingWindow)
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

    // Current control block properties
    LzssControlWord control = { };
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

      if (skipLength) {
        skipLength -= 1;
        continue;
      }

      if (!matchLength) {
        control.sequenceLength += 1;
        continue;
      }

      if (control.sequenceLength || control.patternLength)
        success &= emitControlBlock(output, control, &src[i - control.sequenceLength]);

      control.patternOffset = i - matchOffset;
      control.patternLength = matchLength;
      control.sequenceLength = 0;

      skipLength = matchLength - 1;
    }

    if (control.sequenceLength || control.patternLength)
      success &= emitControlBlock(output, control, &src[size - control.sequenceLength]);

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
    size_t offset = 0;

#ifdef AS_HAS_X86_INTRINSICS
    // If possible, compare 16 bytes at once to compare performance, and
    // find the first mismatching byte based on the mask of differences.
    size_t doubleQuadCount = maxLength / sizeof(__m128i);
    offset = doubleQuadCount * sizeof(__m128i);

    for (size_t i = 0; i < doubleQuadCount; i++) {
      __m128i dqa = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a) + i);
      __m128i dqb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b) + i);

      uint32_t diffMask = _mm_movemask_epi8(_mm_cmpeq_epi8(dqa, dqb)) ^ 0xFFFF;

      if (diffMask)
        return i * sizeof(__m128i) + tzcnt(diffMask);
    }
#endif

    // Same idea as above, compare 8 bytes at once.
    size_t qwordCount = maxLength / sizeof(uint64_t);
    size_t qwordOffset = offset / sizeof(uint64_t);
    offset = qwordCount * sizeof(uint64_t);

    for (size_t i = qwordOffset; i < qwordCount; i++) {
      uint64_t qa, qb;

      std::memcpy(&qa, &a[sizeof(uint64_t) * i], sizeof(uint64_t));
      std::memcpy(&qb, &b[sizeof(uint64_t) * i], sizeof(uint64_t));

      uint64_t delta = qa ^ qb;

      if (delta) {
        uint64_t mismatchByte = tzcnt(delta) / 8;
        return i * sizeof(uint64_t) + mismatchByte;
      }
    }

    // If we still have bytes to check, don't bother optimizing
    // this further, we'll rarely hit this code path anyway.
    for (size_t i = offset; i < maxLength; i++) {
      if (a[i] != b[i])
        return i;
    }

    return maxLength;
  }


  bool emitControlBlock(
          WrBufferedStream&             output,
    const LzssControlWord&              control,
    const void*                         data) {
    WrStream writer(output);

    // Encode the first byte of the sequence length. This is
    // important as we need to know the continuation bit.
    uint32_t seqHead = uint8_t(control.sequenceLength & 0xF);

    if (control.sequenceLength > 0xF)
      seqHead |= 0x10;

    // Control words are encoded using the following format. Sequence
    // lengths are encoded with variable length, with the msb of each
    // byte serving as a continuaton marker and up to 7 bits of data.
    // pfx   len   ofs   seq
    // 0     3     7     5+
    // 10    4     13    5+
    // 110   8     16    5+
    // 111   -     -     5+

    // Choose a block encoding based on pattern properties.
    bool success = true;

    if (control.patternLength) {
      uint32_t length = control.patternLength - MinPatternLength;
      uint32_t offset = control.patternOffset - 1;

      if (length < 0x8 && offset < 0x80) {
        uint32_t word = (length << 12) | (offset << 5) | seqHead;

        success &= writer.write(uint8_t(word >> 8))
                && writer.write(uint8_t(word >> 0));
      } else if (length < 0x10 && offset < 2000) {
        uint32_t word = (0x1 << 23) | (length << 18) | (offset << 5) | seqHead;

        success &= writer.write(uint8_t(word >> 16))
                && writer.write(uint8_t(word >> 8))
                && writer.write(uint8_t(word >> 0));
      } else if (length < 0x100 && offset < 0x10000) {
        uint32_t word = (0x3 << 30) | (length << 21) | (offset << 5) | seqHead;

        success &= writer.write(uint8_t(word >> 24))
                && writer.write(uint8_t(word >> 16))
                && writer.write(uint8_t(word >> 8))
                && writer.write(uint8_t(word >> 0));
      } else {
        // Some number is too big somehow
        return false;
      }
    } else {
      uint32_t word = (0x7 << 5) | seqHead;

      success &= writer.write(uint8_t(word));
    }

    // Encode remaining bytes of sequence length,
    // least significant bits first in memory.
    if (control.sequenceLength > 0xF) {
      uint32_t length = control.sequenceLength >> 4;

      while (length) {
        uint8_t seqByte = length & 0x7F;

        if (length >>= 7)
          length |= 0x80;

        success &= writer.write(seqByte);
      }
    }

    // Write out sequence data, if any
    if (control.sequenceLength)
      success &= writer.write(data, control.sequenceLength);

    return success;
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


std::optional<LzssControlWord> lzssDecodeControlWord(
        RdMemoryView&                   input) {
  constexpr size_t MinPatternLength = 4;

  LzssControlWord result = { };
  RdStream reader(input);

  // Decode actual control word itself
  std::array<uint8_t, 4> bytes = { };
  uint32_t word = 0;

  if (!reader.read(bytes[0]))
    return std::nullopt;

  if (!(bytes[0] & 0x80)) {
    if (!reader.read(bytes[1]))
      return std::nullopt;

    word = (uint32_t(bytes[0]) <<  8)
         | (uint32_t(bytes[1]) <<  0);

    result.patternLength = bextract(word, 12,  3) + MinPatternLength;
    result.patternOffset = bextract(word,  5,  7) + 1;
  } else if (!(bytes[0] & 0x40)) {
    if (!reader.read(bytes[1])
     || !reader.read(bytes[2]))
      return std::nullopt;

    word = (uint32_t(bytes[0]) << 16)
         | (uint32_t(bytes[1]) <<  8)
         | (uint32_t(bytes[2]) <<  0);

    result.patternLength = bextract(word, 18,  4) + MinPatternLength;
    result.patternOffset = bextract(word,  5, 13) + 1;
  } else if (!(bytes[0] & 0x20)) {
    if (!reader.read(bytes[1])
     || !reader.read(bytes[2])
     || !reader.read(bytes[3]))
      return std::nullopt;

    word = (uint32_t(bytes[0]) << 24)
         | (uint32_t(bytes[1]) << 16)
         | (uint32_t(bytes[2]) <<  8)
         | (uint32_t(bytes[3]) <<  0);

    result.patternLength = bextract(word, 21,  8) + MinPatternLength;
    result.patternOffset = bextract(word,  5, 16) + 1;
  } else {
    // No pattern present, only a sequence length
    word = uint32_t(bytes[0]);
  }

  // Decode sequence length
  result.sequenceLength = word & 0xF;

  if (word & 0x10) {
    uint32_t shift = 4;
    uint8_t byte = 0;

    do {
      if (!reader.read(byte))
        return std::nullopt;

      result.sequenceLength |= uint32_t(byte & 0x7F) << shift;
      shift += 7;
    } while (byte & 0x80);
  }

  return result;
}


bool lzssDecode(
        WrMemoryView                    output,
        RdMemoryView                    input) {
  auto dst = reinterpret_cast<uint8_t*>(output.getData());

  size_t size = output.getSize();
  size_t written = 0;

  while (written < size) {
    auto control = lzssDecodeControlWord(input);

    if (!control)
      return false;

    // Validate against output bounds
    if (written + control->patternLength + control->sequenceLength > size)
      return false;

    if (control->patternLength) {
      if (control->patternOffset > written)
        return false;

      // The control word only encodes relative offsets
      size_t absoluteOffset = written - control->patternOffset;

      if (absoluteOffset + control->patternLength > written)
        return false;

      std::memcpy(&dst[written], &dst[absoluteOffset], control->patternLength);
      written += control->patternLength;
    }

    if (!input.read(&dst[written], control->sequenceLength))
      return false;

    written += control->sequenceLength;
  }

  return true;
}

}
