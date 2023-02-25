#pragma once

#include <filesystem>

#include "../util/util_flags.h"
#include "../util/util_iface.h"

namespace as {

/**
 * \brief File open mode
 */
enum class IoOpenMode : uint32_t {
  /** Open file for reading, and fail if the file
   *  does not exist. */
  eRead           = 0,
  /** Open file for writing and preserve its contents,
   * and fail if the file does not already exist. */
  eWrite          = 1,
  /** Open file for writing and preserve its contents, or
   *  create a new file if it does not already exist. */
  eWriteOrCreate  = 2,
  /** Create an empty file and open it for writing. If
   *  the file already exists, it will be overwritten. */
  eCreate         = 3,
  /** Create an empty file if the file does not exist,
   *  or fail if the file does already exist. */
  eCreateOrFail   = 4,
};


/**
 * \brief File mode
 */
enum class IoMode : uint32_t {
  /** File can be used for read operations */
  eRead           = 0,
  /** File can be used for write operations */
  eWrite          = 1,
};


/**
 * \brief I/O request status
 */
enum class IoStatus : uint32_t {
  /** Request successfully completed */
  eSuccess  = 0,
  /** Request failed with an error */
  eError    = 1,
  /** Request pending execution */
  ePending  = 2,
  /** Request not yet submitted */
  eReset    = 3,
};


/**
 * \brief File interface
 */
class IoFileIface {

public:

  IoFileIface(
          std::filesystem::path         path,
          IoMode                        mode)
  : m_path(std::move(path)), m_mode(mode) { }

  virtual ~IoFileIface() { }

  /**
   * \brief Queries file mode
   *
   * This will \e not be equal to the 
   * \returns File mode
   */
  IoMode getMode() const {
    return m_mode;
  }

  /**
   * \brief Queries file path
   * \returns File path
   */
  std::filesystem::path getPath() const {
    return m_path.c_str();
  }

  /**
   * \brief Queries current file size
   *
   * Note that this will not return useful data
   * if used on a file with pending write requests.
   * \returns Current file size
   */
  virtual uint64_t getSize() = 0;

  /**
   * \brief Performs a synchronous read operation
   *
   * This \e must not be called while there are any
   * pending asynchronous read requests for this file,
   * and is \e not thread-safe.
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] dst Destination pointer
   * \returns Status of the operation
   */
  virtual IoStatus read(
          uint64_t                      offset,
          uint64_t                      size,
          void*                         dst) = 0;

  /**
   * \brief Performs a synchronous write operation
   *
   * This \e must not be called while there are any
   * pending asynchronous read requests for this file,
   * and is \e not thread-safe.
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] src Data to write to the file
   */
  virtual IoStatus write(
          uint64_t                      offset,
          uint64_t                      size,
    const void*                         src) = 0;

protected:

  std::filesystem::path m_path;
  IoMode                m_mode;

};

/** See IoFileIface. */
using IoFile = IfaceRef<IoFileIface>;

}
