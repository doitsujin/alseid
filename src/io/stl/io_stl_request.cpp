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
    auto& item = m_items[i];
    auto& file = static_cast<IoStlFile&>(*item.file);

    switch (item.type) {
      case IoRequestType::eNone:
        status = IoStatus::eSuccess;
        break;

      case IoRequestType::eRead:
        status = file.read(item.offset, item.size, item.dst);
        break;

      case IoRequestType::eWrite:
        status = file.write(item.offset, item.size, item.src);
        break;
    }

    if (status == IoStatus::eSuccess && item.cb)
      status = item.cb(item);

    item = IoBufferedRequest();

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
