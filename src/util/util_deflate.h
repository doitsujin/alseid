#pragma once

#include <vector>

#include "util_stream.h"

namespace as {

/**
 * \brief GDeflate file header
 *
 * Stores an indirect dispatch command that
 * contains the page count.
 */
struct GDeflateHeader {
  /* Page count */
  uint32_t workgroupCountX;
  /* 1 */
  uint32_t workgroupCountY;
  /* 1 */
  uint32_t workgroupCountZ;
  /* Unused */
  uint32_t reserved;
};


/**
 * \brief GDeflate block entry header
 *
 * Stores the offset and compressed size of each page.
 * This is not very compact, but can be used directly
 * for GPU decompression.
 */
struct GDeflatePage {
  uint32_t pageOffset;
  uint32_t pageSize;
};


/**
 * \brief Compresses data using libdeflate
 *
 * \param [in] output Output vector
 * \param [in] input Input memory
 * \returns \c true on success
 */
bool deflateEncode(
        WrVectorStream&                 output,
        RdMemoryView                    input);


/**
 * \brief Decompresses data using libdeflate
 *
 * \param [in] output Output memory
 * \param [in] input Input memory
 * \returns \c true on success
 */
bool deflateDecode(
        WrMemoryView                    output,
        RdMemoryView                    input);


/**
 * \brief Compresses data using gdeflate
 *
 * \param [in] output Output vector
 * \param [in] input Input memory
 * \returns \c true on success
 */
bool gdeflateEncode(
        WrVectorStream&                 output,
        RdMemoryView                    input);


/**
 * \brief Decompresses data using gdeflate
 *
 * \param [in] output Output memory
 * \param [in] input Input memory
 * \returns \c true on success
 */
bool gdeflateDecode(
        WrMemoryView                    output,
        RdMemoryView                    input);


}
