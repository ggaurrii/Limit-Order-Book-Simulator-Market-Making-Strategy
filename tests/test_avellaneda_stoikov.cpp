#include "test_runner.hpp"
#include "lob/AvellanedaStoikovMarketMaker.hpp"
#include "lob/MarketMaker.hpp"
#include "lob/MarketSimulator.hpp"
#include "lob/MatchingEngine.hpp"

#include <cmath>
#include <vector>

using namespace lob;

TEST(AvellanedaStoikovTest, ReservationPriceFormulation) {
    AvellanedaStoikovConfig config;
    config.gamma = 0.1;
    AvellanedaStoikovMarketMaker mm(config);

    Price fv = 100;
    double sigma = 2.0;
    double tau = 1.0;

    // q = 0 -> reservation price == fair value (100.0)
    double r_zero = mm.calculate_reservation_price(fv, 0, sigma, tau);
    EXPECT_DOUBLE_EQ(r_zero, 100.0);

    // q = +10 (long) -> r = 100 - (10 * 0.1 * 4 * 1.0) = 100 - 4.0 = 96.0
    double r_long = mm.calculate_reservation_price(fv, 10, sigma, tau);
    EXPECT_DOUBLE_EQ(r_long, 96.0);

    // q = -10 (short) -> r = 100 - (-10 * 0.1 * 4 * 1.0) = 100 + 4.0 = 104.0
    double r_short = mm.calculate_reservation_price(fv, -10, sigma, tau);
    EXPECT_DOUBLE_EQ(r_short, 104.0);
}

TEST(AvellanedaStoikovTest, VolatilityEstimatorCausality) {
    AvellanedaStoikovConfig config;
    config.initial_volatility = 2.0;
    config.volatility_window = 10;
    AvellanedaStoikovMarketMaker mm(config);

    // Initial state with 0 history -> returns fallback initial_volatility
    EXPECT_DOUBLE_EQ(mm.current_volatility(), 2.0);

    // Feed price series with low volatility (constant 100)
    for (int i = 0; i < 5; ++i) {
        mm.update_volatility(100);
    }
    double low_vol = mm.current_volatility();

    // Reset and feed high volatility series (alternating 100, 120, 100, 120)
    mm.reset();
    mm.update_volatility(100);
    mm.update_volatility(120);
    mm.update_volatility(100);
    mm.update_volatility(120);
    double high_vol = mm.current_volatility();

    EXPECT_TRUE(high_vol > low_vol);
}

TEST(AvellanedaStoikovTest, OptimalSpreadSensitivity) {
    AvellanedaStoikovConfig config;
    config.gamma = 0.1;
    config.kappa = 1.5;
    AvellanedaStoikovMarketMaker mm(config);

    double spread_low_vol = mm.calculate_optimal_spread(1.0, 1.0);
    double spread_high_vol = mm.calculate_optimal_spread(3.0, 1.0);

    // Higher volatility must widen optimal spread
    EXPECT_TRUE(spread_high_vol > spread_low_vol);
    EXPECT_TRUE(spread_low_vol > 0.0);
}

TEST(AvellanedaStoikovTest, InventoryLimitsSuppression) {
    AvellanedaStoikovConfig config;
    config.max_inventory = 20;
    AvellanedaStoikovMarketMaker mm(config);
    MatchingEngine engine;

    engine.submit_limit_order(1, Side::Buy, 90, 50, 1000);
    engine.submit_limit_order(2, Side::Sell, 110, 50, 1001);

    // Fill buy order to reach max_inventory (+20)
    std::vector<MarketEvent> events;
    MarketEvent exec;
    exec.event_type = EventType::OrderExecuted;
    exec.order_id = config.strategy_id_start;
    exec.side = Side::Buy;
    exec.price = 100;
    exec.quantity = 20;
    exec.remaining_quantity = 0;
    events.push_back(exec);

    engine.submit_limit_order(config.strategy_id_start, Side::Buy, 95, 20, 1002);
    mm.refresh_quotes(engine, 1003);
    mm.on_market_events(events);

    EXPECT_EQ(mm.inventory(), 20);

    // Refresh quotes at max long -> Bid quote suppressed (0), Ask quote active
    mm.refresh_quotes(engine, 1004);
    EXPECT_EQ(mm.active_bid_id(), static_cast<OrderId>(0));
    EXPECT_TRUE(mm.active_ask_id() != 0);
}

