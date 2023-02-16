#include "../../util/util_error.h"
#include "../../util/util_log.h"
#include "../../util/util_math.h"

#include "io_uring.h"
#include "io_uring_request.h"

namespace as {

IoUring::IoUring(
        uint32_t                      workerCount) {
  Log::info("Initializing io_uring I/O");

  // Initialize file descriptor table with invalid FDs
  for (auto& fd : m_fdTable)
    fd = -1;

  // Mark all entries in the FD table as free
  for (auto& mask : m_fdAllocator)
    mask = 0;

  if (io_uring_queue_init(QueueDepth, &m_ring, 0))
    throw Error("IoUring: io_uring_queue_init() failed");

  // Large fixed buffers may not be supported on all systems. Query
  // the limit and select a viable buffer size based on that.
  ::rlimit limit = { };
  getrlimit(RLIMIT_MEMLOCK, &limit);

  size_t streamBufferSize = std::min<rlim_t>(limit.rlim_cur / 4, MaxStreamBufferSize);
  m_useFixed = streamBufferSize >= MinStreamBufferSize;

  if (!m_useFixed)
    streamBufferSize = MaxStreamBufferSize;

  // Allocate a fixed buffer to use for stream operations
  m_streamBuffer = std::calloc(1, streamBufferSize);
  m_streamAllocator = ChunkAllocator(uint32_t(streamBufferSize));

  // Even if registering the fixed buffer fails, we should keep
  // the fixed buffer around to avoid frequent allocations when
  // performing stream operations.
  if (m_useFixed) {
    ::iovec streamBufferDesc;
    streamBufferDesc.iov_base = m_streamBuffer;
    streamBufferDesc.iov_len = streamBufferSize;

    m_useFixed = !io_uring_register_buffers(&m_ring, &streamBufferDesc, 1);

    if (m_useFixed)
      Log::info("IoUring: Using fixed ", (streamBufferSize >> 20), " MiB stream buffer");
    else
      Log::warn("IoUring: io_uring_register_buffers() failed, using plain memory");
  } else {
    Log::info("IoUring: Not using fixed stream buffer");
  }

  // Try to allocate a file descriptor table. If this is
  // not supported, use plain file descriptors instead.
  m_useFdTable = !io_uring_register_files_sparse(&m_ring, MaxFds);

  if (!m_useFdTable)
    Log::warn("IoUring: io_uring_register_files_sparse() failed, using plain fds");

  m_consumer = std::thread([this] { consume(); });

  // Start worker threads
  workerCount = std::max(workerCount, 1u);
  m_callbackWorkers.reserve(workerCount);

  for (uint32_t i = 0; i < workerCount; i++)
    m_callbackWorkers.emplace_back([this] { notify(); });
}


IoUring::~IoUring() {
  Log::info("Shutting down io_uring I/O");

  std::unique_lock lock(m_mutex);
  std::unique_lock callbackLock(m_callbackMutex);
  submit();

  m_stop = true;

  m_consumerCond.notify_one();
  lock.unlock();

  m_callbackCond.notify_all();
  callbackLock.unlock();

  m_consumer.join();

  for (auto& worker : m_callbackWorkers)
    worker.join();

  io_uring_unregister_files(&m_ring);
  io_uring_queue_exit(&m_ring);

  std::free(m_streamBuffer);

  for (auto* workItem : m_workItems)
    delete workItem;
}


IoBackend IoUring::getBackendType() const {
  return IoBackend::eIoUring;
}


IoFile IoUring::open(
  const std::filesystem::path&        path,
        IoOpenMode                    mode) {
  int openFlags = 0;

  switch (mode) {
    case IoOpenMode::eRead:
      openFlags = O_RDONLY;
      break;

    case IoOpenMode::eWrite:
      openFlags = O_WRONLY;
      break;

    case IoOpenMode::eWriteOrCreate:
      openFlags = O_WRONLY | O_CREAT;
      break;

    case IoOpenMode::eCreate:
      openFlags = O_WRONLY | O_CREAT | O_TRUNC;
      break;

    case IoOpenMode::eCreateOrFail:
      openFlags = O_WRONLY | O_CREAT | O_EXCL;
      break;
  }

  mode_t openMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd = ::open(path.c_str(), openFlags, openMode);

  if (fd < 0)
    return IoFile();

  IoMode fileMode = mode == IoOpenMode::eRead ? IoMode::eRead : IoMode::eWrite;
  return IoFile(std::make_shared<IoUringFile>(shared_from_this(), path, fileMode, fd, registerFile(fd)));
}


IoRequest IoUring::createRequest() {
  return IoRequest(std::make_shared<IoUringRequest>());
}


bool IoUring::submit(
  const IoRequest&                    request) {
  std::unique_lock lock(m_mutex);

  if (!request || request->getStatus() != IoStatus::eReset)
    return false;

  auto& uringRequest = static_cast<IoUringRequest&>(*request);
  uringRequest.setPending();

  bool result = uringRequest.processRequests(
    [this, request] (uint32_t index, IoBufferedRequest& item) {
      if (item.type == IoRequestType::eNone)
        return true;

      auto& file = static_cast<IoUringFile&>(*item.file);
      auto workItem = allocWorkItem();

      workItem->request = request;
      workItem->requestIndex = index;
      workItem->index = file.getIndex();
      workItem->fd = file.getFd();
      workItem->offset = item.offset;
      workItem->size = item.size;

      switch (item.type) {
        case IoRequestType::eRead:
          workItem->type = IoUringWorkItemType::eRead;
          workItem->dst = static_cast<char*>(item.dst);
          break;

        case IoRequestType::eWrite:
          workItem->type = IoUringWorkItemType::eWrite;
          workItem->src = static_cast<const char*>(item.src);
          break;

        case IoRequestType::eStream:
          workItem->type = IoUringWorkItemType::eStream;

          // Try to allocate memory from the fixed buffer,
          // otherwise allocate a new memory block.
          if (workItem->size <= m_streamAllocator.capacity()) {
            uint32_t alignment = 4096;
            uint32_t size = align(uint32_t(workItem->size), alignment);

            auto offset = m_streamAllocator.alloc(size, alignment);

            if (offset) {
              workItem->flags |= IoUringWorkItemFlag::eStreamBuffer;
              workItem->bufferRange.offset = *offset;
              workItem->bufferRange.size = size;
              workItem->dst = static_cast<char*>(m_streamBuffer) + workItem->bufferRange.offset;
            }
          }

          if (!workItem->dst) {
            workItem->flags |= IoUringWorkItemFlag::eStreamAlloc;
            workItem->bufferAlloc = static_cast<char*>(std::calloc(1, workItem->size));
            workItem->dst = workItem->bufferAlloc;
          }

          item.dst = workItem->dst;
          break;

        default:
          throw Error("IoUring: Unsupported request type");
      }

      return enqueue(workItem);
    });

  return result && submit();
}


int IoUring::registerFile(
        int                           fd) {
  if (!m_useFdTable)
    return -1;

  std::unique_lock lock(m_mutex);

  int index = -1;

  for (uint32_t i = 0; i < m_fdAllocator.size() && index < 0; i++) {
    uint32_t bit = tzcnt(~m_fdAllocator[i]);

    if (bit < 64) {
      index = 64 * i + bit;

      m_fdTable[index] = fd;
      m_fdAllocator[i] |= 1ull << bit;
    }
  }

  if (index < 0)
    return index;

  auto item = allocWorkItem();
  item->type = IoUringWorkItemType::eRegister;
  item->index = index;
  item->fd = 1;

  enqueue(item);
  return index;
}


void IoUring::unregisterFile(
        int                           index) {
  std::unique_lock lock(m_mutex);

  auto item = allocWorkItem();
  item->type = IoUringWorkItemType::eRegister;
  item->index = index;
  item->fd = 1;

  uint32_t set = uint32_t(index) / 64;
  uint32_t bit = uint32_t(index) % 64;

  m_fdTable[index] = -1;
  m_fdAllocator[set] &= ~(1ull << bit);

  enqueue(item);
}


bool IoUring::enqueue(
      IoUringWorkItem*              item) {
  io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);

