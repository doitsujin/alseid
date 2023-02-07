#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace as {

struct HashMemberProc {
  template<typename T>
  size_t operator () (const T& object) const {
    return object.hash();
  }
};

class HashState {
  
public:
  
  void add(uint32_t hash) {
    m_value ^= hash + 0x9e3779b9
              + (m_value << 6)
              + (m_value >> 2);
  }
  
  void add(uint64_t hash) {
    add(uint32_t(hash >> 0));
    add(uint32_t(hash >> 32));
  }

  template<typename T>
  void add(T* ptr) {
    add(reinterpret_cast<uintptr_t>(ptr));
  }

  operator size_t () const {
    return m_value;
  }
  
private:
  
  size_t m_value = 0;
  
};

inline size_t hashFloat(float f) {
  uint32_t i;
  std::memcpy(&i, &f, sizeof(f));
  return i;
}

class UniqueHash {

public:

  bool operator == (const UniqueHash& other) const {
    return !std::memcmp(m_data.data(), other.m_data.data(), m_data.size());
  }

  bool operator != (const UniqueHash& other) const {
    return std::memcmp(m_data.data(), other.m_data.data(), m_data.size());
  }

  size_t hash() const {
    size_t result;
    std::memcpy(&result, m_data.data(), sizeof(result));
    return result;
  }

  std::string toString() const;

  static UniqueHash compute(
          size_t                        size,
    const void*                         data);

private:

  alignas(16)
  std::array<char, 16> m_data = { };

};

}