TEST(AvellanedaStoikovTest, HorizonMonotonicDecrease) {
    AvellanedaStoikovConfig config;
    config.terminal_horizon = 10000;
    AvellanedaStoikovMarketMaker mm(config);

    double tau0 = mm.remaining_horizon_ratio(0);
    double tau5000 = mm.remaining_horizon_ratio(5000);
    double tau10000 = mm.remaining_horizon_ratio(10000);
    double tau15000 = mm.remaining_horizon_ratio(15000);

    EXPECT_DOUBLE_EQ(tau0, 1.0);
    EXPECT_DOUBLE_EQ(tau5000, 0.5);
    EXPECT_DOUBLE_EQ(tau10000, 0.0);
    EXPECT_DOUBLE_EQ(tau15000, 0.0); // Non-negative bound
}

TEST(AvellanedaStoikovTest, ExecutionAccountingAndPnL) {
    AvellanedaStoikovConfig config;
    config.initial_cash = 100000.0;
    AvellanedaStoikovMarketMaker mm(config);

    std::vector<MarketEvent> buy_events;
    MarketEvent b_exec;
    b_exec.event_type = EventType::OrderExecuted;
    b_exec.order_id = config.strategy_id_start;
    b_exec.side = Side::Buy;
    b_exec.price = 100;
    b_exec.quantity = 10;
    b_exec.remaining_quantity = 0;
    buy_events.push_back(b_exec);

    MatchingEngine engine;
    engine.submit_limit_order(1, Side::Buy, 90, 10, 1000);
    engine.submit_limit_order(2, Side::Sell, 110, 10, 1001);
    mm.refresh_quotes(engine, 1002);
    buy_events[0].order_id = mm.active_bid_id();

    mm.on_market_events(buy_events);

    EXPECT_EQ(mm.inventory(), 10);
    EXPECT_DOUBLE_EQ(mm.cash(), 100000.0 - 1000.0);
    EXPECT_DOUBLE_EQ(mm.realized_pnl(), 0.0);
    EXPECT_DOUBLE_EQ(mm.unrealized_pnl(100), 0.0);
    EXPECT_DOUBLE_EQ(mm.unrealized_pnl(105), 50.0);

    std::vector<MarketEvent> sell_events;
    MarketEvent s_exec;
    s_exec.event_type = EventType::OrderExecuted;
    s_exec.order_id = mm.active_ask_id();
    s_exec.side = Side::Sell;
    s_exec.price = 105;
    s_exec.quantity = 10;
    s_exec.remaining_quantity = 0;
    sell_events.push_back(s_exec);

    mm.on_market_events(sell_events);

    EXPECT_EQ(mm.inventory(), 0);
    EXPECT_DOUBLE_EQ(mm.cash(), 100050.0);
    EXPECT_DOUBLE_EQ(mm.realized_pnl(), 50.0);
    EXPECT_DOUBLE_EQ(mm.total_pnl(105), 50.0);
}

TEST(AvellanedaStoikovTest, EventStreamEqualityForReplay) {
    SimulatorConfig sim_cfg;
    MarketSimulator sim1(sim_cfg, 42);
    MarketSimulator sim2(sim_cfg, 42);

    for (int i = 0; i < 500; ++i) {
        SimulatorEvent ev1 = sim1.next_event();
        SimulatorEvent ev2 = sim2.next_event();

        EXPECT_EQ(ev1.action, ev2.action);
        EXPECT_EQ(ev1.order_id, ev2.order_id);
        EXPECT_EQ(ev1.price, ev2.price);
        EXPECT_EQ(ev1.quantity, ev2.quantity);
    }
}

TEST(AvellanedaStoikovTest, FullSimulationIntegration) {
    AvellanedaStoikovConfig config;
    config.max_inventory = 50;

    MarketSimulator sim(MarketRegime::Normal, 42);
    MatchingEngine engine;
    AvellanedaStoikovMarketMaker mm(config);

    for (std::size_t i = 0; i < 2000; ++i) {
        auto evts = sim.process_next_event(engine);
        mm.on_market_events(evts);
        if (i % config.quote_refresh_interval == 0) {
            mm.refresh_quotes(engine, sim.current_time());
        }
    }

    EXPECT_TRUE(std::abs(mm.inventory()) <= config.max_inventory);
    EXPECT_TRUE(mm.max_abs_inventory() <= config.max_inventory);

    Price mark = 0;
    if (!mm.calculate_fair_value(engine.book(), mark)) {
        mark = sim.reference_price();
    }
    double total = mm.total_pnl(mark);
    double expected_total = mm.realized_pnl() + mm.unrealized_pnl(mark);
    EXPECT_DOUBLE_EQ(total, expected_total);
}
