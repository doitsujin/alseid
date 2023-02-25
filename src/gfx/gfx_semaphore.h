#pragma once

#include <chrono>

#include "../util/util_iface.h"

namespace as {

/**
 * \brief Semaphore description
 */
struct GfxSemaphoreDesc {
  /** Debug name of the semaphore. */
  const char* debugName = nullptr;
  /** Initial semaphore value. */
  uint64_t initialValue = 0;
};


/**
 * \brief Semaphore interface
 *
 * Semaphores are used to synchronize GPU and CPU work, as
 * well as to synchronize GPU submissions across different
 * queues.
 */
class GfxSemaphoreIface {

public:

  virtual ~GfxSemaphoreIface() { }

  /**
   * \brief Queries current semaphore value
   *
   * Note that if submissions are pending that signal this
   * semaphore, the returned value may be immediately out
   * of date.
   * \returns Current semaphore value
   */
  virtual uint64_t getCurrentValue() = 0;

  /**
   * \brief Waits for semaphore to reach the given value
   *
   * This blocks the calling thread until the internal counter
   * reaches at least the desired value, or the wait times out.
   * \note Calling this with a timeout of 0 is equivalent to
   *    calling \c getCurrentValue and comparing the returned
   *    value with the desired semaphore value.
   * \param [in] value Desired semaphore value
   * \param [in] timeout Timeout, in nanoseconds
   * \returns \c true if the semaphore reached the desired
   *    value, or \c false if a timeout occured.
   */
  virtual bool wait(
          uint64_t                      value,
          std::chrono::nanoseconds      timeout) = 0;

  /**
   * \brief Signals semaphore to given value
   *
   * Performs a signal operation on the CPU.
   * \param [in] value Desired semaphore value
   */
  virtual void signal(
          uint64_t                      value) = 0;

  /**
   * \brief Waits for semaphore to reach the given value
   *
   * Convenience overload that is equivalent to using
   * a timeout of \c std::chrono::nanoseconds::max().
   * \param [in] value Desired semaphore value
   */
  void wait(
          uint64_t                      value) {
    wait(value, std::chrono::nanoseconds::max());
  }

};

/** See GfxSemaphoreIface. */
using GfxSemaphore = IfaceRef<GfxSemaphoreIface>;

}
