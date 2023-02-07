#pragma once

#include <array>

namespace as {

template<typename T, typename... Args>
auto make_array(Args... args) {
  return std::array<T, sizeof...(Args)>({
    T(args)...
  });
}

}