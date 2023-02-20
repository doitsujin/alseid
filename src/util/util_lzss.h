#pragma once

#include "util_stream.h"

namespace as {

/**
 * \brief Encodes binary using LZSS
 *
 * \param [in] output Output stream
 * \param [in] input Input stream
 * \param [in] window Sliding window size. If 0, the
 *    maximum supported window size will be used.
 */
bool lzssEncode(
        WrBufferedStream&               output,
        RdMemoryView                    input,
        size_t                          window);

/**
 * \brief Decodes LZSS-encoded binary
 *
 * \param [in] output Output memory view
 * \param [in] input Input memory view
 */
bool lzssDecode(
        WrMemoryView                    output,
        RdMemoryView                    input);

}
