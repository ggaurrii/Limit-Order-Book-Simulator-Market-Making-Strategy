#ifndef LOB_SIMULATOR_CONFIG_HPP
#define LOB_SIMULATOR_CONFIG_HPP

#include "lob/Types.hpp"
#include "lob/MarketRegime.hpp"

namespace lob {

struct SimulatorConfig {
    Price initial_reference_price;
    double price_volatility; // Standard deviation of reference price steps
    double price_drift;      // Directional price drift per step

    // Probabilities (must sum to 1.0)
    double limit_buy_prob;
    double limit_sell_prob;
    double market_buy_prob;
    double market_sell_prob;
    double cancel_prob;

    // Limit order placement offsets relative to reference price (in ticks)
    int min_tick_offset;
    int max_tick_offset;

    // Order quantity parameters
    Quantity min_quantity;
    Quantity max_quantity;

    // Simulated time increment per event in nanoseconds
    Timestamp time_step_ns;

    SimulatorConfig()
        : initial_reference_price(10000) // $100.00
        , price_volatility(0.8)
        , price_drift(0.0)
        , limit_buy_prob(0.35)
        , limit_sell_prob(0.35)
        , market_buy_prob(0.08)
        , market_sell_prob(0.08)
        , cancel_prob(0.14)
        , min_tick_offset(1)
        , max_tick_offset(10)
        , min_quantity(10)
        , max_quantity(200)
        , time_step_ns(1000000) // 1ms
    {}
};

inline SimulatorConfig get_regime_config(MarketRegime regime) {
    SimulatorConfig config;
    switch (regime) {
        case MarketRegime::Normal:
            // Standard defaults
            break;
        case MarketRegime::HighVolatility:
            config.price_volatility = 2.5;
            config.limit_buy_prob = 0.25;
            config.limit_sell_prob = 0.25;
            config.market_buy_prob = 0.20;
            config.market_sell_prob = 0.20;
            config.cancel_prob = 0.10;
            config.min_tick_offset = 1;
            config.max_tick_offset = 25;
            config.min_quantity = 20;
            config.max_quantity = 500;
            break;
        case MarketRegime::LowLiquidity:
            config.price_volatility = 1.2;
            config.limit_buy_prob = 0.30;
            config.limit_sell_prob = 0.30;
            config.market_buy_prob = 0.10;
            config.market_sell_prob = 0.10;
            config.cancel_prob = 0.20;
            config.min_tick_offset = 3;
            config.max_tick_offset = 15;
            config.min_quantity = 5;
            config.max_quantity = 50;
            break;
        case MarketRegime::BuyImbalance:
            config.price_drift = 0.5;
            config.price_volatility = 1.0;
            config.limit_buy_prob = 0.45;
            config.limit_sell_prob = 0.15;
            config.market_buy_prob = 0.25;
            config.market_sell_prob = 0.05;
            config.cancel_prob = 0.10;
            config.min_tick_offset = 1;
            config.max_tick_offset = 10;
            config.min_quantity = 20;
            config.max_quantity = 300;
            break;
        case MarketRegime::SellImbalance:
            config.price_drift = -0.5;
            config.price_volatility = 1.0;
            config.limit_buy_prob = 0.15;
            config.limit_sell_prob = 0.45;
            config.market_buy_prob = 0.05;
            config.market_sell_prob = 0.25;
            config.cancel_prob = 0.10;
            config.min_tick_offset = 1;
            config.max_tick_offset = 10;
            config.min_quantity = 20;
            config.max_quantity = 300;
            break;
    }
    return config;
}

} // namespace lob

#endif // LOB_SIMULATOR_CONFIG_HPP
