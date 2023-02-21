#include <algorithm>

#include "job.h"

namespace as {

JobIface::JobIface(
        uint32_t                      itemCount,
        uint32_t                      itemGroup)
: m_itemCount (itemCount)
, m_itemGroup (itemGroup) {

}


JobIface::~JobIface() {

}


bool JobIface::getWorkItems(
        uint32_t&                     index,
        uint32_t&                     count) {
  uint32_t next = m_next.load(std::memory_order_acquire);
  uint32_t size = std::min(m_itemCount - next, m_itemGroup);

  while (size && !m_next.compare_exchange_weak(next, next + size,
      std::memory_order_acquire, std::memory_order_relaxed))
    size = std::min(m_itemCount - next, m_itemGroup);

  index = next;
  count = size;

  return next + size < m_itemCount;
}


bool JobIface::notifyWorkItems(
        uint32_t                      count) {
  uint32_t done = m_done.fetch_add(count,
    std::memory_order_release) + count;
  return done == m_itemCount;
}




JobsIface::JobsIface(uint32_t threadCount) {
  m_workers.resize(std::max(1u, threadCount));

  for (uint32_t i = 0; i < m_workers.size(); i++)
    m_workers[i] = std::thread([this, i] { runWorker(i); });
}


JobsIface::~JobsIface() {
  waitAll();

  { std::lock_guard lock(m_mutex);
    m_queue.push(nullptr);
    m_queueCond.notify_all(); }

  for (auto& worker : m_workers)
    worker.join();
}


void JobsIface::wait(
  const Job&                          job) {
  if (job && !job->isDone()) {
    std::unique_lock lock(m_mutex);
    m_pendingCond.wait(lock, [job] {
      return job->isDone();
    });
  }
}


void JobsIface::waitAll() {
  std::unique_lock lock(m_mutex);
  m_pendingCond.wait(lock, [this] {
    return !m_pending;
  });
}


bool JobsIface::registerDependency(
  const Job&                          job,
  const Job&                          dep) {
  if (!dep || dep->isDone())
    return false;

  job->addDependency();
  m_dependencies.insert({ dep, job  });
  return true;
}


void JobsIface::enqueueJob(
        Job                           job) {
  m_queue.push(std::move(job));
}


void JobsIface::notifyJob(
  const Job&                          job) {
  auto range = m_dependencies.equal_range(job);
  bool notify = false;

  if (range.first != range.second) {
    for (auto i = range.first; i != range.second; i++) {
      if (i->second->notifyDependency()) {
        enqueueJob(std::move(i->second));
        notify = true; 
      }
    }

    m_dependencies.erase(range.first, range.second);
  }

  if (notify)
    m_queueCond.notify_all();

  m_pending -= 1;
  m_pendingCond.notify_all();
}


void JobsIface::runWorker(
        uint32_t                      workerId) {
  std::unique_lock lock(m_mutex, std::defer_lock);

  while (true) {
    if (!lock)
      lock.lock();

    // Wait until a job becomes available
    m_queueCond.wait(lock, [this] {
      return !m_queue.empty();
    });

    // Fetch job and remove it from the queue if we
    // remove the last set of work items from it.
    Job job = m_queue.front();

    if (!job)
      break;

    uint32_t invocationIndex = 0;
    uint32_t invocationCount = 0;

    if (!job->getWorkItems(invocationIndex, invocationCount))
      m_queue.pop();

    if (!invocationCount)
      continue;

    lock.unlock();

    // Execute job until we run out of work items.
    // This is done to reduce lock contention.
    bool done = false;

    do {
      job->execute(invocationIndex, invocationCount);
      done = job->notifyWorkItems(invocationCount);
      job->getWorkItems(invocationIndex, invocationCount);
    } while (invocationCount);

    // If the job is done, enqueue jobs that depend on it
    if (done) {
      lock.lock();
      notifyJob(job);
    }
  }
}


Jobs::Jobs(
        uint32_t                      threadCount)
: IfaceRef<JobsIface>(std::make_shared<JobsIface>(threadCount)) {

}

}
