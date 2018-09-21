if(NOT DEFINED PCRE_FOUND)
  find_path(PCRE_INCLUDE NAMES pcre.h)
  find_library(PCRE_LIBRARY NAMES libpcre.a libpcre.dylib libpcre.so)
  if(PCRE_INCLUDE AND PCRE_LIBRARY)
    set(PCRE_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(PCRE_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

if(PCRE_FOUND)
  message("Using built-in pcre: ${PCRE_INCLUDE} ${PCRE_LIBRARY}")

  add_custom_target(pcre_sub)

  add_library(pcre INTERFACE)
  target_include_directories(pcre INTERFACE ${PCRE_INCLUDE})
  target_link_libraries(pcre INTERFACE ${PCRE_LIBRARY})
  return()
endif()

if(DISABLE_TP_PCRE)
  message(FATAL_ERROR "pcre not found and third-party disabled")
endif()

message("Using third-party pcre")

include(ExternalProject)

set(PCRE_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)

set(PCRE_LIBRARY ${PCRE_PREFIX}/lib/libpcre.a CACHE PATH "" FORCE)

ExternalProject_Add(
  pcre_sub
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
  INSTALL_DIR ${PCRE_PREFIX}
  BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} pcre
  INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} # Copy over the build type (debug, release)
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> # Install locally
    -DPCRE_BUILD_TESTS=OFF # Don't need pcre's tests
    -DPCRE_SHOW_REPORT=OFF # Turn off console spam
    -DPCRE_SUPPORT_UTF=ON # Support 'u' modifier
    -Wno-dev # Don't need to know about cmake warnings that we can't do anything about
  BUILD_BYPRODUCTS ${PCRE_LIBRARY} ${PCRE_PREFIX}/include/pcre.h
)

add_library(pcre INTERFACE)
add_dependencies(pcre pcre_sub)
target_include_directories(pcre INTERFACE ${PCRE_PREFIX}/include)
target_link_libraries(pcre INTERFACE ${PCRE_LIBRARY})
