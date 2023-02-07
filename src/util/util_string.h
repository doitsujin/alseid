#pragma once

#include <string>
#include <sstream>
#include <utility>

namespace as {

  inline std::string strcat(std::string&& arg) {
    return std::string(arg);
  }

  template<typename... Args>
  std::string strcat(Args... args) {
    std::stringstream stream;
    (stream << ... << args);
    return stream.str();
  }

}