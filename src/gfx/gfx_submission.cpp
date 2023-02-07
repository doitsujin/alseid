#include "gfx_submission.h"

namespace as {

GfxCommandSubmission::GfxCommandSubmission() {

}


GfxCommandSubmission::~GfxCommandSubmission() {

}


void GfxCommandSubmission::addCommandList(
        GfxCommandList&&              commandList) {
  m_commandLists.push_back(std::move(commandList));
}


void GfxCommandSubmission::addWaitSemaphore(
        GfxSemaphore                  semaphore,
        uint64_t                      value) {
  GfxSemaphoreEntry entry;
  entry.semaphore = std::move(semaphore);
  entry.value = value;

  m_waitSemaphores.push_back(std::move(entry));
}


void GfxCommandSubmission::addSignalSemaphore(
        GfxSemaphore                  semaphore,
        uint64_t                      value) {
  GfxSemaphoreEntry entry;
  entry.semaphore = std::move(semaphore);
  entry.value = value;

  m_signalSemaphores.push_back(std::move(entry));
}


void GfxCommandSubmission::clear() {
  m_commandLists.clear();
  m_waitSemaphores.clear();
  m_signalSemaphores.clear();
}


bool GfxCommandSubmission::isEmpty() const {
  return m_commandLists.empty()
      && m_waitSemaphores.empty()
      && m_signalSemaphores.empty();
}


GfxCommandSubmissionInternal GfxCommandSubmission::getInternalInfo() const {
  GfxCommandSubmissionInternal result = { };
  result.commandListCount = m_commandLists.size();
  result.commandLists = m_commandLists.data();
  result.waitSemaphoreCount = m_waitSemaphores.size();
  result.waitSemaphores = m_waitSemaphores.data();
  result.signalSemaphoreCount = m_signalSemaphores.size();
  result.signalSemaphores = m_signalSemaphores.data();
  return result;
}

}