  if (!sqe) {
    Log::err("IoUring: io_uring_get_sqe() failed");
    return false;
  }

  // We can't read or write more than INT_MAX bytes per sqe, so just
  // reduce the submission size here and let the consumer requeue the
  // work item as necessary.
  uint64_t size = std::min<uint64_t>(item->size, 1ull << 30);

  // For read/write operations, use the registered file descriptor
  // index if possible.
  int fd = item->index < 0 ? item->fd : item->index;

  switch (item->type) {
    case IoUringWorkItemType::eRead:
      io_uring_prep_read(sqe, fd, item->dst, size, item->offset);
      break;

    case IoUringWorkItemType::eWrite:
      io_uring_prep_write(sqe, fd, item->src, size, item->offset);
      break;

    case IoUringWorkItemType::eStream:
      if ((item->flags & IoUringWorkItemFlag::eStreamBuffer) && m_useFixed)
        io_uring_prep_read_fixed(sqe, fd, item->dst, size, item->offset, 0);
      else
        io_uring_prep_read(sqe, fd, item->dst, size, item->offset);
      break;

    case IoUringWorkItemType::eRegister:
      io_uring_prep_files_update(sqe, &m_fdTable[item->index], item->fd, item->index);
      break;
  }

  io_uring_sqe_set_data(sqe, item);

  if (item->type != IoUringWorkItemType::eRegister && item->index > -1)
    sqe->flags |= IOSQE_FIXED_FILE;

  m_opsInQueue += 1;

