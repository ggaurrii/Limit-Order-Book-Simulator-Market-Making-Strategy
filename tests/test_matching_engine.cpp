#include "test_runner.hpp"
#include "lob/MatchingEngine.hpp"

using namespace lob;

TEST(MatchingEngineTest, SimpleFullFill) {
    MatchingEngine engine;

    // Post Ask @ 100 Qty 50
    auto events1 = engine.submit_limit_order(1, Side::Sell, 100, 50, 1000);
    EXPECT_EQ(events1.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(events1[0].event_type, EventType::OrderAccepted);

    // Crossing Buy @ 100 Qty 50
    auto events2 = engine.submit_limit_order(2, Side::Buy, 100, 50, 1001);
    
    // Accepted + TradeGenerated + TakerExec + MakerExec
    EXPECT_EQ(events2.size(), static_cast<std::size_t>(4));
    EXPECT_EQ(events2[0].event_type, EventType::OrderAccepted);
    EXPECT_EQ(events2[1].event_type, EventType::TradeGenerated);
    EXPECT_EQ(events2[1].trade.price, static_cast<Price>(100));
    EXPECT_EQ(events2[1].trade.quantity, static_cast<Quantity>(50));
    EXPECT_EQ(events2[1].trade.buy_order_id, static_cast<OrderId>(2));
    EXPECT_EQ(events2[1].trade.sell_order_id, static_cast<OrderId>(1));

    EXPECT_TRUE(engine.book().empty());
}

TEST(MatchingEngineTest, PartialFill) {
    MatchingEngine engine;

    // Post Ask @ 100 Qty 30
    engine.submit_limit_order(1, Side::Sell, 100, 30, 1000);

    // Crossing Buy @ 100 Qty 50
    auto events = engine.submit_limit_order(2, Side::Buy, 100, 50, 1001);

    // Should generate trade of 30, buyer has 20 remaining resting in book
    EXPECT_EQ(events[1].trade.quantity, static_cast<Quantity>(30));
    
    Price bid_p = 0;
    Quantity bid_v = 0;
    EXPECT_TRUE(engine.book().get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_p, static_cast<Price>(100));
    EXPECT_EQ(bid_v, static_cast<Quantity>(20));
    EXPECT_TRUE(engine.book().ask_empty());
}

TEST(MatchingEngineTest, TimePriorityMatching) {
    MatchingEngine engine;

    // Two asks at price 100
    engine.submit_limit_order(1, Side::Sell, 100, 20, 1000); // Ask 1 first
    engine.submit_limit_order(2, Side::Sell, 100, 30, 1001); // Ask 2 second

    // Crossing Buy of 25
    auto events = engine.submit_limit_order(3, Side::Buy, 100, 25, 1002);

    // Should fill Ask 1 completely (20), then Ask 2 partially (5)
    std::vector<Trade> trades;
    for (const auto& evt : events) {
        if (evt.event_type == EventType::TradeGenerated) {
            trades.push_back(evt.trade);
        }
    }

    EXPECT_EQ(trades.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(trades[0].sell_order_id, static_cast<OrderId>(1));
    EXPECT_EQ(trades[0].quantity, static_cast<Quantity>(20));

    EXPECT_EQ(trades[1].sell_order_id, static_cast<OrderId>(2));
    EXPECT_EQ(trades[1].quantity, static_cast<Quantity>(5));

    // Ask 2 should have 25 remaining
    Price ask_p = 0;
    Quantity ask_v = 0;
    EXPECT_TRUE(engine.book().get_best_ask(ask_p, ask_v));
    EXPECT_EQ(ask_v, static_cast<Quantity>(25));
}

TEST(MatchingEngineTest, MarketOrderSweepAndCancelUnfilled) {
    MatchingEngine engine;

    // Asks at 100 (20), 101 (30), 102 (40)
    engine.submit_limit_order(1, Side::Sell, 100, 20, 1000);
    engine.submit_limit_order(2, Side::Sell, 101, 30, 1001);
    engine.submit_limit_order(3, Side::Sell, 102, 40, 1002);

    // Market Buy for 100
    auto events = engine.submit_market_order(4, Side::Buy, 100, 1003);

    std::vector<Trade> trades;
    bool order_cancelled = false;
    Quantity cancelled_qty = 0;

    for (const auto& evt : events) {
        if (evt.event_type == EventType::TradeGenerated) {
            trades.push_back(evt.trade);
        } else if (evt.event_type == EventType::OrderCancelled && evt.order_id == 4) {
            order_cancelled = true;
            cancelled_qty = evt.quantity;
        }
    }

    // Sweeps 20 @ 100, 30 @ 101, 40 @ 102 = 90 filled. Remaining 10 cancelled.
    EXPECT_EQ(trades.size(), static_cast<std::size_t>(3));
    EXPECT_TRUE(order_cancelled);
    EXPECT_EQ(cancelled_qty, static_cast<Quantity>(10));

    EXPECT_TRUE(engine.book().empty());
}

TEST(MatchingEngineTest, CancellationEvents) {
    MatchingEngine engine;
    engine.submit_limit_order(1, Side::Buy, 100, 50, 1000);

    auto events = engine.cancel_order(1, 1001);
    EXPECT_EQ(events.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(events[0].event_type, EventType::OrderCancelled);
    EXPECT_EQ(events[0].order_id, static_cast<OrderId>(1));

    // Cancel non-existent order
    auto rej_events = engine.cancel_order(999, 1002);
    EXPECT_EQ(rej_events.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(rej_events[0].event_type, EventType::OrderRejected);
}

TEST(MatchingEngineTest, InvalidOrderRejections) {
    MatchingEngine engine;

    // Zero quantity
    auto evts1 = engine.submit_limit_order(1, Side::Buy, 100, 0, 1000);
    EXPECT_EQ(evts1[0].event_type, EventType::OrderRejected);

    // Invalid price <= 0
    auto evts2 = engine.submit_limit_order(2, Side::Buy, -10, 50, 1001);
    EXPECT_EQ(evts2[0].event_type, EventType::OrderRejected);

    // Duplicate OrderId
    engine.submit_limit_order(3, Side::Buy, 100, 50, 1002);
    auto evts3 = engine.submit_limit_order(3, Side::Buy, 100, 50, 1003);
    EXPECT_EQ(evts3[0].event_type, EventType::OrderRejected);
}

int main() {
    return ::test_framework::run_all_tests();
}
