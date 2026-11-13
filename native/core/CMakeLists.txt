add_library(leasing_core STATIC
    src/LeasingCalculator.cpp
)
add_library(leasing::core ALIAS leasing_core)

target_include_directories(leasing_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(leasing_core
    PUBLIC leasing::common)
