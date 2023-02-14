#include "io_stl_file.h"

namespace as {

IoStlFile::IoStlFile(
        std::filesystem::path         path,
        std::ifstream&&               stream)
: IoFileIface(std::move(path), IoMode::eRead)
, m_istream(std::move(stream))
, m_fileSize(computeFileSize()) {

}


IoStlFile::IoStlFile(
        std::filesystem::path         path,
        std::ofstream&&               stream)
: IoFileIface(std::move(path), IoMode::eWrite)
, m_ostream(std::move(stream))
, m_fileSize(computeFileSize()) {

}


IoStlFile::~IoStlFile() {

}


uint64_t IoStlFile::getSize() {
  return m_fileSize.load();
}


IoStatus IoStlFile::read(
        uint64_t                      offset,
        uint64_t                      size,
        void*                         dst) {
  if (m_mode != IoMode::eRead)
    return IoStatus::eError;

  if (size + offset > m_fileSize)
    return IoStatus::eError;

  if (!size)
    return IoStatus::eSuccess;

  if (!m_istream.seekg(offset))
    return IoStatus::eError;

  if (!m_istream.read(reinterpret_cast<char*>(dst), size))
    return IoStatus::eError;

  return IoStatus::eSuccess;
}


IoStatus IoStlFile::write(
        uint64_t                      offset,
        uint64_t                      size,
  const void*                         src) {
  if (m_mode != IoMode::eWrite)
    return IoStatus::eError;

  if (offset > m_fileSize)
    return IoStatus::eError;

  if (!size)
    return IoStatus::eSuccess;

  if (!m_ostream.seekp(offset))
    return IoStatus::eError;

  if (!m_ostream.write(reinterpret_cast<const char*>(src), size))
    return IoStatus::eError;

  // The file size is only atomic so that reads and writes
  // are atomic, but only one thread can perform writes at
  // a time, so we do not need an atomic CAS loop here.
  if (offset + size > m_fileSize)
    m_fileSize = offset + size;

  return IoStatus::eSuccess;
}


uint64_t IoStlFile::computeFileSize() const {
  return std::filesystem::file_size(m_path);
}

}
