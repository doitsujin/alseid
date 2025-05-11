#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace as {

/**
 * \brief Offsets a pointer by the given amount
 *
 * \param [in] base Base pointer
 * \param [in] offset Offset
 */
template<typename T>
T* ptroffset(T* base, size_t offset) {
  using c_ptr_t = std::conditional_t<std::is_const_v<T>, const char*, char*>;
  auto c_data = reinterpret_cast<c_ptr_t>(base);
  return reinterpret_cast<T*>(c_data + offset);
}


/**
 * \brief Holds live reference to contained object
 *
 * Keeps the container alive while storing a pointer to a contained
 * object. Useful when contained objects are not heap-allocated or
 * referencing them directly would introduce circiular dependencies.
 */
template<typename T, typename C>
class ContainedPtr {

public:

  ContainedPtr() = default;

  ContainedPtr(std::nullptr_t) { }

  ContainedPtr(T& object, std::shared_ptr<C> container)
  : m_container(std::move(container)), m_object(&object) { }

  ContainedPtr(T& object, const std::weak_ptr<C>& container)
  : m_container(container.lock()), m_object(m_container ? &object : nullptr) { }

  ContainedPtr(ContainedPtr&& other)
  : m_container(std::exchange(other.m_container, std::shared_ptr<C>()))
  , m_object(std::exchange(other.m_object, nullptr)) { }

  ContainedPtr(const ContainedPtr& other)
  : m_container(other.m_container)
  , m_object(other.m_object) { }

  ContainedPtr& operator = (ContainedPtr&& other) {
    m_container = std::exchange(other.m_container, std::shared_ptr<C>());
    m_object = std::exchange(other.m_object, nullptr);
    return *this;
  }

  ContainedPtr& operator = (const ContainedPtr& other) {
    m_container = other.m_container;
    m_object = other.m_object;
    return *this;
  }

  std::shared_ptr<C> container() const {
    return m_container;
  }

  T& operator * () const { return *m_object; }
  T* operator -> () const { return m_object; }
  T* get() const { return m_object; }

  explicit operator bool () const {
    return m_object != nullptr;
  }

private:

  std::shared_ptr<C>  m_container = nullptr;
  T*                  m_object    = nullptr;

};

}
