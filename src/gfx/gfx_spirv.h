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
 * \param [in] writer Byte stream to write to
 * \param [in] reader Byte stream to read from
 * \param [in] size SPIR-V binary size in bytes
 */
bool encodeSpirvBinary(
        OutStream&                    writer,
        InStream&                     reader,
        size_t                        size);


/**
 * \brief Decompresses SPIR-V binary
 *
 * \param [in] writer Byte stream to write to
 * \param [in] reader Byte stream to read from
 * \returns \c true if decoding was successful
 */
bool decodeSpirvBinary(
        OutStream&                    writer,
        InStream&                     reader);


/**
 * \brief Computes size of decoded SPIR-V binary
 *
 * \param [in] reader Stream containing compressed binary.
 *    Note that this takes a copy of the stream.
 * \returns Size of the decompressed binary, in bytes,
 *    or \c 0 if the binary is invalid.
 */
template<typename Stream>
size_t getDecodedSpirvSize(
        Stream                        reader) {
  uint32_t dwordsTotal = 0;

  return reader.read(dwordsTotal)
    ? size_t(dwordsTotal * sizeof(uint32_t))
    : size_t(0);
}


/**
 * \brief Gets shader description for a SPIR-V binary
 *
 * Note that the module name will be set to \c nullptr.
 * \param [in] size Compressed binary size
 * \param [in] code Compressed binary data
 * \returns Shader binary description, or \c nullopt on error.
 */
std::optional<GfxShaderDesc> reflectSpirvBinary(
        size_t                        size,
  const void*                         code);

}
