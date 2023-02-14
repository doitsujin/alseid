#pragma once

#include <vector>

#include "../io_request.h"

namespace as {

/**
 * \brief STL I/O request
 *
 * This implementation merely buffers requests and
 * provides a method to process them in one go.
 */
class IoStlRequest : public IoRequestIface {

public:

  IoStlRequest();

  ~IoStlRequest();

  /**
   * \brief Executes queued requests
   *
   * Once all requests are executed, this will notify
   * any waiting thread and invoke callbacks.
   */
  void execute();

  /**
   * \brief Sets status to pending
   */
  void setPending();

};

}
