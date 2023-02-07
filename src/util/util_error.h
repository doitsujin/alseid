#pragma once

#include <cstdio>
#include <cstring>
#include <exception>

namespace as {

class Error : public std::exception {

public:

  Error() noexcept {
    std::memset(m_message, 0, sizeof(m_message));
  }

  Error(const char* message) noexcept {
    std::memset(m_message, 0, sizeof(m_message));
    std::strncpy(m_message, message, sizeof(m_message) - 1);
  }

  Error(const Error& e) noexcept = default;

  const char* what() const noexcept {
    return m_message;
  }

protected:

  char m_message[1024];

};

}
