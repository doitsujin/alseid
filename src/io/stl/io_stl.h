#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>

#include "../io.h"

#include "io_stl_file.h"

namespace as {

/**
 * \brief STL implementation of the I/O interface
 *
 * This implements I/O operations using fstream functions,
 * with asynchronous I/O using one worker for reading and
 * one worker for writing.
 */
class IoStl : public IoIface {

public:

  IoStl();

  ~IoStl();

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

private:

  std::mutex              m_mutex;
  std::condition_variable m_cond;
  std::queue<IoRequest>   m_queue;
  std::thread             m_worker;

  void run();

};

}
