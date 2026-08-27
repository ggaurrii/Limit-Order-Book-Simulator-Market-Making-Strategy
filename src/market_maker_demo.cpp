#include "lob/MarketSimulator.hpp"
#include "lob/MatchingEngine.hpp"
#include "lob/MarketMaker.hpp"

#include <iostream>
#include <iomanip>

void run_market_maker_demo(lob::MarketRegime regime, std::uint64_t seed, std::size_t num_events) {
    std::cout << "\n=================================================================\n";
    std::cout << "MARKET MAKER SIMULATION DEMO: Regime = " << lob::to_string(regime)
              << ", Seed = " << seed
              << ", Events = " << num_events << "\n";
    std::cout << "=================================================================\n";

    lob::SimulatorConfig sim_config = lob::get_regime_config(regime);
    lob::MarketSimulator simulator(sim_config, seed);
    lob::MatchingEngine engine;

    lob::MarketMakerConfig mm_config;
    mm_config.quote_size = 10;
    mm_config.base_half_spread = 2;
    mm_config.max_inventory = 100;
    mm_config.inventory_skew_factor = 0.1;
    mm_config.quote_refresh_interval = 10;
    mm_config.initial_cash = 1000000.0;

    lob::MarketMaker mm(mm_config);

    // Warm up order book with initial market liquidity (e.g. 50 events)
    for (std::size_t i = 0; i < 50; ++i) {
        simulator.process_next_event(engine);
    }

    // Main simulation loop with strict chronological causality
    for (std::size_t i = 50; i < num_events; ++i) {
        // 1. External flow generated & processed
        lob::SimulatorEvent sim_evt = simulator.next_event();
        std::vector<lob::MarketEvent> engine_events;

        if (sim_evt.action == lob::ActionType::SubmitLimit) {
            engine_events = engine.submit_limit_order(sim_evt.order_id, sim_evt.side, sim_evt.price, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::SubmitMarket) {
            engine_events = engine.submit_market_order(sim_evt.order_id, sim_evt.side, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::Cancel) {
            engine_events = engine.cancel_order(sim_evt.order_id, sim_evt.timestamp);
        }

        // 2. Strategy observes market execution events
        mm.on_market_events(engine_events);

        // 3. Periodic strategy quote refresh
        if (i % mm_config.quote_refresh_interval == 0) {
            mm.refresh_quotes(engine, simulator.current_time());
        }
    }

    // Determine mark price for final MTM valuation
    lob::Price mark_price = 0;
    if (!mm.calculate_fair_value(engine.book(), mark_price)) {
        mark_price = simulator.reference_price();
    }

    double init_cash = mm_config.initial_cash;
    double final_cash = mm.cash();
    int final_inv = mm.inventory();
    double realized_pnl = mm.realized_pnl();
    double unrealized_pnl = mm.unrealized_pnl(mark_price);
    double total_pnl = mm.total_pnl(mark_price);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Initial Cash:              $" << init_cash << "\n";
    std::cout << "Final Cash:                $" << final_cash << "\n";
    std::cout << "Final Inventory:            " << final_inv << " units\n";
    std::cout << "Max Absolute Inventory:     " << mm.max_abs_inventory() << " units\n";
    std::cout << "Mark Price:                 " << mark_price << " ticks\n";
    std::cout << "---------------------------------------------------\n";
    std::cout << "Realized PnL:              $" << realized_pnl << "\n";
    std::cout << "Unrealized PnL:            $" << unrealized_pnl << "\n";
    std::cout << "Total PnL:                 $" << total_pnl << "\n";
    std::cout << "---------------------------------------------------\n";
    std::cout << "Strategy Orders Submitted:  " << mm.orders_submitted() << "\n";
    std::cout << "Strategy Orders Cancelled:  " << mm.orders_cancelled() << "\n";
    std::cout << "Strategy Orders Filled:     " << mm.orders_filled() << "\n";
    std::cout << "Strategy Buy Volume:        " << mm.buy_volume() << "\n";
    std::cout << "Strategy Sell Volume:       " << mm.sell_volume() << "\n";
    std::cout << "---------------------------------------------------\n";
    std::cout << "Final Fair Value:           " << mm.fair_value() << "\n";
    std::cout << "Final Reservation Price:    " << mm.reservation_price() << "\n";
    std::cout << "Final Bid Quote:            " << mm.current_bid_quote() << "\n";
    std::cout << "Final Ask Quote:            " << mm.current_ask_quote() << "\n";
    std::cout << "===================================================\n";
}

int main() {
    std::cout << "Baseline Inventory-Aware Market Maker Simulation (Phase 3)\n";

    // 1. Normal Regime Simulation (Seed 42)
    run_market_maker_demo(lob::MarketRegime::Normal, 42, 10000);

    // 2. High Volatility Regime Simulation (Seed 42)
    run_market_maker_demo(lob::MarketRegime::HighVolatility, 42, 10000);

    // 3. Buy Imbalance Regime Simulation (Seed 42)
    run_market_maker_demo(lob::MarketRegime::BuyImbalance, 42, 10000);

    return 0;
}
