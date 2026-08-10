#include <iostream>
#include <cmath>
#include <cassert>
#include <sstream>
#include <random>
#include <stdexcept>

#include "core/types.h"
#include "core/result.h"
#include "models/gbm_pricer.h"
#include "strategy/avellaneda_stoikov.h"
#include "market/matching_engine.h"
#include "risk/portfolio.h"
#include "engine.h"

using namespace mms;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                         \
    do {                                                   \
        tests_run++;                                       \
        try {                                              \
            test_##name();                                 \
            tests_passed++;                                \
            std::cerr << "  PASS  " << #name << "\n";      \
        } catch (const std::exception& e) {                \
            std::cerr << "  FAIL  " << #name               \
                      << ": " << e.what() << "\n";         \
        }                                                  \
    } while (0)

#define ASSERT_TRUE(cond)                                  \
    do { if (!(cond)) throw std::runtime_error(            \
        std::string("assertion failed: ") + #cond          \
        + " at line " + std::to_string(__LINE__)); } while(0)

#define ASSERT_NEAR(a, b, tol)                             \
    do { if (std::abs((a)-(b)) > (tol)) {                  \
        std::ostringstream ss;                             \
        ss << "expected " << (a) << " ≈ " << (b)          \
           << " (tol=" << (tol) << ", diff="               \
           << std::abs((a)-(b)) << ") at line "            \
           << __LINE__;                                    \
        throw std::runtime_error(ss.str()); } } while(0)

// ============================================================
// A-S Strategy Tests
// ============================================================

static void test_as_zero_inventory() {
    AvellanedaStoikov as(20.0, 1.5);
    auto q = as.compute(100.0, 0.05, 0);
    ASSERT_NEAR(q.reservation_price, 100.0, 1e-10);
    ASSERT_TRUE(q.bid < 100.0);
    ASSERT_TRUE(q.ask > 100.0);
    ASSERT_NEAR(q.ask - q.bid, q.spread, 1e-10);
    // Symmetric around mid when inventory=0
    ASSERT_NEAR(q.ask - q.reservation_price, q.reservation_price - q.bid, 1e-10);
}

static void test_as_positive_inventory() {
    AvellanedaStoikov as(20.0, 1.5);
    auto q = as.compute(100.0, 0.05, 5);
    // Positive inventory: reservation < mid (encourage selling)
    ASSERT_TRUE(q.reservation_price < 100.0);
    double expected_r = 100.0 - 5 * 20.0 * 0.05 * 0.05;
    ASSERT_NEAR(q.reservation_price, expected_r, 1e-10);
}

static void test_as_negative_inventory() {
    AvellanedaStoikov as(20.0, 1.5);
    auto q = as.compute(100.0, 0.05, -5);
    // Negative inventory: reservation > mid (encourage buying)
    ASSERT_TRUE(q.reservation_price > 100.0);
    double expected_r = 100.0 - (-5) * 20.0 * 0.05 * 0.05;
    ASSERT_NEAR(q.reservation_price, expected_r, 1e-10);
}

static void test_as_spread_formula() {
    AvellanedaStoikov as(20.0, 1.5);
    auto q = as.compute(100.0, 0.05, 0);
    double gamma = 20.0, k = 1.5, var = 0.05 * 0.05;
    double expected_spread = gamma * var + (2.0 / gamma) * std::log(1.0 + gamma / k);
    ASSERT_NEAR(q.spread, expected_spread, 1e-10);
}

static void test_as_invalid_gamma() {
    bool caught = false;
    try { AvellanedaStoikov as(0.0, 1.5); }
    catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);

    caught = false;
    try { AvellanedaStoikov as(-1.0, 1.5); }
    catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

