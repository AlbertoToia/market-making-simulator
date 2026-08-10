#pragma once
#include "core/types.h"
#include <variant>
#include <queue>
#include <vector>

namespace mms {

struct MarketUpdateEvent {
    Price new_mid_price;
};

// A deterministic fill of one of the MM's quotes.
// mm_side = BUY  → external seller hit the MM's bid → MM buys, inventory += 1
// mm_side = SELL → external buyer hit the MM's ask  → MM sells, inventory -= 1
struct QuoteHitEvent {
    Side mm_side;
};

using EventData = std::variant<MarketUpdateEvent, QuoteHitEvent>;

struct Event {
    TimeStamp timestamp;
    EventData data;

    // Reversed for min-heap: earliest timestamp has highest priority
    bool operator<(const Event& other) const {
        return timestamp > other.timestamp;
    }
};

using EventQueue = std::priority_queue<Event, std::vector<Event>>;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

} // namespace mms
