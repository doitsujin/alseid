#pragma once

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>
#include <vector>

#include "util_assert.h"
#include "util_bitstream.h"
#include "util_likely.h"
#include "util_small_vector.h"

namespace as {

template<typename T>
constexpr size_t HuffmanCodeCount = size_t(1u << (sizeof(T) * 8));

/**
 * \brief Huffman code point counter
 *
 * Stores the number of times each 8/16-bit code
 * occured in a given input stream.
 */
template<typename CodeType, typename CountType>
class HuffmanCounter {
  constexpr static size_t CodeCount = HuffmanCodeCount<CodeType>;
public:

  /**
   * \brief Zero-initializes counters
   */
  HuffmanCounter() {

  }

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
          size_t                        size) {
    size_t offset = 0;

    if constexpr (sizeof(CodeType) == 2) {
      auto words = reinterpret_cast<const uint16_t*>(data);

      for (size_t i = 0; i < size / 2; i++)
        count(words[i], 1);

      offset += size & ~size_t(1);
    }

    // Also handle trailing bytes for 16-bit encoders
    auto bytes = reinterpret_cast<const uint8_t*>(data);

    for (size_t i = offset; i < size; i++)
      count(bytes[i], 1);
  }

  /**
   * \brief Adds counters from a stream object
   *
   * Convenience method that reads as many
   * bytes as possible from the given stream.
   * \param [in] reader Stream to read from
   */
  void add(
          InStream&                     reader) {
    std::array<char, 4096> buffer;

    size_t read;

    do {
      read = reader.load(buffer.data(), buffer.size());
      add(buffer.data(), read);
    } while (read);
  }

  /**
   * \brief Accumulates counters from another counter
   *
   * This is useful when performing compression in parallel.
   * \param [in] counter The other counter struct to add
   */
  void accumulate(
    const HuffmanCounter&               counter) {
    for (uint32_t i = 0; i < counter.m_codeCount; i++) {
      CodeType code = counter.m_codes[i];
      count(code, counter.m_counts[code]);
    }
  }

  /**
   * \brief Queries counter for a given code point
   *
   * \param [in] code Code point
   * \returns Number of occurences of that code point
   */
  CountType operator [] (CodeType code) const {
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

  std::array<CountType, CodeCount> m_counts = { };
  std::array<CodeType,  CodeCount> m_codes = { };

  uint32_t m_codeCount = 0;

  void count(
          CodeType                      code,
          CountType                     count) {
    if (!m_counts[code])
      m_codes[m_codeCount++] = code;

    m_counts[code] += count;
  }

};


/**
 * \brief Huffman encoding table
 *
 * Accelerated data structure created from a Huffman trie
 * that can be used for encoding data. This structure is
 * large and should not be copied needlessly.
 */
template<typename CodeType>
class HuffmanEncoder {
  constexpr static size_t CodeCount = HuffmanCodeCount<CodeType>;

  template<typename CodeType_>
  friend class HuffmanTrie;
public:

  HuffmanEncoder() {

  }

  ~HuffmanEncoder() {

  }

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
          size_t                        size) const {
    bool success = true;
    size_t offset = 0;

    if constexpr (sizeof(CodeType) == 2) {
      auto words = reinterpret_cast<const uint16_t*>(data);

      for (size_t i = 0; i < size / 2; i++) {
        auto& e = m_entries[words[i]];
        success &= stream.write(getCodeBits(e), e.bitCount);
      }

      offset += size & ~size_t(1);
    }

    // Also handle trailing bytes for 16-bit encoders
    auto bytes = reinterpret_cast<const uint8_t*>(data);

    for (size_t i = offset; i < size; i++) {
      auto& e = m_entries[bytes[i]];
      success &= stream.write(getCodeBits(e), e.bitCount);
    }

    return success;
  }

  /**
   * \brief Encodes data
   *
   * \param [in] stream Bit stream to write to
   * \param [in] reader Source data stream
   * \returns \c true on success, \c false on error
   */
  bool encode(
          BitstreamWriter&              stream,
          InStream&                     reader) const {
    std::array<char, 4096> buffer;

    size_t read;

    do {
      read = reader.load(buffer.data(), buffer.size());

      if (!encode(stream, buffer.data(), read))
        return false;
    } while (read);

    return true;
  }

  /**
   * \brief Computes encoded data size in bytes
   *
   * \param [in] counter Corresponding counter
   * \returns Byte count of encoded file
   */
  template<typename CountType>
  uint64_t computeEncodedSize(
    const HuffmanCounter<CodeType, CountType>& counter) const {
    uint64_t bitCount = 0;

    auto iter = counter.getUniqueCodes();

    for (auto i = iter.first; i != iter.second; i++)
      bitCount += m_entries[*i].bitCount * counter[*i];

    return (bitCount + 7) / 8;
  }

private:

