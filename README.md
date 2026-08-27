# Limit Order Book & Matching Engine (Phase 1)

High-performance, deterministic C++ Limit Order Book (LOB) and Matching Engine supporting price-time priority, market and limit orders, order cancellations, partial fills, depth queries, and trade execution generation.

## Features

- **Price-Time Priority (FIFO)**: Queue per price level guarantees strict time priority for identical prices.
- **$O(1)$ Order Cancellation**: Unordered map lookup combined with doubly-linked list nodes allows immediate order cancellation.
- **Fixed-Point Integer Pricing**: Replaces floating-point numbers with tick units to ensure arithmetic accuracy.
- **Comprehensive Testing**: Lightweight, zero-dependency unit test suite validating empty book behavior, price priority, time priority, partial/full fills, market order sweeps, cancellations, and invalid input handling.

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
│       └── MarketEvent.hpp    # Order and market event definitions
├── src/
│   ├── OrderBook.cpp
│   ├── MatchingEngine.cpp
│   └── main.cpp               # Interactive CLI demo harness
└── tests/
    ├── test_runner.hpp        # Minimalist C++ unit test framework
    ├── test_order_book.cpp    # Unit tests for OrderBook data structures
    └── test_matching_engine.cpp # Unit tests for MatchingEngine rules
```

## Building and Testing

### Using `mingw32-make` / `g++` directly:
```bash
mingw32-make test
.\test_runner.exe
```

Or compile manually:
```bash
g++ -std=c++14 -O3 -Iinclude src/OrderBook.cpp src/MatchingEngine.cpp tests/test_order_book.cpp tests/test_matching_engine.cpp -o test_runner.exe
.\test_runner.exe
```

### Using CMake:
```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```
