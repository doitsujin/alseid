#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "../util/util_iface.h"
#include "../util/util_small_vector.h"

#include "io_file.h"

namespace as {

class IoBufferedRequest;

/**
 * \brief I/O request callback
 *
 * A callback that will be called after completion.
 * Takes the request status as an argument.
 */
using IoRequestCallback = std::function<void (IoStatus)>;


/**
 * \brief I/O operation callback
 *
 * This is executed on successful completion of a single
 * request. If the callback returns an error, the entire
 * request will be treated as failed.
 */
using IoCallback = std::function<IoStatus (const IoBufferedRequest&)>;


/**
 * \brief Buffered request type
 */
enum class IoRequestType : uint32_t {
  eNone         = 0,
  eRead         = 1,
  eWrite        = 2,
  eStream       = 3,
};


/**
 * \brief Buffered request
 *
 * Used internally to store read and write requests.
 */
struct IoBufferedRequest {
  IoRequestType type    = IoRequestType::eNone;
  IoFile        file;
  uint64_t      offset  = 0;
  uint64_t      size    = 0;
  const void*   src     = nullptr;
  void*         dst     = nullptr;
  IoCallback    cb;
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
   * Blocks the calling thread until the request completes either
   * successfully or with an error. This includes the competion
   * of all per-request callbacks.
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
          IoRequestCallback             callback);

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
          void*                         dst) {
    auto& item = allocItem();
    item.type = IoRequestType::eRead;
    item.file = std::move(file);
    item.offset = offset;
    item.size = size;
    item.dst = dst;
  }

  /**
   * \brief Enqueues a read operation with a callback
   *
   * The callback may perform expensive operations such as
   * decompression, and will be scheduled to a worker thread
   * after the I/O operation itself has completed.
   * \param [in] file File to read from
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] dst Pointer to write read data to
   * \param [in] callback Callback. Takes a pointer to
   *    the destination data and the number of bytes read.
   */
  template<typename Cb>
  void read(
          IoFile                        file,
          uint64_t                      offset,
          uint64_t                      size,
          void*                         dst,
          Cb&&                          callback) {
    auto& item = allocItem();
    item.type = IoRequestType::eRead;
    item.file = std::move(file);
    item.offset = offset;
    item.size = size;
    item.dst = dst;
    item.cb = IoCallback([
      cb = std::move(callback)
    ] (const IoBufferedRequest& item) {
      return cb(item.dst, item.size);
    });
  }

  /**
   * \brief Enqueues a stream operation
   *
   * Stream operations essentially perform reads into a buffer that
   * is provided by the backend. This is useful in situations where
   * the raw data is immediately processed and discarded, e.g. when
   * decompressing data that is stored in an archive.
   *
   * The data pointer passed to the callback will be invalidated
   * immediately after the callback has finished execution.
   * \param [in] file File to read from
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] callback Callback. Takes a pointer to
   */
  template<typename Cb>
  void stream(
          IoFile                        file,
          uint64_t                      offset,
          uint64_t                      size,
          Cb&&                          callback) {
    auto& item = allocItem();
    item.type = IoRequestType::eStream;
    item.file = std::move(file);
    item.offset = offset;
    item.size = size;
    item.cb = IoCallback([
      cb = std::move(callback)
    ] (const IoBufferedRequest& item) {
      return cb(item.dst, item.size);
    });
  }

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
    const void*                         src) {
    auto& item = allocItem();
    item.type = IoRequestType::eWrite;
    item.file = std::move(file);
    item.offset = offset;
    item.size = size;
    item.src = src;
  }

  /**
   * \brief Enqueues a write operation with callback
   *
   * \param [in] file File to write to
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to write
   * \param [in] src Pointer to read source data from
   * \param [in] callback Callback. Takes a pointer to
   *    the source data and the number of bytes written.
   */
  template<typename Cb>
  void write(
          IoFile                        file,
          uint64_t                      offset,
          uint64_t                      size,
    const void*                         src,
          Cb&&                          callback) {
    auto& item = allocItem();
    item.type = IoRequestType::eWrite;
    item.file = std::move(file);
    item.offset = offset;
    item.size = size;
    item.src = src;
    item.cb = IoCallback([
      cb = std::move(callback)
    ] (const IoBufferedRequest& item) {
      return cb(item.src, item.size);
    });
  }

protected:

  std::mutex                  m_mutex;
  std::condition_variable     m_cond;
  std::atomic<IoStatus>       m_status = { IoStatus::eReset };

  small_vector<IoRequestCallback, 4> m_callbacks;

  small_vector<IoBufferedRequest, 16> m_items;

  void setStatus(
          IoStatus                      status);

  IoBufferedRequest& allocItem();

};

/** See IoRequestIface. */
using IoRequest = IfaceRef<IoRequestIface>;

}