  struct Entry {
    uint32_t bitCount;
    uint32_t codeBitsLo;
    uint32_t codeBitsHi;
  };

  std::array<Entry, CodeCount> m_entries = { };

  void setCode(
          CodeType                      code,
          uint32_t                      bitCount,
          uint64_t                      codeBits) {
    auto& e = m_entries[code];

    e.bitCount = bitCount;
    e.codeBitsLo = uint32_t(codeBits);
    e.codeBitsHi = uint32_t(codeBits >> 32);
  }

  static uint64_t getCodeBits(
    const Entry&                        e) {
    // The code is split into two dwords so that the lookup
    // entry fits into 12 byts, so recombine them here.
    return uint64_t(e.codeBitsLo) | (uint64_t(e.codeBitsHi) << 32);
  }

};


/**
 * \brief Huffman decoding table
 *
 * Accelerated data structure for decoding compressed data.
 * Also provides methods to write the decoding table to a
 * stream in a compact representation, and to load the same
 * representation from a stream.
 */
template<typename CodeType>
class HuffmanDecoder {
  constexpr static uint32_t BitCount = 8u * sizeof(CodeType);
  constexpr static uint32_t LengthBits = 2 + sizeof(CodeType);

  constexpr static size_t CodeCount = HuffmanCodeCount<CodeType>;
  constexpr static size_t NodeCount = CodeCount * 2 - 1;

  template<typename CodeType_>
  friend class HuffmanTrie;
public:

  HuffmanDecoder() {

  }

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
          size_t                        size) const {
    bool success = true;

    for (uint32_t i = 0; i < size; i += sizeof(CodeType)) {
      // Perform a lookup in the optimized lookup table.
      // In many cases this will lead us to a leaf node.
      const Entry* e = &m_lookup[stream.peek(BitCount)];
      stream.read(e->bits);

      if (unlikely(e->next)) {
        // Perform slow lookups in the compact decoding
        // table as necessary
        uint32_t offset = decodeOffset(e->data);
        uint32_t index = stream.read(e->next);

        e = &m_entries[offset + index];

        while (e->bits) {
          offset = decodeOffset(e->data);
          index = stream.read(e->bits);

          e = &m_entries[offset + index];
        }
      }

      CodeType code = e->data;

      if (likely(i + sizeof(CodeType) <= size))
        success &= writer.write(code);
      else
        success &= writer.write(uint8_t(code));
    }

    return success;
  }

  /**
   * \brief Reads decoding table from bit stream
   *
   * \param [in] stream Bit stream to read from
   * \returns \c true on success, \c false on error
   */
  bool deserialize(
          BitstreamReader&              stream) {
    CodeType entryCountCompressed = CodeType(stream.read(BitCount));

    // The entry count is stored in a compressed way
    // so it only takes up two bytes, expand it here.
    m_entryCount = decodeOffset(entryCountCompressed);

    for (uint32_t i = 0; i < m_entryCount; i++) {
      Entry e = { };

      if (!stream.read(1))
        e.bits = uint8_t(stream.read(LengthBits)) + 1;

      e.data = CodeType(stream.read(BitCount));

      // Validate that entry is valid. Entry offsets are
      // compressed in the same way as the node count.
      if (e.bits && e.data >= entryCountCompressed)
        return false;

      m_entries[i] = e;
    }

    createLookupTable();
    return true;
  }

  /**
   * \brief Writes decoding table to bit stream
   * \param [in] stream Bit stream to write to
   */
  bool serialize(
          BitstreamWriter&              stream) const {
    bool success = true;

    // The entry count is always odd, so we don't need to store
    // the last bit. This way it will always fit into 8/16 bits.
    stream.write(encodeOffset(m_entryCount), BitCount);

    // We can compress each entry with 5 bits for the bit count,
    // since the depth is at most 8/16 for the worst case and zero
    // for leaf nodes. Offsets are compressed like the entry count.
    for (uint32_t i = 0; i < m_entryCount; i++) {
      auto& e = m_entries[i];

      if (e.bits) {
        success &= stream.write(0, 1);
        success &= stream.write(e.bits - 1, LengthBits);
      } else {
        success &= stream.write(1, 1);
      }

      success &= stream.write(e.data, BitCount);
    }

    return success;
  }

  /**
   * \brief Computes decoding table size, in bytes
   * \returns Decoding table size, in bytes
   */
  size_t computeSize() const {
    size_t bitCount = BitCount + (BitCount + 1) * m_entryCount;

    for (uint32_t i = 0; i < m_entryCount; i++) {
      if (m_entries[i].bits)
        bitCount += LengthBits;
    }

    return (bitCount + 7) / 8;
  }

