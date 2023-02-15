#include "io_stl_file.h"
#include "io_stl_request.h"

namespace as {

IoStlRequest::IoStlRequest() {

}


IoStlRequest::~IoStlRequest() {

}


void IoStlRequest::execute() {
  IoStatus status = IoStatus::eSuccess;

  for (size_t i = 0; i < m_items.size(); i++) {
    const auto& item = m_items[i];
    auto& file = static_cast<IoStlFile&>(*item.file);

    if (item.dst)
      status = file.read(item.offset, item.size, item.dst);
    else if (item.src)
      status = file.write(item.offset, item.size, item.src);

    if (status == IoStatus::eSuccess && item.cb)
      status = item.cb();

    if (status == IoStatus::eError)
      break;
  }

  m_items.clear();
  setStatus(status);
}


void IoStlRequest::setPending() {
  setStatus(IoStatus::ePending);
}

}
