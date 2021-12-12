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
    )

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/submodules/Catch2/extras)

target_include_directories(tests PUBLIC ${luajit_INCLUDE_DIRS})
target_include_directories(tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tests ${luajit_LIBRARIES} Catch2::Catch2WithMain)
include(CTest)
include(Catch)
catch_discover_tests(tests)

