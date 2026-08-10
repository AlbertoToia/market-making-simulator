#pragma once
#include <cstdint>

namespace mms {

using Price = double;
using Quantity = int32_t;
using TimeStamp = double; // Virtual seconds from simulation start
using Volatility = double;

enum class Side {
    BUY,
    SELL
};

} // namespace mms
