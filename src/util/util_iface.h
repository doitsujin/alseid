#pragma once

#include <memory>

namespace as {

/**
 * \brief Interface reference
 *
 * Provides a const-correct reference-counted
 * pointer to an object of the given type.
 */
template<typename T>
class IfaceRef {

public:

  IfaceRef() { }

  IfaceRef(std::nullptr_t) { }

  explicit IfaceRef(std::shared_ptr<T>&& iface)
  : m_iface(std::move(iface)) { }

  operator bool () const {
    return bool(m_iface);
  }

  T* operator -> () const {
    return m_iface.operator -> ();
  }

  T& operator * () const {
    return m_iface.operator * ();
  }

  std::shared_ptr<T> getShared() const {
    return m_iface;
  }

  std::weak_ptr<T> getWeak() const {
    return m_iface;
  }

  bool operator == (const IfaceRef&) const = default;
  bool operator != (const IfaceRef&) const = default;

  size_t hash() const {
    return reinterpret_cast<uintptr_t>(m_iface.get());
  }

private:

  std::shared_ptr<T> m_iface;

};


/**
 * \brief Plain reference
 *
 * Used for objects that are owned by another
 * object and share the same lifetime.
 */
template<typename T>
class PtrRef {

public:

  PtrRef() { }

  PtrRef(std::nullptr_t) { }

  explicit PtrRef(T& iface)
  : m_iface(&iface) { }

  operator bool () const {
    return bool(m_iface);
  }

  T* operator -> () const {
    return m_iface;
  }

  T& operator * () const {
    return *m_iface;
  }

  bool operator == (const PtrRef&) const = default;
  bool operator != (const PtrRef&) const = default;

  size_t hash() const {
    return reinterpret_cast<uintptr_t>(m_iface);
  }

private:

  T* m_iface = nullptr;

};

}