static void test_as_invalid_k() {
    bool caught = false;
    try { AvellanedaStoikov as(20.0, 0.0); }
    catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

// ============================================================
// Portfolio Tests
// ============================================================

static void test_portfolio_buy() {
    Portfolio p;
    p.process_execution(Side::BUY, 1, 100.0);
    ASSERT_TRUE(p.inventory() == 1);
    ASSERT_NEAR(p.cash(), -100.0, 1e-10);
}

static void test_portfolio_sell() {
    Portfolio p;
    p.process_execution(Side::SELL, 1, 100.0);
    ASSERT_TRUE(p.inventory() == -1);
    ASSERT_NEAR(p.cash(), 100.0, 1e-10);
}

static void test_portfolio_mtm() {
    Portfolio p;
    p.process_execution(Side::BUY, 2, 100.0);
    // cash = -200, inventory = 2, mid = 105 → PnL = -200 + 2*105 = 10
    ASSERT_NEAR(p.total_pnl(105.0), 10.0, 1e-10);
}

static void test_portfolio_realized_pnl() {
    Portfolio p;
    p.process_execution(Side::BUY, 1, 100.0);
    p.process_execution(Side::SELL, 1, 102.0);
    // Bought at 100, sold at 102: realized = +2
    ASSERT_NEAR(p.realized_pnl(), 2.0, 1e-10);
    ASSERT_TRUE(p.inventory() == 0);
    ASSERT_NEAR(p.cash(), 2.0, 1e-10);
}

static void test_portfolio_realized_pnl_short() {
    Portfolio p;
    p.process_execution(Side::SELL, 1, 102.0);
    p.process_execution(Side::BUY, 1, 100.0);
    // Sold at 102, bought at 100: realized = +2
    ASSERT_NEAR(p.realized_pnl(), 2.0, 1e-10);
    ASSERT_TRUE(p.inventory() == 0);
    ASSERT_NEAR(p.cash(), 2.0, 1e-10);
}

static void test_portfolio_pnl_consistency() {
    // total_pnl must always equal cash + inventory * mid
    Portfolio p;
    p.process_execution(Side::BUY, 3, 100.0);
    p.process_execution(Side::SELL, 1, 101.0);
    Price mid = 99.5;
    ASSERT_NEAR(p.total_pnl(mid), p.cash() + p.inventory() * mid, 1e-10);
}

// ============================================================
// GBM Tests
// ============================================================

static void test_gbm_zero_vol() {
    GbmPricer pricer(100.0, 0.0, 0.0);
    std::mt19937 rng(42);
    for (int i = 0; i < 100; i++) {
        pricer.next_price(1.0, rng);
    }
    // With zero vol and zero drift, price should remain constant
    ASSERT_NEAR(pricer.current_price(), 100.0, 1e-10);
}

static void test_gbm_reproducibility() {
    GbmPricer p1(100.0, 0.001, 0.0);
    GbmPricer p2(100.0, 0.001, 0.0);
    std::mt19937 rng1(123);
    std::mt19937 rng2(123);
    for (int i = 0; i < 50; i++) {
        Price a = p1.next_price(1.0, rng1);
        Price b = p2.next_price(1.0, rng2);
        ASSERT_NEAR(a, b, 1e-15);
    }
}

static void test_gbm_statistical_sanity() {
    // Run 10000 GBM steps, verify realized vol is close to configured
    double sigma = 0.001;
    GbmPricer pricer(100.0, sigma, 0.0);
    std::mt19937 rng(42);
    int N = 10000;
    std::vector<double> log_returns;
    Price prev = pricer.current_price();
    for (int i = 0; i < N; i++) {
        Price next = pricer.next_price(1.0, rng);
        log_returns.push_back(std::log(next / prev));
        prev = next;
    }
    double mean = 0.0;
    for (double lr : log_returns) mean += lr;
    mean /= N;
    double var = 0.0;
    for (double lr : log_returns) var += (lr - mean) * (lr - mean);
    var /= N;
    double realized_vol = std::sqrt(var);
    // Should be close to sigma (within 5% relative error for N=10000)
    ASSERT_NEAR(realized_vol, sigma, sigma * 0.05);
}

static void test_gbm_invalid_price() {
    bool caught = false;
    try { GbmPricer p(0.0, 0.001); }
    catch (const std::invalid_argument&) { caught = true; }
    ASSERT_TRUE(caught);
}

// ============================================================
// Matching Engine Tests
// ============================================================

static void test_matching_queue_and_fees() {
    MatchingEngine engine(100, 0.001, 0.0);

    engine.update_quotes(100.0, 101.0);
    ASSERT_TRUE(engine.queue_bid() == 100);

    // 1. Partial consumption
    auto report1 = engine.execute(Side::BUY, 60, 10);
    ASSERT_TRUE(report1.quantity == 0);
    ASSERT_TRUE(engine.queue_bid() == 40);

    // 2. Full consumption + fill
    auto report2 = engine.execute(Side::BUY, 50, 10); 
    ASSERT_TRUE(report2.quantity == 10);
    ASSERT_TRUE(engine.queue_bid() == 0);
    ASSERT_NEAR(report2.fee, 10.0 * 100.0 * 0.001, 1e-7);

    // 3. Price update resets queue
    engine.update_quotes(99.0, 101.0);
    ASSERT_TRUE(engine.queue_bid() == 100);
}

static void test_portfolio_fees() {
    Portfolio port(0.0);
    port.process_execution(Side::BUY, 10, 100.0, 1.0); // fee = $1
    ASSERT_NEAR(port.cash(), -1000.0 - 1.0, 1e-7);
    ASSERT_NEAR(port.cumulative_fees(), 1.0, 1e-7);
    ASSERT_NEAR(port.gross_pnl(100.0), 0.0, 1e-7);
    ASSERT_NEAR(port.total_pnl(100.0), -1.0, 1e-7); // net pnl
}

static void test_execution_realism_baseline() {
    SimulationParameters params;
    params.latency = 0.0;
    params.maker_fee = 0.0;
    params.queue_position_base = 0;
    params.toxicity_drift_impact = 0.0;
    params.toxicity_intensity_impact = 0.0;
    params.seed = 42;
    params.duration = 60.0;

    SimulationEngine engine(params);
    SimulationResult result = engine.run();
    
    ASSERT_TRUE(result.timeline.size() > 0);
    ASSERT_NEAR(result.summary.total_fees, 0.0, 1e-7);
    ASSERT_NEAR(result.summary.final_pnl, result.summary.gross_pnl, 1e-7);
}

// ============================================================
// Simulation Engine Integration Tests
// ============================================================

static void test_engine_quotes_refresh_after_fill() {
    // Verify that quotes are recomputed after each fill by checking
    // that a QUOTE_UPDATE event fires with different reservation prices
    // when inventory changes.
    SimulationParameters params;
    params.sigma_annual = 0.20;
    params.gamma = 20.0;
    params.k = 1.5;
    params.A = 5.0;  // High rate to guarantee fills
    params.duration = 60.0;
    params.seed = 42;

    SimulationEngine engine(params);
    auto result = engine.run();

    // Find a quote update event following a fill
    for (size_t i = 1; i < result.timeline.size(); i++) {
        const auto& prev = result.timeline[i-1];
        const auto& curr = result.timeline[i];
        if (curr.event_type == EventType::QUOTE_UPDATE &&
            (prev.event_type == EventType::QUOTE_HIT_BUY || prev.event_type == EventType::QUOTE_HIT_SELL)) {
            if (prev.inventory != curr.inventory) {
                // Different inventory → reservation price must differ
                ASSERT_TRUE(std::abs(prev.reservation_price - curr.reservation_price) > 1e-10);
                return;
            }
        }
    }
}

static void test_engine_bid_below_mid_ask_above_mid() {
    // After the engine recomputes quotes, bid < mid < ask should hold
    // at the moment of computation (not necessarily during stale periods,
    // but there ARE no stale periods now).
    SimulationParameters params;
    params.sigma_annual = 0.20;
    params.gamma = 20.0;
    params.k = 1.5;
    params.A = 2.0;
    params.duration = 300.0;
    params.seed = 42;

    SimulationEngine engine(params);
    auto result = engine.run();

    // With moderate gamma, small inventory should keep quotes inside mid
    // Check only events where |inventory| < 3 (large inventory can push quotes past mid)
    for (const auto& e : result.timeline) {
        if (std::abs(e.inventory) <= 2) {
            ASSERT_TRUE(e.bid < e.mid_price);
            ASSERT_TRUE(e.ask > e.mid_price);
        }
    }
}

static void test_engine_pnl_consistency() {
    SimulationParameters params;
    params.sigma_annual = 0.20;
    params.gamma = 20.0;
    params.k = 1.5;
    params.A = 2.0;
    params.duration = 60.0;
    params.seed = 42;

    SimulationEngine engine(params);
    auto result = engine.run();

    for (const auto& e : result.timeline) {
        double expected = e.cash + e.inventory * e.mid_price;
        ASSERT_NEAR(e.total_pnl, expected, 0.01);
    }
}

static void test_engine_buy_increases_inventory() {
    SimulationParameters params;
    params.sigma_annual = 0.20;
    params.gamma = 20.0;
    params.k = 1.5;
    params.A = 5.0;
    params.duration = 60.0;
    params.seed = 42;

    SimulationEngine engine(params);
    auto result = engine.run();

    for (size_t i = 1; i < result.timeline.size(); i++) {
        if (result.timeline[i].event_type == EventType::QUOTE_HIT_BUY) {
            ASSERT_TRUE(result.timeline[i].inventory > result.timeline[i-1].inventory);
        }
        if (result.timeline[i].event_type == EventType::QUOTE_HIT_SELL) {
            ASSERT_TRUE(result.timeline[i].inventory < result.timeline[i-1].inventory);
        }
    }
}

static void test_engine_no_events_beyond_duration() {
    SimulationParameters params;
    params.duration = 10.0;
    params.seed = 42;

    SimulationEngine engine(params);
    auto result = engine.run();

    for (const auto& e : result.timeline) {
        ASSERT_TRUE(e.timestamp <= params.duration);
    }
}

static void test_execution_realism_fees() {
    SimulationParameters params;
    params.maker_fee = 0.01; // Huge fee to make it obvious
    params.latency = 0.0;
    params.duration = 60.0;
    params.seed = 42;

    SimulationEngine engine(params);
    SimulationResult result = engine.run();
    
    ASSERT_TRUE(result.summary.total_fees > 0.0);
    ASSERT_NEAR(result.summary.gross_pnl - result.summary.total_fees, result.summary.final_pnl, 1e-7);
}

static void test_execution_realism_queue() {
    SimulationParameters params;
    params.queue_position_base = 10000; // Impossible to clear
    params.duration = 60.0;
    params.seed = 42;

    SimulationEngine engine(params);
    SimulationResult result = engine.run();
    
    // With an impossible queue, we should never get filled
    ASSERT_TRUE(result.summary.total_fills == 0);
}

static void test_execution_realism_latency() {
    SimulationParameters params_zero;
    params_zero.latency = 0.0;
    params_zero.seed = 42;
    params_zero.duration = 10.0;
    SimulationEngine engine_zero(params_zero);
    auto result_zero = engine_zero.run();

    SimulationParameters params_lat;
    params_lat.latency = 1.0; // 1 second latency
    params_lat.seed = 42;
    params_lat.duration = 10.0;
    SimulationEngine engine_lat(params_lat);
    auto result_lat = engine_lat.run();

    // Latency changes event ordering and fills. 
    // We just verify it produces different but valid results.
    ASSERT_TRUE(result_zero.summary.total_fills != result_lat.summary.total_fills ||
                result_zero.summary.final_pnl != result_lat.summary.final_pnl);
}

static void test_execution_realism_toxicity() {
    SimulationParameters params;
    params.toxicity_rho = 0.99;
    params.toxicity_drift_impact = 0.5;
    params.toxicity_intensity_impact = 2.0; 
    params.duration = 600.0;
    params.seed = 42;

    SimulationEngine engine(params);
    SimulationResult result = engine.run();

    // With massive toxicity, the MM should lose money or perform worse than baseline
    SimulationParameters params_base;
    params_base.duration = 600.0;
    params_base.seed = 42;
    SimulationEngine engine_base(params_base);
    SimulationResult result_base = engine_base.run();

    ASSERT_TRUE(result.summary.final_pnl < result_base.summary.final_pnl);
}

static void test_engine_annualized_vol() {
    SimulationParameters params;
    params.sigma_annual = 0.20;
    params.duration = 3600.0;
    params.seed = 42;

    SimulationEngine engine(params);
    auto result = engine.run();

    ASSERT_TRUE(result.summary.realized_volatility > 0.10);
    ASSERT_TRUE(result.summary.realized_volatility < 0.40);
}

int main() {
    std::cerr << "Running tests...\n\n";

    // A-S tests
    TEST(as_zero_inventory);
    TEST(as_positive_inventory);
    TEST(as_negative_inventory);
    TEST(as_spread_formula);
    TEST(as_invalid_gamma);
    TEST(as_invalid_k);

    // Portfolio tests
    TEST(portfolio_buy);
    TEST(portfolio_sell);
    TEST(portfolio_mtm);
    TEST(portfolio_realized_pnl);
    TEST(portfolio_realized_pnl_short);
    TEST(portfolio_pnl_consistency);

    // GBM tests
    TEST(gbm_zero_vol);
    TEST(gbm_reproducibility);
    TEST(gbm_statistical_sanity);
    TEST(gbm_invalid_price);

    // Matching engine tests
    TEST(matching_queue_and_fees);
    
    // Execution realism tests
    TEST(portfolio_fees);
    TEST(execution_realism_baseline);
    TEST(execution_realism_fees);
    TEST(execution_realism_queue);
    TEST(execution_realism_latency);
    TEST(execution_realism_toxicity);

    // Integration tests
    TEST(engine_quotes_refresh_after_fill);
    TEST(engine_bid_below_mid_ask_above_mid);
    TEST(engine_pnl_consistency);
    TEST(engine_buy_increases_inventory);
    TEST(engine_no_events_beyond_duration);
    TEST(engine_annualized_vol);

    std::cerr << "\n" << tests_passed << "/" << tests_run << " tests passed.\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
