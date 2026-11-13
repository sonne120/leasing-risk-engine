option(LEASING_WITH_STDEXEC "Build the P2300 async_demo module" OFF)

if(NOT LEASING_WITH_STDEXEC)
    message(STATUS "async_demo: skipped (pass -DLEASING_WITH_STDEXEC=ON to enable)")
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG        main          # pinned at integration time
    GIT_SHALLOW    TRUE
)


set(STDEXEC_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING           OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(stdexec)

add_library(leasing_async STATIC
    src/McStdexec.cpp
)
add_library(leasing::async ALIAS leasing_async)

target_include_directories(leasing_async
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(leasing_async
    PUBLIC  leasing::common
            leasing::montecarlo
            leasing::risk
            STDEXEC::stdexec)

if(NOT MSVC)
    target_compile_options(leasing_async PRIVATE -Wno-deprecated-declarations)
endif()
