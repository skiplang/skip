if(APPLE)
  # libunwind doesn't work on OSX
  return()
endif()

if(NOT DEFINED LIBUNWIND_FOUND AND NOT FORCE_TP_LIBUNWIND)
  find_path(LIBUNWIND_INCLUDE NAMES libunwind.h)
  find_library(LIBUNWIND_LIB NAMES libunwind.a libunwind.dylib libunwind.so)
  if(LIBUNWIND_INCLUDE AND LIBUNWIND_LIB)
    set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIB} CACHE STRING "" FORCE)
    set(LIBUNWIND_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(LIBUNWIND_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

add_library(libunwind INTERFACE)

if(LIBUNWIND_FOUND AND NOT FORCE_TP_LIBUNWIND)
  message("Using built-in libunwind: ${LIBUNWIND_INCLUDE} ${LIBUNWIND_LIBRARIES}")

else()
  if(DISABLE_TP_LIBUNWIND)
    message(FATAL_ERROR "libunwind not found and third-party disabled")
  endif()

  message("Using third-party libunwind")

  include(ExternalProject)

  set(LIBUNWIND_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)
  set(LIBUNWIND_INCLUDE ${LIBUNWIND_PREFIX}/include CACHE FILEPATH "" FORCE)
  set(LIBUNWIND_LIBRARIES ${LIBUNWIND_PREFIX}/lib/libunwind.a CACHE STRING "" FORCE)

  set(VARS
    CC=${CMAKE_C_COMPILER}
    CXX=${CMAKE_CXX_COMPILER}
    )

  ExternalProject_Add(
    libunwind_sub
    CONFIGURE_COMMAND
    ./autogen.sh &&
    ./configure --prefix=<INSTALL_DIR> --enable-minidebuginfo=no --disable-shared --enable-block-signals=no CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
    ${VARS}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    BUILD_IN_SOURCE 1
    INSTALL_DIR ${LIBUNWIND_PREFIX}
    BUILD_COMMAND make -j ${SYSTEM_NPROC}
    USES_TERMINAL_BUILD 1
    INSTALL_COMMAND make install
    BUILD_BYPRODUCTS ${LIBUNWIND_LIBRARIES}
    )

  add_dependencies(libunwind libunwind_sub)
endif()

target_include_directories(libunwind INTERFACE ${LIBUNWIND_INCLUDE})
target_link_libraries(libunwind INTERFACE ${LIBUNWIND_LIBRARIES})
