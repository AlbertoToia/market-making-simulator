#include <iostream>
#include <iomanip>
#include "engine.h"

using namespace mms;

static void print_csv(const SimulationResult& result) {
    std::cout << "timestamp,event_type,mid_price,log_return,"
              << "bid,ask,reservation_price,spread,bid_distance,ask_distance,"
              << "lambda_bid,lambda_ask,"
              << "inventory,inventory_notional,cash,"
              << "realized_pnl,unrealized_pnl,total_pnl,"
              << "execution_price,execution_quantity\n";

    for (const auto& e : result.timeline) {
        const char* type = (e.event_type == EventType::MARKET_UPDATE) ? "MARKET"
                         : (e.event_type == EventType::QUOTE_HIT_BUY) ? "HIT_BID"
                         : "HIT_ASK";

        std::cout << std::fixed << std::setprecision(6)
                  << e.timestamp << "," << type << ","
                  << e.mid_price << "," << e.log_return << ","
                  << e.bid << "," << e.ask << ","
                  << e.reservation_price << "," << e.spread << ","
                  << e.bid_distance << "," << e.ask_distance << ","
                  << e.lambda_bid << "," << e.lambda_ask << ","
                  << e.inventory << "," << e.inventory_notional << ","
                  << e.cash << ","
                  << e.realized_pnl << "," << e.unrealized_pnl << ","
                  << e.total_pnl << ","
                  << e.execution_price << "," << e.execution_quantity << "\n";
    }
}

static void print_summary(const SimulationResult& result) {
    const auto& s = result.summary;
    const auto& p = result.parameters;

    std::cerr << "\n=== Simulation Summary ===\n"
              << std::fixed << std::setprecision(4)
              << "Parameters:\n"
              << "  sigma_annual: " << p.sigma_annual << "\n"
              << "  gamma:        " << p.gamma << "\n"
              << "  k:            " << p.k << "\n"
              << "  A:            " << p.A << "\n"
              << "  mu:           " << p.mu << "\n"
              << "  price:        " << p.initial_price << "\n"
              << "  duration:     " << p.duration << "s\n"
              << "  tick_dt:      " << p.market_tick_dt << "s\n"
              << "  seed:         " << p.seed << "\n"
              << "\nPerformance:\n"
              << "  Final PnL:           " << s.final_pnl << "\n"
              << "  Total Return:        " << s.total_return * 100 << "%\n"
              << "  Realized Volatility: " << s.realized_volatility * 100 << "%\n"
              << "  Sharpe Ratio:        " << s.sharpe_ratio
              << " (annualized from 1-min samples, idealized)\n"
              << "  Max Drawdown:        " << s.max_drawdown
              << " (" << s.max_drawdown_pct * 100 << "%)\n"
              << "\nInventory:\n"
              << "  Max:  " << s.max_inventory << "\n"
              << "  Min:  " << s.min_inventory << "\n"
              << "  Avg:  " << s.average_inventory << "\n"
              << "  Std:  " << s.inventory_std << "\n"
              << "\nExecution:\n"
              << "  Total Fills: " << s.total_fills
              << " (buy=" << s.buy_fills << ", sell=" << s.sell_fills << ")\n"
              << "  Total Volume: " << s.total_volume << "\n"
              << "  Avg Spread:   " << s.average_spread << "\n"
              << "\nMeta:\n"
              << "  Total Events:  " << s.total_events << "\n"
              << "  Sim Duration:  " << s.simulation_duration << "s\n\n";
}

