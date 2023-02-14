#include "../../util/util_log.h"

#include "io_stl.h"
#include "io_stl_file.h"
#include "io_stl_request.h"

namespace as {

IoStl::IoStl() {
  Log::info("Initializing STL I/O");

  m_worker = std::thread([this] { run(); });
}


IoStl::~IoStl() {
  Log::info("Shutting down STL I/O");

  std::unique_lock lock(m_mutex);
  m_queue.push(IoRequest());
  m_cond.notify_one();
  lock.unlock();

  m_worker.join();
}


IoBackend IoStl::getBackendType() const {
  return IoBackend::eStl;
}


IoFile IoStl::open(
  const std::filesystem::path&        path,
        IoOpenMode                    mode) {
  if (mode == IoOpenMode::eRead || mode == IoOpenMode::eWrite || mode == IoOpenMode::eCreateOrFail) {
    // If necessary, try to open a read stream to check if the file exists
    std::ifstream istream(path, std::ios_base::in | std::ios_base::binary);
    bool expectSuccess = mode != IoOpenMode::eCreateOrFail;

    if (bool(istream) != expectSuccess)
      return IoFile();

    if (mode == IoOpenMode::eRead)
      return IoFile(std::make_shared<IoStlFile>(path, std::move(istream)));
  }

  // Try to open write stream and create file object on success
  std::ios_base::openmode openMode = std::ios_base::out | std::ios_base::binary;

  if (mode == IoOpenMode::eCreate || mode == IoOpenMode::eCreateOrFail)
    openMode |= std::ios_base::trunc;

  std::ofstream ostream(path, openMode);

  if (!ostream)
    return IoFile();

  return IoFile(std::make_shared<IoStlFile>(path, std::move(ostream)));
}


IoRequest IoStl::createRequest() {
  return IoRequest(std::make_shared<IoStlRequest>());
}


bool IoStl::submit(
  const IoRequest&                    request) {
  std::unique_lock lock(m_mutex);

  if (!request || request->getStatus() != IoStatus::eReset)
    return false;

  static_cast<IoStlRequest&>(*request).setPending();

  m_queue.push(request);
  m_cond.notify_one();
  return true;
}


void IoStl::run() {
  while (true) {
    std::unique_lock lock(m_mutex);

    m_cond.wait(lock, [this] {
      return !m_queue.empty();
    });

    IoRequest request = std::move(m_queue.front());
    m_queue.pop();

    if (!request)
      return;

    lock.unlock();

    auto& stlRequest = static_cast<IoStlRequest&>(*request);
    stlRequest.execute();
  }
}

}
