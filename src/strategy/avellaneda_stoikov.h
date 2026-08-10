#pragma once
#include "core/types.h"
#include <cmath>
#include <stdexcept>

namespace mms {

struct Quotes {
    Price bid = 0.0;
    Price ask = 0.0;
    Price reservation_price = 0.0;
    double spread = 0.0;
};

class AvellanedaStoikov {
public:
    AvellanedaStoikov(double gamma, double k) : gamma_(gamma), k_(k) {
        if (gamma_ <= 0.0) throw std::invalid_argument("gamma must be positive");
        if (k_ <= 0.0) throw std::invalid_argument("k must be positive");
    }

    // Computes optimal bid/ask quotes.
    // Uses a normalized unit-horizon approximation (T-t = 1) of the
    // Avellaneda-Stoikov (2008) model. The full model has (T-t) factors
    // in both the reservation price skew and the spread; setting T-t = 1
    // yields a stationary approximation suitable for continuous simulation.
    //
    // sigma_dollar: dollar volatility = mid_price * sigma_percentage
    Quotes compute(Price mid_price, double sigma_dollar, Quantity inventory) const {
        double variance = sigma_dollar * sigma_dollar;

        // Reservation price: skewed by inventory risk
        double r = mid_price - (inventory * gamma_ * variance);

        // Optimal spread: inventory risk premium + adverse selection component
        double delta = (gamma_ * variance) + (2.0 / gamma_) * std::log(1.0 + (gamma_ / k_));

        return Quotes{
            .bid = r - delta / 2.0,
            .ask = r + delta / 2.0,
            .reservation_price = r,
            .spread = delta
        };
    }

    double gamma() const { return gamma_; }
    double k() const { return k_; }

private:
    double gamma_;
    double k_;
};

} // namespace mms
