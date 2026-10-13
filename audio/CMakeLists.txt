# Xbox DirectSound on OpenAL Soft (portable).
add_library(audio STATIC
    dsound.cpp
    format.cpp
    output.cpp
    voice.cpp
)
target_include_directories(audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# OpenAL Soft: the system package on Linux (Fedora: openal-soft-devel, plus .i686 for
# the loader), built from source on Windows, where there is no system copy.
if(WIN32)
    include(FetchContent)
    set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(openal_soft
        GIT_REPOSITORY https://github.com/kcat/openal-soft.git
        GIT_TAG 1.24.2)
    FetchContent_MakeAvailable(openal_soft)
    target_link_libraries(audio PUBLIC OpenAL::OpenAL)
else()
    find_package(OpenAL 1.22 REQUIRED)
    target_link_libraries(audio PUBLIC OpenAL::OpenAL)
endif()

target_link_libraries(audio PUBLIC xbox platform)
