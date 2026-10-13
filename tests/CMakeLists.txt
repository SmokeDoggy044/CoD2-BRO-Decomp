# Tests for the portable kernel layer (xbox/). Run with `ctest --preset <name>` or
# `ctest --test-dir <build dir>`.
#
# xbox/ stores the title's pointers as 32-bit values (XbPtr), which is only valid in a
# 32-bit address space, so the tests are built for 32-bit targets only for now.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(STATUS "xbox/ tests skipped: they need a 32-bit build (XbPtr is 32 bits)")
    return()
endif()

find_package(Threads REQUIRED)

foreach(test critical_section_test dispatcher_test filesystem_test memory_test)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE xbox Threads::Threads)
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(${test} PROPERTIES TIMEOUT 60)
endforeach()
