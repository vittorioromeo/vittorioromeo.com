project(vittorioromeo_dot_info)

cmake_minimum_required(VERSION 3.5)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# download CPM to manage external dependencies
file(
    DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
    EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME discount
    GIT_REPOSITORY https://github.com/Orc/discount.git
    GIT_TAG v2.2.7
    DOWNLOAD_ONLY
    SYSTEM TRUE
)
add_definitions(-Wno-error -Wno-implicit-int -Wno-incompatible-pointer-types)

if (discount_ADDED)
    add_definitions("-DDESTRUCTOR=")
    include("${discount_SOURCE_DIR}/cmake/CMakeLists.txt")
endif()

add_definitions(-Wno-error -Wno-implicit-int -Wno-incompatible-pointer-types)

# Include `vrm_cmake`.
list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_SOURCE_DIR}/../vrm_cmake/cmake/"
    "${CMAKE_SOURCE_DIR}/extlibs/vrm_cmake/cmake/")

include(vrm_cmake)

# Basic project setup.
vrm_cmake_init_project(vittorioromeo_dot_info)
vrm_cmake_find_extlib(vrm_pp)
vrm_cmake_find_extlib(vrm_core)
vrm_cmake_find_extlib(SSVUtils)
find_library(LIB_MARKDOWN markdown)
find_path(INC_MARKDOWN mkdio.h)

#vrm_cmake_add_common_compiler_flags()
add_definitions(-Wno-error -Wno-implicit-int -Wno-incompatible-pointer-types)

set(VRDI_SRC_DIR "${VITTORIOROMEO_DOT_INFO_SOURCE_DIR}/src/")
set(VRDI_INC_DIR "${VITTORIOROMEO_DOT_INFO_SOURCE_DIR}/include/")

include_directories("${VRDI_INC_DIR}")

add_executable(${PROJECT_NAME} "${VRDI_SRC_DIR}/main.cpp")
target_link_libraries(${PROJECT_NAME} libmarkdown)
add_definitions(-Wno-error -Wno-implicit-int -Wno-incompatible-pointer-types)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/build/)
