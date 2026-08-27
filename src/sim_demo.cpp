#include "lob/MarketSimulator.hpp"
#include "lob/MatchingEngine.hpp"

#include <iostream>
#include <iomanip>
#include <vector>

void run_simulation_demo(lob::MarketRegime regime, std::uint64_t seed, std::size_t num_events) {
    std::cout << "\n==================================================\n";
    std::cout << "RUNNING SIMULATION: Regime = " << lob::to_string(regime) 
              << ", Seed = " << seed 
              << ", Target Events = " << num_events << "\n";
    std::cout << "==================================================\n";

    lob::MarketSimulator simulator(regime, seed);
    lob::MatchingEngine engine;

    std::size_t limit_orders = 0;
    std::size_t market_orders = 0;
    std::size_t cancels = 0;
    std::size_t total_trades = 0;
    lob::Quantity total_traded_volume = 0;

    for (std::size_t i = 0; i < num_events; ++i) {
        lob::SimulatorEvent sim_evt = simulator.next_event();
        std::vector<lob::MarketEvent> engine_events;

        if (sim_evt.action == lob::ActionType::SubmitLimit) {
            limit_orders++;
            engine_events = engine.submit_limit_order(sim_evt.order_id, sim_evt.side, sim_evt.price, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::SubmitMarket) {
            market_orders++;
            engine_events = engine.submit_market_order(sim_evt.order_id, sim_evt.side, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::Cancel) {
            cancels++;
            engine_events = engine.cancel_order(sim_evt.order_id, sim_evt.timestamp);
        }

        for (const auto& evt : engine_events) {
            if (evt.event_type == lob::EventType::TradeGenerated) {
                total_trades++;
                total_traded_volume += evt.trade.quantity;
            }
        }
    }

    std::cout << "Generated Events Count: " << simulator.generated_event_count() << "\n";
    std::cout << "  - Limit Orders:      " << limit_orders << "\n";
    std::cout << "  - Market Orders:     " << market_orders << "\n";
    std::cout << "  - Cancellations:     " << cancels << "\n";
    std::cout << "Matching Engine Trades: " << total_trades << "\n";
    std::cout << "Total Volume Traded:    " << total_traded_volume << "\n";

    lob::Price best_bid = 0, best_ask = 0, spread = 0;
    lob::Quantity bid_vol = 0, ask_vol = 0;

    bool has_bid = engine.book().get_best_bid(best_bid, bid_vol);
    bool has_ask = engine.book().get_best_ask(best_ask, ask_vol);

    std::cout << "\nFinal Book State:\n";
    if (has_bid) {
        std::cout << "  Best Bid: " << std::setw(8) << best_bid << " (Vol: " << bid_vol << ")\n";
    } else {
        std::cout << "  Best Bid: NONE\n";
    }

    if (has_ask) {
        std::cout << "  Best Ask: " << std::setw(8) << best_ask << " (Vol: " << ask_vol << ")\n";
    } else {
        std::cout << "  Best Ask: NONE\n";
    }

    if (engine.book().get_spread(spread)) {
        std::cout << "  Spread:   " << std::setw(8) << spread << "\n";
    } else {
        std::cout << "  Spread:   N/A\n";
    }

    std::cout << "Final Ref Price: " << std::setw(6) << simulator.reference_price() << "\n";
    std::cout << "Active Orders in Book: " << engine.book().total_order_count() << "\n";
}

int main() {
    std::cout << "Synthetic Market Event Simulator & Engine Integration (Phase 2)\n";

    // 1. Reproducibility Run (Seed = 42, Normal Regime)
    run_simulation_demo(lob::MarketRegime::Normal, 42, 10000);

    // 2. High Volatility Regime
    run_simulation_demo(lob::MarketRegime::HighVolatility, 42, 10000);

    // 3. Buy Imbalance Regime
    run_simulation_demo(lob::MarketRegime::BuyImbalance, 42, 10000);

    return 0;
}
