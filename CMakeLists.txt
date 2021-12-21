cmake_minimum_required(VERSION 3.20.5)
project(luaffi)

set(CMAKE_CXX_STANDARD 20)

option(ENABLE_TESTS "Enables tests" ON)

include(CMake/git_submodule_build.cmake)
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/3rd")

include_directories(SYSTEM "${CMAKE_BINARY_DIR}/3rd/include")
link_directories("${CMAKE_BINARY_DIR}/3rd/lib")
link_directories("${CMAKE_BINARY_DIR}/3rd/lib64")

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
