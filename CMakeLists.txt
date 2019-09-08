cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)
message("-- The cmake version is ${CMAKE_VERSION}")

if (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  message(FATAL_ERROR
    "You are trying to cmake in-tree and this is not supported.  Please run "
    "the following instead:\n"
    "  $ rm CMakeCache.txt\n"
    "  $ mkdir build\n"
    "  $ cd build\n"
    "  $ cmake ..\n"
    "  $ ninja")
endif()

# cmake includes
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

set(PACKAGE_NAME "skip")
set(PACKAGE_VERSION "0.1")

set(GIT_EXISTS FALSE)
if (EXISTS ${CMAKE_SOURCE_DIR}/.git)
  set(GIT_EXISTS TRUE)
endif ()

project(${PACKAGE_NAME} LANGUAGES C CXX ASM)

# Force the use of c++14 without GNU extensions
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(SkipOptions)
include(SkipCompiler)
include(SkipUtil)
include(SkipFunctions)

if (APPLE)
  set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif ()

if(USE_JEMALLOC)
  add_definitions(-DUSE_JEMALLOC)
endif()

set(ENABLE_ASYNC_MYSQL OFF)
set(ENABLE_MCROUTER OFF)
set(ENABLE_GOOGLETEST ON)
set(JEMALLOC_ENABLED ON)

find_package(Clang)

# This allows us to strip unused code using -gc-sections
add_compile_options(-ffunction-sections -fdata-sections)

if(CMAKE_VERSION VERSION_LESS "3.11.0")
  # Allow use of 'test' target
  cmake_policy(SET CMP0037 OLD)
endif()
add_custom_target(test)

# TODO: Workaround for T22755682 (LLVM bug).
set(SKIP_COMPILE_FLAGS --nbe-flags=--nogoto)

set(LKG_DIR ${CMAKE_SOURCE_DIR}/lkg)
set(TOOLS_DIR ${CMAKE_SOURCE_DIR}/tools)

add_subdirectory(third-party)
add_subdirectory(lkg)
add_subdirectory(src)
add_subdirectory(tests)

if (NOT BUILD_TOOLS)
  message(FATAL_ERROR "BUILD_TOOLS not set")
endif()
