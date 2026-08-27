# Limit Order Book, Synthetic Simulator & Baseline Market Maker (Phases 1–3)

High-performance, deterministic C++ Limit Order Book (LOB), Matching Engine, Synthetic Market Event Simulator, and Inventory-Aware Market-Making Strategy for quantitative trading research.

> [!NOTE]
> This is a quantitative trading project for backtesting, strategy evaluation, and market-making simulation. It is a baseline quantitative framework and not a production trading system.

## Features

- **Phase 1 — Core Matching Engine**:
  - **Price-Time Priority (FIFO)**: Queues per price level guarantee strict time priority.
  - **$O(1)$ Order Cancellation**: Hash map lookup combined with doubly-linked list nodes allows immediate $O(1)$ order cancellations.
  - **Fixed-Point Integer Pricing**: Prices represented in tick units (`int64_t`) to prevent floating-point rounding inaccuracies.
  - **Trade & Event Stream**: Generates deterministic execution trades, acceptances, cancellations, and rejections with sequence numbers.

- **Phase 2 — Synthetic Market Simulator**:
  - **Reproducible RNG (`std::mt19937_64`)**: Explicit random seed control (`seed = 42`) yields 100% deterministic event streams across runs.
  - **Configurable Market Regimes**: `NORMAL`, `HIGH_VOLATILITY`, `LOW_LIQUIDITY`, `BUY_IMBALANCE`, `SELL_IMBALANCE`.
  - **Evolving Reference Price Process**: Random walk with configurable volatility and directional drift parameters.
  - **Active Order Cancellation Tracking**: Tracks open resting orders to sample valid cancellation targets.

- **Phase 3 — Inventory-Aware Baseline Market Maker**:
  - **Fair Value & Reservation Price**: Fair value estimated from top-of-book mid-price $S = \frac{\text{Best Bid} + \text{Best Ask}}{2}$. Reservation price skews by inventory position: $R = S - (\gamma \times I)$.
  - **Inventory Limits (`max_inventory`)**: Suppresses buying when $I \ge \text{max\_inventory}$ and selling when $I \le -\text{max\_inventory}$. Caps quote quantities to respect boundaries.
  - **Quote Lifecycle Management**: Periodically cancels stale strategy quotes and replaces them with updated inventory-skewed quotes.
  - **Exact MTM PnL Accounting**: Computes cash balance, inventory, weighted average cost basis, realized PnL, unrealized mark-to-market PnL, and total PnL.
  - **Strict Chronological Causality**: Strategy operates strictly without future information, reacting to execution events and observable order book states.

## Directory Structure

```
limitorderbook/
├── CMakeLists.txt
├── Makefile
├── README.md
├── .gitignore
├── include/
│   └── lob/
│       ├── Types.hpp             # Basic types, enums, fixed-point price
│       ├── Order.hpp             # Order struct and node definitions
│       ├── PriceLevel.hpp        # Price level queue with total volume
│       ├── OrderBook.hpp         # Bid & Ask trees and ID lookup table
│       ├── MatchingEngine.hpp    # Order lifecycle & trade execution logic
│       ├── Trade.hpp             # Execution trade record
│       ├── MarketEvent.hpp       # Order and market event definitions
│       ├── MarketRegime.hpp      # Market regime enums
│       ├── SimulatorConfig.hpp   # Simulator configuration parameters
│       ├── SimulatorEvent.hpp    # Generated event representation
│       ├── MarketSimulator.hpp   # Synthetic market event simulator
│       ├── MarketMakerConfig.hpp # Market maker configuration parameters
│       └── MarketMaker.hpp       # Inventory-aware market-making strategy
├── src/
│   ├── OrderBook.cpp
│   ├── MatchingEngine.cpp
│   ├── MarketSimulator.cpp
│   ├── MarketMaker.cpp
│   ├── main.cpp                  # Interactive Phase 1 CLI demo
│   ├── sim_demo.cpp              # Phase 2 Market Simulator integration demo
│   └── market_maker_demo.cpp     # Phase 3 Market Maker strategy demo
└── tests/
    ├── test_runner.hpp           # Minimalist C++ unit test framework
    ├── test_order_book.cpp       # Unit tests for OrderBook data structures
    ├── test_matching_engine.cpp    # Unit tests for MatchingEngine rules
    ├── test_simulator.cpp       # Unit tests for MarketSimulator determinism & regimes
    └── test_market_maker.cpp     # Unit tests for MarketMaker inventory & PnL accounting
```

## Building and Testing

### Using `mingw32-make` / `g++` directly:

#### Compile & Run All 34 Unit Tests:
```bash
mingw32-make test
.\test_runner.exe
```

#### Run Phase 3 Market Maker Strategy Demo:
```bash
mingw32-make market_maker_demo.exe
.\market_maker_demo.exe
```

#### Run Phase 2 Simulator Integration Demo:
```bash
mingw32-make sim_demo.exe
.\sim_demo.exe
```

#### Run Phase 1 LOB Demo:
```bash
mingw32-make lob_demo.exe
.\lob_demo.exe
```

### Using CMake:
```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

## Example Market Maker Output (`market_maker_demo.exe`)

Running the baseline inventory-aware market maker in `NORMAL` regime (`seed = 42` over 10,000 events):

```
=================================================================
MARKET MAKER SIMULATION DEMO: Regime = NORMAL, Seed = 42, Events = 10000
=================================================================
Initial Cash:              $1000000.00
Final Cash:                $105696.00
Final Inventory:            90 units
Max Absolute Inventory:     100 units
Mark Price:                 9954 ticks
---------------------------------------------------
Realized PnL:              $694.80
Unrealized PnL:            $861.20
Total PnL:                 $1556.00
---------------------------------------------------
Strategy Orders Submitted:  1919
Strategy Orders Cancelled:  1308
Strategy Orders Filled:     625
Strategy Buy Volume:        3109
Strategy Sell Volume:       3019
---------------------------------------------------
Final Fair Value:           9954
Final Reservation Price:    9945.00
Final Bid Quote:            9943
Final Ask Quote:            9947
===================================================
```
