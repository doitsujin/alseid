#pragma once

#include "../util/util_small_vector.h"

#include "gfx_command_list.h"
#include "gfx_semaphore.h"

namespace as {

/**
 * \brief Semaphore entry
 *
 * Data structure used internally to hold info
 * about a semaphore wait or signal operation.
 */
struct GfxSemaphoreEntry {
  GfxSemaphore semaphore;
  uint64_t value;
};


/**
 * \brief Internal command submission info
 *
 * Provides access to the arrays managed by the command
 * submission helper class. Internal use only.
 */
struct GfxCommandSubmissionInternal {
  size_t                    commandListCount;
  const GfxCommandList*     commandLists;
  size_t                    waitSemaphoreCount;
  const GfxSemaphoreEntry*  waitSemaphores;
  size_t                    signalSemaphoreCount;
  const GfxSemaphoreEntry*  signalSemaphores;
};


/**
 * \brief Command submission helper
 *
 * Helper class that can be used to bundle up command list
 * objects as well as semaphores for command submissions.
 */
class GfxCommandSubmission {

public:

  GfxCommandSubmission();

  ~GfxCommandSubmission();

  GfxCommandSubmission             (const GfxCommandSubmission&) = delete;
  GfxCommandSubmission& operator = (const GfxCommandSubmission&) = delete;

  GfxCommandSubmission             (GfxCommandSubmission&&) = default;
  GfxCommandSubmission& operator = (GfxCommandSubmission&&) = default;

  /**
   * \brief Adds a command list to the submission
   *
   * \param [in] commandList The command list
   */
  void addCommandList(
          GfxCommandList&&              commandList);

  /**
   * \brief Adds a semaphore to wait for
   *
   * Blocks all command buffers in this submission, as well as
   * all subsequent submissions to the same queue, until the
   * given semaphore has reached the desired value.
   * \param [in] semaphore The semaphore
   * \param [in] value Semaphore value to wait for
   */
  void addWaitSemaphore(
          GfxSemaphore                  semaphore,
          uint64_t                      value);

  /**
   * \brief Adds a semaphore to signal
   *
   * Signals the given semaphore with the given value once all
   * command buffers in this submission have completed execution.
   * \param [in] semaphore The semaphore to signal
   * \param [in] value Semaphore value to set
   */
  void addSignalSemaphore(
          GfxSemaphore                  semaphore,
          uint64_t                      value);

  /**
   * \brief Clears submission object
   *
   * Removes all entries from the object.
   */
  void clear();

  /**
   * \brief Checks if the command submission is empty
   *
   * A submission is empty if it has no command buffers and
   * no semaphore operations.
   * \returns \c true if the submission is empty
   */
  bool isEmpty() const;

  /**
   * \brief Command submission info
   *
   * Provides access to the arrays stored inside the
   * structure. This is only intended to be used by
   * backends to process command submissions.
   * \returns Command submission info
   */
  GfxCommandSubmissionInternal getInternalInfo() const;

private:

  small_vector<GfxCommandList,    32> m_commandLists;
  small_vector<GfxSemaphoreEntry, 16> m_waitSemaphores;
  small_vector<GfxSemaphoreEntry, 16> m_signalSemaphores;

};

}
