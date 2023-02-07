#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "job.h"

namespace as {

/**
 * \brief Job manager
 *
 * Provides a job queue as well as the
 * worker threads to execute those jobs.
 */
class JobManager {

public:

  JobManager();
  ~JobManager();

  JobManager             (const JobManager&) = delete;
  JobManager& operator = (const JobManager&) = delete;

  /**
   * \brief Number of workers
   * \returns Worker count
   */
  uint32_t getWorkerCount() const {
    return uint32_t(m_workers.size());
  }

  /**
   * \brief Creates a job
   *
   * \tparam T Job template
   * \param [in] proc Function to execute
   * \param [in] args Constructor arguments
   * \returns The dispatched job object
   */
  template<template<class> class T, typename Fn, typename... Args>
  auto create(
          Fn&&                      proc,
          Args...                   args) {
    return std::make_shared<T<Fn>>(
      std::move(proc),
      std::forward<Args>(args)...);
  }

  /**
   * \brief Dispatches a job
   *
   * The dependency list may contain single jobs or
   * iterator pairs that walk over a set of jobs.
   * The dependency list may include null pointers,
   * which will be ignored. Jobs without dependencies
   * will be added to the queue immediately.
   * \param [in] job Job to execute
   * \param [in] dependencies Job dependencies
   * \returns The dispatched job object
   */
  template<typename... Deps>
  auto dispatch(
          std::shared_ptr<Job>      job,
          Deps...                   dependencies) {
    std::lock_guard lock(m_mutex);
    m_pending += 1;

    if (!registerDependencies(false, job, std::forward<Deps>(dependencies)...)) {
      enqueueJob(job);
      m_queueCond.notify_all();
    }

    return job;
  }

  /**
   * \brief Waits for given job to finish
   * \param [in] job Job to wait for
   */
  void wait(
    const std::shared_ptr<Job>&     job);

  /**
   * \brief Waits for all pending jobs to finish
   */
  void waitAll();

private:

  bool registerDependencies(
          bool                      wait,
    const std::shared_ptr<Job>&     job) {
    return wait;
  }

  template<typename... Args>
  bool registerDependencies(
          bool                      wait,
    const std::shared_ptr<Job>&     job,
    const std::shared_ptr<Job>&     dep,
          Args...                   args) {
    wait |= registerDependency(job, dep);
    return registerDependencies(wait,
      job, std::forward<Args>(args)...);
  }

  template<typename Iter, typename... Args>
  bool registerDependencies(
          bool                      wait,
    const std::shared_ptr<Job>&     job,
          std::pair<Iter, Iter>     iter,
          Args...                   args) {
    for (auto i = iter.first; i != iter.second; i++)
      wait |= registerDependency(job, *i);

    return registerDependencies(wait,
      job, std::forward<Args>(args)...);
  }

  bool registerDependency(
    const std::shared_ptr<Job>&     job,
    const std::shared_ptr<Job>&     dep);

  void enqueueJob(
          std::shared_ptr<Job>      job);

  void notifyJob(
    const std::shared_ptr<Job>&     job);

  void runWorker(
          uint32_t                  workerId);

  std::mutex                        m_mutex;
  std::condition_variable           m_queueCond;
  std::queue<std::shared_ptr<Job>>  m_queue;
  std::unordered_multimap<
    std::shared_ptr<Job>,
    std::shared_ptr<Job>>           m_dependencies;

  std::condition_variable           m_pendingCond;
  uint64_t                          m_pending = 0ull;

  std::vector<std::thread>          m_workers;

};

}
