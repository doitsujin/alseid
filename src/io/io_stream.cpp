#include "io_stream.h"

namespace as {

InFileStream::InFileStream() {

}


InFileStream::InFileStream(IoFile file)
: m_file(std::move(file))
, m_size(m_file->getSize()) {

}


InFileStream::~InFileStream() {

}


size_t InFileStream::readFromSource(
        void*                         data,
        size_t                        size) {
  size_t read = std::min(size, m_size - m_offset);

  if (data) {
    if (m_file->read(m_offset, read, data) != IoStatus::eSuccess)
      return 0;
  }

  m_offset += read;
  return read;
}




OutFileStream::OutFileStream() {

}


OutFileStream::OutFileStream(IoFile file)
: m_file(std::move(file))
, m_size(m_file->getSize()) {

}


OutFileStream::~OutFileStream() {
  flush();
}


std::pair<size_t, size_t> OutFileStream::writeToContainer(
  const void*                         data,
        size_t                        size) {
  if (m_file->write(m_size, size, data) != IoStatus::eSuccess)
    return std::make_pair(size_t(0), size_t(0));

  m_size += size;
  return std::make_pair(size, size_t(-1));
}

}
