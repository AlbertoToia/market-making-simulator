#pragma once
#include "core/types.h"
#include "core/result.h"
#include "models/gbm_pricer.h"
#include "strategy/avellaneda_stoikov.h"
#include "market/matching_engine.h"
#include "risk/portfolio.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <limits>

namespace mms {

constexpr double TRADING_SECONDS_PER_YEAR = 252.0 * 6.5 * 3600.0;
constexpr double INF_TIME = std::numeric_limits<double>::infinity();

class SimulationEngine {
public:
    explicit SimulationEngine(const SimulationParameters& params)
        : params_(params)
    {
        if (params_.sigma_annual < 0.0) throw std::invalid_argument("sigma must be non-negative");
        if (params_.gamma <= 0.0) throw std::invalid_argument("gamma must be positive");
        if (params_.k <= 0.0) throw std::invalid_argument("k must be positive");
        if (params_.A < 0.0) throw std::invalid_argument("A must be non-negative");
        if (params_.duration <= 0.0) throw std::invalid_argument("duration must be positive");
        if (params_.initial_price <= 0.0) throw std::invalid_argument("initial_price must be positive");
        if (params_.market_tick_dt <= 0.0) throw std::invalid_argument("market_tick_dt must be positive");
    }

    SimulationResult run() {
        double sigma_per_sec = params_.sigma_annual / std::sqrt(TRADING_SECONDS_PER_YEAR);
        double drift_per_sec = params_.mu / TRADING_SECONDS_PER_YEAR;

        std::mt19937 rng(params_.seed);
        GbmPricer pricer(params_.initial_price, sigma_per_sec, drift_per_sec);
        AvellanedaStoikov strategy(params_.gamma, params_.k);
        MatchingEngine matching(params_.queue_position_base, params_.maker_fee, params_.taker_fee);
        Portfolio portfolio;

        SimulationResult result;
        result.parameters = params_;
        result.timeline.reserve(static_cast<size_t>(params_.duration * 10));

        double t = 0.0;
        Price mid_price = pricer.current_price();
        Price prev_mid_price = mid_price;
        double toxicity = 0.0;

        // Initialize Quotes
        Quotes desired_quotes = compute_quotes(strategy, mid_price, sigma_per_sec, portfolio.inventory());
        Quotes active_quotes = desired_quotes; // Assume zero latency for the very first quote
        Quotes scheduled_quotes = desired_quotes;
        matching.update_quotes(active_quotes.bid, active_quotes.ask);

        double lambda_bid = 0.0, lambda_ask = 0.0;
        compute_intensities(active_quotes, mid_price, toxicity, lambda_bid, lambda_ask);

        // Event Schedule
        double t_market = params_.market_tick_dt;
        double t_quote_update = INF_TIME;
        double t_bid_hit = sample_exponential(lambda_bid, rng);
        double t_ask_hit = sample_exponential(lambda_ask, rng);

        record(result, t, EventType::MARKET_UPDATE, mid_price, prev_mid_price, toxicity,
               active_quotes, lambda_bid, lambda_ask, matching, portfolio, 0.0, 0, 0.0);

        // Core Discrete Event Loop
        while (t < params_.duration) {
            double t_next = std::min({t_market, t_quote_update, t_bid_hit, t_ask_hit});
            if (t_next > params_.duration) break;
            
            t = t_next;

            if (t_next == t_market) {
                // 1. Market Update Event
                prev_mid_price = mid_price;

                // Update Toxicity (AR-1 process)
                std::normal_distribution<double> norm(0.0, 1.0);
                double epsilon = norm(rng);
                toxicity = params_.toxicity_rho * toxicity + std::sqrt(1.0 - params_.toxicity_rho * params_.toxicity_rho) * epsilon;
                
                // Drift impact from toxicity
                double toxicity_drift = params_.toxicity_drift_impact * toxicity;

                // Advance Price
                mid_price = pricer.next_price(params_.market_tick_dt, rng, toxicity_drift);
                t_market += params_.market_tick_dt;

                // AS decides new quotes
                desired_quotes = compute_quotes(strategy, mid_price, sigma_per_sec, portfolio.inventory());
                schedule_quote_update(t, active_quotes, desired_quotes, scheduled_quotes, t_quote_update);

                // Intensities change because mid_price and toxicity changed
                compute_intensities(active_quotes, mid_price, toxicity, lambda_bid, lambda_ask);
                t_bid_hit = t + sample_exponential(lambda_bid, rng);
                t_ask_hit = t + sample_exponential(lambda_ask, rng);

                record(result, t, EventType::MARKET_UPDATE, mid_price, prev_mid_price, toxicity,
                       active_quotes, lambda_bid, lambda_ask, matching, portfolio, 0.0, 0, 0.0);

            } else if (t_next == t_quote_update) {
                // 2. Quote Update Event (Latency elapsed)
                active_quotes = scheduled_quotes;
                matching.update_quotes(active_quotes.bid, active_quotes.ask);
                t_quote_update = INF_TIME;

                // Maybe conditions changed while we waited, AS might want something else now
                desired_quotes = compute_quotes(strategy, mid_price, sigma_per_sec, portfolio.inventory());
                schedule_quote_update(t, active_quotes, desired_quotes, scheduled_quotes, t_quote_update);

                // Intensities change because active quotes changed
                compute_intensities(active_quotes, mid_price, toxicity, lambda_bid, lambda_ask);
                t_bid_hit = t + sample_exponential(lambda_bid, rng);
                t_ask_hit = t + sample_exponential(lambda_ask, rng);

                record(result, t, EventType::QUOTE_UPDATE, mid_price, mid_price, toxicity,
                       active_quotes, lambda_bid, lambda_ask, matching, portfolio, 0.0, 0, 0.0);

            } else if (t_next == t_bid_hit) {
                // 3. Bid Hit Event (External Seller hits MM Bid)
                Quantity incoming_vol = sample_incoming_volume(rng);
                auto report = matching.execute(Side::BUY, incoming_vol);
                
                if (report.quantity > 0) {
                    portfolio.process_execution(report.mm_side, report.quantity, report.price, report.fee);
                    
                    // Inventory changed, AS might want to update quotes
                    desired_quotes = compute_quotes(strategy, mid_price, sigma_per_sec, portfolio.inventory());
                    schedule_quote_update(t, active_quotes, desired_quotes, scheduled_quotes, t_quote_update);
                }

                record(result, t, EventType::QUOTE_HIT_BUY, mid_price, mid_price, toxicity,
                       active_quotes, lambda_bid, lambda_ask, matching, portfolio, report.price, report.quantity, report.fee);

                t_bid_hit = t + sample_exponential(lambda_bid, rng);

            } else if (t_next == t_ask_hit) {
                // 4. Ask Hit Event (External Buyer hits MM Ask)
                Quantity incoming_vol = sample_incoming_volume(rng);
                auto report = matching.execute(Side::SELL, incoming_vol);
                
                if (report.quantity > 0) {
                    portfolio.process_execution(report.mm_side, report.quantity, report.price, report.fee);
                    
                    desired_quotes = compute_quotes(strategy, mid_price, sigma_per_sec, portfolio.inventory());
                    schedule_quote_update(t, active_quotes, desired_quotes, scheduled_quotes, t_quote_update);
                }

                record(result, t, EventType::QUOTE_HIT_SELL, mid_price, mid_price, toxicity,
                       active_quotes, lambda_bid, lambda_ask, matching, portfolio, report.price, report.quantity, report.fee);

                t_ask_hit = t + sample_exponential(lambda_ask, rng);
            }
        }

        compute_summary(result);
        return result;
    }

private:
    SimulationParameters params_;

