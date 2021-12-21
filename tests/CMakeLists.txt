if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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

    option(ENABLE_ASAN_FOR_TESTS "Enable address sanitizer in tests" ON)
    if (ENABLE_ASAN_FOR_TESTS)
        set(_asan_compile_flags -fsanitize=address -fno-omit-frame-pointer)
        set(_asan_link_flags -fsanitize=address)
    endif()

    foreach(_flag ${_cxx_flags})
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${_flag}>)
    endforeach()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(luajit REQUIRED luajit)

git_submodule_update()

git_submodule_build(Catch2
    CMAKE_ARGS
        -DCATCH_BUILD_TESTING=OFF
        -DCATCH_INSTALL_DOCS=OFF
        -DCATCH_INSTALL_EXTRAS=OFF
        )

find_package(Catch2 REQUIRED)

add_executable(
    tests
    basic_types.cpp
    functions.cpp
    usertypes.cpp
    )

if (ENABLE_ASAN_FOR_TESTS)
    target_compile_options(tests PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${_asan_compile_flags}>)
    target_link_options(tests PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${_asan_link_flags}>)
endif()

add_executable(benchmarks benchmarks.cpp)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/submodules/Catch2/extras)

target_include_directories(tests PUBLIC ${luajit_INCLUDE_DIRS})
target_include_directories(tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tests ${luajit_LIBRARIES} Catch2::Catch2WithMain)

target_include_directories(benchmarks PUBLIC ${luajit_INCLUDE_DIRS})
target_include_directories(benchmarks PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(benchmarks ${luajit_LIBRARIES} Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)
catch_discover_tests(benchmarks)

