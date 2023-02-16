#include "io_uring_request.h"

namespace as {

IoUringRequest::IoUringRequest() {

}


IoUringRequest::~IoUringRequest() {

}


void IoUringRequest::notify(
        uint32_t                      index,
        IoStatus                      status) {
  // Each sub-request can only ever be notified once, so we
  // do not need synchronization to access any of this data
  IoBufferedRequest& item = m_items[index];

  if (status == IoStatus::eSuccess && item.cb)
    status = item.cb(item);

  // Reset item to free callback etc
  item = IoBufferedRequest();

  // Realistically this should only be success or error
  if (status != IoStatus::eSuccess)
    m_pendingStatus = status;

  // If we reach this and the pending count is zero, all other
  // notifications and callbacks have completed and been made
  // visible, so we can safely mark the request as complete.
  uint32_t pending = m_pendingCount.fetch_sub(1, std::memory_order_release) - 1;

  if (!pending) {
    m_items.clear();
    setStatus(m_pendingStatus.load());
  }
}


bool IoUringRequest::hasCallback(
        uint32_t                      index) {
  return bool(m_items[index].cb);
}


void IoUringRequest::setPending() {
  m_pendingCount = m_items.size();
  setStatus(IoStatus::ePending);
}

}
