#include "util_huff_lzss.h"
#include "util_huffman.h"
#include "util_lzss.h"

namespace as {

constexpr size_t HuffLzssChunkSize = 1u << 16;


bool huffLzssDecode(
        WrMemoryView                  output,
        RdMemoryView                  input) {
  BitstreamReader bitstream(input);
  size_t lzssSize = bitstream.read(32);

  // Data is Huffman-compressed in chunks
  std::vector<char> lzssData;
  lzssData.reserve(lzssSize);

  WrVectorStream lzssStream(lzssData);

  for (size_t i = 0; i < lzssSize; i += HuffLzssChunkSize) {
    size_t chunkSize = std::min(lzssSize - i, HuffLzssChunkSize);

    if (bitstream.read(1)) {
      HuffmanDecoder<uint8_t> decoder;

      if (!decoder.deserialize(bitstream)
       || !decoder.decode(lzssStream, bitstream, chunkSize))
        return false;
    } else {
      for (uint32_t i = 0; i < chunkSize; i++) {
        if (!WrStream(lzssStream).write(uint8_t(bitstream.read(8))))
          return false;
      }
    }
  }

  // Ensure that all data is visible and that we actually
  // read the correct amount
  lzssStream.flush();

  if (lzssStream.getSize() != lzssSize)
    return false;

  // Now that the Huffman portion is done, decode the lzss
  return lzssDecode(output, lzssData);
}


bool huffLzssEncode(
        WrVectorStream&               output,
        RdMemoryView                  input) {
  // Encode the entire binary with LZSS first
  std::vector<char> lzssData;

  if (!lzssEncode(Lwrap<WrVectorStream>(lzssData), input, 0))
    return false;

  BitstreamWriter bitstream(output);

  // Process data in chunks of 64k, and after each one, decide
  // whether to merge it with the current block or whether to
  // start a new block.
  bool success = bitstream.write(lzssData.size(), 32);

  for (size_t i = 0; i < lzssData.size(); i += HuffLzssChunkSize) {
    auto data = &lzssData[i];
    auto size = std::min(lzssData.size() - i, HuffLzssChunkSize);

    // Create Huffman objects for the current chunk
    HuffmanCounter<uint8_t> counter;
    counter.add(data, size);

    HuffmanTrie trie(counter);
    auto encoder = trie.createEncoder();
    auto decoder = trie.createDecoder();;

    size_t chunkSize = decoder.computeSize() +
      encoder.computeEncodedSize(counter);

    // Only actually use Huffman compression if it is beneficial
    // for the current chunk. Otherwise, it may bloat the size of
    // tiny chunks, or chunks with high-entropy data.
    if (chunkSize < size) {
      success &= bitstream.write(1, 1)
              && decoder.serialize(bitstream)
              && encoder.encode(bitstream, &lzssData[i], size);
    } else {
      success &= bitstream.write(0, 1);

      for (size_t j = 0; j < size; j++)
        success &= bitstream.write(uint8_t(data[j]), 8);
    }
  }

  return success;
}

}
