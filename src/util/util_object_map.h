#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

#include "util_math.h"

namespace as {

/**
 * \brief Object map
 *
 * Implements a map-like container, except that this uses arrays internally in
 * order to provide constant-time thread-safe insertion, deletion and lookup.
 */
template<typename T,
  uint32_t BottomLevelBits  = 16u,
  uint32_t TopLevelBits     = 8u>
class ObjectMap {
  struct alignas(T) Storage {
    char data[sizeof(T)];
  };

  struct BottomLevel {
    std::array<std::atomic<uint64_t>, 1u << (BottomLevelBits - 6u)> objectMask  = { };
    std::array<Storage,               1u << (BottomLevelBits)>      objects     = { };
  };

public:

  ObjectMap() = default;

  ObjectMap             (const ObjectMap&) = delete;
  ObjectMap& operator = (const ObjectMap&) = delete;

  /**
   * \brief Frees objects
   *
   * Iterates over all layers and frees any remaining live objects,
   * that users of this class do not have to track allocations.
   */
  ~ObjectMap() {
    for (const auto& layerAtomic : m_layers) {
      BottomLevel* layer = layerAtomic.load();

      if (!layer)
        continue;

      for (size_t i = 0; i < layer->objectMask.size(); i++) {
        for (uint64_t mask = layer->objectMask[i]; mask; mask &= mask - 1) {
          uint32_t bit = tzcnt(mask);
          uint32_t index = (i << 6u) + bit;

          reinterpret_cast<T*>(layer->objects[index].data)->~T();
        }
      }

      delete layer;
    }
  }

  /**
   * \brief Creates new object at the given index
   *
   * \param [in] index Element index
   * \param [in] args Constructor arguments
   * \returns Reference to new object
   */
  template<typename... Args>
  T& emplace(uint32_t index, Args&&... args) {
    uint32_t layerIndex = index >> BottomLevelBits;
    uint32_t arrayIndex = index & ((1u << BottomLevelBits) - 1u);

    // Create new bottom-level array and swap it in as needed
    BottomLevel* layer = m_layers[layerIndex].load();

    if (!layer) {
      BottomLevel* newLayer = new BottomLevel();

      if (m_layers[layerIndex].compare_exchange_strong(layer, newLayer, std::memory_order_relaxed))
        layer = newLayer;
      else
        delete newLayer;
    }

    // Mark object as used and destroy existing object
    // at the same index if there is one
    uint32_t maskIndex = arrayIndex >> 6u;
    uint32_t maskShift = arrayIndex & 0x3fu;
    uint64_t maskBit = 1ull << maskShift;

    void* ptr = layer->objects[arrayIndex].data;

    if (layer->objectMask[maskIndex].fetch_or(maskBit) & maskBit)
      reinterpret_cast<T*>(ptr)->~T();

    return *(new (ptr) T(std::forward<Args>(args)...));
  }

  /**
   * \brief Frees object with the given index
   *
   * Amounts to a no-op if the object has already been freed.
   * \param [in] index Index of the element to free
   */
  void erase(uint32_t index) {
    uint32_t layerIndex = index >> BottomLevelBits;
    uint32_t arrayIndex = index & ((1u << BottomLevelBits) - 1u);

    uint32_t maskIndex = arrayIndex >> 6u;
    uint32_t maskShift = arrayIndex & 0x3fu;
    uint64_t maskBit = 1ull << maskShift;

    // Mark object as unused and destroy it
    auto layer = m_layers[layerIndex].load();

    if (layer && (layer->objectMask[maskIndex].fetch_and(~maskBit) & maskBit))
      reinterpret_cast<T*>(layer->objects[arrayIndex].data)->~T();
  }

  /**
   * \brief Checks whether an object exists at the given index
   *
   * \param [in] index Index to check
   * \returns \c true if the given index is valid
   */
  bool hasObjectAt(uint32_t index) const {
    uint32_t layerIndex = index >> BottomLevelBits;
    uint32_t arrayIndex = index & ((1u << BottomLevelBits) - 1u);

    uint32_t maskIndex = arrayIndex >> 6u;
    uint32_t maskShift = arrayIndex & 0x3fu;
    uint64_t maskBit = 1ull << maskShift;

    auto layer = m_layers[layerIndex].load();
    return layer && (layer->objectMask[maskIndex].load() & maskBit);
  }

  /**
   * \brief Retrieves object at a given index
   *
   * \param [in] index Element index. Must be valid.
   * \returns Reference to object
   */
  T& operator [] (uint32_t index) {
    return *reinterpret_cast<T*>(getPointer(index));
  }

  /**
   * \brief Retrieves object at a given index
   *
   * Const version which returns a read-only reference.
   * \param [in] index Element index. Must be valid.
   * \returns Const-reference to object
   */
  const T& operator [] (uint32_t index) const {
    return *reinterpret_cast<const T*>(getPointer(index));
  }

private:

  std::array<std::atomic<BottomLevel*>, 1u << TopLevelBits> m_layers = { };

  void* getPointer(uint32_t index) const {
    auto layer = m_layers[index >> BottomLevelBits].load();
    return layer->objects[index & ((1u << BottomLevelBits) - 1u)].data;
  }

};


/**
 * \brief Object index allocator
 *
 * Helper class that goes in tandem with \c ObjectMap in that this
 * can be used to conveniently allocate and recycle object indices.
 * This class is entirely thread-safe, but not lock-free.
 */
class ObjectAllocator {

public:

  /**
   * \brief Returns maximum allocation count
   *
   * Useful to determine an upper bound for indices allocated.
   * \returns Maximum allocation count
   */
  uint32_t getCount() const {
    return m_next.load();
  }

  /**
   * \brief Allocates index
   * \returns Newly allocated index
   */
  uint32_t allocate() {
    std::lock_guard lock(m_mutex);

    if (m_free.empty())
      return m_next++;

    uint32_t result = m_free.back();
    m_free.pop_back();
    return result;
  }

  /**
   * \brief Frees index
   *
   * Adds given index to the free list so that it
   * can be reused for subsequent allocations.
   * \param [in] index Index to release
   */
  void free(uint32_t index) {
    std::lock_guard lock(m_mutex);
    m_free.push_back(index);
  }

private:

  std::mutex            m_mutex;
  std::atomic<uint32_t> m_next = 0u;
  std::vector<uint32_t> m_free;

};

}