    Quotes compute_quotes(const AvellanedaStoikov& strategy, Price mid,
                          double sigma_per_sec, Quantity inventory) const {
        double sigma_dollar = mid * sigma_per_sec;
        return strategy.compute(mid, sigma_dollar, inventory);
    }

    void schedule_quote_update(double current_t, const Quotes& active, const Quotes& desired, Quotes& scheduled, double& t_quote_update) const {
        if (!quotes_equal(active, desired)) {
            if (params_.latency <= 0.0) {
                // Instant update
                scheduled = desired;
                t_quote_update = current_t; 
            } else if (t_quote_update == INF_TIME || !quotes_equal(scheduled, desired)) {
                // Schedule new update
                scheduled = desired;
                t_quote_update = current_t + params_.latency;
            }
        }
    }

    bool quotes_equal(const Quotes& a, const Quotes& b) const {
        return std::abs(a.bid - b.bid) < 1e-9 && std::abs(a.ask - b.ask) < 1e-9;
    }

    void compute_intensities(const Quotes& quotes, Price mid, double toxicity,
                             double& lambda_bid, double& lambda_ask) const {
        double delta_bid = std::max(0.0, mid - quotes.bid);
        double delta_ask = std::max(0.0, quotes.ask - mid);
        
        double toxicity_factor_bid = std::exp(-params_.toxicity_intensity_impact * toxicity);
        double toxicity_factor_ask = std::exp(params_.toxicity_intensity_impact * toxicity);

        lambda_bid = params_.A * std::exp(-params_.k * delta_bid) * toxicity_factor_bid;
        lambda_ask = params_.A * std::exp(-params_.k * delta_ask) * toxicity_factor_ask;
    }

