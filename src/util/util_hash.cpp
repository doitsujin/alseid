#include "../third_party/sha1/sha1.h"

#include "util_hash.h"

namespace as {

UniqueHash UniqueHash::compute(
        size_t                        size,
  const void*                         data) {
  std::array<uint8_t, 20> sha1Digest = { };

  SHA1_CTX ctx;
  SHA1Init(&ctx);
  
  auto ptr = reinterpret_cast<const uint8_t*>(data);
  SHA1Update(&ctx, ptr, size);

  SHA1Final(sha1Digest.data(), &ctx);

  // Truncate the result
  UniqueHash result;
  std::memcpy(result.m_data.data(), sha1Digest.data(), result.m_data.size());
  return result;
}


std::string UniqueHash::toString() const {
  static const std::array<char, 16> ch = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'e' };

  std::string result;
  result.resize(m_data.size() * 2);

  for (uint32_t i = 0; i < m_data.size(); i++) {
    result[2 * i + 0] = ch[uint8_t(m_data[i]) >> 4];
    result[2 * i + 1] = ch[uint8_t(m_data[i]) & 0xF];
  }

  return result;
}

}
