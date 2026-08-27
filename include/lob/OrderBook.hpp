#ifndef LOB_ORDER_BOOK_HPP
#define LOB_ORDER_BOOK_HPP

#include "lob/Types.hpp"
#include "lob/Order.hpp"
#include "lob/PriceLevel.hpp"

#include <map>
#include <unordered_map>
#include <vector>
#include <utility>
#include <functional>
#include <cstddef>

namespace lob {

struct LevelInfo {
    Price price;
    Quantity volume;
    std::size_t order_count;
};

class OrderBook {
public:
    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskMap = std::map<Price, PriceLevel, std::less<Price>>;

    struct OrderLocation {
        Side side;
        Price price;
        PriceLevel::OrderIterator iterator;
    };

    OrderBook() = default;

    // Post limit order directly to the book
    bool add_order(const Order& order);

    // Cancel active order by OrderId
    bool cancel_order(OrderId order_id);
    bool cancel_order(OrderId order_id, Order& out_cancelled_order);

    // Check if order exists
    bool has_order(OrderId order_id) const;

    // Get reference to existing order by ID
    const Order* find_order(OrderId order_id) const;

    // Top of book queries
    bool get_best_bid(Price& price, Quantity& volume) const;
    bool get_best_ask(Price& price, Quantity& volume) const;
    bool get_spread(Price& spread) const;
    bool get_mid_price(double& mid_price) const;

    bool empty() const { return m_bids.empty() && m_asks.empty(); }
    bool bid_empty() const { return m_bids.empty(); }
    bool ask_empty() const { return m_asks.empty(); }

    std::size_t total_order_count() const { return m_order_map.size(); }
    std::size_t bid_level_count() const { return m_bids.size(); }
    std::size_t ask_level_count() const { return m_asks.size(); }

    // Book depth queries
    std::vector<LevelInfo> get_bid_depth(std::size_t levels) const;
    std::vector<LevelInfo> get_ask_depth(std::size_t levels) const;

    // Accessors for matching engine internal operations
    BidMap& bids() { return m_bids; }
    const BidMap& bids() const { return m_bids; }

    AskMap& asks() { return m_asks; }
    const AskMap& asks() const { return m_asks; }

    std::unordered_map<OrderId, OrderLocation>& order_map() { return m_order_map; }
    const std::unordered_map<OrderId, OrderLocation>& order_map() const { return m_order_map; }

    void clear();

private:
    BidMap m_bids;
    AskMap m_asks;
    std::unordered_map<OrderId, OrderLocation> m_order_map;
};

} // namespace lob

#endif // LOB_ORDER_BOOK_HPP
