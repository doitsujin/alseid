#include <iostream>

#include "util_log.h"

namespace as {

Log Log::s_instance;

Log::Log() {

}


Log::~Log() {

}


void Log::setLogFile_(
  const std::string&                  path) {
  std::lock_guard lock(m_mutex);
  m_file = std::ofstream(path, std::ios::trunc);
}


const char* Log::stringFromSeverity(
        LogSeverity                   severity) {
  switch (severity) {
    case LogSeverity::eInfo:  return "info:  ";
    case LogSeverity::eWarn:  return "warn:  ";
    case LogSeverity::eError: return "error: ";
    default:                  return "?????: ";
  }
}

}
