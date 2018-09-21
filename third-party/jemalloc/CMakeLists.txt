if(NOT DEFINED JEMALLOC_FOUND)
  find_path(JEMALLOC_INCLUDE jemalloc/jemalloc.h)
  find_library(JEMALLOC_LIBRARY jemalloc)

  if (JEMALLOC_INCLUDE AND JEMALLOC_LIBRARY)
    set(JEMALLOC_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(JEMALLOC_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

if(JEMALLOC_FOUND)
  message("Using built-in jemalloc: ${JEMALLOC_INCLUDE} ${JEMALLOC_LIBRARY}")

  # We have a locally installed one
  add_library(jemalloc INTERFACE)
  target_include_directories(jemalloc INTERFACE ${JEMALLOC_INCLUDE})
  target_link_libraries(jemalloc INTERFACE ${JEMALLOC_LIBRARY})

  return()
endif()

if(DISABLE_TP_JEMALLOC)
  message(FATAL_ERROR "jemalloc not found and third-party disabled")
endif()

include(ExternalProject)

message("Building third-party jemalloc")

find_program(AUTOCONF autoconf)
if (NOT AUTOCONF)
  message(FATAL_ERROR "autoconf not found")
endif()

set(JEMALLOC_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)
ExternalProject_Add(
  jemalloc_sub
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
  CONFIGURE_COMMAND ${AUTOCONF}
  COMMAND ./configure CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} --prefix=${JEMALLOC_PREFIX} --with-mangling --disable-shared
  BUILD_COMMAND make -j ${SYSTEM_NPROC} build_lib_static
  BUILD_IN_SOURCE 1
  INSTALL_DIR ${JEMALLOC_PREFIX}
  INSTALL_COMMAND make -j ${SYSTEM_NPROC} install_bin install_include install_lib
  BUILD_BYPRODUCTS ${JEMALLOC_PREFIX}/lib/libjemalloc_pic.a
  )

set(JEMALLOC_INCLUDE ${JEMALLOC_PREFIX}/include CACHE FILEPATH "" FORCE)
set(JEMALLOC_LIBRARY ${JEMALLOC_PREFIX}/lib/libjemalloc_pic.a CACHE FILEPATH "" FORCE)

add_library(jemalloc INTERFACE)
add_dependencies(jemalloc jemalloc_sub)
target_compile_definitions(jemalloc INTERFACE -DPIC)
target_include_directories(jemalloc INTERFACE ${JEMALLOC_INCLUDE})
target_link_libraries(jemalloc INTERFACE ${JEMALLOC_LIBRARY} pthread)
