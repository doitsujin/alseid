#include "../../util/util_error.h"
#include "../../util/util_log.h"
#include "../../util/util_math.h"

#include "io_uring.h"
#include "io_uring_request.h"

namespace as {

IoUring::IoUring() {
  Log::info("Initializing io_uring I/O");

  // Initialize file descriptor table with invalid FDs
  for (auto& fd : m_fdTable)
    fd = -1;

  // Mark all entries in the FD table as free
  for (auto& mask : m_fdAllocator)
    mask = 0;

  if (io_uring_queue_init(QueueDepth, &m_ring, 0))
    throw Error("IoUring: io_uring_queue_init() failed");

  // Try to allocate a file descriptor table. If this is
  // not supported, use plain file descriptors instead.
  m_useFdTable = !io_uring_register_files_sparse(&m_ring, MaxFds);

  if (!m_useFdTable)
    Log::warn("IoUring: io_uring_register_files_sparse() failed, using plain fds");

  m_consumer = std::thread([this] { consume(); });
}


IoUring::~IoUring() {
  Log::info("Shutting down io_uring I/O");

  std::unique_lock lock(m_mutex);
  submit();

  m_stop = true;

  m_consumerCond.notify_one();
  lock.unlock();

  m_consumer.join();

  io_uring_unregister_files(&m_ring);
  io_uring_queue_exit(&m_ring);

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
    [this, request] (uint32_t index, const IoBufferedRequest& item) {
      auto& file = static_cast<IoUringFile&>(*item.file);
      auto workItem = allocWorkItem();

      workItem->request = request;
      workItem->requestIndex = index;
      workItem->type = item.dst
        ? IoUringWorkItemType::eRead
        : IoUringWorkItemType::eWrite;
      workItem->fd = file.getFd();
      workItem->index = file.getIndex();
      workItem->offset = item.offset;
      workItem->size = item.size;
      workItem->dst = reinterpret_cast<char*>(item.dst);
      workItem->src = reinterpret_cast<const char*>(item.src);

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
  item->fd = 1;
  item->index = index;

  enqueue(item);
  return index;
}


void IoUring::unregisterFile(
        int                           index) {
  std::unique_lock lock(m_mutex);

  auto item = allocWorkItem();
  item->type = IoUringWorkItemType::eRegister;
  item->fd = 1;
  item->index = index;

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
  if (!m_workItems.empty()) {
    auto* item = m_workItems.back();
    m_workItems.pop_back();
    return item;
  }

  return new IoUringWorkItem();
}


void IoUring::freeWorkItem(
        IoUringWorkItem*              item) {
  *item = IoUringWorkItem();
  m_workItems.push_back(item);
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
      request.notify(item->requestIndex, IoStatus::eSuccess);
    }

    lock.lock();
    m_opsInFlight -= 1;

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

    // If all submitted operations have completed, submit
    // again so that any requeued operations get executed
    if (!m_opsInFlight && m_opsInQueue)
      submit();
  }
}

}
