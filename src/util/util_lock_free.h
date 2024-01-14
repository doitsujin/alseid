#pragma once

#include <atomic>
#include <array>

namespace as {

/**
 * \brief Helper function to atomically store a maximum value
 *
 * Updates the atomic in question to be the greater
 * of the currently stored and desired values.
 * \param [out] value Atomic value to update
 * \param [in] desired Desired value
 * \returns Value previously stored in the atomic
 */
template<typename T>
T atomicMax(std::atomic<T>& value, T desired) {
  T current = value.load(std::memory_order_acquire);

  while (desired > current) {
    if (value.compare_exchange_strong(current, desired,
        std::memory_order_release,
        std::memory_order_acquire))
      break;
  }

  return current;
}


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


/**
 * \brief Lock-free growing index list
 *
 * Employs roughly the same strategies as the object map class,
 * but is simpler in that entries are default-initialized, and
 * only append, clear, iteration, and access operations are
 * supported.
 */
template<typename T,
  uint32_t TopLevelBits     = 12u,
  uint32_t BottomLevelBits  = 12u>
class LockFreeGrowList {
  static constexpr size_t BottomLevelMask = (1u << BottomLevelBits) - 1u;

  struct BottomLevel {
    std::array<T, 1u << (BottomLevelBits)> objects = { };
  };
public:

  class Iterator {

  public:

    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = T*;
    using reference         = T&;

    Iterator() = default;

    Iterator(const LockFreeGrowList* list, size_t index)
    : m_list(list), m_index(index), m_layer(getLayer()) { }

    reference operator * () const {
      return m_layer->objects[m_index & BottomLevelMask];
    }

    pointer operator -> () const {
      return &m_layer->objects[m_index & BottomLevelMask];
    }

    Iterator& operator ++ () {
      advance();
      return *this;
    }

    Iterator operator ++ (int) {
      Iterator tmp(m_list, m_index);
      advance();
      return tmp;
    }

    bool operator == (const Iterator& other) const {
      return m_list == other.m_list && m_index == other.m_index;
    }

    bool operator != (const Iterator& other) const {
      return m_list != other.m_list || m_index != other.m_index;
    }

  private:

    const LockFreeGrowList* m_list  = nullptr;
    size_t                  m_index = 0;
    BottomLevel*            m_layer = nullptr;

    void advance() {
      m_index += 1;

      if (!(m_index & BottomLevelMask))
        m_layer = getLayer();
    }

    BottomLevel* getLayer() const {
      size_t layerIndex = m_index >> BottomLevelBits;

      if (layerIndex >= (1u << TopLevelBits))
        return nullptr;

      return m_list->m_layers[layerIndex].load();
    }

  };

  using iterator = Iterator;

  LockFreeGrowList() = default;

  LockFreeGrowList             (const LockFreeGrowList&) = delete;
  LockFreeGrowList& operator = (const LockFreeGrowList&) = delete;

  ~LockFreeGrowList() {
    for (const auto& layerAtomic : m_layers) {
      BottomLevel* layer = layerAtomic.load();

      if (!layer)
        break;

      delete layer;
    }
  }

  /**
   * \brief Checks whether the list is empty
   * \returns \c true if there are no entries
   */
  bool empty() {
    return !size();
  }

  /**
   * \brief Retrieves current size
   * \returns Current size
   */
  size_t size() const {
    return m_size.load(std::memory_order_acquire);
  }

  /**
   * \brief Clears list
   *
   * Resets the size to zero. Only safe to use when no
   * items are being added to the list at the same time.
   */
  void clear() {
    m_size.store(0, std::memory_order_release);
  }

  /**
   * \brief Appends a single item to the list
   * \param [in] item Item to add
   */
  void push_back(const T& item) {
    alloc() = item;
  }

  void push_back(T&& item) {
    alloc() = std::move(item);
  }

  /**
   * \brief Retrieves iterator to the beginning of the list
   * \returns Beginning iterator
   */
  auto begin() const {
    return Iterator(this, 0);
  }

  /**
   * \brief Retrieves iterator to the beginning of the list
   * \returns Beginning iterator
   */
  auto end() const {
    return Iterator(this, m_size.load());
  }

  /**
   * \brief Retrieves item at a given index
   *
   * \param [in] index Index
   * \returns Item reference
   */
  const T& operator [] (size_t index) const { return getRef(index); }
        T& operator [] (size_t index)       { return getRef(index); }

private:

  std::atomic<size_t> m_size = { size_t(0) };

  std::array<std::atomic<BottomLevel*>, 1u << TopLevelBits> m_layers = { };

  T& getRef(size_t index) const {
    auto layer = m_layers[index >> BottomLevelBits].load();
    return layer->objects[index & BottomLevelMask];
  }

  T& alloc() {
    size_t index = m_size.fetch_add(1u);

    // Extract layer and array index parts from the index
    size_t layerIndex = index >> BottomLevelBits;
    size_t arrayIndex = index & BottomLevelMask;

    // Create new bottom-level array and swap it in as needed
    BottomLevel* layer = m_layers[layerIndex].load();

    if (!layer) {
      BottomLevel* newLayer = new BottomLevel();

      if (m_layers[layerIndex].compare_exchange_strong(layer, newLayer, std::memory_order_relaxed))
        layer = newLayer;
      else
        delete newLayer;
    }

    return layer->objects[arrayIndex];
  }

};

}