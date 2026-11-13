# bridge — C++/CLI assembly. Windows + MSVC only.
#
# Targets:
#   leasing_bridge (shared .dll, mixed-mode /clr)
#     ├─ NativeFacade.cpp         (compiled as pure native C++)
#     └─ ManagedEntry.cpp         (compiled with /clr; brings in headers)
#
# The native .cpp has /clr EXPLICITLY DISABLED via file property — otherwise
# the templated Simulation::run instantiations drag in std::thread code that
# does not compile under /clr.

if(NOT (MSVC AND WIN32))
    message(STATUS "LeasingBridge: skipping — C++/CLI needs MSVC on Windows.")
    return()
endif()

add_library(leasing_bridge SHARED
    NativeFacade.cpp
    ManagedEntry.cpp
)

# Assembly-wide settings.
set_target_properties(leasing_bridge PROPERTIES
    COMMON_LANGUAGE_RUNTIME ""           # /clr (not /clr:pure, not /clr:safe)
    DOTNET_TARGET_FRAMEWORK "net8.0-windows"
    VS_DOTNET_REFERENCES "System;System.Core;System.Data;WindowsBase"
    OUTPUT_NAME "LeasingBridge"
)

# Force the pure-native file off /clr. The managed one inherits /clr from
# the target-level COMMON_LANGUAGE_RUNTIME.
set_source_files_properties(NativeFacade.cpp PROPERTIES
    COMPILE_OPTIONS "/clr-"
)

# MSVC with /clr dislikes some warnings that native-only code enables, so
# soften the global level for this target.
target_compile_options(leasing_bridge PRIVATE /W3 /EHa)

# Static libraries we wrap. Because leasing::core/montecarlo use PIC-safe
# templates and no vcruntime surprises, mixed-mode linking just works.
target_link_libraries(leasing_bridge
    PRIVATE leasing::core leasing::montecarlo leasing::pipeline
            leasing::risk leasing::common)

target_include_directories(leasing_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
