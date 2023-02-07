#include <algorithm>

#include "job.h"

namespace as {

Job::Job(
        uint32_t              itemCount,
        uint32_t              itemGroup)
: m_itemCount (itemCount)
, m_itemGroup (itemGroup) {

}


Job::~Job() {

}


bool Job::getWorkItems(
        uint32_t&             index,
        uint32_t&             count) {
  uint32_t next = m_next.load(std::memory_order_acquire);
  uint32_t size = std::min(m_itemCount - next, m_itemGroup);

  while (size && !m_next.compare_exchange_weak(next, next + size,
      std::memory_order_acquire, std::memory_order_relaxed))
    size = std::min(m_itemCount - next, m_itemGroup);

  index = next;
  count = size;

  return next + size < m_itemCount;
}


bool Job::notifyWorkItems(
        uint32_t              count) {
  uint32_t done = m_done.fetch_add(count,
    std::memory_order_release) + count;
  return done == m_itemCount;
}

}
