if(NOT DEFINED GFLAGS_FOUND)
  find_path(GFLAGS_INCLUDE NAMES gflags/gflags.h HINTS ${GFLAGS_HINT}/include)
  find_library(GFLAGS_LIBRARY NAMES libgflags.a libgflags.dylib libgflags.so)
  if(GFLAGS_INCLUDE AND GFLAGS_LIBRARY)
    set(GFLAGS_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(GFLAGS_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

if(GFLAGS_FOUND)
  message("Using built-in gflags: ${GFLAGS_INCLUDE} ${GFLAGS_LIBRARY}")

  add_library(gflags INTERFACE)
  target_include_directories(gflags INTERFACE ${GFLAGS_INCLUDE})
  target_link_libraries(gflags INTERFACE ${GFLAGS_LIBRARY})

  get_filename_component(GFLAGS_LIB_DIR ${GFLAGS_LIBRARY} DIRECTORY CACHE)
  return()
endif()

if(DISABLE_TP_GFLAGS)
  message(FATAL_ERROR "gflags not found and third-party disabled")
endif()

message("Using third-party gflags")

include(ExternalProject)

set(GFLAGS_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)
set(GFLAGS_INCLUDE ${GFLAGS_PREFIX}/include CACHE FILEPATH "" FORCE)
set(GFLAGS_LIB_DIR ${GFLAGS_PREFIX}/lib CACHE FILEPATH "" FORCE)
set(GFLAGS_LIBRARY ${GFLAGS_LIB_DIR}/libgflags.a CACHE FILEPATH "" FORCE)

ExternalProject_Add(
  gflags_sub
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
  INSTALL_DIR ${GFLAGS_PREFIX}
  BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
  USES_TERMINAL_BUILD 1
  INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} # Copy over the build type (debug, release)
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> # Install locally
    -DGFLAGS_NAMESPACE=gflags
    -DGFLAGS_BUILD_SHARED_LIBS=OFF
    -DGFLAGS_BUILD_STATIC_LIBS=ON
    -DGFLAGS_BUILD_gflags_LIB=ON
    -DGFLAGS_BUILD_TESTING=OFF
    -DGFLAGS_BUILD_PACKAGING=OFF
    -DCMAKE_CXX_FLAGS=-fPIC
    -DGFLAGS_INSTALL_HEADERS=ON
    -Wno-dev # Don't need to know about cmake warnings that we can't do anything about
  BUILD_BYPRODUCTS ${GFLAGS_LIBRARY}
  )

add_library(gflags INTERFACE)
add_dependencies(gflags gflags_sub)
target_include_directories(gflags INTERFACE ${GFLAGS_PREFIX}/include)
target_link_libraries(gflags INTERFACE ${GFLAGS_PREFIX}/lib/libgflags.a)
