# common — header-only library of lock-free primitives.
# Exposed as INTERFACE target so other modules just link `leasing::common` and
# get the include path and any platform libs (pthread) for free.

add_library(leasing_common INTERFACE)
add_library(leasing::common ALIAS leasing_common)

target_include_directories(leasing_common
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

# Threading support — pthread on Linux/macOS, nothing extra on MSVC.
find_package(Threads REQUIRED)
target_link_libraries(leasing_common INTERFACE Threads::Threads)
