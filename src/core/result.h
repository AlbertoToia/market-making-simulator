#pragma once
#include "core/types.h"
#include <vector>
#include <string>
#include <cstdint>

namespace mms {

struct SimulationParameters {
    // Baseline AS
    double sigma_annual = 0.20;
    double gamma = 20.0;
    double k = 1.5;
    double A = 2.0;
    double mu = 0.0;
    Price initial_price = 100.0;
    double duration = 3600.0;
    double market_tick_dt = 1.0;
    uint64_t seed = 42;

    // Execution Realism
    double latency = 0.0;
    double maker_fee = 0.0;
    double taker_fee = 0.0;
    Quantity queue_position_base = 0;

    // Toxicity AR(1) Model
    double toxicity_rho = 0.99; // Autocorrelation (how persistent is toxicity)
    double toxicity_drift_impact = 0.0; // How much toxicity pushes the price drift
    double toxicity_intensity_impact = 0.0; // How much toxicity boosts opposing fills
};

enum class EventType {
    MARKET_UPDATE,
    QUOTE_HIT_BUY,
    QUOTE_HIT_SELL,
    QUOTE_UPDATE
};

struct TimelineEntry {
    // Timing
    TimeStamp timestamp = 0.0;
    EventType event_type = EventType::MARKET_UPDATE;

    // Market
    Price mid_price = 0.0;
    double log_return = 0.0;
    double toxicity = 0.0;

    // Active Quotes (what the market sees)
    Price bid = 0.0;
    Price ask = 0.0;
    Price reservation_price = 0.0;
    double spread = 0.0;
    double bid_distance = 0.0;
    double ask_distance = 0.0;
    Quantity queue_ahead_bid = 0;
    Quantity queue_ahead_ask = 0;

    // Intensities
    double lambda_bid = 0.0;
    double lambda_ask = 0.0;

    // Portfolio
    Quantity inventory = 0;
    double inventory_notional = 0.0;
    double cash = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double total_pnl = 0.0; // Net PnL (after fees)

    // Execution (non-zero only for QuoteHitEvents)
    Price execution_price = 0.0;
    Quantity execution_quantity = 0;
    double fee_paid = 0.0;
};

struct SimulationSummary {
    // PnL
    double final_pnl = 0.0; // Net PnL
    double gross_pnl = 0.0;
    double total_fees = 0.0;
    double total_return = 0.0;
    double realized_volatility = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double max_drawdown_pct = 0.0;

    // Inventory
    Quantity max_inventory = 0;
    Quantity min_inventory = 0;
    double average_inventory = 0.0;
    double inventory_std = 0.0;

    // Execution
    int total_fills = 0;
    int buy_fills = 0;
    int sell_fills = 0;
    int total_volume = 0;

    // Quotes
    double average_spread = 0.0;
    double average_bid_distance = 0.0;
    double average_ask_distance = 0.0;

    // Meta
    int total_events = 0;
    double simulation_duration = 0.0;
};

struct SimulationResult {
    SimulationParameters parameters;
    std::vector<TimelineEntry> timeline;
    SimulationSummary summary;
};

} // namespace mms
