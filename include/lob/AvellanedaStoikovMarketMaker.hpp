#ifndef LOB_AVELLANEDA_STOIKOV_MARKET_MAKER_HPP
#define LOB_AVELLANEDA_STOIKOV_MARKET_MAKER_HPP

#include "lob/IMarketMakingStrategy.hpp"
#include "lob/AvellanedaStoikovConfig.hpp"

#include <vector>
#include <cstddef>

namespace lob {

class AvellanedaStoikovMarketMaker : public IMarketMakingStrategy {
public:
    explicit AvellanedaStoikovMarketMaker(const AvellanedaStoikovConfig& config = AvellanedaStoikovConfig());
    ~AvellanedaStoikovMarketMaker() override = default;

    // Process execution and trade events emitted by MatchingEngine
    void on_market_events(const std::vector<MarketEvent>& events) override;

    // Refresh strategy bid and ask quotes using Avellaneda-Stoikov model
    void refresh_quotes(MatchingEngine& engine, Timestamp ts = 0) override;

    // Cancel all active strategy quotes
    void cancel_active_quotes(MatchingEngine& engine, Timestamp ts = 0);

    // Model components
    bool calculate_fair_value(const OrderBook& book, Price& out_fair_value) const;
    void update_volatility(Price mid_price);
    double current_volatility() const;
    double remaining_horizon_ratio(Timestamp ts) const;

    double calculate_reservation_price(Price fair_value, int inventory, double sigma, double tau) const;
    double calculate_optimal_spread(double sigma, double tau) const;

    // Inspection API
    int inventory() const override { return m_inventory; }
    double cash() const override { return m_cash; }
    double realized_pnl() const override { return m_realized_pnl; }
    double unrealized_pnl(Price mark_price) const override;
    double total_pnl(Price mark_price) const override;

    OrderId active_bid_id() const override { return m_active_bid_id; }
    OrderId active_ask_id() const override { return m_active_ask_id; }

    Price fair_value() const override { return m_last_fair_value; }
    double reservation_price() const override { return m_last_reservation_price; }
    Price current_bid_quote() const override { return m_last_bid_quote; }
    Price current_ask_quote() const override { return m_last_ask_quote; }

    std::size_t orders_submitted() const override { return m_orders_submitted; }
    std::size_t orders_cancelled() const override { return m_orders_cancelled; }
    std::size_t orders_filled() const override { return m_orders_filled; }
    Quantity buy_volume() const override { return m_buy_volume; }
    Quantity sell_volume() const override { return m_sell_volume; }
    int max_abs_inventory() const override { return m_max_abs_inventory; }

    const AvellanedaStoikovConfig& config() const { return m_config; }

    void reset() override;

private:
    OrderId get_next_strategy_order_id() { return m_next_order_id++; }

    AvellanedaStoikovConfig m_config;
    double m_cash;
    int m_inventory;
    int m_max_abs_inventory;

    double m_cost_basis;
    double m_realized_pnl;

    // Volatility Estimator state
    std::vector<double> m_returns_buffer;
    std::size_t m_buffer_head;
    std::size_t m_buffer_count;
    Price m_last_observed_mid;
    double m_cached_volatility;

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

#endif // LOB_AVELLANEDA_STOIKOV_MARKET_MAKER_HPP
