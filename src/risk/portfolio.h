#pragma once
#include "core/types.h"
#include <cmath>

namespace mms {

class Portfolio {
public:
    explicit Portfolio(double initial_cash = 0.0)
        : cash_(initial_cash), inventory_(0), avg_cost_(0.0), realized_pnl_(0.0), cumulative_fees_(0.0) {}

    // Update state after an execution (from the MM's perspective).
    // Uses average cost basis for realized PnL:
    //   - Opening/increasing: updates avg_cost
    //   - Closing/reducing: realizes PnL at (exec_price - avg_cost)
    //   - Flipping (crossing zero): splits into close + open
    void process_execution(Side mm_side, Quantity qty, Price exec_price, double fee_paid = 0.0) {
        cash_ -= fee_paid;
        cumulative_fees_ += fee_paid;
        
        if (mm_side == Side::BUY) {
            cash_ -= qty * exec_price;
            if (inventory_ >= 0) {
                // Increasing long or opening from flat: update avg cost
                double total_cost = avg_cost_ * inventory_ + exec_price * qty;
                inventory_ += qty;
                avg_cost_ = total_cost / inventory_;
            } else {
                // Reducing short position: realize PnL
                Quantity closing = std::min(qty, static_cast<Quantity>(-inventory_));
                realized_pnl_ += closing * (avg_cost_ - exec_price);
                inventory_ += qty;
                if (inventory_ > 0) {
                    // Flipped to long: new cost basis is the execution price
                    avg_cost_ = exec_price;
                } else if (inventory_ == 0) {
                    avg_cost_ = 0.0;
                }
            }
        } else {
            cash_ += qty * exec_price;
            if (inventory_ <= 0) {
                // Increasing short or opening from flat: update avg cost
                double total_cost = avg_cost_ * (-inventory_) + exec_price * qty;
                inventory_ -= qty;
                avg_cost_ = total_cost / (-inventory_);
            } else {
                // Reducing long position: realize PnL
                Quantity closing = std::min(qty, inventory_);
                realized_pnl_ += closing * (exec_price - avg_cost_);
                inventory_ -= qty;
                if (inventory_ < 0) {
                    // Flipped to short: new cost basis is the execution price
                    avg_cost_ = exec_price;
                } else if (inventory_ == 0) {
                    avg_cost_ = 0.0;
                }
            }
        }
    }

    // Mark-to-market NET PnL = cash + inventory * current_price
    double total_pnl(Price mid_price) const {
        return cash_ + inventory_ * mid_price;
    }

    // Mark-to-market GROSS PnL = net_pnl + cumulative_fees
    double gross_pnl(Price mid_price) const {
        return total_pnl(mid_price) + cumulative_fees_;
    }

    double unrealized_pnl(Price mid_price) const {
        if (inventory_ == 0) return 0.0;
        if (inventory_ > 0)
            return inventory_ * (mid_price - avg_cost_);
        else
            return (-inventory_) * (avg_cost_ - mid_price);
    }

    double cash() const { return cash_; }
    Quantity inventory() const { return inventory_; }
    double realized_pnl() const { return realized_pnl_; }
    double avg_cost() const { return avg_cost_; }
    double cumulative_fees() const { return cumulative_fees_; }

private:
    double cash_;
    Quantity inventory_;
    double avg_cost_;
    double realized_pnl_; // Note: Does not include fees directly. Fees are in cash_.
    double cumulative_fees_;
};

} // namespace mms
