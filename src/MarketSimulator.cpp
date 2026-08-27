#include "lob/MarketSimulator.hpp"
#include <cmath>
#include <algorithm>

namespace lob {

MarketSimulator::MarketSimulator(const SimulatorConfig& config, std::uint64_t seed)
    : m_config(config)
    , m_regime(MarketRegime::Normal)
    , m_rng(seed)
    , m_reference_price(config.initial_reference_price)
    , m_accumulated_ref_price(static_cast<double>(config.initial_reference_price))
    , m_current_time(0)
    , m_next_order_id(1)
    , m_generated_event_count(0)
{}

MarketSimulator::MarketSimulator(MarketRegime regime, std::uint64_t seed)
    : m_config(get_regime_config(regime))
    , m_regime(regime)
    , m_rng(seed)
    , m_reference_price(m_config.initial_reference_price)
    , m_accumulated_ref_price(static_cast<double>(m_config.initial_reference_price))
    , m_current_time(0)
    , m_next_order_id(1)
    , m_generated_event_count(0)
{}

void MarketSimulator::reset(std::uint64_t seed) {
    m_rng.seed(seed);
    m_reference_price = m_config.initial_reference_price;
    m_accumulated_ref_price = static_cast<double>(m_config.initial_reference_price);
    m_current_time = 0;
    m_next_order_id = 1;
    m_generated_event_count = 0;
    m_active_orders.clear();
}

void MarketSimulator::set_regime(MarketRegime regime) {
    m_regime = regime;
    set_config(get_regime_config(regime));
}

void MarketSimulator::set_config(const SimulatorConfig& config) {
    m_config = config;
}

void MarketSimulator::update_reference_price() {
    std::normal_distribution<double> dist(m_config.price_drift, m_config.price_volatility);
    double step = dist(m_rng);
    m_accumulated_ref_price += step;
    if (m_accumulated_ref_price < 1.0) {
        m_accumulated_ref_price = 1.0;
    }
    m_reference_price = static_cast<Price>(std::round(m_accumulated_ref_price));
}

SimulatorEvent MarketSimulator::next_event() {
    m_current_time += m_config.time_step_ns;
    m_generated_event_count++;

    update_reference_price();

    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    double r = prob_dist(m_rng);

    double c1 = m_config.limit_buy_prob;
    double c2 = c1 + m_config.limit_sell_prob;
    double c3 = c2 + m_config.market_buy_prob;
    double c4 = c3 + m_config.market_sell_prob;

    std::uniform_int_distribution<Quantity> qty_dist(m_config.min_quantity, m_config.max_quantity);
    std::uniform_int_distribution<int> offset_dist(m_config.min_tick_offset, m_config.max_tick_offset);

    // If cancel sampled but no active orders, map r into [0, c4) to emit a limit/market order
    if (r >= c4 && m_active_orders.empty()) {
        r = (c4 > 0.0) ? (r - c4) / (1.0 - c4 + 1e-9) * (c4 - 1e-9) : 0.0;
    }

    if (r < c1) {
        // Limit Buy
        OrderId id = m_next_order_id++;
        Quantity qty = qty_dist(m_rng);
        int offset = offset_dist(m_rng);
        Price price = std::max(static_cast<Price>(1), m_reference_price - offset);
        m_active_orders.push_back(id);
        return SimulatorEvent{ActionType::SubmitLimit, id, Side::Buy, OrderType::Limit, price, qty, m_current_time};
    } 
    else if (r < c2) {
        // Limit Sell
        OrderId id = m_next_order_id++;
        Quantity qty = qty_dist(m_rng);
        int offset = offset_dist(m_rng);
        Price price = m_reference_price + offset;
        m_active_orders.push_back(id);
        return SimulatorEvent{ActionType::SubmitLimit, id, Side::Sell, OrderType::Limit, price, qty, m_current_time};
    } 
    else if (r < c3) {
        // Market Buy
        OrderId id = m_next_order_id++;
        Quantity qty = qty_dist(m_rng);
        return SimulatorEvent{ActionType::SubmitMarket, id, Side::Buy, OrderType::Market, 0, qty, m_current_time};
    } 
    else if (r < c4) {
        // Market Sell
        OrderId id = m_next_order_id++;
        Quantity qty = qty_dist(m_rng);
        return SimulatorEvent{ActionType::SubmitMarket, id, Side::Sell, OrderType::Market, 0, qty, m_current_time};
    } 
    else {
        // Cancel Order
        std::uniform_int_distribution<std::size_t> index_dist(0, m_active_orders.size() - 1);
        std::size_t idx = index_dist(m_rng);
        OrderId cancel_id = m_active_orders[idx];

        // Swap and pop
        m_active_orders[idx] = m_active_orders.back();
        m_active_orders.pop_back();

        return SimulatorEvent{ActionType::Cancel, cancel_id, Side::Unknown, OrderType::Limit, 0, 0, m_current_time};
    }
}

std::vector<MarketEvent> MarketSimulator::process_next_event(MatchingEngine& engine) {
    SimulatorEvent evt = next_event();
    if (evt.action == ActionType::SubmitLimit) {
        return engine.submit_limit_order(evt.order_id, evt.side, evt.price, evt.quantity, evt.timestamp);
    } else if (evt.action == ActionType::SubmitMarket) {
        return engine.submit_market_order(evt.order_id, evt.side, evt.quantity, evt.timestamp);
    } else {
        return engine.cancel_order(evt.order_id, evt.timestamp);
    }
}

} // namespace lob
