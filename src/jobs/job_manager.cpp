#include <algorithm>
#include <memory>

#include "job_manager.h"

namespace as {

JobManager::JobManager() {
  m_workers.resize(std::thread::hardware_concurrency());

  for (uint32_t i = 0; i < m_workers.size(); i++)
    m_workers[i] = std::thread([this, i] { runWorker(i); });
}


JobManager::~JobManager() {
  waitAll();

  { std::lock_guard lock(m_mutex);
    m_queue.push(nullptr);
    m_queueCond.notify_all(); }

  for (auto& worker : m_workers)
    worker.join();
}


void JobManager::wait(
  const std::shared_ptr<Job>&     job) {
  std::unique_lock lock(m_mutex);
  m_pendingCond.wait(lock, [job] {
    return job->isDone();
  });
}


void JobManager::waitAll() {
  std::unique_lock lock(m_mutex);
  m_pendingCond.wait(lock, [this] {
    return !m_pending;
  });
}


bool JobManager::registerDependency(
  const std::shared_ptr<Job>&     job,
  const std::shared_ptr<Job>&     dep) {
  if (!dep || dep->isDone())
    return false;

  job->addDependency();
  m_dependencies.insert({ dep, job  });
  return true;
}


void JobManager::enqueueJob(
        std::shared_ptr<Job>      job) {
  m_queue.push(std::move(job));
}


void JobManager::notifyJob(
  const std::shared_ptr<Job>&     job) {
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


void JobManager::runWorker(
        uint32_t                  workerId) {
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
    std::shared_ptr<Job> job = m_queue.front();

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

}