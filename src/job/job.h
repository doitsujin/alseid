#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../util/util_hash.h"
#include "../util/util_iface.h"

namespace as {

/**
 * \brief Job interface
 */
class JobIface {

public:

  /**
   * \brief Initializes job
   *
   * \param [in] itemCount Work item count
   * \param [in] itemGroup Work group size
   */
  JobIface(
          uint32_t                      itemCount,
          uint32_t                      itemGroup);

  virtual ~JobIface();

  /**
   * \brief Executes given set of work items
   *
   * Each worker may choose to execute multiple work items
   * at once, which is often more efficient than executing
   * one item at a time.
   * \param [in] ctx Job context
   * \param [in] index Index of first invocation
   * \param [in] count Number of invocations
   */
  virtual void execute(
          uint32_t                      index,
          uint32_t                      count) = 0;

  /**
   * \brief Checks whether job is done
   *
   * For jobs with dependencies, this checks whether
   * all dependencies are satisfied as well.
   * \returns \c true if job has finished executing
   */
  bool isDone() const {
    return m_done.load(std::memory_order_acquire) == m_itemCount;
  }

  /**
   * \brief Gets range of work items to execute
   *
   * \param [out] index Index of first work item
   * \param [out] count Work item count. May be zero.
   * \returns \c false if and only if the last set of work
   *    items has been successfully extracted and the job
   *    should be removed from the queue.
   */
  bool getWorkItems(
          uint32_t&                     index,
          uint32_t&                     count);

  /**
   * \brief Marks given number of work items as done
   *
   * Called by the worker after executing a given set
   * of work items. This should be used to determine
   * whether or not dependent jobs can be started.
   * \returns \c true if all work items are done.
   */
  bool completeWorkItems(
          uint32_t                      count);

private:

  uint32_t m_itemCount = 0u;
  uint32_t m_itemGroup = 0u;

  std::atomic<uint32_t> m_next = { 0u };
  std::atomic<uint32_t> m_done = { 0u };

};

/** See JobIface. */
using Job = IfaceRef<JobIface>;


/**
 * \brief Simple job
 *
 * Executes one single invocation.
 */
template<typename Fn>
class SimpleJob : public JobIface {

public:

  SimpleJob(Fn&& proc)
  : JobIface(1, 1), m_proc(std::move(proc)) { }

  void execute(uint32_t index, uint32_t count) {
    m_proc();
  }

private:

  Fn m_proc;

};


/**
 * \brief Batch job
 *
 * Executes the given functions multiple times,
 * with one function call per invocation. The
 * workgroup size should be chosen so that it
 * reduces function call overhead.
 */
template<typename Fn>
class BatchJob : public JobIface {

public:

  BatchJob(Fn&& proc, uint32_t itemCount, uint32_t itemGroup)
  : JobIface(itemCount, itemGroup), m_proc(std::move(proc)) { }

  void execute(uint32_t index, uint32_t count) {
    for (uint32_t i = index; i < index + count; i++)
      m_proc(i);
  }

private:

  Fn m_proc;

};


/**
 * \brief Complex job
 *
 * Executes the given function once per workgroup,
 * with a known invocation index and count. This
 * is useful for jobs that compute data locally
 * and then perform some sort of reduction step.
 */
template<typename Fn>
class ComplexJob : public JobIface {

public:

  ComplexJob(Fn&& proc, uint32_t itemCount, uint32_t itemGroup)
  : JobIface(itemCount, itemGroup), m_proc(std::move(proc)) { }

  void execute(uint32_t index, uint32_t count) {
    m_proc(index, count);
  }

private:

  Fn m_proc;

};


/**
 * \brief Cooperative job
 *
 * Extends the complex job interface with a shared
 * payload that is passed to the function as the
 * first parameter.
 */
template<typename Fn>
class CooperativeJob : public JobIface {
  template<typename Proc>
  struct PayloadType;

  template<typename Ret, typename Arg, typename... Args>
  struct PayloadType<std::function<Ret (Arg, Args...)>> {
    using Type = std::remove_cv_t<std::remove_reference_t<Arg>>;
  };

  using Payload = typename PayloadType<decltype(std::function(Fn()))>::Type;
public:

  template<typename... Args>
  CooperativeJob(Fn&& proc, uint32_t itemCount, uint32_t itemGroup, Args&&... args)
  : JobIface  (itemCount, itemGroup)
  , m_proc    (std::move(proc))
  , m_payload (std::forward<Args>(args)...) { }

  void execute(uint32_t index, uint32_t count) {
    m_proc(m_payload, index, count);
  }

private:

  Fn      m_proc;
  Payload m_payload;

};


/**
 * \brief Job manager interface
 *
 * Provides a job queue as well as the
 * worker threads to execute those jobs.
 */
class JobsIface {

public:

  JobsIface(
          uint32_t                      threadCount);

  ~JobsIface();

  JobsIface             (const JobsIface&) = delete;
  JobsIface& operator = (const JobsIface&) = delete;

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
  Job create(
          Fn&&                          proc,
          Args...                       args) {
    return Job(std::make_shared<T<Fn>>(
      std::move(proc),
      std::forward<Args>(args)...));
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
  Job dispatch(
          Job                           job) {
    std::lock_guard lock(m_mutex);
    enqueueJobLocked(job);
    return job;
  }

  /**
   * \brief Waits for given job to finish
   * \param [in] job Job to wait for
   */
  void wait(
    const Job&                          job);

  /**
   * \brief Waits for given set of jobs to finish
   *
   * \param [in] begin Begin iterator
   * \param [in] end End iterator
   */
  template<typename Iter>
  void wait(
          Iter                          begin,
          Iter                          end) {
    while (begin != end)
      wait(*(begin++));
  }

  /**
   * \brief Waits for all pending jobs to finish
   */
  void waitAll();

private:

  void enqueueJobLocked(
          Job                           job);

  void notifyJobLocked(
    const Job&                          job);

  void runWorker(
          uint32_t                      workerId);

  std::mutex                        m_mutex;
  std::condition_variable           m_queueCond;
  std::queue<Job>                   m_queue;

  std::condition_variable           m_pendingCond;
  uint64_t                          m_pending = 0ull;

  std::vector<std::thread>          m_workers;

};


/**
 * \brief Job manager
 *
 * See JobsIface.
 */
class Jobs : public IfaceRef<JobsIface> {

public:

  Jobs() { }
  Jobs(std::nullptr_t) { }

  /**
   * \brief Initializes job manager
   * \param [in] threadCount Number of worker threads
   */
  explicit Jobs(
          uint32_t                      threadCount);

};

}
