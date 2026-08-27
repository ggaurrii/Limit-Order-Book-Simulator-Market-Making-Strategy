#include "test_runner.hpp"
#include "lob/OrderBook.hpp"

using namespace lob;

TEST(OrderBookTest, EmptyBook) {
    OrderBook book;
    EXPECT_TRUE(book.empty());
    EXPECT_TRUE(book.bid_empty());
    EXPECT_TRUE(book.ask_empty());
    EXPECT_EQ(book.total_order_count(), static_cast<std::size_t>(0));

    Price price = 0;
    Quantity volume = 0;
    double mid = 0.0;

    EXPECT_FALSE(book.get_best_bid(price, volume));
    EXPECT_FALSE(book.get_best_ask(price, volume));
    EXPECT_FALSE(book.get_spread(price));
    EXPECT_FALSE(book.get_mid_price(mid));
}

TEST(OrderBookTest, SingleBidAskPlacement) {
    OrderBook book;
    Order buy1(1, Side::Buy, OrderType::Limit, 100, 50, 1000);
    Order sell1(2, Side::Sell, OrderType::Limit, 105, 30, 1001);

    EXPECT_TRUE(book.add_order(buy1));
    EXPECT_TRUE(book.add_order(sell1));

    EXPECT_EQ(book.total_order_count(), static_cast<std::size_t>(2));

    Price bid_p = 0, ask_p = 0, spread = 0;
    Quantity bid_v = 0, ask_v = 0;
    double mid = 0.0;

    EXPECT_TRUE(book.get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_p, static_cast<Price>(100));
    EXPECT_EQ(bid_v, static_cast<Quantity>(50));

    EXPECT_TRUE(book.get_best_ask(ask_p, ask_v));
    EXPECT_EQ(ask_p, static_cast<Price>(105));
    EXPECT_EQ(ask_v, static_cast<Quantity>(30));

    EXPECT_TRUE(book.get_spread(spread));
    EXPECT_EQ(spread, static_cast<Price>(5));

    EXPECT_TRUE(book.get_mid_price(mid));
    EXPECT_DOUBLE_EQ(mid, 102.5);
}

TEST(OrderBookTest, PricePriorityOrdering) {
    OrderBook book;
    book.add_order(Order(1, Side::Buy, OrderType::Limit, 100, 10, 1));
    book.add_order(Order(2, Side::Buy, OrderType::Limit, 102, 20, 2));
    book.add_order(Order(3, Side::Buy, OrderType::Limit, 101, 15, 3));

    Price bid_p = 0;
    Quantity bid_v = 0;
    EXPECT_TRUE(book.get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_p, static_cast<Price>(102));

    auto depth = book.get_bid_depth(3);
    EXPECT_EQ(depth.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(depth[0].price, static_cast<Price>(102));
    EXPECT_EQ(depth[1].price, static_cast<Price>(101));
    EXPECT_EQ(depth[2].price, static_cast<Price>(100));
}

TEST(OrderBookTest, TimePrioritySamePrice) {
    OrderBook book;
    book.add_order(Order(1, Side::Buy, OrderType::Limit, 100, 10, 1));
    book.add_order(Order(2, Side::Buy, OrderType::Limit, 100, 20, 2));

    Price bid_p = 0;
    Quantity bid_v = 0;
    EXPECT_TRUE(book.get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_v, static_cast<Quantity>(30)); // 10 + 20

    const Order* ord1 = book.find_order(1);
    const Order* ord2 = book.find_order(2);
    EXPECT_TRUE(ord1 != nullptr);
    EXPECT_TRUE(ord2 != nullptr);

    auto depth = book.get_bid_depth(1);
    EXPECT_EQ(depth[0].order_count, static_cast<std::size_t>(2));
}

TEST(OrderBookTest, OrderCancellation) {
    OrderBook book;
    book.add_order(Order(1, Side::Buy, OrderType::Limit, 100, 10, 1));
    book.add_order(Order(2, Side::Buy, OrderType::Limit, 100, 20, 2));

    EXPECT_TRUE(book.has_order(1));
    EXPECT_TRUE(book.cancel_order(1));
    EXPECT_FALSE(book.has_order(1));
    EXPECT_EQ(book.total_order_count(), static_cast<std::size_t>(1));

    Price bid_p = 0;
    Quantity bid_v = 0;
    EXPECT_TRUE(book.get_best_bid(bid_p, bid_v));
    EXPECT_EQ(bid_v, static_cast<Quantity>(20));

    // Cancel second order, price level should be removed
    EXPECT_TRUE(book.cancel_order(2));
    EXPECT_TRUE(book.bid_empty());
    EXPECT_EQ(book.bid_level_count(), static_cast<std::size_t>(0));
}

TEST(OrderBookTest, InvalidAndDuplicateOrders) {
    OrderBook book;
    EXPECT_TRUE(book.add_order(Order(1, Side::Buy, OrderType::Limit, 100, 10, 1)));
    EXPECT_FALSE(book.add_order(Order(1, Side::Buy, OrderType::Limit, 100, 10, 2))); // Duplicate ID

    Order zero_qty(2, Side::Buy, OrderType::Limit, 100, 0, 3);
    zero_qty.remaining_quantity = 0;
    EXPECT_FALSE(book.add_order(zero_qty));
}
