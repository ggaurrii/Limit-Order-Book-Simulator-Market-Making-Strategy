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

TEST(MatchingEngineTest, CrossingLimitOrderWithRemainder) {
    MatchingEngine engine;

    // Asks at 100 (20), 101 (30), 105 (40)
    engine.submit_limit_order(1, Side::Sell, 100, 20, 1000);
    engine.submit_limit_order(2, Side::Sell, 101, 30, 1001);
    engine.submit_limit_order(3, Side::Sell, 105, 40, 1002);

    // Limit Buy at 102 with Qty 70
    // Crosses ask 100 (20 filled) and ask 101 (30 filled). Total filled = 50.
    // Does NOT cross ask 105 (102 < 105).
    // Remaining qty 20 rests on Bid book at limit price 102.
    auto events = engine.submit_limit_order(4, Side::Buy, 102, 70, 1003);

    std::vector<Trade> trades;
    for (const auto& evt : events) {
        if (evt.event_type == EventType::TradeGenerated) {
            trades.push_back(evt.trade);
        }
    }

    EXPECT_EQ(trades.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(trades[0].trade_id, static_cast<std::uint64_t>(1));
    EXPECT_EQ(trades[0].price, static_cast<Price>(100));
    EXPECT_EQ(trades[0].quantity, static_cast<Quantity>(20));

    EXPECT_EQ(trades[1].trade_id, static_cast<std::uint64_t>(2));
    EXPECT_EQ(trades[1].price, static_cast<Price>(101));
    EXPECT_EQ(trades[1].quantity, static_cast<Quantity>(30));

    Price bid_p = 0, ask_p = 0;
    Quantity bid_v = 0, ask_v = 0;

    EXPECT_TRUE(engine.book().get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_p, static_cast<Price>(102));
    EXPECT_EQ(bid_v, static_cast<Quantity>(20));

    EXPECT_TRUE(engine.book().get_best_ask(ask_p, ask_v));
    EXPECT_EQ(ask_p, static_cast<Price>(105));
    EXPECT_EQ(ask_v, static_cast<Quantity>(40));
}

TEST(MatchingEngineTest, MiddleOrderCancellationFIFO) {
    MatchingEngine engine;

    // Three bids at price 100
    engine.submit_limit_order(1, Side::Buy, 100, 10, 1000); // Head
    engine.submit_limit_order(2, Side::Buy, 100, 20, 1001); // Middle
    engine.submit_limit_order(3, Side::Buy, 100, 30, 1002); // Tail

    // Cancel middle order #2
    auto cnl_events = engine.cancel_order(2, 1003);
    EXPECT_EQ(cnl_events.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(cnl_events[0].event_type, EventType::OrderCancelled);
    EXPECT_EQ(cnl_events[0].order_id, static_cast<OrderId>(2));

    // Submit Limit Sell at 100 for Qty 25
    // Should fill Order #1 (10), skip Order #2 (cancelled), and fill Order #3 partially (15)
    auto events = engine.submit_limit_order(4, Side::Sell, 100, 25, 1004);

    std::vector<Trade> trades;
    for (const auto& evt : events) {
        if (evt.event_type == EventType::TradeGenerated) {
            trades.push_back(evt.trade);
        }
    }

    EXPECT_EQ(trades.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(trades[0].buy_order_id, static_cast<OrderId>(1));
    EXPECT_EQ(trades[0].quantity, static_cast<Quantity>(10));

    EXPECT_EQ(trades[1].buy_order_id, static_cast<OrderId>(3));
    EXPECT_EQ(trades[1].quantity, static_cast<Quantity>(15));

    // Order #3 should have 15 remaining resting on Bid book
    Price bid_p = 0;
    Quantity bid_v = 0;
    EXPECT_TRUE(engine.book().get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_p, static_cast<Price>(100));
    EXPECT_EQ(bid_v, static_cast<Quantity>(15));
}

TEST(MatchingEngineTest, MarketOrderAgainstEmptyBook) {
    MatchingEngine engine;

    // Market buy against empty ask book
    auto buy_events = engine.submit_market_order(1, Side::Buy, 50, 1000);
    EXPECT_EQ(buy_events.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(buy_events[0].event_type, EventType::OrderAccepted);
    EXPECT_EQ(buy_events[1].event_type, EventType::OrderCancelled);
    EXPECT_EQ(buy_events[1].quantity, static_cast<Quantity>(50));
    EXPECT_TRUE(engine.book().empty());

    // Market sell against empty bid book
    auto sell_events = engine.submit_market_order(2, Side::Sell, 30, 1001);
    EXPECT_EQ(sell_events.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(sell_events[0].event_type, EventType::OrderAccepted);
    EXPECT_EQ(sell_events[1].event_type, EventType::OrderCancelled);
    EXPECT_EQ(sell_events[1].quantity, static_cast<Quantity>(30));
    EXPECT_TRUE(engine.book().empty());
}

TEST(MatchingEngineTest, LargeMultiLevelSweep) {
    MatchingEngine engine;

    // Create 4 ask levels: 100 (10), 101 (20), 102 (30), 103 (40)
    engine.submit_limit_order(1, Side::Sell, 100, 10, 1000);
    engine.submit_limit_order(2, Side::Sell, 101, 20, 1001);
    engine.submit_limit_order(3, Side::Sell, 102, 30, 1002);
    engine.submit_limit_order(4, Side::Sell, 103, 40, 1003);

    // Limit Buy @ 105 for Qty 150 (sweeps all asks = 100, rests 50 @ 105)
    auto events = engine.submit_limit_order(5, Side::Buy, 105, 150, 1004);

    std::vector<Trade> trades;
    for (const auto& evt : events) {
        if (evt.event_type == EventType::TradeGenerated) {
            trades.push_back(evt.trade);
        }
    }

    EXPECT_EQ(trades.size(), static_cast<std::size_t>(4));
    EXPECT_EQ(trades[0].price, static_cast<Price>(100));
    EXPECT_EQ(trades[1].price, static_cast<Price>(101));
    EXPECT_EQ(trades[2].price, static_cast<Price>(102));
    EXPECT_EQ(trades[3].price, static_cast<Price>(103));

    EXPECT_TRUE(engine.book().ask_empty());
    
    Price bid_p = 0;
    Quantity bid_v = 0;
    EXPECT_TRUE(engine.book().get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_p, static_cast<Price>(105));
    EXPECT_EQ(bid_v, static_cast<Quantity>(50));
}

TEST(MatchingEngineTest, SequenceAndTradeMonotonicity) {
    MatchingEngine engine;

    EXPECT_EQ(engine.current_sequence(), static_cast<SequenceNumber>(1));
    EXPECT_EQ(engine.current_trade_id(), static_cast<std::uint64_t>(1));

    auto ev1 = engine.submit_limit_order(1, Side::Sell, 100, 10, 1000);
    SequenceNumber prev_seq = ev1.back().sequence_number;

    auto ev2 = engine.submit_limit_order(2, Side::Buy, 100, 10, 1001);
    for (const auto& evt : ev2) {
        EXPECT_TRUE(evt.sequence_number > prev_seq);
        prev_seq = evt.sequence_number;
    }

    std::uint64_t prev_trade_id = 0;
    for (const auto& evt : ev2) {
        if (evt.event_type == EventType::TradeGenerated) {
            EXPECT_TRUE(evt.trade.trade_id > prev_trade_id);
            prev_trade_id = evt.trade.trade_id;
        }
    }
}

TEST(MatchingEngineTest, MatchingEngineReset) {
    MatchingEngine engine;

    engine.submit_limit_order(1, Side::Buy, 100, 50, 1000);
    engine.submit_limit_order(2, Side::Sell, 105, 30, 1001);
    engine.submit_limit_order(3, Side::Buy, 105, 10, 1002); // Executes against #2

    EXPECT_FALSE(engine.book().empty());
    EXPECT_TRUE(engine.book().total_order_count() > 0);
    EXPECT_TRUE(engine.current_sequence() > 1);
    EXPECT_TRUE(engine.current_trade_id() > 1);

    engine.reset();

    EXPECT_TRUE(engine.book().empty());
    EXPECT_EQ(engine.book().total_order_count(), static_cast<std::size_t>(0));
    EXPECT_FALSE(engine.book().has_order(1));
    EXPECT_FALSE(engine.book().has_order(2));
    EXPECT_FALSE(engine.book().has_order(3));
    EXPECT_EQ(engine.current_sequence(), static_cast<SequenceNumber>(1));
    EXPECT_EQ(engine.current_trade_id(), static_cast<std::uint64_t>(1));
}

int main() {
    return ::test_framework::run_all_tests();
}