    double sample_exponential(double lambda, std::mt19937& rng) const {
        if (lambda <= 0.0) return INF_TIME;
        std::exponential_distribution<double> dist(lambda);
        return dist(rng);
    }

    Quantity sample_incoming_volume(std::mt19937& rng) const {
        // Simple exponential model for order size. Mean size = 100
        std::exponential_distribution<double> dist(1.0 / 100.0);
        return std::max(1, static_cast<int>(std::round(dist(rng))));
    }

    void record(SimulationResult& result, double t, EventType type,
                Price mid, Price prev_mid, double toxicity, const Quotes& quotes,
                double lambda_bid, double lambda_ask,
                const MatchingEngine& matching,
                const Portfolio& portfolio, Price exec_price, Quantity exec_qty, double fee_paid) {

        TimelineEntry entry;
        entry.timestamp = t;
        entry.event_type = type;
        entry.mid_price = mid;
        entry.log_return = (prev_mid > 0.0 && mid > 0.0 && prev_mid != mid)
                           ? std::log(mid / prev_mid) : 0.0;
        entry.toxicity = toxicity;

        entry.bid = quotes.bid;
        entry.ask = quotes.ask;
        entry.reservation_price = quotes.reservation_price;
        entry.spread = quotes.spread;
        entry.bid_distance = std::max(0.0, mid - quotes.bid);
        entry.ask_distance = std::max(0.0, quotes.ask - mid);
        
        entry.queue_ahead_bid = matching.queue_bid();
        entry.queue_ahead_ask = matching.queue_ask();

        entry.lambda_bid = lambda_bid;
        entry.lambda_ask = lambda_ask;

        entry.inventory = portfolio.inventory();
        entry.inventory_notional = portfolio.inventory() * mid;
        entry.cash = portfolio.cash();
        entry.realized_pnl = portfolio.realized_pnl();
        entry.unrealized_pnl = portfolio.unrealized_pnl(mid);
        entry.total_pnl = portfolio.total_pnl(mid);

        entry.execution_price = exec_price;
        entry.execution_quantity = exec_qty;
        entry.fee_paid = fee_paid;

        result.timeline.push_back(entry);
    }

