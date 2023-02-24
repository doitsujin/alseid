#include <libdeflate.h>

#include "util_deflate.h"
#include "util_math.h"

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


bool gdeflateEncode(
        WrVectorStream&                 output,
        RdMemoryView                    input) {
  auto* encoder = libdeflate_alloc_gdeflate_compressor(12);

  if (!encoder)
    return false;

  // Compute amount of data we need to allocate, as well as how many pages are needed.
  size_t pageCount = 0;
  size_t maxSize = libdeflate_gdeflate_compress_bound(encoder, input.getSize(), &pageCount);
  size_t maxPageSize = maxSize / pageCount;

  // Initialize page structs for the compressor function
  std::vector<libdeflate_gdeflate_out_page> pages(pageCount);
  std::vector<char> data(maxSize);

  for (size_t i = 0; i < pageCount; i++) {
    pages[i].data = &data[i * maxPageSize];
    pages[i].nbytes = maxPageSize;
  }

  // Perform the actual compression and free encoder object. We're
  // done with gdeflate after this, we only need to write the data.
  size_t compressed = libdeflate_gdeflate_compress(encoder,
    input.getData(), input.getSize(), pages.data(), pages.size());

  libdeflate_free_gdeflate_compressor(encoder);

  if (!compressed)
    return false;

  // Now we can actually write the bit stream
  WrStream writer(output);
  bool success = true;

  GDeflateHeader header = { };
  header.workgroupCountX = pageCount;
  header.workgroupCountY = 1;
  header.workgroupCountZ = 1;
  header.uncompressedSize = input.getSize();

  success &= writer.write(header);

  // Generate page metadata and write it out
  std::vector<GDeflatePage> metadata(pageCount);
  size_t pageOffset = sizeof(GDeflateHeader) + sizeof(GDeflatePage) * pageCount;

  for (size_t i = 0; i < pageCount; i++) {
    metadata[i].pageOffset = pageOffset;
    metadata[i].pageSize = pages[i].nbytes;

    pageOffset += align(size_t(metadata[i].pageSize), sizeof(uint32_t));
  }

  success &= writer.write(metadata);

  // Write actual compressed data
  for (size_t i = 0; i < pageCount; i++) {
    size_t pageSize = pages[i].nbytes;
    writer.write(pages[i].data, pageSize);

    // Pad page for dword alignment
    if ((pageSize &= (sizeof(uint32_t) - 1))) {
      std::array<char, 3> padding = { };
      writer.write(padding.data(), sizeof(uint32_t) - pageSize);
    }
  }

  return success;
}


bool gdeflateDecode(
        WrMemoryView                    output,
        RdMemoryView                    input) {
  RdStream reader(input);

  // Read header containing the block count
  GDeflateHeader header = { };

  if (!reader.read(header))
    return false;

  // Read block metadata
  std::vector<GDeflatePage> metadata(header.workgroupCountX);

  if (!reader.read(metadata))
    return false;

  // Create block structs for libdeflate
  std::vector<libdeflate_gdeflate_in_page> pages(header.workgroupCountX);

  for (size_t i = 0; i < pages.size(); i++) {
    pages[i].data = input.getData(metadata[i].pageOffset);
    pages[i].nbytes = metadata[i].pageSize;
  }

  // Perform actual decompression
  auto* decoder = libdeflate_alloc_gdeflate_decompressor();

  if (!decoder)
    return false;

  auto status = libdeflate_gdeflate_decompress(decoder,
    pages.data(), pages.size(), output.getData(), output.getSize(), nullptr);

  libdeflate_free_gdeflate_decompressor(decoder);
  return status == LIBDEFLATE_SUCCESS;
}


}
