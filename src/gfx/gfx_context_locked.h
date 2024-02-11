#pragma once

#include "gfx_context.h"

namespace as {

class GfxLockableContext;

/**
 * \brief Locked context
 *
 * Helper class to gain thread-safe access
 * to a lockable context object.
 */
class GfxLockedContext {

public:

  GfxLockedContext() = default;

  GfxLockedContext(
          GfxLockableContext&           lockable);

  GfxLockedContext(GfxLockedContext&& other)
  : m_lockable(other.m_lockable) {
    other.m_lockable = nullptr;
  }

  GfxLockedContext& operator = (GfxLockedContext&& other);

  ~GfxLockedContext();

  GfxContextIface& operator * () const;
  GfxContextIface* operator -> () const;

private:

  GfxLockableContext* m_lockable = nullptr;

};


/**
 * \brief Lockable context
 *
 * Helper class to provide access to a context in situations
 * where multiple threads will concurrently perform work and may
 * record a small number of graphics commands in any order.
 */
class GfxLockableContext {
  friend class GfxLockedContext;
public:

  GfxLockableContext() = default;

  /**
   * \brief Initializes lockable context
   * \param [in] context Underlying context
   */
  GfxLockableContext(
          GfxContext                    context)
  : m_context(std::move(context)) { }

  /**
   * \brief Locks context
   * \returns Locked context object
   */
  GfxLockedContext lock() {
    return GfxLockedContext(*this);
  }

private:

  std::mutex  m_lock;
  GfxContext  m_context;

};


inline GfxLockedContext::GfxLockedContext(
        GfxLockableContext&           lockable)
: m_lockable(&lockable) {
  m_lockable->m_lock.lock();
}


inline GfxLockedContext::~GfxLockedContext() {
  if (m_lockable)
    m_lockable->m_lock.unlock();
}


inline GfxLockedContext& GfxLockedContext::operator = (GfxLockedContext&& other) {
  if (m_lockable)
    m_lockable->m_lock.unlock();

  m_lockable = other.m_lockable;
  other.m_lockable = nullptr;
  return *this;
}


inline GfxContextIface& GfxLockedContext::operator * () const {
  return m_lockable->m_context.operator * ();
}


inline GfxContextIface* GfxLockedContext::operator -> () const {
  return m_lockable->m_context.operator -> ();
}

}
