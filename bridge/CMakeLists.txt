
if(NOT (MSVC AND WIN32))
    message(STATUS "LeasingBridge: skipping — C++/CLI needs MSVC on Windows.")
    return()
endif()

add_library(leasing_bridge SHARED
    NativeFacade.cpp
    ManagedEntry.cpp
)


set_target_properties(leasing_bridge PROPERTIES
    COMMON_LANGUAGE_RUNTIME ""           # /clr (not /clr:pure, not /clr:safe)
    DOTNET_TARGET_FRAMEWORK "net8.0-windows"
    VS_DOTNET_REFERENCES "System;System.Core;System.Data;WindowsBase"
    OUTPUT_NAME "LeasingBridge"
)

set_source_files_properties(NativeFacade.cpp PROPERTIES
    COMPILE_OPTIONS "/clr-"
)


target_compile_options(leasing_bridge PRIVATE /W3 /EHa)

target_link_libraries(leasing_bridge
    PRIVATE leasing::core leasing::montecarlo leasing::pipeline
            leasing::risk leasing::common)

target_include_directories(leasing_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
