#pragma once

#include <atomic>
#include <fstream>

#include "../io_file.h"

namespace as {

/**
 * \brief STL file implementation
 */
class IoStlFile : public IoFileIface {

public:

  IoStlFile(
          std::filesystem::path         path,
          std::ifstream&&               stream);

  IoStlFile(
          std::filesystem::path         path,
          std::ofstream&&               stream);

  ~IoStlFile();

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

  std::ifstream m_istream;
  std::ofstream m_ostream;

  std::atomic<uint64_t> m_fileSize = { 0ull };

  uint64_t computeFileSize() const;

};

}
