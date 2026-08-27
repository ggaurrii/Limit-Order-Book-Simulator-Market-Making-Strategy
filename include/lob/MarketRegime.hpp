#ifndef LOB_MARKET_REGIME_HPP
#define LOB_MARKET_REGIME_HPP

#include <cstdint>
#include <string>

namespace lob {

enum class MarketRegime : std::uint8_t {
    Normal,
    HighVolatility,
    LowLiquidity,
    BuyImbalance,
    SellImbalance
};

inline std::string to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::Normal: return "NORMAL";
        case MarketRegime::HighVolatility: return "HIGH_VOLATILITY";
        case MarketRegime::LowLiquidity: return "LOW_LIQUIDITY";
        case MarketRegime::BuyImbalance: return "BUY_IMBALANCE";
        case MarketRegime::SellImbalance: return "SELL_IMBALANCE";
    }
    return "UNKNOWN";
}

} // namespace lob

#endif // LOB_MARKET_REGIME_HPP
