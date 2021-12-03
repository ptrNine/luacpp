cmake_minimum_required(VERSION 3.20.5)
project(luaffi)

set(CMAKE_CXX_STANDARD 20)

include(CMake/git_submodule_build.cmake)
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/3rd/include")
link_directories("${CMAKE_BINARY_DIR}/3rd/lib")
link_directories("${CMAKE_BINARY_DIR}/3rd/lib64")

set(_cxx_flags
    -fstrict-aliasing
    -Wall
    -Wextra
    -Wpedantic
    -Wcast-align
    -Wconversion
    -Wctor-dtor-privacy
    -Wextra-semi
    -Wfloat-equal
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wsign-conversion
    -Wsign-promo
    -Werror=unused-result
    -Wno-unused-function
    -Wno-missing-braces
)
foreach(_flag ${_cxx_flags})
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${_flag}>)
endforeach()

find_package(PkgConfig REQUIRED)
pkg_check_modules(luajit REQUIRED luajit)

add_executable(luaffi luaffi.cpp)
target_include_directories(luaffi PUBLIC ${luajit_INCLUDE_DIRS})
target_link_libraries(luaffi ${luajit_LIBRARIES})
