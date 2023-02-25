#pragma once

#include "../util/util_flags.h"
#include "../util/util_iface.h"

#include "io_file.h"
#include "io_request.h"

namespace as {

/**
 * \brief I/O instance flags
 */
enum class IoBackend : uint32_t {
  /** Platform-specific default */
  eDefault          = 0,
  /** STL fstream backend */
  eStl              = 1,
  /** Linux io_uring backend */
  eIoUring          = 2,
};


/**
 * \brief I/O interface
 */
class IoIface {

public:

  virtual ~IoIface() { }

  /**
   * \brief Queries backend type
   * \returns Backend type
   */
  virtual IoBackend getBackendType() const = 0;

  /**
   * \brief Opens a file
   *
   * \param [in] path File path, in a platform-specific format
   * \param [in] mode Mode to open the file with
   * \returns File object on success, or \c nullptr on error.
   */
  virtual IoFile open(
    const std::filesystem::path&        path,
          IoOpenMode                    mode) = 0;

  /**
   * \brief Creates an I/O request object
   * \returns Asynchronous I/O request
   */
  virtual IoRequest createRequest() = 0;

  /**
   * \brief Submits an I/O request
   *
   * Note that any given request can only be submitted once.
   * \param [in] request Request to submit
   * \returns \c true on success
   */
  virtual bool submit(
    const IoRequest&                    request) = 0;

};


/**
 * \brief I/O system
 *
 * See Ioiface.
 */
class Io : public IfaceRef<IoIface> {

public:

  Io() { }
  Io(std::nullptr_t) { }

  /**
   * \brief Initializes I/O system with the given backend
   *
   * \param [in] backend The preferred I/O backend
   * \param [in] workerCount Number of worker threads. The
   *    backend will always create at least one worker to
   *    process request callbacks on.
   */
  Io(
          IoBackend                     backend,
          uint32_t                      workerCount);

private:

  static std::shared_ptr<IoIface> initBackend(
          IoBackend                     backend,
          uint32_t                      workerCount);

};

}
