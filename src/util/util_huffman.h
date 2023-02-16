#pragma once

#include <array>
#include <vector>

#include "util_bitstream.h"

namespace as {

/**
 * \brief Huffman code point counter
 *
 * Stores the number of times each 16-bit code point
 * occured in a given input stream.
 */
class HuffmanCounter {

public:

  constexpr static size_t MaxCodeCount = size_t(1u << 16);

  /**
   * \brief Zero-initializes counters
   */
  HuffmanCounter();

  /**
   * \brief Adds counters from another stream
   *
   * This is useful when accumulating data
   * from multiple streams.
   * \param [in] data Byte stream
   * \param [in] size Data size, in bytes
   */
  void add(
    const void*                         data,
          size_t                        size);

  /**
   * \brief Adds counters from a stream object
   *
   * Convenience method that reads as many
   * bytes as possible from the given stream.
   * \param [in] reader Stream to read from
   */
  void add(
          InStream&                     reader);

  /**
   * \brief Accumulates counters from another counter
   *
   * This is useful when performing compression in parallel.
   * \param [in] counter The other counter struct to add
   */
  void accumulate(
    const HuffmanCounter&               counter);

  /**
   * \brief Queries counter for a given code point
   *
   * \param [in] code Code point
   * \returns Number of occurences of that code point
   */
  uint64_t operator [] (uint16_t code) const {
    return m_counts[code];
  }

  /**
   * \brief Iterator pair over unique codes
   * \returns Iterator pair over unique codes
   */
  auto getUniqueCodes() const {
    return std::make_pair(m_codes.begin(), m_codes.begin() + m_codeCount);
  }

private:

  std::array<uint64_t, MaxCodeCount> m_counts = { };
  std::array<uint16_t, MaxCodeCount> m_codes = { };

  uint32_t m_codeCount = 0;

  void count(
          uint16_t                      code,
          uint64_t                      count);

};


/**
 * \brief Huffman encoding table
 *
 * Accelerated data structure created from a Huffman trie
 * that can be used for encoding data. This structure is
 * large and should not be copied needlessly.
 */
class HuffmanEncoder {
  friend class HuffmanTrie;
public:

  HuffmanEncoder();

  ~HuffmanEncoder();

  /**
   * \brief Encodes data
   *
   * \param [in] stream Bit stream to write to
   * \param [in] size Number of bytes to encode
   * \param [in] data Data to encode
   * \returns \c true on success, \c false on error
   */
  bool encode(
          BitstreamWriter&              stream,
    const void*                         data,
          size_t                        size) const;

  /**
   * \brief Encodes data
   *
   * \param [in] stream Bit stream to write to
   * \param [in] reader Source data stream
   * \returns \c true on success, \c false on error
   */
  bool encode(
          BitstreamWriter&              stream,
          InStream&                     reader) const;

  /**
   * \brief Computes encoded data size in bytes
   *
   * \param [in] counter Corresponding counter
   * \returns Byte count of encoded file
   */
  uint64_t computeEncodedSize(
    const HuffmanCounter&               counter) const;

private:

  struct Entry {
    uint32_t bitCount;
    uint32_t codeBitsLo;
    uint32_t codeBitsHi;
  };

  std::array<Entry, HuffmanCounter::MaxCodeCount> m_entries = { };

  void setCode(
          uint16_t                      code,
          uint32_t                      bitCount,
          uint64_t                      codeBits);

  static uint64_t getCodeBits(
    const Entry&                        e);

};


/**
 * \brief Huffman decoding table
 *
 * Accelerated data structure for decoding compressed data.
 * Also provides methods to write the decoding table to a
 * stream in a compact representation, and to load the same
 * representation from a stream.
 */
class HuffmanDecoder {
  constexpr static size_t MaxCodeCount = HuffmanCounter::MaxCodeCount;
  constexpr static size_t MaxNodeCount = MaxCodeCount * 2 - 1;

  friend class HuffmanTrie;
public:

  HuffmanDecoder();

