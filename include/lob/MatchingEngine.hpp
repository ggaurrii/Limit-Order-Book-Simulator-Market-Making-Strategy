#ifndef LOB_MATCHING_ENGINE_HPP
#define LOB_MATCHING_ENGINE_HPP

#include "lob/OrderBook.hpp"
#include "lob/MarketEvent.hpp"
#include "lob/Trade.hpp"

#include <vector>

namespace lob {

class MatchingEngine {
public:
    MatchingEngine()
        : m_next_sequence(1)
        , m_next_trade_id(1) {}

    // Main entry point for submitting orders
    std::vector<MarketEvent> submit_order(Order order);

    // Convenience functions for limit and market order submission
    std::vector<MarketEvent> submit_limit_order(OrderId id, Side side, Price price, Quantity qty, Timestamp ts = 0);
    std::vector<MarketEvent> submit_market_order(OrderId id, Side side, Quantity qty, Timestamp ts = 0);

    // Cancel order by ID
    std::vector<MarketEvent> cancel_order(OrderId id, Timestamp ts = 0);

    // Access underlying OrderBook
    const OrderBook& book() const { return m_book; }
    OrderBook& book() { return m_book; }

    SequenceNumber current_sequence() const { return m_next_sequence; }
    std::uint64_t current_trade_id() const { return m_next_trade_id; }

    void reset();

private:
    std::vector<MarketEvent> match_limit_buy(Order& buy_order);
    std::vector<MarketEvent> match_limit_sell(Order& sell_order);
    std::vector<MarketEvent> match_market_buy(Order& buy_order);
    std::vector<MarketEvent> match_market_sell(Order& sell_order);

    SequenceNumber get_next_sequence() { return m_next_sequence++; }
    std::uint64_t get_next_trade_id() { return m_next_trade_id++; }

    OrderBook m_book;
    SequenceNumber m_next_sequence;
    std::uint64_t m_next_trade_id;
};

} // namespace lob

#endif // LOB_MATCHING_ENGINE_HPP
