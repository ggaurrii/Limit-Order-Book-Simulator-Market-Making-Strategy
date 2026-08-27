#ifndef LOB_MARKET_MAKER_HPP
#define LOB_MARKET_MAKER_HPP

#include "lob/Types.hpp"
#include "lob/MarketMakerConfig.hpp"
#include "lob/MarketEvent.hpp"
#include "lob/OrderBook.hpp"
#include "lob/MatchingEngine.hpp"

#include "lob/IMarketMakingStrategy.hpp"

namespace lob {

class MarketMaker : public IMarketMakingStrategy {
public:
    explicit MarketMaker(const MarketMakerConfig& config = MarketMakerConfig());
    ~MarketMaker() override = default;

    // Process execution and trade events emitted by MatchingEngine
    void on_market_events(const std::vector<MarketEvent>& events);

    // Refresh strategy bid and ask quotes against current book state
    void refresh_quotes(MatchingEngine& engine, Timestamp ts = 0);

    // Cancel all active strategy quotes
    void cancel_active_quotes(MatchingEngine& engine, Timestamp ts = 0);

    // Fair value and reservation price calculations
    bool calculate_fair_value(const OrderBook& book, Price& out_fair_value) const;
    double calculate_reservation_price(Price fair_value, int inventory) const;

    // Inspection API
    int inventory() const { return m_inventory; }
    double cash() const { return m_cash; }
    double realized_pnl() const { return m_realized_pnl; }
    double unrealized_pnl(Price mark_price) const;
    double total_pnl(Price mark_price) const;

    OrderId active_bid_id() const { return m_active_bid_id; }
    OrderId active_ask_id() const { return m_active_ask_id; }

    Price fair_value() const { return m_last_fair_value; }
    double reservation_price() const { return m_last_reservation_price; }
    Price current_bid_quote() const { return m_last_bid_quote; }
    Price current_ask_quote() const { return m_last_ask_quote; }

    std::size_t orders_submitted() const { return m_orders_submitted; }
    std::size_t orders_cancelled() const { return m_orders_cancelled; }
    std::size_t orders_filled() const { return m_orders_filled; }
    Quantity buy_volume() const { return m_buy_volume; }
    Quantity sell_volume() const { return m_sell_volume; }
    int max_abs_inventory() const { return m_max_abs_inventory; }

    const MarketMakerConfig& config() const { return m_config; }

    void reset();

private:
    OrderId get_next_strategy_order_id() { return m_next_order_id++; }

    MarketMakerConfig m_config;
    double m_cash;
    int m_inventory;
    int m_max_abs_inventory;

    // Cost basis tracking for exact realized vs unrealized PnL
    double m_cost_basis;
    double m_realized_pnl;

    OrderId m_active_bid_id;
    OrderId m_active_ask_id;
    OrderId m_next_order_id;

    Price m_last_fair_value;
    double m_last_reservation_price;
    Price m_last_bid_quote;
    Price m_last_ask_quote;

    std::size_t m_orders_submitted;
    std::size_t m_orders_cancelled;
    std::size_t m_orders_filled;
    Quantity m_buy_volume;
    Quantity m_sell_volume;
};

} // namespace lob

#endif // LOB_MARKET_MAKER_HPP
