#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "../util/util_iface.h"
#include "../util/util_small_vector.h"

#include "io_file.h"

namespace as {

/**
 * \brief I/O request callback
 *
 * A callback that will be called after completion.
 * Takes the request status as an argument.
 */
using IoCallback = std::function<void (IoStatus)>;


/**
 * \brief Buffered request
 *
 * Used internally to store read and write requests.
 */
struct IoBufferedRequest {
  IoFile      file;
  uint64_t    offset  = 0;
  uint64_t    size    = 0;
  const void* src     = nullptr;
  void*       dst     = nullptr;
};


/**
 * \brief I/O request
 *
 * A request object can be used to batch read and write requests
 * and submit them for asynchronous execution in one go. The
 * object provides convenience methods for synchronization.
 */
class IoRequestIface {

public:

  IoRequestIface();

  virtual ~IoRequestIface();

  /**
   * \brief Queries request status
   *
   * Returns the current status of the request. Note that if
   * the request is pending, the result may be immediately
   * out of date.
   * \returns Request status
   */
  IoStatus getStatus() const {
    return m_status.load(std::memory_order_acquire);
  }

  /**
   * \brief Waits for request completion
   *
   * Blocks the calling thread until the request
   * completes either successfully or with an error.
   * \returns Request status after waiting
   */
  IoStatus wait();

  /**
   * \brief Registers a completion callback
   *
   * Callbacks will be excecuted after completion of the request,
   * including on error. This allows applications to forget about
   * I/O request objects, but errors must be handled appropriately.
   *
   * If the request has already completed at the time this is
   * called, the callback will be executed immediately.
   *
   * \note Callbacks execute on a worker thread that processes
   *    I/O operations, and should therefore be reasonably short.
   * \param [in] callback Callback
   */
  void executeOnCompletion(
          IoCallback&&                  callback);

  /**
   * \brief Enqueues a read operation
   *
   * \param [in] file File to read from
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] dst Pointer to write read data to
   */
  void read(
          IoFile                        file,
          uint64_t                      offset,
          uint64_t                      size,
          void*                         dst);

  /**
   * \brief Enqueues a write operation
   *
   * \param [in] file File to write to
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to write
   * \param [in] src Pointer to read source data from
   */
  void write(
          IoFile                        file,
          uint64_t                      offset,
          uint64_t                      size,
    const void*                         src);

protected:

  std::mutex                  m_mutex;
  std::condition_variable     m_cond;
  std::atomic<IoStatus>       m_status = { IoStatus::eReset };

  small_vector<IoCallback, 4> m_callbacks;

  small_vector<IoBufferedRequest, 16> m_items;

  void setStatus(
          IoStatus                      status);

};

using IoRequest = IfaceRef<IoRequestIface>;

}
