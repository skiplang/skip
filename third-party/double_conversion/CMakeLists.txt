if(NOT DEFINED DOUBLE_CONVERSION_FOUND)
  find_path(DOUBLE_CONVERSION_INCLUDE NAMES double-conversion/double-conversion.h)
  find_library(DOUBLE_CONVERSION_LIBRARY NAMES libdouble-conversion.a libdouble-conversion.dylib libdouble-conversion.so)
  if(DOUBLE_CONVERSION_INCLUDE AND DOUBLE_CONVERSION_LIBRARY)
    get_filename_component(DOUBLE_CONVERSION_LIBDIR ${DOUBLE_CONVERSION_LIBRARY} DIRECTORY CACHE)
    set(DOUBLE_CONVERSION_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(DOUBLE_CONVERSION_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

add_library(double-conversion INTERFACE)

if(DOUBLE_CONVERSION_FOUND)
  message("Using built-in double-conversion: ${DOUBLE_CONVERSION_INCLUDE} ${DOUBLE_CONVERSION_LIBRARY}")
else()
  if(DISABLE_TP_DOUBLE_CONVERSION)
    message(FATAL_ERROR "double-conversion not found and third-party disabled")
  endif()

  message("Using third-party double-conversion")

  include(ExternalProject)

  set(DOUBLE_CONVERSION_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)
  set(DOUBLE_CONVERSION_INCLUDE ${DOUBLE_CONVERSION_PREFIX}/include CACHE FILEPATH "" FORCE)
  set(DOUBLE_CONVERSION_LIBDIR ${DOUBLE_CONVERSION_PREFIX}/lib CACHE FILEPATH "" FORCE)
  set(DOUBLE_CONVERSION_LIBRARY ${DOUBLE_CONVERSION_LIBDIR}/libdouble-conversion.a CACHE FILEPATH "" FORCE)

  ExternalProject_Add(
    double-conversion_sub
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    INSTALL_DIR ${DOUBLE_CONVERSION_PREFIX}
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
    USES_TERMINAL_BUILD 1
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} # Copy over the build type (debug, release)
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> # Install locally
      -DCMAKE_CXX_FLAGS=-fPIC
      -Wno-dev # Don't need to know about cmake warnings that we can't do anything about
    BUILD_BYPRODUCTS ${DOUBLE_CONVERSION_LIBRARY}
    )

  add_dependencies(double-conversion double-conversion_sub)
endif()

target_include_directories(double-conversion INTERFACE ${DOUBLE_CONVERSION_INCLUDE})
target_link_libraries(double-conversion INTERFACE ${DOUBLE_CONVERSION_LIBRARY})
