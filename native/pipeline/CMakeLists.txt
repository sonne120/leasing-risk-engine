add_library(leasing_pipeline INTERFACE)
add_library(leasing::pipeline ALIAS leasing_pipeline)

target_include_directories(leasing_pipeline INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(leasing_pipeline INTERFACE
    leasing::common
    leasing::montecarlo)
