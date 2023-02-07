#pragma once

#include <optional>
#include <vector>

#include "../util/util_bytestream.h"
#include "../util/util_small_vector.h"
#include "../util/util_types.h"

#include "gfx_shader.h"

namespace as {

/**
 * \brief Compresses SPIR-V binary
 *
 * \param [in] bytestream Byte stream to write to
 * \param [in] size SPIR-V binary size
 * \param [in] code SPIR-V binary data
 */
void encodeSpirvBinary(
        BytestreamWriter&             bytestream,
        size_t                        size,
  const void*                         code);


/**
 * \brief Decompresses SPIR-V binary
 *
 * \param [in] writer Byte stream to write to
 * \param [in] size SPIR-V binary size
 * \param [in] code SPIR-V binary data
 * \returns \c true if decoding was successful
 */
bool decodeSpirvBinary(
        BytestreamWriter&             writer,
        size_t                        size,
  const void*                         code);


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
