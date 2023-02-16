#pragma once

#include <array>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "../../alloc/alloc_chunk.h"

#include "../../util/util_flags.h"

#include "../io.h"

#include "io_uring_file.h"
#include "io_uring_include.h"

namespace as {

/**
 * \brief Work item type
 */
enum class IoUringWorkItemType : uint16_t {
  eRead     = 0,
  eWrite    = 1,
  eStream   = 2,
  eRegister = 3,
};


/**
 * \brief Work item flags
 */
enum class IoUringWorkItemFlag : uint16_t {
  eStreamBuffer   = (1u << 0),
  eStreamAlloc    = (1u << 1),
  eFlagEnum       = 0
};

using IoUringWorkItemFlags = Flags<IoUringWorkItemFlag>;


/**
 * \brief Stream buffer info
 */
struct IoUringBufferInfo {
  uint32_t              offset;
  uint32_t              size;
};


/**
 * \brief Work item
 *
 * Heap-allocated object that is passed as
 * userdata to SQEs and processed by CQEs.
 */
struct IoUringWorkItem {
  IoRequest             request;
  uint32_t              requestIndex;
  IoUringWorkItemType   type;
  IoUringWorkItemFlags  flags;
  int                   index;
  int                   fd;
  uint64_t              offset;
  uint64_t              size;

  union {
    IoUringBufferInfo   bufferRange;
    char*               bufferAlloc;
  };

  union {
    const char*         src;
    char*               dst;
  };
};


/**
 * \brief Linux io_uring implementation of the I/O interface
 *
 * Implements asynchronous I/O on top of io_uring, while
 * using standard posix functions for synchronous I/O.
 */
class IoUring : public IoIface
, public std::enable_shared_from_this<IoUring> {
  constexpr static uint32_t QueueDepth = 128;
  constexpr static uint32_t MaxFds = 256;

  constexpr static size_t MinStreamBufferSize =  8u << 20;
  constexpr static size_t MaxStreamBufferSize = 64u << 20;
public:

  IoUring(
          uint32_t                      workerCount);

  ~IoUring();

  /**
   * \brief Queries backend type
   * \returns Backend type
   */
  IoBackend getBackendType() const override;

  /**
   * \brief Opens a file
   *
   * \param [in] path File path
   * \param [in] mode Mode to open the file with
   * \returns File object on success, or \c nullptr on error.
   */
  IoFile open(
    const std::filesystem::path&        path,
          IoOpenMode                    mode) override;

  /**
   * \brief Creates an I/O request object
   * \returns Asynchronous I/O request
   */
  IoRequest createRequest() override;

  /**
   * \brief Submits an I/O request
   *
   * \param [in] request Request to submit
   * \returns \c true on success
   */
  bool submit(
    const IoRequest&                    request) override;

  /**
   * \brief Unregisters a file
   * \param [in] index File index
   */
  void unregisterFile(
          int                           index);

private:

  io_uring m_ring = { };

  std::mutex                    m_mutex;

  uint32_t                      m_opsInQueue  = 0;
  uint32_t                      m_opsInFlight = 0;

  bool                          m_useFdTable  = false;
  bool                          m_useFixed    = false;
  bool                          m_stop        = false;

  void*                         m_streamBuffer = nullptr;
  ChunkAllocator<uint32_t>      m_streamAllocator;

  std::vector<IoUringWorkItem*> m_workItems;

  std::array<int,       MaxFds>       m_fdTable;
  std::array<uint64_t,  MaxFds / 64>  m_fdAllocator;

  std::condition_variable       m_consumerCond;
  std::thread                   m_consumer;

  std::mutex                    m_callbackMutex;
  std::condition_variable       m_callbackCond;
  std::queue<IoUringWorkItem*>  m_callbackQueue;
  std::vector<std::thread>      m_callbackWorkers;

  int registerFile(
          int                           fd);

  bool enqueue(
          IoUringWorkItem*              item);

  bool submit();

  IoUringWorkItem* allocWorkItem();

  void freeWorkItem(
          IoUringWorkItem*              item);

  void consume();

  void notify();

  static IoUringWorkItemType getRequestType(
          IoRequestType                   type);

};

}
