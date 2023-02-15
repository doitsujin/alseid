#include <algorithm>
#include <tuple>
#include <utility>

#include "util_assert.h"
#include "util_huffman.h"
#include "util_likely.h"
#include "util_small_vector.h"

namespace as {

HuffmanCounter::HuffmanCounter() {

}


void HuffmanCounter::add(
  const void*                         data,
        size_t                        size) {
  // Our Huffman implementation operates on 16-bit codes
  auto words = reinterpret_cast<const uint16_t*>(data);
  auto bytes = reinterpret_cast<const uint8_t*>(data);

  for (size_t i = 0; i < size / 2; i++)
    m_counts[words[i]] += 1;

  // Deal with the last byte if there is one
  if (size & 1)
    m_counts[uint8_t(bytes[size - 1])] += 1;
}


void HuffmanCounter::add(
        InStream&                     reader) {
  std::array<char, 4096> buffer;

  size_t read;

  do {
    read = reader.load(buffer.data(), buffer.size());
    add(buffer.data(), read);
  } while (read);
}


void HuffmanCounter::accumulate(
  const HuffmanCounter&               counter) {
  for (size_t i = 0; i < m_counts.size(); i++)
    m_counts[i] += counter.m_counts[i];
}




HuffmanEncoder::HuffmanEncoder() {

}


HuffmanEncoder::~HuffmanEncoder() {

}


bool HuffmanEncoder::encode(
        BitstreamWriter&              stream,
  const void*                         data,
        size_t                        size) const {
  bool success = true;

  auto words = reinterpret_cast<const uint16_t*>(data);
  auto bytes = reinterpret_cast<const uint8_t*>(data);

  for (size_t i = 0; i < size / 2; i++) {
    auto& e = m_entries[words[i]];
    success &= stream.write(getCodeBits(e), e.bitCount);
  }

  if (size & 1) {
    auto& e = m_entries[uint8_t(bytes[size - 1])];
    success &= stream.write(getCodeBits(e), e.bitCount);
  }

  return success;
}


bool HuffmanEncoder::encode(
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


void HuffmanEncoder::setCode(
        uint16_t                      code,
        uint32_t                      bitCount,
        uint64_t                      codeBits) {
  auto& e = m_entries[code];

  e.bitCount = bitCount;
  e.codeBitsLo = uint32_t(codeBits);
  e.codeBitsHi = uint32_t(codeBits >> 32);
}


uint64_t HuffmanEncoder::getCodeBits(
  const Entry&                        e) {
  // The code is split into two dwords so that the lookup
  // entry fits into 12 byts, so recombine them here.
  return uint64_t(e.codeBitsLo) | (uint64_t(e.codeBitsHi) << 32);
}




HuffmanDecoder::HuffmanDecoder() {

}


bool HuffmanDecoder::decode(
        OutStream&                    writer,
        BitstreamReader&              stream,
        size_t                        size) {
  bool success = true;

  for (uint32_t i = 0; i < size; i += 2) {
    // Perform a lookup in the 16-bit lookup table. In
    // many cases this will lead us to a leaf node.
    const Entry* e = &m_lookup[stream.peek(16)];
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

    uint16_t code = e->data;

    if (likely(i + 2 <= size))
      success &= writer.write(code);
    else
      success &= writer.write(uint8_t(code));
  }

  return success;
}


bool HuffmanDecoder::read(
        BitstreamReader&              stream) {
  uint16_t entryCountCompressed = uint16_t(stream.read(16));

  // The entry count is stored in a compressed way
  // so it only takes up two bytes, expand it here.
  m_entryCount = decodeOffset(entryCountCompressed);

  for (uint32_t i = 0; i < m_entryCount; i++) {
    Entry e = { };
    e.bits = uint8_t(stream.read(5));
    e.data = uint16_t(stream.read(16));

    // Validate that entry is valid. Entry offsets are
    // compressed in the same way as the node count.
    if (e.bits && e.data >= entryCountCompressed)
      return false;

    m_entries[i] = e;
  }

  createLookupTable();
  return true;
}


bool HuffmanDecoder::write(
        BitstreamWriter&              stream) const {
  bool success = true;

  // The entry count is always odd, so we don't need to store
  // the last bit. This way it will always fit into 16 bits.
  stream.write(encodeOffset(m_entryCount), 16);

  // We can compress each entry with 5 bits for the bit count,
  // since the depth is at most 16 for the worst case and zero
  // for leaf nodes. Offsets are compressed like the entry count.
  for (uint32_t i = 0; i < m_entryCount; i++) {
    auto& e = m_entries[i];

    success &= stream.write(e.bits, 5);
    success &= stream.write(e.data, 16);
  }

  return success;
}


uint32_t HuffmanDecoder::allocate(
        uint32_t                      depth) {
  uint32_t index = m_entryCount;
  m_entryCount += 1u << depth;
  return index;
}


void HuffmanDecoder::setLeafEntry(
        uint32_t                      entry,
        uint16_t                      code) {
  Entry e = { };
  e.data = code;

  m_entries[entry] = e;
}


void HuffmanDecoder::setDecodeEntry(
        uint32_t                      entry,
        uint32_t                      bits,
        uint32_t                      offset) {
  Entry e = { };
  e.bits = uint8_t(bits);
  e.data = encodeOffset(offset);

  m_entries[entry] = e;
}


void HuffmanDecoder::createLookupTable() {
  for (size_t i = 0; i < m_lookup.size(); i++) {
    const Entry* e = &m_entries[0];
    uint32_t bits = 0;

    while (e->bits && bits + e->bits <= 16) {
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


uint16_t HuffmanDecoder::encodeOffset(
        uint32_t                      offset) {
  return uint16_t(offset >> 1);
}


uint32_t HuffmanDecoder::decodeOffset(
        uint16_t                      compressed) {
  return (uint32_t(compressed) << 1) + 1;
}




HuffmanTrie::HuffmanTrie() {

}


HuffmanTrie::HuffmanTrie(
  const HuffmanCounter&               counter) {
  // Write leaf nodes to the internal data structure
  std::array<BuildNode, HuffmanCounter::MaxCodeCount> nodeHeap;
  size_t nodeCount = 0;

  for (size_t i = 0; i < HuffmanCounter::MaxCodeCount; i++) {
    if (counter[i]) {
      size_t nodeIndex = nodeCount++;

      TrieNode trieNode = { };
      trieNode.code = uint16_t(i);
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


HuffmanTrie::~HuffmanTrie() {
  
}


HuffmanEncoder HuffmanTrie::createEncoder() const {
  HuffmanEncoder result;

  populateEncoder(result, m_nodeCount - 1, 0, 0);
  return result;
}


HuffmanDecoder HuffmanTrie::createDecoder() const {
  HuffmanDecoder decoder;

  uint32_t srcIndex = 0;

  small_vector<std::pair<uint32_t, uint32_t>, MaxNodeCount> nodes;
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


void HuffmanTrie::populateEncoder(
        HuffmanEncoder&               encoder,
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


uint32_t HuffmanTrie::getDecodingDepth(
        uint32_t                      nodeIndex) const {
  auto& node = m_nodes[nodeIndex];

  uint32_t srcIndex = 0;

  small_vector<std::pair<uint32_t, uint32_t>, MaxNodeCount> childNodes;
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


uint32_t HuffmanTrie::traverse(
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




bool encodeHuffmanBinary(
        OutStream&                    writer,
  const void*                         data,
        size_t                        size) {
  BitstreamWriter bitstream(writer);

  HuffmanCounter counter;
  counter.add(data, size);

  HuffmanTrie trie(counter);
  HuffmanEncoder encoder = trie.createEncoder();
  HuffmanDecoder decoder = trie.createDecoder();

  return decoder.write(bitstream)
      && bitstream.write(uint32_t(size), 32)
      && encoder.encode(bitstream, data, size);
}


bool decodeHuffmanBinary(
        OutStream&                    writer,
        InStream&                     reader) {
  BitstreamReader bitstream(reader);

  HuffmanDecoder decoder;

  if (!decoder.read(bitstream))
    return false;

  uint32_t byteCount = bitstream.read(32);
  decoder.decode(writer, bitstream, byteCount);
  return true;
}

}
