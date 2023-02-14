#include "../../util/util_log.h"

#include "io_uring.h"
#include "io_uring_file.h"

namespace as {

IoUringFile::IoUringFile(
        std::shared_ptr<IoUring>      io,
        std::filesystem::path         path,
        IoMode                        mode,
        int                           fd,
        int                           index)
: IoFileIface(path, mode)
, m_io    (std::move(io))
, m_fd    (fd)
, m_index (index) {
  struct ::stat s = { };
  int err = ::fstat(m_fd, &s);

  if (err)
    Log::err("IoUring: fstat() failed with error code ", err);

  m_fileSize.store(s.st_size);
}


IoUringFile::~IoUringFile() {
  if (m_index > -1)
    m_io->unregisterFile(m_index);

  ::close(m_fd);
}


uint64_t IoUringFile::getSize() {
  return m_fileSize.load();
}


IoStatus IoUringFile::read(
        uint64_t                      offset,
        uint64_t                      size,
        void*                         dst) {
  if (m_mode != IoMode::eRead)
    return IoStatus::eError;

  if (size + offset > m_fileSize)
    return IoStatus::eError;

  if (!size)
    return IoStatus::eSuccess;

  if (::lseek(m_fd, offset, SEEK_SET) < 0)
    return IoStatus::eError;

  auto data = reinterpret_cast<char*>(dst);

  while (size) {
    ssize_t read = ::read(m_fd, data, std::min<uint64_t>(size, 1ull << 30));

    if (read < 0)
      return IoStatus::eError;

    size -= read;
    data += read;
  }

  return IoStatus::eSuccess;
}


IoStatus IoUringFile::write(
        uint64_t                      offset,
        uint64_t                      size,
  const void*                         src) {
  if (m_mode != IoMode::eWrite)
    return IoStatus::eError;

  if (offset > m_fileSize)
    return IoStatus::eError;

  if (!size)
    return IoStatus::eSuccess;

  if (::lseek(m_fd, offset, SEEK_SET) < 0)
    return IoStatus::eError;

  auto data = reinterpret_cast<const char*>(src);

  while (size) {
    ssize_t written = ::write(m_fd, data, std::min<uint64_t>(size, 1ull << 30));

    if (written < 0)
      return IoStatus::eError;

    offset += written;
    size -= written;
    data += written;

    // We can non-atomically update the file size
    // since only one thread at a time can write
    if (offset > m_fileSize)
      m_fileSize = offset;
  }

  return IoStatus::eSuccess;
}

}