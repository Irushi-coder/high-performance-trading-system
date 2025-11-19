cmake_minimum_required(VERSION 3.15)
project(HighPerformanceTradingSystem VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -pedantic")

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(CORE_SOURCES
    src/core/order.cpp
    src/core/types.cpp
)

set(ENGINE_SOURCES
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
)

set(NETWORK_SOURCES
    src/network/server.cpp
)

set(UTILS_SOURCES
    src/utils/logger.cpp
    src/utils/timer.cpp
)

# Main executable (Phase 1)
add_executable(trading_system
    src/main.cpp
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${NETWORK_SOURCES}
    ${UTILS_SOURCES}
)

# Order Book Test executable (Phase 2)
add_executable(test_order_book
    src/test_order_book.cpp
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${UTILS_SOURCES}
)

# Matching Engine Test executable (Phase 3)
add_executable(test_matching_engine
    src/test_matching_engine.cpp
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${UTILS_SOURCES}
)

# Performance Test executable (Phase 4)
add_executable(test_performance
    src/test_performance.cpp
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${UTILS_SOURCES}
)

# Link libraries
target_link_libraries(trading_system
    pthread
)

target_link_libraries(test_order_book
    pthread
)

target_link_libraries(test_matching_engine
    pthread
)

# Testing
enable_testing()
add_subdirectory(tests)

# Benchmarks
add_subdirectory(benchmarks)

# Installation
install(TARGETS trading_system DESTINATION bin)