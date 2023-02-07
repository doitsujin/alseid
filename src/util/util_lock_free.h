#pragma once

#include <atomic>

namespace as {

/**
 * \brief Lock-free list
 *
 * Supports lock-free iteration as well as insertion.
 * Items cannot be removed once added, since that would
 * require locking around deletion and iteration.
 */
template<typename T>
class LockFreeList {

  struct Item {
    Item(T&& data_)
    : data(std::move(data_)), next(nullptr) { }
    Item(const T& data_)
    : data(data_), next(nullptr) { }
    template<typename... Args>
    Item(Args... args)
    : data(std::forward<Args>(args)...), next(nullptr) { }

    T     data;
    Item* next;
  };

public:

  class Iterator {

  public:

    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = T*;
    using reference         = T&;

    Iterator()
    : m_item(nullptr) { }

    Iterator(Item* e)
    : m_item(e) { }

    reference operator * () const {
      return m_item->data;
    }

    pointer operator -> () const {
      return &m_item->data;
    }

    Iterator& operator ++ () {
      m_item = m_item->next;
      return *this;
    }

    Iterator operator ++ (int) {
      Iterator tmp(m_item);
      m_item = m_item->next;
      return tmp;
    }

    bool operator == (const Iterator& other) const { return m_item == other.m_item; }
    bool operator != (const Iterator& other) const { return m_item != other.m_item; }

  private:

    Item* m_item;

  };

  using iterator = Iterator;

  LockFreeList()
  : m_head(nullptr) { }
  LockFreeList(LockFreeList&& other)
  : m_head(other.m_head.exchange(nullptr)) { }

  LockFreeList& operator = (LockFreeList&& other) {
    freeList(m_head.exchange(other.m_head.exchange(nullptr)));
    return *this;
  }

  ~LockFreeList() {
    freeList(m_head.load());
  }

  auto begin() const { return Iterator(m_head); }
  auto end() const { return Iterator(nullptr); }

  Iterator insert(const T& data) {
    return insertItem(new Item(data));
  }

  Iterator insert(T&& data) {
    return insertItem(new Item(std::move(data)));
  }

  template<typename... Args>
  Iterator emplace(Args... args) {
    return insertItem(new Item(std::forward<Args>(args)...));
  }

private:

  std::atomic<Item*> m_head;

  Iterator insertItem(Item* e) {
    Item* next = m_head.load(std::memory_order_acquire);

    do {
      e->next = next;
    } while (!m_head.compare_exchange_weak(next, e,
      std::memory_order_release,
      std::memory_order_acquire));

    return Iterator(e);
  }

  void freeList(Item* e) {
    while (e) {
      Item* next = e->next;;
      delete e;
      e = next;
    }
  }

};

}