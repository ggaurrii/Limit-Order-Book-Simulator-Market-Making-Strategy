#ifndef LOB_I_MARKET_MAKING_STRATEGY_HPP
#define LOB_I_MARKET_MAKING_STRATEGY_HPP

#include "lob/Types.hpp"
#include "lob/MarketEvent.hpp"
#include "lob/OrderBook.hpp"
#include "lob/MatchingEngine.hpp"

#include <vector>
#include <cstddef>

namespace lob {

class IMarketMakingStrategy {
public:
    virtual ~IMarketMakingStrategy() = default;

    virtual void on_market_events(const std::vector<MarketEvent>& events) = 0;
    virtual void refresh_quotes(MatchingEngine& engine, Timestamp ts = 0) = 0;

    virtual int inventory() const = 0;
    virtual double cash() const = 0;
    virtual double realized_pnl() const = 0;
    virtual double unrealized_pnl(Price mark_price) const = 0;
    virtual double total_pnl(Price mark_price) const = 0;

    virtual OrderId active_bid_id() const = 0;
    virtual OrderId active_ask_id() const = 0;

    virtual Price fair_value() const = 0;
    virtual double reservation_price() const = 0;
    virtual Price current_bid_quote() const = 0;
    virtual Price current_ask_quote() const = 0;

    virtual std::size_t orders_submitted() const = 0;
    virtual std::size_t orders_cancelled() const = 0;
    virtual std::size_t orders_filled() const = 0;
    virtual Quantity buy_volume() const = 0;
    virtual Quantity sell_volume() const = 0;
    virtual int max_abs_inventory() const = 0;

    virtual void reset() = 0;
};

} // namespace lob

#endif // LOB_I_MARKET_MAKING_STRATEGY_HPP
