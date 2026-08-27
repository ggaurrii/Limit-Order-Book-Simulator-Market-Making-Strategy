#ifndef LOB_MARKET_MAKER_CONFIG_HPP
#define LOB_MARKET_MAKER_CONFIG_HPP

#include "lob/Types.hpp"
#include <cstddef>

namespace lob {

struct MarketMakerConfig {
    Quantity quote_size;               // Lot size per quote (default: 10)
    Price base_half_spread;            // Half-spread in ticks (default: 2)
    int max_inventory;                 // Absolute inventory limit (default: 100)
    double inventory_skew_factor;      // Skew ticks per unit inventory (default: 0.1)
    std::size_t quote_refresh_interval;// Event step refresh rate (default: 10)
    Price minimum_quote_distance;      // Min distance from top-of-book (default: 1)
    double initial_cash;               // Starting cash (default: 1,000,000.0)
    int initial_inventory;             // Starting inventory (default: 0)
    OrderId strategy_id_start;         // Starting OrderId for strategy orders (default: 1000000)

    MarketMakerConfig()
        : quote_size(10)
        , base_half_spread(2)
        , max_inventory(100)
        , inventory_skew_factor(0.1)
        , quote_refresh_interval(10)
        , minimum_quote_distance(1)
        , initial_cash(1000000.0)
        , initial_inventory(0)
        , strategy_id_start(1000000)
    {}
};

} // namespace lob

#endif // LOB_MARKET_MAKER_CONFIG_HPP
