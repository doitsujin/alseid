#pragma once

#include <optional>
#include <vector>

#include "../util/util_math.h"

namespace as {

/**
 * \brief Chunk allocator
 *
 * Simple allocator that allows suballocating memory from a larger,
 * fixed-size chunk using a free list. The allocator attempts to
 * reduce fragmentation by employing a worst-fit algorithm that
 * takes alignment requirements into account.
 */
template<typename T>
class ChunkAllocator {

public:

  ChunkAllocator()
  : m_capacity(0){ }

  /**
   * \brief Initializes chunk allocator
   * \param [in] capacity Allocator capacity
   */
  explicit ChunkAllocator(T capacity)
  : m_capacity(capacity) {
    m_freeList.push_back(Range(0, capacity));
  }

  /**
   * \brief Returns capacity
   * \returns Capacity
   */
  T capacity() const {
    return m_capacity;
  }

  /**
   * \brief Checks if the allocator is empty
   * \returns \c true if nothing is allocated.
   */
  bool isEmpty() const {
    return m_freeList.size() == 1
        && m_freeList.back().size == m_capacity;
  }

  /**
   * \brief Tries to allocate memory
   *
   * \param [in] size Amount to allocate
   * \param [in] alignment Offset alignment
   * \returns Allocation offset if successful
   */
  std::optional<T> alloc(T size, T alignment) {
    Range* pickRange = nullptr;

    for (size_t i = 0; i < m_freeList.size(); i++) {
      Range* currRange = &m_freeList[i];

      // Only consider ranges that can accomodate our allocation
      if (currRange->offset + currRange->size < align(currRange->offset, alignment) + size)
        continue;

      if (pickRange) {
        bool pickAligned = !(pickRange->offset & (alignment - 1));
        bool currAligned = !(currRange->offset & (alignment - 1));

        if (pickAligned != currAligned) {
          // Always prefer aligned ranges over unaligned ones
          // in order to reduce the size of the free list
          pickRange = pickAligned ? pickRange : currRange;
        } else if (currRange->size == size) {
          // Always use a range that fits exactly if we can
          pickRange = currRange;
          break;
        } else if (currRange->size > pickRange->size) {
          // Otherwise, select the largest range we can find
          // that still matches the alignment constraints
          pickRange = currRange;
        }
      } else {
        // First suitable range
        pickRange = currRange;
      }
    }

    if (!pickRange)
      return std::nullopt;

    // Remove range from the free list as necessary
    T alignedOffset = align(pickRange->offset, alignment);

    if (pickRange->size == size) {
      // Allocation uses the entire range, remove it
      *pickRange = m_freeList.back();
      m_freeList.pop_back();
    } else if (pickRange->offset == alignedOffset) {
      // Range offset is aligned, update it in place
      pickRange->offset += size;
      pickRange->size -= size;
    } else {
      // Range is not aligned, so we may have to split it
      // into two parts as long as none of them are empty
      T rangeEnd = pickRange->offset + pickRange->size;
      T allocEnd = alignedOffset + size;

      if (allocEnd < rangeEnd)
        m_freeList.push_back(Range(allocEnd, rangeEnd - allocEnd));

      pickRange->size = alignedOffset - pickRange->offset;
    }

    return std::make_optional(alignedOffset);
  }

  /**
   * \brief Frees memory range
   *
   * \param [in] offset Offset of allocation
   * \param [in] size Size of allocation
   */
  void free(T offset, T size) {
    Range range(offset, size);
    Range* pickRange = nullptr;

    for (size_t i = 0; i < m_freeList.size(); i++) {
      Range* currRange = &m_freeList[i];

      if (doRangesTouch(range, *currRange)) {
        range = mergeRanges(range, *currRange);

        if (!pickRange) {
          // Update current free range in place
          pickRange = currRange;
          *pickRange = range;
        } else {
          // There can be at most two ranges which
          // touch the current new free range, so
          // remove the current one and return
          *pickRange = range;
          *currRange = m_freeList.back();
          m_freeList.pop_back();
          break;
        }
      }
    }

    if (!pickRange)
      m_freeList.push_back(range);
  }

private:

  struct Range {
    Range() { }
    Range(T offset_, T size_)
    : offset(offset_), size(size_) { }

    T offset;
    T size;
  };

  T                   m_capacity;
  std::vector<Range>  m_freeList;

  static bool doRangesTouch(const Range& a, const Range& b) {
    return a.offset + a.size == b.offset
        || b.offset + b.size == a.offset;
  }

  static Range mergeRanges(const Range& a, const Range& b) {
    return Range(std::min(a.offset, b.offset), a.size + b.size);
  }

};

}
