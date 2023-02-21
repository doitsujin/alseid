#include <libdeflate.h>

#include "util_deflate.h"

namespace as {

bool deflateEncode(
        WrVectorStream&                 output,
        RdMemoryView                    input) {
  auto* encoder = libdeflate_alloc_compressor(12);

  if (!encoder)
    return false;

  auto& vector = output.getVector();
  size_t oldSize = vector.size();

  size_t maxSize = libdeflate_deflate_compress_bound(encoder, input.getSize());
  vector.resize(oldSize + maxSize);

  size_t compressed = libdeflate_deflate_compress(encoder,
    input.getData(), input.getSize(),
    &vector[oldSize], maxSize);

  vector.resize(oldSize + compressed);

  libdeflate_free_compressor(encoder);
  return compressed != 0;
}


bool deflateDecode(
        WrMemoryView                    output,
        RdMemoryView                    input) {
  auto* decoder = libdeflate_alloc_decompressor();

  if (!decoder)
    return false;

  auto status = libdeflate_deflate_decompress(decoder,
    input.getData(), input.getSize(),
    output.getData(), output.getSize(), nullptr);

  libdeflate_free_decompressor(decoder);
  return status == LIBDEFLATE_SUCCESS;
}

}