static void print_json(const SimulationResult& result) {
    const auto& p = result.parameters;
    const auto& s = result.summary;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "{\n";
    std::cout << "  \"parameters\": {\n"
              << "    \"sigma_annual\": " << p.sigma_annual << ",\n"
              << "    \"gamma\": " << p.gamma << ",\n"
              << "    \"k\": " << p.k << ",\n"
              << "    \"A\": " << p.A << ",\n"
              << "    \"mu\": " << p.mu << ",\n"
              << "    \"initial_price\": " << p.initial_price << ",\n"
              << "    \"duration\": " << p.duration << ",\n"
              << "    \"market_tick_dt\": " << p.market_tick_dt << ",\n"
              << "    \"seed\": " << p.seed << "\n"
              << "  },\n";

    std::cout << "  \"summary\": {\n"
              << "    \"final_pnl\": " << s.final_pnl << ",\n"
              << "    \"gross_pnl\": " << s.gross_pnl << ",\n"
              << "    \"total_fees\": " << s.total_fees << ",\n"
              << "    \"total_return\": " << s.total_return << ",\n"
              << "    \"realized_volatility\": " << s.realized_volatility << ",\n"
              << "    \"sharpe_ratio\": " << s.sharpe_ratio << ",\n"
              << "    \"max_drawdown\": " << s.max_drawdown << ",\n"
              << "    \"max_drawdown_pct\": " << s.max_drawdown_pct << ",\n"
              << "    \"max_inventory\": " << s.max_inventory << ",\n"
              << "    \"min_inventory\": " << s.min_inventory << ",\n"
              << "    \"average_inventory\": " << s.average_inventory << ",\n"
              << "    \"inventory_std\": " << s.inventory_std << ",\n"
              << "    \"total_fills\": " << s.total_fills << ",\n"
              << "    \"buy_fills\": " << s.buy_fills << ",\n"
              << "    \"sell_fills\": " << s.sell_fills << ",\n"
              << "    \"total_volume\": " << s.total_volume << ",\n"
              << "    \"average_spread\": " << s.average_spread << ",\n"
              << "    \"average_bid_distance\": " << s.average_bid_distance << ",\n"
              << "    \"average_ask_distance\": " << s.average_ask_distance << ",\n"
              << "    \"total_events\": " << s.total_events << ",\n"
              << "    \"simulation_duration\": " << s.simulation_duration << "\n"
              << "  },\n";

    std::cout << "  \"timeline\": [\n";
    for (size_t i = 0; i < result.timeline.size(); ++i) {
        const auto& e = result.timeline[i];
        const char* type = (e.event_type == EventType::MARKET_UPDATE) ? "MARKET_UPDATE"
                         : (e.event_type == EventType::QUOTE_HIT_BUY) ? "QUOTE_HIT_BUY"
                         : (e.event_type == EventType::QUOTE_HIT_SELL) ? "QUOTE_HIT_SELL"
                         : "QUOTE_UPDATE";

        std::cout << "    {\n"
                  << "      \"timestamp\": " << e.timestamp << ",\n"
                  << "      \"event_type\": \"" << type << "\",\n"
                  << "      \"mid_price\": " << e.mid_price << ",\n"
                  << "      \"log_return\": " << e.log_return << ",\n"
                  << "      \"toxicity\": " << e.toxicity << ",\n"
                  << "      \"bid\": " << e.bid << ",\n"
                  << "      \"ask\": " << e.ask << ",\n"
                  << "      \"reservation_price\": " << e.reservation_price << ",\n"
                  << "      \"spread\": " << e.spread << ",\n"
                  << "      \"bid_distance\": " << e.bid_distance << ",\n"
                  << "      \"ask_distance\": " << e.ask_distance << ",\n"
                  << "      \"queue_ahead_bid\": " << e.queue_ahead_bid << ",\n"
                  << "      \"queue_ahead_ask\": " << e.queue_ahead_ask << ",\n"
                  << "      \"lambda_bid\": " << e.lambda_bid << ",\n"
                  << "      \"lambda_ask\": " << e.lambda_ask << ",\n"
                  << "      \"inventory\": " << e.inventory << ",\n"
                  << "      \"inventory_notional\": " << e.inventory_notional << ",\n"
                  << "      \"cash\": " << e.cash << ",\n"
                  << "      \"realized_pnl\": " << e.realized_pnl << ",\n"
                  << "      \"unrealized_pnl\": " << e.unrealized_pnl << ",\n"
                  << "      \"total_pnl\": " << e.total_pnl << ",\n"
                  << "      \"execution_price\": " << e.execution_price << ",\n"
                  << "      \"execution_quantity\": " << e.execution_quantity << ",\n"
                  << "      \"fee_paid\": " << e.fee_paid << "\n"
                  << "    }" << (i < result.timeline.size() - 1 ? "," : "") << "\n";
    }
    std::cout << "  ]\n}\n";
}

int main(int argc, char* argv[]) {
    SimulationParameters params;
    params.sigma_annual = 0.20;
    params.gamma = 20.0;
    params.k = 1.5;
    params.A = 2.0;
    params.mu = 0.0;
    params.initial_price = 100.0;
    params.duration = 3600.0;
    params.market_tick_dt = 2.0; // matching frontend for fewer events
    params.seed = 42;
    params.latency = 0.05;
    params.maker_fee = 0.0001;
    params.queue_position_base = 100;
    params.toxicity_rho = 0.99;
    params.toxicity_drift_impact = 0.0;
    params.toxicity_intensity_impact = 0.5;

    bool output_json = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") {
            output_json = true;
        } else if (arg == "--sigma" && i + 1 < argc) {
            params.sigma_annual = std::stod(argv[++i]);
        } else if (arg == "--gamma" && i + 1 < argc) {
            params.gamma = std::stod(argv[++i]);
        } else if (arg == "--k" && i + 1 < argc) {
            params.k = std::stod(argv[++i]);
        } else if (arg == "--A" && i + 1 < argc) {
            params.A = std::stod(argv[++i]);
        } else if (arg == "--price" && i + 1 < argc) {
            params.initial_price = std::stod(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            params.duration = std::stod(argv[++i]);
        } else if (arg == "--tick" && i + 1 < argc) {
            params.market_tick_dt = std::stod(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            params.seed = std::stoull(argv[++i]);
        } else if (arg == "--latency" && i + 1 < argc) {
            params.latency = std::stod(argv[++i]);
        } else if (arg == "--maker-fee" && i + 1 < argc) {
            params.maker_fee = std::stod(argv[++i]);
        } else if (arg == "--queue-base" && i + 1 < argc) {
            params.queue_position_base = std::stoi(argv[++i]);
        } else if (arg == "--toxicity-rho" && i + 1 < argc) {
            params.toxicity_rho = std::stod(argv[++i]);
        } else if (arg == "--toxicity-drift" && i + 1 < argc) {
            params.toxicity_drift_impact = std::stod(argv[++i]);
        } else if (arg == "--toxicity-intensity" && i + 1 < argc) {
            params.toxicity_intensity_impact = std::stod(argv[++i]);
        }
    }

    SimulationEngine engine(params);
    auto result = engine.run();

    if (output_json) {
        print_json(result);
    } else {
        print_csv(result);
        print_summary(result);
    }

    return 0;
}
