#ifndef LOB_MARKET_SIMULATOR_HPP
#define LOB_MARKET_SIMULATOR_HPP

#include "lob/Types.hpp"
#include "lob/MarketRegime.hpp"
#include "lob/SimulatorConfig.hpp"
#include "lob/SimulatorEvent.hpp"
#include "lob/MatchingEngine.hpp"

#include <random>
#include <vector>
#include <cstdint>

namespace lob {

class MarketSimulator {
public:
    explicit MarketSimulator(const SimulatorConfig& config, std::uint64_t seed = 42);
    explicit MarketSimulator(MarketRegime regime = MarketRegime::Normal, std::uint64_t seed = 42);

    // Generate next synthetic market event
    SimulatorEvent next_event();

    // Convenience function: generates next event and processes it directly on MatchingEngine
    std::vector<MarketEvent> process_next_event(MatchingEngine& engine);

    // Dynamic regime change
    void set_regime(MarketRegime regime);
    void set_config(const SimulatorConfig& config);

    // State queries
    Price reference_price() const { return m_reference_price; }
    Timestamp current_time() const { return m_current_time; }
    MarketRegime current_regime() const { return m_regime; }
    std::size_t generated_event_count() const { return m_generated_event_count; }
    std::size_t active_order_count() const { return m_active_orders.size(); }
    const SimulatorConfig& config() const { return m_config; }

    void reset(std::uint64_t seed = 42);

private:
    void update_reference_price();
    ActionType sample_action_type();

    SimulatorConfig m_config;
    MarketRegime m_regime;
    std::mt19937_64 m_rng;
    Price m_reference_price;
    double m_accumulated_ref_price;
    Timestamp m_current_time;
    OrderId m_next_order_id;
    std::vector<OrderId> m_active_orders;
    std::size_t m_generated_event_count;
};

} // namespace lob

#endif // LOB_MARKET_SIMULATOR_HPP
