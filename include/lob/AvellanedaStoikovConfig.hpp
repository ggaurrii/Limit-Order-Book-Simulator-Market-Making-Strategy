#ifndef LOB_AVELLANEDA_STOIKOV_CONFIG_HPP
#define LOB_AVELLANEDA_STOIKOV_CONFIG_HPP

#include "lob/Types.hpp"
#include <cstddef>

namespace lob {

struct AvellanedaStoikovConfig {
    Quantity quote_size;               // Lot size per quote (default: 10)
    double gamma;                      // Risk aversion parameter (default: 0.1)
    double kappa;                      // Order arrival intensity parameter (default: 1.5)
    std::size_t volatility_window;     // Rolling window size for return observations (default: 50)
    double initial_volatility;         // Fallback initial volatility estimate in ticks (default: 2.0)
    Timestamp terminal_horizon;        // Terminal horizon in nanoseconds/steps (default: 10,000,000,000 ns)
    int max_inventory;                 // Absolute inventory limit (default: 100)
    Price minimum_quote_distance;      // Min distance from top-of-book (default: 1)
    std::size_t quote_refresh_interval;// Event step refresh rate (default: 10)
    double initial_cash;               // Starting cash (default: 1,000,000.0)
    int initial_inventory;             // Starting inventory (default: 0)
    OrderId strategy_id_start;         // Starting OrderId for A-S strategy (default: 2000000)

    AvellanedaStoikovConfig()
        : quote_size(10)
        , gamma(0.1)
        , kappa(1.5)
        , volatility_window(50)
        , initial_volatility(2.0)
        , terminal_horizon(10000000000ULL) // 10s = 10,000 steps @ 1ms/step
        , max_inventory(100)
        , minimum_quote_distance(1)
        , quote_refresh_interval(10)
        , initial_cash(1000000.0)
        , initial_inventory(0)
        , strategy_id_start(2000000)
    {}
};

} // namespace lob

#endif // LOB_AVELLANEDA_STOIKOV_CONFIG_HPP
