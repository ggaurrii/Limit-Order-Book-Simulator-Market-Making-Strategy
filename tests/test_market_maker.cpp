#include "test_runner.hpp"
#include "lob/MarketMaker.hpp"
#include "lob/MarketSimulator.hpp"
#include "lob/MatchingEngine.hpp"

#include <cmath>

using namespace lob;

TEST(MarketMakerTest, FairValueCalculation) {
    MarketMakerConfig config;
    MarketMaker mm(config);
    OrderBook book;

    Price fv = 0;
    // Empty book -> returns false
    EXPECT_FALSE(mm.calculate_fair_value(book, fv));

    // One-sided book (bids only) -> returns false
    book.add_order(Order(1, Side::Buy, OrderType::Limit, 100, 10, 1000));
    EXPECT_FALSE(mm.calculate_fair_value(book, fv));

    // Two-sided book (bids @ 100, asks @ 104) -> mid-price = 102
    book.add_order(Order(2, Side::Sell, OrderType::Limit, 104, 10, 1001));
    EXPECT_TRUE(mm.calculate_fair_value(book, fv));
    EXPECT_EQ(fv, static_cast<Price>(102));
}

TEST(MarketMakerTest, InventorySkewReservationPrice) {
    MarketMakerConfig config;
    config.inventory_skew_factor = 0.5;
    MarketMaker mm(config);

    Price fv = 100;

    // Zero inventory -> Reservation price == Fair value
    double r_zero = mm.calculate_reservation_price(fv, 0);
    EXPECT_DOUBLE_EQ(r_zero, 100.0);

    // Positive inventory (+10 long) -> Reservation price lowers to 95.0
    double r_long = mm.calculate_reservation_price(fv, 10);
    EXPECT_DOUBLE_EQ(r_long, 95.0);

    // Negative inventory (-10 short) -> Reservation price raises to 105.0
    double r_short = mm.calculate_reservation_price(fv, -10);
    EXPECT_DOUBLE_EQ(r_short, 105.0);
}

TEST(MarketMakerTest, QuoteGenerationAndValidity) {
    MarketMakerConfig config;
    config.base_half_spread = 2;
    MarketMaker mm(config);
    MatchingEngine engine;

    // Set up market book: Bids @ 98, Asks @ 102 (mid = 100)
    engine.submit_limit_order(1, Side::Buy, 98, 50, 1000);
    engine.submit_limit_order(2, Side::Sell, 102, 50, 1001);

    mm.refresh_quotes(engine, 1002);

    EXPECT_TRUE(mm.current_bid_quote() > 0);
    EXPECT_TRUE(mm.current_ask_quote() > 0);
    EXPECT_TRUE(mm.current_bid_quote() < mm.current_ask_quote());

    // With 0 inventory, target bid = 100 - 2 = 98, target ask = 100 + 2 = 102
    // Minimum quote distance keeps quotes passive relative to book
    EXPECT_TRUE(mm.current_bid_quote() <= 98);
    EXPECT_TRUE(mm.current_ask_quote() >= 102);
}

TEST(MarketMakerTest, InventoryLimitsSuppression) {
    MarketMakerConfig config;
    config.max_inventory = 20;
    MarketMaker mm(config);
    MatchingEngine engine;

    engine.submit_limit_order(1, Side::Buy, 90, 50, 1000);
    engine.submit_limit_order(2, Side::Sell, 110, 50, 1001);

    // Simulate Buy fill that pushes inventory to max_inventory (+20)
    std::vector<MarketEvent> events;
    MarketEvent exec;
    exec.event_type = EventType::OrderExecuted;
    exec.order_id = config.strategy_id_start;
    exec.side = Side::Buy;
    exec.price = 100;
    exec.quantity = 20;
    exec.remaining_quantity = 0;
    events.push_back(exec);

    // Submit initial dummy order ID matching active bid
    engine.submit_limit_order(config.strategy_id_start, Side::Buy, 95, 20, 1002);
    mm.refresh_quotes(engine, 1003);
    mm.on_market_events(events);

    EXPECT_EQ(mm.inventory(), 20);

    // Refresh quotes at max long inventory -> Bid quote must be suppressed (0), Ask quote active
    mm.refresh_quotes(engine, 1004);
    EXPECT_EQ(mm.active_bid_id(), static_cast<OrderId>(0));
    EXPECT_TRUE(mm.active_ask_id() != 0);
}

