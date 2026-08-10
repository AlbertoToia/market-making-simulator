#pragma once
#include "core/types.h"
#include <random>
#include <cmath>
#include <stdexcept>

namespace mms {

class GbmPricer {
public:
    GbmPricer(Price initial_price, Volatility sigma_per_second, double drift_per_second = 0.0)
        : current_price_(initial_price), sigma_(sigma_per_second), drift_(drift_per_second)
    {
        if (initial_price <= 0.0) throw std::invalid_argument("initial_price must be positive");
        if (sigma_per_second < 0.0) throw std::invalid_argument("sigma must be non-negative");
    }

    // Exact GBM solution: S(t+dt) = S(t) * exp((mu - 0.5*sigma^2)*dt + sigma*sqrt(dt)*Z)
    // Now accepts an exogenous toxicity drift (e.g. from AR(1) latent state)
    Price next_price(double dt, std::mt19937& rng, double toxicity_drift = 0.0) {
        std::normal_distribution<double> norm(0.0, 1.0);
        double z = norm(rng);
        double total_drift = drift_ + toxicity_drift;
        current_price_ *= std::exp((total_drift - 0.5 * sigma_ * sigma_) * dt + sigma_ * std::sqrt(dt) * z);
        return current_price_;
    }

    Price current_price() const { return current_price_; }
    Volatility sigma() const { return sigma_; }

private:
    Price current_price_;
    Volatility sigma_;
    double drift_;
};

} // namespace mms
