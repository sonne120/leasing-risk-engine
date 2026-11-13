
# Modules (in dependency order):
#   common       — ring buffer, mempool, affinity, timing (header-only)
#   core         — deterministic leasing math (contracts, schedules, NPV/IRR)
#   montecarlo   — SDE, schemes, path generator
#   risk         — VaR, CVaR, stress tests
#   pipeline     — ring-buffer-based streaming simulation
#   api          — C ABI shim consumed by the C++/CLI bridge and WPF
cmake_minimum_required(VERSION 3.20)
project(leasing-risk-engine
        VERSION      0.1.0
        DESCRIPTION  "Native quantitative core for a leasing WPF application"
        LANGUAGES    CXX)

set(CMAKE_CXX_STANDARD           20)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)
set(CMAKE_CXX_EXTENSIONS         OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LEASING_BUILD_TESTS "Build native tests" ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
endif()

add_subdirectory(native/common)
add_subdirectory(native/core)
add_subdirectory(native/montecarlo)
add_subdirectory(native/pipeline)
add_subdirectory(native/risk)
add_subdirectory(native/async_demo)

# C++/CLI bridge — Windows + MSVC only 
add_subdirectory(bridge)

if(LEASING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(native/tests)
endif()