TEST(MarketMakerTest, QuoteLifecycleNoAccumulation) {
    MarketMakerConfig config;
    MarketMaker mm(config);
    MatchingEngine engine;

    engine.submit_limit_order(1, Side::Buy, 90, 50, 1000);
    engine.submit_limit_order(2, Side::Sell, 110, 50, 1001);

    // Refresh quotes 5 times
    for (int i = 0; i < 5; ++i) {
        mm.refresh_quotes(engine, 1002 + i);
    }

    // Only 2 active strategy orders (1 bid, 1 ask) should exist in book
    EXPECT_EQ(mm.orders_cancelled(), static_cast<std::size_t>(8)); // 4 pairs of previous quote cancels
    EXPECT_EQ(mm.orders_submitted(), static_cast<std::size_t>(10)); // 5 pairs submitted
    EXPECT_TRUE(mm.active_bid_id() != 0);
    EXPECT_TRUE(mm.active_ask_id() != 0);
}

TEST(MarketMakerTest, ExecutionAccountingAndPnL) {
    MarketMakerConfig config;
    config.initial_cash = 100000.0;
    MarketMaker mm(config);

    // Buy 10 units @ price 100
    std::vector<MarketEvent> buy_events;
    MarketEvent b_exec;
    b_exec.event_type = EventType::OrderExecuted;
    b_exec.order_id = config.strategy_id_start;
    b_exec.side = Side::Buy;
    b_exec.price = 100;
    b_exec.quantity = 10;
    b_exec.remaining_quantity = 0;
    buy_events.push_back(b_exec);

    // Make strategy bid ID active
    MatchingEngine engine;
    engine.submit_limit_order(1, Side::Buy, 90, 10, 1000);
    engine.submit_limit_order(2, Side::Sell, 110, 10, 1001);
    mm.refresh_quotes(engine, 1002);
    OrderId bid_id = mm.active_bid_id();
    buy_events[0].order_id = bid_id;

    mm.on_market_events(buy_events);

    EXPECT_EQ(mm.inventory(), 10);
    EXPECT_DOUBLE_EQ(mm.cash(), 100000.0 - 1000.0); // Cash = 99000
    EXPECT_DOUBLE_EQ(mm.realized_pnl(), 0.0);
    EXPECT_DOUBLE_EQ(mm.unrealized_pnl(100), 0.0); // Mark @ 100
    EXPECT_DOUBLE_EQ(mm.unrealized_pnl(105), 50.0); // Mark @ 105 (+5 per unit * 10)

    // Sell 10 units @ price 105 (closing long position)
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
    EXPECT_DOUBLE_EQ(mm.cash(), 100000.0 + 50.0); // 99000 + 1050 = 100050
    EXPECT_DOUBLE_EQ(mm.realized_pnl(), 50.0);
    EXPECT_DOUBLE_EQ(mm.unrealized_pnl(105), 0.0);
    EXPECT_DOUBLE_EQ(mm.total_pnl(105), 50.0);
}

TEST(MarketMakerTest, StrategyDeterminism) {
    MarketMakerConfig config;

    MarketSimulator sim1(MarketRegime::Normal, 42);
    MatchingEngine engine1;
    MarketMaker mm1(config);

    MarketSimulator sim2(MarketRegime::Normal, 42);
    MatchingEngine engine2;
    MarketMaker mm2(config);

    for (std::size_t i = 0; i < 1000; ++i) {
        auto evts1 = sim1.process_next_event(engine1);
        mm1.on_market_events(evts1);
        if (i % config.quote_refresh_interval == 0) {
            mm1.refresh_quotes(engine1, sim1.current_time());
        }

        auto evts2 = sim2.process_next_event(engine2);
        mm2.on_market_events(evts2);
        if (i % config.quote_refresh_interval == 0) {
            mm2.refresh_quotes(engine2, sim2.current_time());
        }
    }

    EXPECT_EQ(mm1.inventory(), mm2.inventory());
    EXPECT_DOUBLE_EQ(mm1.cash(), mm2.cash());
    EXPECT_DOUBLE_EQ(mm1.realized_pnl(), mm2.realized_pnl());
    EXPECT_EQ(mm1.orders_filled(), mm2.orders_filled());
}

TEST(MarketMakerTest, FullSimulationIntegration) {
    MarketMakerConfig config;
    config.max_inventory = 50;

    MarketSimulator sim(MarketRegime::Normal, 42);
    MatchingEngine engine;
    MarketMaker mm(config);

    for (std::size_t i = 0; i < 2000; ++i) {
        auto evts = sim.process_next_event(engine);
        mm.on_market_events(evts);
        if (i % config.quote_refresh_interval == 0) {
            mm.refresh_quotes(engine, sim.current_time());
        }
    }

    // Verify inventory never exceeded max_inventory bound
    EXPECT_TRUE(std::abs(mm.inventory()) <= config.max_inventory);
    EXPECT_TRUE(mm.max_abs_inventory() <= config.max_inventory);

    // Verify PnL accounting consistency
    Price mark = 0;
    if (!mm.calculate_fair_value(engine.book(), mark)) {
        mark = sim.reference_price();
    }
    double total = mm.total_pnl(mark);
    double expected_total = mm.realized_pnl() + mm.unrealized_pnl(mark);
    EXPECT_DOUBLE_EQ(total, expected_total);
}
