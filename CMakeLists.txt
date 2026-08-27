cmake_minimum_required(VERSION 3.10)
project(LimitOrderBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

include_directories(include)

add_library(lob_core STATIC
    src/OrderBook.cpp
    src/MatchingEngine.cpp
    src/MarketSimulator.cpp
    src/MarketMaker.cpp
    src/AvellanedaStoikovMarketMaker.cpp
)

add_executable(lob_demo
    src/main.cpp
)
target_link_libraries(lob_demo PRIVATE lob_core)

add_executable(sim_demo
    src/sim_demo.cpp
)
target_link_libraries(sim_demo PRIVATE lob_core)

add_executable(market_maker_demo
    src/market_maker_demo.cpp
)
target_link_libraries(market_maker_demo PRIVATE lob_core)

add_executable(strategy_comparison_demo
    src/strategy_comparison_demo.cpp
)
target_link_libraries(strategy_comparison_demo PRIVATE lob_core)

add_executable(test_runner
    tests/test_order_book.cpp
    tests/test_matching_engine.cpp
    tests/test_simulator.cpp
    tests/test_market_maker.cpp
    tests/test_avellaneda_stoikov.cpp
)
target_link_libraries(test_runner PRIVATE lob_core)

enable_testing()
add_test(NAME LOB_Unit_Tests COMMAND test_runner)
