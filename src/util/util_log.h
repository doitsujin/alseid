#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>

#include "util_string.h"

namespace as {

/**
 * \brief Log message severity
 */
enum class LogSeverity : uint32_t {
  /** Purely informational message */
  eInfo   = 0,
  /** Something went wrong in a recoverable manner
   *  and the side effects are controllable */
  eWarn   = 1,
  /** Something went wrong and can either not be
   *  recovered, or will have severe side effects */
  eError  = 2,
};


/**
 * \brief Logger
 */
class Log {

public:

  ~Log();

  /**
   * \brief Logs an informational message
   * \param [in] args Log message
   */
  template<typename... Args>
  static void info(const Args&... args) {
    message(LogSeverity::eInfo, args...);
  }

  /**
   * \brief Logs a warning
   * \param [in] args Log message
   */
  template<typename... Args>
  static void warn(const Args&... args) {
    message(LogSeverity::eWarn, args...);
  }

  /**
   * \brief Logs an error message
   * \param [in] args Log message
   */
  template<typename... Args>
  static void err(const Args&... args) {
    message(LogSeverity::eError, args...);
  }

  /**
   * \brief Logs a message
   *
   * \param [in] severity Message severity
   * \param [in] args Log message
   */
  template<typename... Args>
  static void message(LogSeverity severity, const Args&... args) {
    s_instance.message_(severity, args...);
  }

  /**
   * \brief Opens a log file to write to
   *
   * If this is not called, log messages will only
   * be printed to the console.
   */
  static void setLogFile(
    const std::string&                  path) {
    s_instance.setLogFile_(path);
  }

  /**
   * \brief Sets log level
   * \param [in] severity Minimum message severity
   */
  static void setLogLevel(LogSeverity severity) {
    s_instance.m_minSeverity = severity;
  }

private:

  static Log s_instance;

  std::atomic<LogSeverity> m_minSeverity = { LogSeverity(0) };

  std::mutex    m_mutex;
  std::ofstream m_file;

  Log();

  void setLogFile_(
    const std::string&                  path);

  template<typename... Args>
  void message_(LogSeverity severity, const Args&... args) {
    if (severity < m_minSeverity)
      return;

    std::lock_guard lock(m_mutex);
    messageStream_(std::cerr, stringFromSeverity(severity), args...);

    if (m_file.is_open())
      messageStream_(m_file, stringFromSeverity(severity), args...);
  }

  template<typename... Args>
  static void messageStream_(std::ostream& stream, const Args&... args) {
    (stream << ... << args) << std::endl;
  }

  static const char* stringFromSeverity(
          LogSeverity                   severity);

};

}
