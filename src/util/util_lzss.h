#pragma once

#include "util_stream.h"

namespace as {

/**
 * \brief Encodes binary using LZSS
 *
 * \param [in] writer Output stream
 * \param [in] data Pointer to uncompressed data
 * \param [in] size Number of bytes to encode
 * \param [in] window Sliding window size
 */
bool lzssEncode(
        OutStream&                      writer,
  const void*                           data,
        size_t                          size,
        size_t                          window);

/**
 * \brief Decodes LZSS-encoded binary
 *
 * \param [in] data Buffer for decompressed data
 * \param [in] size Size of decompressed binary
 * \param [in] reader Input stream
 */
bool lzssDecode(
        void*                           data,
        size_t                          size,
        InStream&                       reader);

}