    void compute_summary(SimulationResult& result) const {
        auto& s = result.summary;
        const auto& tl = result.timeline;
        if (tl.empty()) return;

        s.total_events = static_cast<int>(tl.size());
        s.simulation_duration = tl.back().timestamp;
        
        s.final_pnl = tl.back().total_pnl;
        
        s.total_fees = 0.0;
        for (const auto& e : tl) {
            s.total_fees += e.fee_paid;
        }
        s.gross_pnl = s.final_pnl + s.total_fees;

        // Inventory statistics
        double inv_sum = 0.0, inv_sq_sum = 0.0;
        s.max_inventory = tl[0].inventory;
        s.min_inventory = tl[0].inventory;
        for (const auto& e : tl) {
            inv_sum += e.inventory;
            inv_sq_sum += static_cast<double>(e.inventory) * e.inventory;
            s.max_inventory = std::max(s.max_inventory, e.inventory);
            s.min_inventory = std::min(s.min_inventory, e.inventory);
        }
        s.average_inventory = inv_sum / tl.size();
        s.inventory_std = std::sqrt(inv_sq_sum / tl.size() - s.average_inventory * s.average_inventory);

        double max_abs_inv = std::max(std::abs(static_cast<double>(s.max_inventory)),
                                      std::abs(static_cast<double>(s.min_inventory)));
        double max_notional = max_abs_inv * params_.initial_price;
        s.total_return = (max_notional > 0.0) ? s.final_pnl / max_notional : 0.0;

        // Execution statistics
        double spread_sum = 0.0, bid_dist_sum = 0.0, ask_dist_sum = 0.0;

        for (const auto& e : tl) {
            if (e.event_type == EventType::QUOTE_HIT_BUY) {
                s.buy_fills += (e.execution_quantity > 0 ? 1 : 0);
                s.total_volume += e.execution_quantity;
            } else if (e.event_type == EventType::QUOTE_HIT_SELL) {
                s.sell_fills += (e.execution_quantity > 0 ? 1 : 0);
                s.total_volume += e.execution_quantity;
            }
            spread_sum += e.spread;
            bid_dist_sum += e.bid_distance;
            ask_dist_sum += e.ask_distance;
        }
        s.total_fills = s.buy_fills + s.sell_fills;
        s.average_spread = spread_sum / tl.size();
        s.average_bid_distance = bid_dist_sum / tl.size();
        s.average_ask_distance = ask_dist_sum / tl.size();

        // Realized volatility from market-update log returns
        std::vector<double> log_returns;
        for (const auto& e : tl) {
            if (e.event_type == EventType::MARKET_UPDATE && e.log_return != 0.0) {
                log_returns.push_back(e.log_return);
            }
        }
        if (log_returns.size() > 1) {
            double mean_lr = std::accumulate(log_returns.begin(), log_returns.end(), 0.0) / log_returns.size();
            double var_lr = 0.0;
            for (double lr : log_returns) var_lr += (lr - mean_lr) * (lr - mean_lr);
            var_lr /= log_returns.size();
            double vol_per_tick = std::sqrt(var_lr);
            s.realized_volatility = vol_per_tick * std::sqrt(TRADING_SECONDS_PER_YEAR / params_.market_tick_dt);
        }

        // Sharpe ratio (Minute sampling)
        constexpr double SHARPE_SAMPLE_INTERVAL = 60.0; 
        std::vector<double> pnl_samples;
        double next_sample_time = SHARPE_SAMPLE_INTERVAL;
        double last_sample_pnl = 0.0;
        for (const auto& e : tl) {
            if (e.timestamp >= next_sample_time) {
                pnl_samples.push_back(e.total_pnl - last_sample_pnl);
                last_sample_pnl = e.total_pnl;
                next_sample_time += SHARPE_SAMPLE_INTERVAL;
            }
        }
        if (pnl_samples.size() > 1) {
            double mean_r = std::accumulate(pnl_samples.begin(), pnl_samples.end(), 0.0) / pnl_samples.size();
            double var_r = 0.0;
            for (double r : pnl_samples) var_r += (r - mean_r) * (r - mean_r);
            var_r /= (pnl_samples.size() - 1);
            double std_r = std::sqrt(var_r);
            if (std_r > 1e-12) {
                constexpr double MINUTES_PER_YEAR = 252.0 * 6.5 * 60.0;
                s.sharpe_ratio = (mean_r / std_r) * std::sqrt(MINUTES_PER_YEAR);
            }
        }

        // Max drawdown
        double peak_pnl = 0.0;
        s.max_drawdown = 0.0;
        for (const auto& e : tl) {
            peak_pnl = std::max(peak_pnl, e.total_pnl);
            double drawdown = peak_pnl - e.total_pnl;
            s.max_drawdown = std::max(s.max_drawdown, drawdown);
        }
        s.max_drawdown_pct = (peak_pnl > 0.0) ? s.max_drawdown / peak_pnl : 0.0;
    }
};

} // namespace mms
