#pragma once

#include "util_error.h"

namespace as {

class Assert : public Error {

public:

  Assert(const char* file, int line, const char* msg) noexcept {
    std::snprintf(m_message, sizeof(m_message), "%s:%d:\nAssert failed: %s\n", file, line, msg);
  }

};

inline void dbg_assert_(bool cond, const char* file, int line, const char* msg) {
  if (!cond) [[unlikely]]
    throw Assert(file, line, msg);
}

inline void dbg_unreachable_(const char* file, int line, const char* msg) {
  throw Assert(file, line, msg);
}

#define dbg_assert(cond) dbg_assert_(bool(cond), __FILE__, __LINE__, #cond)

#define dbg_unreachable(msg) dbg_unreachable_(__FILE__, __LINE__, msg)

}
