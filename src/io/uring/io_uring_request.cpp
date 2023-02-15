#include "io_uring_request.h"

namespace as {

IoUringRequest::IoUringRequest() {

}


IoUringRequest::~IoUringRequest() {

}


void IoUringRequest::notify(
        uint32_t                      index,
        IoStatus                      status) {
  if (status == IoStatus::eSuccess && m_items[index].cb)
    status = m_items[index].cb();

  if (status != IoStatus::eSuccess)
    m_pendingStatus = status;

  m_items[index] = IoBufferedRequest();

  if (!(--m_pendingCount)) {
    m_items.clear();
    setStatus(m_pendingStatus);
  }
}


void IoUringRequest::setPending() {
  m_pendingCount = m_items.size();
  setStatus(IoStatus::ePending);
}

}
