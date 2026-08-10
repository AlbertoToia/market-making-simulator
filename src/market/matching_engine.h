#pragma once
#include "core/types.h"

namespace mms {

struct ExecutionReport {
    Side mm_side;
    Quantity quantity;
    Price price;
    double fee; // Positive means fee paid, negative means rebate
};

class MatchingEngine {
public:
    explicit MatchingEngine(Quantity queue_base, double maker_fee, double taker_fee = 0.0)
        : queue_base_(queue_base), maker_fee_(maker_fee),
          queue_ahead_bid_(0), queue_ahead_ask_(0),
          current_bid_(0.0), current_ask_(0.0) { (void)taker_fee; }

    // When MM updates quotes, if price changes, reset queue
    void update_quotes(Price new_bid, Price new_ask) {
        if (new_bid != current_bid_) {
            current_bid_ = new_bid;
            queue_ahead_bid_ = queue_base_;
        }
        if (new_ask != current_ask_) {
            current_ask_ = new_ask;
            queue_ahead_ask_ = queue_base_;
        }
    }

    // Process incoming market order volume hitting the specified side of the LOB.
    // mm_side = BUY means external seller hit the bid.
    // Returns report. If qty == 0, we were entirely shielded by the queue.
    ExecutionReport execute(Side mm_side, Quantity incoming_volume, Quantity our_size = 1) {
        ExecutionReport report;
        report.mm_side = mm_side;
        report.quantity = 0;
        report.price = (mm_side == Side::BUY) ? current_bid_ : current_ask_;
        report.fee = 0.0;

        Quantity& queue_ahead = (mm_side == Side::BUY) ? queue_ahead_bid_ : queue_ahead_ask_;

        if (incoming_volume <= queue_ahead) {
            // Queue absorbed the entire order
            queue_ahead -= incoming_volume;
            return report;
        }

        // Queue is exhausted by the incoming volume
        Quantity remaining_volume = incoming_volume - queue_ahead;
        queue_ahead = 0;

        // We get filled up to our size
        Quantity fill_qty = std::min(remaining_volume, our_size);
        
        report.quantity = fill_qty;
        
        // Fee calculation (we are the maker)
        double notional = fill_qty * report.price;
        report.fee = notional * maker_fee_;

        return report;
    }

    Quantity queue_bid() const { return queue_ahead_bid_; }
    Quantity queue_ask() const { return queue_ahead_ask_; }

private:
    Quantity queue_base_;
    double maker_fee_;

    Quantity queue_ahead_bid_;
    Quantity queue_ahead_ask_;
    Price current_bid_;
    Price current_ask_;
};

} // namespace mms