private:

  struct Entry {
    uint8_t bits;
    uint8_t next;
    CodeType data;
  };

  uint32_t                     m_entryCount = 0;
  std::array<Entry, NodeCount> m_entries = { };

  std::array<Entry, CodeCount> m_lookup = { };

  uint32_t allocate(
          uint32_t                      depth) {
    uint32_t index = m_entryCount;
    m_entryCount += 1u << depth;
    return index;
  }

  void setLeafEntry(
          uint32_t                      entry,
          CodeType                      code) {
    Entry e = { };
    e.data = code;

    m_entries[entry] = e;
  }

  void setDecodeEntry(
          uint32_t                      entry,
          uint32_t                      bits,
          uint32_t                      offset) {
    Entry e = { };
    e.bits = uint8_t(bits);
    e.data = encodeOffset(offset);

    m_entries[entry] = e;
  }

  void createLookupTable() {
    for (size_t i = 0; i < m_lookup.size(); i++) {
      const Entry* e = &m_entries[0];
      uint32_t bits = 0;

      while (e->bits && bits + e->bits <= BitCount) {
        uint32_t offset = (uint32_t(e->data) << 1) + 1;
        uint32_t index = (i >> bits) & ((1u << e->bits) - 1);

        bits += e->bits;

        e = &m_entries[offset + index];
      }

      Entry lookupEntry = { };
      lookupEntry.bits = bits;
      lookupEntry.next = e->bits;
      lookupEntry.data = e->data;

      m_lookup[i] = lookupEntry;
    }
  }

  static CodeType encodeOffset(
          uint32_t                      offset) {
    return CodeType(offset >> 1);
  }

  static uint32_t decodeOffset(
          CodeType                      compressed) {
    return (uint32_t(compressed) << 1) + 1;
  }

};


/**
 * \brief Huffman trie
 *
 * Constructs a huffman trie from an arbitrary byte
 * stream and allows creating encoding and decoding
 * tables from it.
 */
template<typename CodeType>
class HuffmanTrie {
  constexpr static size_t CodeCount = HuffmanCodeCount<CodeType>;
  constexpr static size_t NodeCount = CodeCount * 2 - 1;
public:

  HuffmanTrie() {

  }

  /**
   * \brief Constructs trie from existing counter struct
   * \param [in] counter Counter struct
   */
  template<typename CountType>
  HuffmanTrie(
    const HuffmanCounter<CodeType, CountType>& counter) {
    // Write leaf nodes to the internal data structure
    std::array<BuildNode, CodeCount> nodeHeap;
    size_t nodeCount = 0;

    for (size_t i = 0; i < CodeCount; i++) {
      if (counter[i]) {
        size_t nodeIndex = nodeCount++;

        TrieNode trieNode = { };
        trieNode.code = CodeType(i);
        m_nodes[nodeIndex] = trieNode;

        BuildNode buildNode = { };
        buildNode.value = counter[i];
        buildNode.index = nodeIndex;
        nodeHeap[nodeIndex] = buildNode;

        std::push_heap(nodeHeap.begin(), nodeHeap.begin() + nodeCount);
      }
    }

    // If the trie is empty, create a dummy node for code 0.
    if (nodeCount < 1) {
      m_nodes[0] = TrieNode();
      nodeHeap[0] = BuildNode();

      nodeCount = 1;
    }

    // If the trie contains only one node, duplicate it so
    // that we don't have to deal with it as a special case
    if (nodeCount < 2) {
      m_nodes[1] = m_nodes[0];

      nodeHeap[1] = nodeHeap[0];
      nodeHeap[1].index = 1;

      nodeCount = 2;
    }

    m_nodeCount = nodeCount;

    // Now, until the node count is 1, combine the two nodes
    // with the smallest number of occurences into one
    while (nodeCount > 1) {
      TrieNode trieNode = { };
      trieNode.left = nodeHeap[0].index;

      BuildNode buildNode = { };
      buildNode.value = nodeHeap[0].value;
      buildNode.index = m_nodeCount;

      std::pop_heap(nodeHeap.begin(), nodeHeap.begin() + (nodeCount--));

      // Extract second node from heap
      trieNode.right = nodeHeap[0].index;
      buildNode.value += nodeHeap[0].value;

      std::pop_heap(nodeHeap.begin(), nodeHeap.begin() + (nodeCount--));

      // Add merged node back into the heap
      nodeHeap[nodeCount++] = buildNode;

      std::push_heap(nodeHeap.begin(), nodeHeap.begin() + nodeCount);

      // Write trie node to internal array
      m_nodes[m_nodeCount++] = trieNode;
    }
  }

