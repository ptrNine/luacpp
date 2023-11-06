set(LUA_VARIANT "luajit" CACHE STRING "Lua variant, can be lua or luajit")
set(LUA_GIT_VERSION "v2.1.ROLLING" CACHE STRING "Lua version from git repository")

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


git_submodule_update()
git_submodule_build(Catch2
    CMAKE_ARGS
        -DCATCH_BUILD_TESTING=OFF
        -DCATCH_INSTALL_DOCS=OFF
        -DCATCH_INSTALL_EXTRAS=OFF
)

if (LUA_VARIANT STREQUAL "lua")
    set(_patch "CMake/lua-make.patch.in")
    set(_lua_submodule_name "lua")
endif()
if (LUA_VARIANT STREQUAL "luajit")
    add_compile_definitions(WITH_LUAJIT)
    set(_lua_submodule_name "LuaJIT")
endif()

git_submodule_make_build(${_lua_submodule_name} GIT_VERSION ${LUA_GIT_VERSION} PATCHES ${_patch})

find_package(Catch2 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(${LUA_VARIANT} REQUIRED ${LUA_VARIANT})

if (LUA_VARIANT STREQUAL "luajit")
    foreach (_lib "${${LUA_VARIANT}_LIBRARIES}")
        list(APPEND _libs "${_lib}.a")
    endforeach()
    list(APPEND _libs "dl")
    set(${LUA_VARIANT}_LIBRARIES "${_libs}")
endif()

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

# TODO: provide valid prefix to pkgconfig file
target_include_directories(tests PUBLIC "${CMAKE_BINARY_DIR}/3rd/${${LUA_VARIANT}_INCLUDE_DIRS}")
target_include_directories(tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tests ${${LUA_VARIANT}_LIBRARIES} Catch2::Catch2WithMain)

target_include_directories(benchmarks PUBLIC "${CMAKE_BINARY_DIR}/3rd/${${LUA_VARIANT}_INCLUDE_DIRS}")
target_include_directories(benchmarks PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(benchmarks ${${LUA_VARIANT}_LIBRARIES} Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)
catch_discover_tests(benchmarks)

