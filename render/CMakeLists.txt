# Xbox Direct3D 8 on a D3D9 back end (portable).
add_library(render STATIC
    backend_d3d9.cpp
    device.cpp
    resource.cpp
)
target_include_directories(render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(render PUBLIC xbox platform)

# SDL3 for the window; D3D9 from the system on Windows, from DXVK (dxvk-native) on
# Linux. Fedora: SDL3-devel and dxvk-native-devel, plus their .i686 packages for the
# loader.
if(WIN32)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10)
    FetchContent_MakeAvailable(sdl3)
    target_link_libraries(render PRIVATE SDL3::SDL3 d3d9)
else()
    find_package(SDL3 REQUIRED CONFIG)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DXVK_D3D9 REQUIRED IMPORTED_TARGET dxvk-d3d9)
    target_link_libraries(render PRIVATE SDL3::SDL3 PkgConfig::DXVK_D3D9)
endif()
