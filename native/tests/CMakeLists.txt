add_executable(ring_tests ring_tests.cpp)
target_link_libraries(ring_tests PRIVATE leasing::common)
add_test(NAME ring_tests COMMAND ring_tests)

add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE leasing::core)
add_test(NAME core_tests COMMAND core_tests)

add_executable(mc_tests mc_tests.cpp)
target_link_libraries(mc_tests PRIVATE leasing::montecarlo)
add_test(NAME mc_tests COMMAND mc_tests)

add_executable(pipeline_tests pipeline_tests.cpp)
target_link_libraries(pipeline_tests PRIVATE leasing::pipeline)
add_test(NAME pipeline_tests COMMAND pipeline_tests)

add_executable(risk_tests risk_tests.cpp)
target_link_libraries(risk_tests PRIVATE leasing::risk)
add_test(NAME risk_tests COMMAND risk_tests)

# async_demo tests exist only when the demo module is enabled.
if(TARGET leasing::async)
    add_executable(async_demo_tests async_demo_tests.cpp)
    target_link_libraries(async_demo_tests PRIVATE leasing::async)
    add_test(NAME async_demo_tests COMMAND async_demo_tests)
endif()