  /**
   * \brief Decodes bit stream
   *
   * \param [in] writer Byte stream to write to
   * \param [in] stream Bit stream
   * \param [in] size Size of the output stream, in bytes
   * \returns \c true on success, \c false on error
   */
  bool decode(
          OutStream&                    writer,
          BitstreamReader&              stream,
          size_t                        size) const;

  /**
   * \brief Reads decoding table from bit stream
   *
   * \param [in] stream Bit stream to read from
   * \returns \c true on success, \c false on error
   */
  bool read(
          BitstreamReader&              stream);

  /**
   * \brief Writes decoding table to bit stream
   * \param [in] stream Bit stream to write to
   */
  bool write(
          BitstreamWriter&              stream) const;

  /**
   * \brief Computes decoding table size, in bytes
   * \returns Decoding table size, in bytes
   */
  size_t computeSize() const;

private:

  struct Entry {
    uint8_t bits;
    uint8_t next;
    uint16_t data;
  };

  uint32_t                        m_entryCount = 0;
  std::array<Entry, MaxNodeCount> m_entries = { };

  std::array<Entry, MaxCodeCount> m_lookup = { };

  uint32_t allocate(
          uint32_t                      depth);

  void setLeafEntry(
          uint32_t                      entry,
          uint16_t                      code);

  void setDecodeEntry(
          uint32_t                      entry,
          uint32_t                      bits,
          uint32_t                      offset);

  void createLookupTable();

  static uint16_t encodeOffset(
          uint32_t                      offset);

  static uint32_t decodeOffset(
          uint16_t                      compressed);

};


/**
 * \brief Huffman trie
 *
 * Constructs a huffman trie from an arbitrary byte
 * stream and allows creating encoding and decoding
 * tables from it.
 */
class HuffmanTrie {
  constexpr static size_t MaxNodeCount = HuffmanCounter::MaxCodeCount * 2 - 1;
public:

  HuffmanTrie();

  /**
   * \brief Constructs trie from existing counter struct
   * \param [in] counter Counter struct
   */
  HuffmanTrie(
    const HuffmanCounter&               counter);

  ~HuffmanTrie();

  /**
   * \brief Creates encoder from trie
   * \returns Huffan encoder
   */
  HuffmanEncoder createEncoder() const;

  /**
   * \brief Creates decoder from trie
   * \returns Huffman decoder
   */
  HuffmanDecoder createDecoder() const;

private:

  struct BuildNode {
    uint64_t value;
    uint32_t index;

    bool operator < (const BuildNode& other) const {
      return value > other.value;
    }
  };

  struct TrieNode {
    uint32_t left;
    uint32_t right;
    uint32_t code;
  };

  size_t m_nodeCount = 0;

  std::array<TrieNode, MaxNodeCount> m_nodes = { };

  void populateEncoder(
          HuffmanEncoder&               encoder,
          uint32_t                      nodeIndex,
          uint32_t                      bitCount,
          uint64_t                      codeBits) const;

  uint32_t getDecodingDepth(
          uint32_t                      nodeIndex) const;

  uint32_t traverse(
          uint32_t                      nodeIndex,
          uint32_t                      depth,
          uint32_t                      bits) const;

};


/**
 * \brief Encodes binary using Huffman compression
 *
 * Convenience function to encode a single blob.
 * The resulting binary will store the full decoding
 * table in addition to the compressed binary itself.
 * \param [in] writer Byte stream to write to
 * \param [in] data Data to encode
 * \param [in] size Number of bytes to encode
 * \returns \c true on success
 */
bool encodeHuffmanBinary(
        OutStream&                    writer,
  const void*                         data,
        size_t                        size);


/**
 * \brief Decodes binary using Huffman compression
 *
 * Can be used to decode binaries compressed with the
 * \ref encodeHuffmanBinary method.
 * \param [in] writer Byte stream to write to
 * \param [in] size Number of bytes to encode
 * \param [in] data Data to encode
 * \returns \c true on success
 */
bool decodeHuffmanBinary(
        OutStream&                    writer,
        InStream&                     reader);

}
