#pragma once

#include <vector>

#include "../io_request.h"

namespace as {

/**
 * \brief Linux io_uring request
 */
class IoUringRequest : public IoRequestIface {

public:

  IoUringRequest();

  ~IoUringRequest();

  /**
   * \brief Notifies completion of sub-request
   *
   * \param [in] index Request index
   * \param [in] status Request status
   */
  void notify(
          uint32_t                      index,
          IoStatus                      status);

  /**
   * \brief Sets status to pending
   */
  void setPending();

  /**
   * \brief Processes requests
   *
   * Iterates over all buffered requests.
   * \param [in] proc Request callback
   */
  template<typename Proc>
  bool processRequests(const Proc& proc) const {
    for (uint32_t i = 0; i < m_items.size(); i++) {
      if (!proc(i, m_items[i]))
        return false;
    }

    return true;
  }

private:

  uint32_t  m_pendingCount = 0u;
  IoStatus  m_pendingStatus = IoStatus::eSuccess;

};

}
