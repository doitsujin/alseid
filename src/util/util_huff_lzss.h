#pragma once

#include "util_stream.h"

namespace as {

bool huffLzssDecode(
        WrMemoryView                  output,
        RdMemoryView                  input);

bool huffLzssEncode(
        WrVectorStream&               output,
        RdMemoryView                  input);

}
