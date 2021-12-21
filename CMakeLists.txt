cmake_minimum_required(VERSION 3.20.5)
project(luaffi VERSION 0.1 LANGUAGES CXX)

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


# Installing

set(_src_headers
    src/luacpp_ctx.hpp
    src/luacpp_lib.hpp
    src/luacpp_basic.hpp
    src/luacpp_utils.hpp
    src/luacpp_details.hpp
    src/luacpp_assist_gen.hpp
    src/luacpp_member_table.hpp
    src/luacpp_usertype_registry.hpp
)

if (NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR lib)
endif()

if (NOT DEFINED INCLUDE_INSTALL_DIR)
    set(INCLUDE_INSTALL_DIR include)
endif()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Development
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/CMake/config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    INSTALL_DESTINATION "${LIB_INSTALL_DIR}/cmake/${PROJECT_NAME}"
)

install(EXPORT ${PROJECT_NAME}-targets
    DESTINATION "${LIB_INSTALL_DIR}/cmake/${PROJECT_NAME}")

install(FILES
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    DESTINATION "${LIB_INSTALL_DIR}/cmake/${PROJECT_NAME}")

install(FILES ${_src_headers}
    DESTINATION ${INCLUDE_INSTALL_DIR}/${PROJECT_NAME} COMPONENT Devel)

