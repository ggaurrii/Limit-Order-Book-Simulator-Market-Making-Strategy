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
)

add_executable(lob_demo
    src/main.cpp
)
target_link_libraries(lob_demo PRIVATE lob_core)

add_executable(test_runner
    tests/test_order_book.cpp
    tests/test_matching_engine.cpp
)
target_link_libraries(test_runner PRIVATE lob_core)

enable_testing()
add_test(NAME LOB_Unit_Tests COMMAND test_runner)
