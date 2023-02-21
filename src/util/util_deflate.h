#pragma once

#include <vector>

#include "util_stream.h"

namespace as {

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
 * \param [in] output Output vector
 * \param [in] input Input memory
 * \returns \c true on success
 */
bool deflateDecode(
        WrMemoryView                    output,
        RdMemoryView                    input);

}
