#include "io_request.h"

namespace as {

IoRequestIface::IoRequestIface() {

}


IoRequestIface::~IoRequestIface() {

}


IoStatus IoRequestIface::wait() {
  std::unique_lock lock(m_mutex);

  m_cond.wait(lock, [this] {
    return getStatus() != IoStatus::ePending;
  });

  return getStatus();
}


void IoRequestIface::executeOnCompletion(
        IoRequestCallback             callback) {
  // Retrieve status in a locked context to ensure that no
  // notify call happens between us retrieving the status
  // and deciding what to do with the callback 
  std::unique_lock lock(m_mutex);
  IoStatus status = getStatus();

  if (status == IoStatus::ePending || status == IoStatus::eReset) {
    m_callbacks.push_back(std::move(callback));
    return;
  }

  // Unlock before executing the callback, we do not want
  // to stall any notify or other addCallback operations.
  lock.unlock();

  // Execute callback immediately with the completion status.
  callback(status);
}


void IoRequestIface::setStatus(
        IoStatus                      status) {
  // Locking here will ensure that any addCallback call executes
  // either before or after, but not during the status update.
  std::unique_lock lock(m_mutex);

  // Use the correct memory order to ensure any thread using
  // only getStatus can observe side effects of the request
  m_status.store(status, std::memory_order_release);

  if (status == IoStatus::eSuccess || status == IoStatus::eError) {
    // Wake up any threads waiting for completion
    m_cond.notify_all();

    // We can unlock here since any subsequent addCallback call will
    // observe the completion status and not modify the callback list.
    lock.unlock();

    // Execute callbacks, and destroy callback objects afterwards
    for (size_t i = 0; i < m_callbacks.size(); i++)
      m_callbacks[i](status);

    m_callbacks.clear();
  }
}


IoBufferedRequest& IoRequestIface::allocItem() {
  return m_items.emplace_back();
}

}
