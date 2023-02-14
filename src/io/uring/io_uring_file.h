#pragma once

#include "../io_file.h"

#include "io_uring_include.h"

namespace as {

class IoUring;

/**
 * \brief Linux io_uring file implementation
 */
class IoUringFile : public IoFileIface {

public:

  IoUringFile(
          std::shared_ptr<IoUring>      io,
          std::filesystem::path         path,
          IoMode                        mode,
          int                           fd,
          int                           index);

  ~IoUringFile();

  /**
   * \brief Queries file descriptor
   * \returns File descriptor
   */
  int getFd() const {
    return m_fd;
  }

  /**
   * \brief Queries descriptor index
   *
   * Negative values indicate that this file
   * is not yet registered with the ring.
   * \returns Descriptor index
   */
  int getIndex() const {
    return m_index;
  }

  /**
   * \brief Queries current file size
   * \returns Current file size
   */
  uint64_t getSize() override;

  /**
   * \brief Performs a synchronous read operation
   *
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] dst Destination pointer
   * \returns Status of the operation
   */
  IoStatus read(
          uint64_t                      offset,
          uint64_t                      size,
          void*                         dst) override;

  /**
   * \brief Performs a synchronous write operation
   *
   * \param [in] offset Offset within the file
   * \param [in] size Number of bytes to read
   * \param [in] src Data to write to the file
   */
  IoStatus write(
          uint64_t                      offset,
          uint64_t                      size,
    const void*                         src) override;

private:

  std::shared_ptr<IoUring> m_io;

  int m_fd    = -1;
  int m_index = -1;

  std::atomic<uint64_t> m_fileSize = { 0ull };

};

}
