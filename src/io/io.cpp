#include "../util/util_error.h"
#include "../util/util_log.h"

#include "io.h"

#include "./stl/io_stl.h"

#ifdef ALSEID_IO_URING
#include "./uring/io_uring.h"
#endif

namespace as {

Io::Io(
        IoBackend                     backend,
        uint32_t                      workerCount)
: IfaceRef<IoIface>(initBackend(backend, workerCount)) {

}


std::shared_ptr<IoIface> Io::initBackend(
        IoBackend                     backend,
        uint32_t                      workerCount) {
  try {
    switch (backend) {
      case IoBackend::eDefault:

  #ifdef ALSEID_IO_URING
      case IoBackend::eIoUring:
        return std::make_shared<IoUring>(workerCount);
  #endif

      case IoBackend::eStl:
        // Handle this later since STL
        // is always a fallback
        break;
    }
  } catch (const Error& e) {
    Log::err(e.what());
  }

  return std::make_shared<IoStl>();
}

}
