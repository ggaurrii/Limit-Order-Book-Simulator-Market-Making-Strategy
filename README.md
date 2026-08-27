# Limit Order Book & Synthetic Market Event Simulator (Phases 1 & 2)

High-performance, deterministic C++ Limit Order Book (LOB), Matching Engine, and Synthetic Market Event Simulator for quantitative trading and market-making research.

> [!NOTE]
> This is a synthetic market event simulator designed for controlled quantitative experiments, backtesting, and market-making strategy evaluation. It does not claim to predict real-world financial markets.

## Features

- **Phase 1 — Core Matching Engine**:
  - **Price-Time Priority (FIFO)**: Queues per price level guarantee strict time priority.
  - **$O(1)$ Order Cancellation**: Hash map lookup combined with doubly-linked list nodes allows immediate $O(1)$ order cancellations.
  - **Fixed-Point Integer Pricing**: Prices represented in tick units (`int64_t`) to prevent floating-point rounding inaccuracies.
  - **Trade & Event Stream**: Generates deterministic execution trades, acceptances, cancellations, and rejections with sequence numbers.

- **Phase 2 — Synthetic Market Simulator**:
  - **Reproducible RNG (`std::mt19937_64`)**: Explicit random seed control (`seed = 42`) yields 100% deterministic event streams across runs.
  - **Configurable Market Regimes**:
    - `NORMAL`: Balanced order flow, standard volatility, and depth.
    - `HIGH_VOLATILITY`: Increased aggressive market order frequency and wider price swings.
    - `LOW_LIQUIDITY`: Wider spreads, lower resting depth, and smaller order sizes.
    - `BUY_IMBALANCE`: Dominant buy-side aggressive flow and positive reference price drift.
    - `SELL_IMBALANCE`: Dominant sell-side aggressive flow and negative reference price drift.
  - **Evolving Reference Price Process**: Random walk with configurable volatility and directional drift parameters.
  - **Active Order Cancellation Tracking**: Tracks open resting orders to sample valid cancellation targets.

## Directory Structure

```
limitorderbook/
├── CMakeLists.txt
├── Makefile
├── README.md
├── .gitignore
├── include/
│   └── lob/
│       ├── Types.hpp          # Basic types, enums, fixed-point price
│       ├── Order.hpp          # Order struct and node definitions
│       ├── PriceLevel.hpp     # Price level queue with total volume
│       ├── OrderBook.hpp      # Bid & Ask trees and ID lookup table
│       ├── MatchingEngine.hpp # Order lifecycle & trade execution logic
│       ├── Trade.hpp          # Execution trade record
│       ├── MarketEvent.hpp    # Order and market event definitions
│       ├── MarketRegime.hpp   # Market regime enums
│       ├── SimulatorConfig.hpp# Simulator configuration parameters
│       ├── SimulatorEvent.hpp # Generated event representation
│       └── MarketSimulator.hpp# Synthetic market event simulator
├── src/
│   ├── OrderBook.cpp
│   ├── MatchingEngine.cpp
│   ├── MarketSimulator.cpp
│   ├── main.cpp               # Interactive Phase 1 CLI demo
│   └── sim_demo.cpp           # Phase 2 Market Simulator integration demo
└── tests/
    ├── test_runner.hpp        # Minimalist C++ unit test framework
    ├── test_order_book.cpp    # Unit tests for OrderBook data structures
    ├── test_matching_engine.cpp # Unit tests for MatchingEngine rules
    └── test_simulator.cpp    # Unit tests for MarketSimulator determinism & regimes
```

## Building and Testing

### Using `mingw32-make` / `g++` directly:

#### Compile & Run All 26 Unit Tests:
```bash
mingw32-make test
.\test_runner.exe
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

## Example Simulator Output (`sim_demo.exe`)

Running the simulator with `seed = 42` over 10,000 events:

```
==================================================
RUNNING SIMULATION: Regime = NORMAL, Seed = 42, Target Events = 10000
==================================================
Generated Events Count: 10000
  - Limit Orders:      7031
  - Market Orders:     1581
  - Cancellations:     1388
Matching Engine Trades: 7138
Total Volume Traded:    377235

Final Book State:
  Best Bid:     9927 (Vol: 1463)
  Best Ask:     9980 (Vol: 785)
  Spread:         53
Final Ref Price:   9909
Active Orders in Book: 1021
```
