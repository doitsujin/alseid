#pragma once

#include <optional>
#include <vector>

#include "../util/util_small_vector.h"
#include "../util/util_stream.h"
#include "../util/util_types.h"

#include "gfx_shader.h"

namespace as {

/**
 * \brief Compresses SPIR-V binary
 *
 * \param [in] output Byte stream to write to
 * \param [in] input Uncompressed SPIR-V binary
 */
bool spirvEncodeBinary(
        WrBufferedStream&             output,
        RdMemoryView                  input);


/**
 * \brief Decompresses SPIR-V binary
 *
 * \param [in] output Memory region to write to
 * \param [in] reader Byte stream to read from
 * \returns \c true if decoding was successful
 */
bool spirvDecodeBinary(
        WrMemoryView                  output,
        RdMemoryView                  input);


/**
 * \brief Computes size of decoded SPIR-V binary
 *
 * \param [in] reader Byte stream to read from
 * \returns Size of the decompressed binary, in bytes,
 *    or \c 0 if the binary is invalid.
 */
size_t spirvGetDecodedSize(
        RdMemoryView                  input);


/**
 * \brief Gets shader description for a SPIR-V binary
 *
 * Note that the module name will be set to \c nullptr.
 * \param [in] size Compressed binary size
 * \param [in] code Compressed binary data
 * \returns Shader binary description, or \c nullopt on error.
 */
std::optional<GfxShaderDesc> spirvReflectBinary(
        size_t                        size,
  const void*                         code);

}
