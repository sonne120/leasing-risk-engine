add_library(leasing_risk INTERFACE)
add_library(leasing::risk ALIAS leasing_risk)

target_include_directories(leasing_risk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(leasing_risk INTERFACE
    leasing::common
    leasing::montecarlo)