  ~HuffmanTrie() {

  }

  /**
   * \brief Creates encoder from trie
   * \returns Huffan encoder
   */
  HuffmanEncoder<CodeType> createEncoder() const {
    HuffmanEncoder<CodeType> result;

    populateEncoder(result, m_nodeCount - 1, 0, 0);
    return result;
  }

  /**
   * \brief Creates decoder from trie
   * \returns Huffman decoder
   */
  HuffmanDecoder<CodeType> createDecoder() const {
    HuffmanDecoder<CodeType> decoder;

    uint32_t srcIndex = 0;

    small_vector<std::pair<uint32_t, uint32_t>, NodeCount> nodes;
    nodes.emplace_back(m_nodeCount - 1, decoder.allocate(0));

    while (srcIndex < nodes.size()) {
      uint32_t nodeIndex;
      uint32_t entryIndex;

      std::tie(nodeIndex, entryIndex) = nodes[srcIndex++];

      auto& node = m_nodes[nodeIndex];

      if (node.left == node.right) {
        decoder.setLeafEntry(entryIndex, node.code);
      } else {
        uint32_t depth = getDecodingDepth(nodeIndex);
        uint32_t offset = decoder.allocate(depth);

        for (uint32_t i = 0; i < (1u << depth); i++)
          nodes.emplace_back(traverse(nodeIndex, depth, i), offset + i);

        decoder.setDecodeEntry(entryIndex, depth, offset);
      }
    }

    decoder.createLookupTable();
    return decoder;
  }

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

  std::array<TrieNode, NodeCount> m_nodes = { };

  void populateEncoder(
          HuffmanEncoder<CodeType>&     encoder,
          uint32_t                      nodeIndex,
          uint32_t                      bitCount,
          uint64_t                      codeBits) const {
    auto& node = m_nodes[nodeIndex];

    if (node.left == node.right) {
      dbg_assert(bitCount <= 64);

      // Leaf node, write out the code that we have
      encoder.setCode(node.code, bitCount, codeBits);
    } else {
      populateEncoder(encoder, node.left, bitCount + 1, codeBits);
      populateEncoder(encoder, node.right, bitCount + 1, codeBits | (1ull << bitCount));
    }
  }

  uint32_t getDecodingDepth(
          uint32_t                      nodeIndex) const {
    auto& node = m_nodes[nodeIndex];

    uint32_t srcIndex = 0;

    small_vector<std::pair<uint32_t, uint32_t>, NodeCount> childNodes;
    childNodes.emplace_back(node.left,  1u);
    childNodes.emplace_back(node.right, 1u);

    while (true) {
      uint32_t nodeIndex;
      uint32_t nodeDepth;

      std::tie(nodeIndex, nodeDepth) = childNodes[srcIndex++];

      auto& node = m_nodes[nodeIndex];

      if (node.left == node.right)
        return nodeDepth;

      childNodes.emplace_back(node.left,  nodeDepth + 1u);
      childNodes.emplace_back(node.right, nodeDepth + 1u);
    }
  }

  uint32_t traverse(
          uint32_t                      nodeIndex,
          uint32_t                      depth,
          uint32_t                      bits) const {
    for (uint32_t i = 0; i < depth; i++) {
      auto& node = m_nodes[nodeIndex];

      nodeIndex = (bits & (1u << i))
        ? node.right
        : node.left;
    }

    return nodeIndex;
  }

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
template<typename CodeType>
bool encodeHuffmanBinary(
        OutStream&                    writer,
  const void*                         data,
        size_t                        size) {
  BitstreamWriter bitstream(writer);

  HuffmanCounter<CodeType, uint32_t> counter;
  counter.add(data, size);

  HuffmanTrie<CodeType> trie(counter);
  HuffmanEncoder<CodeType> encoder = trie.createEncoder();
  HuffmanDecoder<CodeType> decoder = trie.createDecoder();

  return decoder.serialize(bitstream)
      && bitstream.write(uint32_t(size), 32)
      && encoder.encode(bitstream, data, size);
}


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
template<typename CodeType>
bool decodeHuffmanBinary(
        OutStream&                    writer,
        InStream&                     reader) {
  BitstreamReader bitstream(reader);

  HuffmanDecoder<CodeType> decoder;

  if (!decoder.deserialize(bitstream))
    return false;

  uint32_t byteCount = bitstream.read(32);
  decoder.decode(writer, bitstream, byteCount);
  return true;
}

}
