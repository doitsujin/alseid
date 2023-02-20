#include "util_huffman.h"

namespace as {

  template class HuffmanCounter<uint16_t>;
  template class HuffmanCounter<uint8_t>;

  template class HuffmanDecoder<uint16_t>;
  template class HuffmanDecoder<uint8_t>;

  template class HuffmanEncoder<uint16_t>;
  template class HuffmanEncoder<uint8_t>;

  template class HuffmanTrie<uint16_t>;
  template class HuffmanTrie<uint8_t>;

}
