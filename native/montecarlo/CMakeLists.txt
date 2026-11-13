
add_library(leasing_montecarlo INTERFACE)
add_library(leasing::montecarlo ALIAS leasing_montecarlo)

target_include_directories(leasing_montecarlo
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(leasing_montecarlo INTERFACE leasing::common)
