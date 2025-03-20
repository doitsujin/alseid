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


bool JobIface::completeWorkItems(
        uint32_t                      count) {
  uint32_t done = m_done.fetch_add(count,
    std::memory_order_acq_rel) + count;
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
  if (!job)
    return;

  if (!runJob(job)) {
    // TODO implement some sort of futex
    while (!job->isDone())
      continue;
  }
}


void JobsIface::waitAll() {
  std::unique_lock lock(m_mutex);
  m_pendingCond.wait(lock, [this] {
    return !m_pending;
  });
}


void JobsIface::enqueueJobLocked(
        Job                           job) {
  m_pending += 1;

  m_queue.push(std::move(job));
  m_queueCond.notify_all();
}


void JobsIface::notifyJobLocked(
  const Job&                          job) {
  if (!(--m_pending))
    m_pendingCond.notify_all();
}


bool JobsIface::runJob(
  const Job&                          job) {
  uint32_t invocationIndex = 0;
  uint32_t invocationCount = 0;

  job->getWorkItems(invocationIndex, invocationCount);

  if (!invocationCount)
    return job->isDone();

  job->execute(invocationIndex, invocationCount);

  if (!job->completeWorkItems(invocationCount))
    return false;

  std::unique_lock lock(m_mutex);
  notifyJobLocked(job);
  return true;
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
      done = job->completeWorkItems(invocationCount);
      job->getWorkItems(invocationIndex, invocationCount);
    } while (invocationCount);

    // If the job is done, enqueue jobs that depend on it
    if (done) {
      lock.lock();

      notifyJobLocked(job);
    }
  }
}


Jobs::Jobs(
        uint32_t                      threadCount)
: IfaceRef<JobsIface>(std::make_shared<JobsIface>(threadCount)) {

}

}
