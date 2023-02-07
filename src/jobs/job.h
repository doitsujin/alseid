#pragma once

#include <atomic>
#include <utility>

namespace as {

/**
 * \brief Job interface
 */
class Job {

public:

  /**
   * \brief Initializes job
   *
   * \param [in] itemCount Work item count
   * \param [in] itemGroup Work group size
   */
  Job(
          uint32_t              itemCount,
          uint32_t              itemGroup);

  virtual ~Job();

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
          uint32_t              index,
          uint32_t              count) const = 0;

  /**
   * \brief Checks whether job is done
   * \returns \c true if job has finished executing
   */
  bool isDone() const {
    return m_done.load() == m_itemCount;
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
          uint32_t&             index,
          uint32_t&             count);

  /**
   * \brief Marks given number of work items as done
   *
   * Called by the worker after executing a given set
   * of work items. This should be used to determine
   * whether or not dependent jobs can be started.
   * \returns \c true if all work items are done.
   */
  bool notifyWorkItems(
          uint32_t              count);

  /**
   * \brief Adds dependency
   *
   * Called by the job manager when dispatching
   * the job with a non-zero dependency count.
   */
  void addDependency() {
    m_deps += 1;
  }

  /**
   * \brief Notifies dependency
   *
   * Decrements dependency count by one.
   * \returns \c true if no dependencies are left.
   */
  bool notifyDependency() {
    return !(--m_deps);
  }

private:

  const uint32_t m_itemCount;
  const uint32_t m_itemGroup;

  std::atomic<uint32_t> m_next = { 0u };
  std::atomic<uint32_t> m_done = { 0u };

  uint32_t m_deps = 0u;

};


/**
 * \brief Simple job
 *
 * Executes one single invocation.
 */
template<typename Fn>
class SimpleJob : public Job {

public:

  SimpleJob(Fn&& proc)
  : Job(1, 1)
  , m_proc(std::move(proc)) { }

  void execute(
          uint32_t              index,
          uint32_t              count) const {
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
class BatchJob : public Job {

public:

  BatchJob(
          Fn&&                  proc,
          uint32_t              itemCount,
          uint32_t              itemGroup)
  : Job(itemCount, itemGroup)
  , m_proc(std::move(proc)) { }

  void execute(
          uint32_t              index,
          uint32_t              count) const {
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
class ComplexJob : public Job {

public:

  ComplexJob(
          Fn&&                  proc,
          uint32_t              itemCount,
          uint32_t              itemGroup)
  : Job(itemCount, itemGroup)
  , m_proc(std::move(proc)) { }

  void execute(
          uint32_t              index,
          uint32_t              count) const {
    m_proc(index, count);
  }

private:

  Fn m_proc;

};

}