  if (m_opsInQueue < QueueDepth)
    return true;

  // Perform a submission if the queue is full
  return submit();
}


bool IoUring::submit() {
  if (!m_opsInQueue)
    return true;

  int submitted = io_uring_submit(&m_ring);

  if (submitted < 0) {
    Log::err("IoUring: io_uring_submit() failed");
    return false;
  }

  m_opsInFlight += submitted;
  m_opsInQueue -= submitted;

  m_consumerCond.notify_one();
  return true;
}


IoUringWorkItem* IoUring::allocWorkItem() {
  IoUringWorkItem* item;

  if (!m_workItems.empty()) {
    item = m_workItems.back();
    m_workItems.pop_back();
  } else {
    item = new IoUringWorkItem();
  }

  // Zero-initialize the object
  *item = IoUringWorkItem();
  return item;
}


void IoUring::freeWorkItem(
        IoUringWorkItem*              item) {
  m_workItems.push_back(item);

  // Free allocated buffer for stream requests
  if (item->flags & IoUringWorkItemFlag::eStreamAlloc)
    std::free(item->bufferAlloc);
  else if (item->flags & IoUringWorkItemFlag::eStreamBuffer)
    m_streamAllocator.free(item->bufferRange.offset, item->bufferRange.size);
}


void IoUring::consume() {
  std::unique_lock lock(m_mutex);

  while (true) {
    bool requeue = false;

    m_consumerCond.wait(lock, [this] {
      return m_opsInFlight || m_stop;
    });

    // Ensure that all pending completion events are processed
    if (m_stop && !m_opsInFlight)
      return;

    // Unlock before waiting for a completion event so that
    // we don't deadlock the entire I/O system
    lock.unlock();

    io_uring_cqe* cqe = nullptr;

    if (io_uring_wait_cqe(&m_ring, &cqe) < 0) {
      Log::err("IoUring: io_uring_wait_cqe() failed, aborting");
      return;
    }

    auto res = cqe->res;
    auto item = reinterpret_cast<IoUringWorkItem*>(io_uring_cqe_get_data(cqe));

    io_uring_cqe_seen(&m_ring, cqe);

    // Process the result. 
    if (item->type == IoUringWorkItemType::eRegister) {
      if (res < 0)
        Log::err("IoUring: Updating registered files failed");
    } else if (res <= 0) {
      // On error, notify the request and destroy the work item
      auto& request = static_cast<IoUringRequest&>(*item->request);
      request.notify(item->requestIndex, IoStatus::eError);
    } else if (uint64_t(res) < item->size) {
      // If only a portion of the request has completed
      // so far, adjust the parameters and re-queue it
      uint64_t size = uint64_t(res);
      item->offset += size;
      item->size -= res;

      if (item->dst) item->dst += size;
      if (item->src) item->src += size;

      requeue = true;
    } else {
      // Otherwise, the entrie request has completed
      auto& request = static_cast<IoUringRequest&>(*item->request);

      if (request.hasCallback(item->requestIndex)) {
        // Forward sub-request to one of the workers to process the
        // callback there. We don't want callbacks to stall I/O.
        std::unique_lock callbackLock(m_callbackMutex);
        m_callbackQueue.push(item);
        m_callbackCond.notify_one();

        item = nullptr;
      } else {
        // If no callback is present, notify sub-request
        // immediately in order to to avoid overhead
        request.notify(item->requestIndex, IoStatus::eSuccess);
      }
    }

    lock.lock();
    m_opsInFlight -= 1;

    if (item) {
      if (requeue) {
        // Requeue item. If this goes wrong for whatever
        // reason, mark the request as failed.
        if (!enqueue(item)) {
          auto& request = static_cast<IoUringRequest&>(*item->request);
          request.notify(item->requestIndex, IoStatus::eError);
          freeWorkItem(item);
        }
      } else {
        // Recycle work item
        freeWorkItem(item);
      }
    }

    // If all submitted operations have completed, submit
    // again so that any requeued operations get executed
    if (!m_opsInFlight && m_opsInQueue)
      submit();
  }
}


void IoUring::notify() {
  while (true) {
    std::unique_lock callbackLock(m_callbackMutex);

    m_callbackCond.wait(callbackLock, [this] {
      return !m_callbackQueue.empty() || m_stop;
    });

    // Ensure that all pending callbacks are processed
    if (m_stop && m_callbackQueue.empty())
      return;

    IoUringWorkItem* item = m_callbackQueue.front();
    m_callbackQueue.pop();

    callbackLock.unlock();

    // We know that the I/O operation itself has completed successfully
    // at this point, so just notify the request with success status.
    auto& request = static_cast<IoUringRequest&>(*item->request);
    request.notify(item->requestIndex, IoStatus::eSuccess);

    // Free work item. This needs the global I/O lock.
    std::unique_lock lock(m_mutex);
    freeWorkItem(item);
  }
}

}
