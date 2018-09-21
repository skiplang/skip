if(NOT DEFINED FOLLY_FOUND)
  find_path(FOLLY_INCLUDE NAMES folly/folly-config.h)
  find_library(FOLLY_LIBRARY NAMES libfolly.a libfolly.dylib libfolly.so)
  if(FOLLY_INCLUDE AND FOLLY_LIBRARY)
    set(FOLLY_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(FOLLY_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

add_library(folly INTERFACE)

if(FOLLY_FOUND)
  message("Using built-in folly: ${FOLLY_INCLUDE} ${FOLLY_LIBRARY}")

else()
  if(DISABLE_TP_FOLLY)
    message(FATAL_ERROR "folly not found and third-party disabled")
  endif()

  message("Using third-party folly")

  include(ExternalProject)

  set(EXTRA_INTERFACE_LIBS)

  find_program(AUTORECONF autoreconf)
  if(NOT AUTORECONF)
    message(FATAL_ERROR "autoreconf not found")
  endif()
  find_program(LIBTOOL libtool)
  if(NOT LIBTOOL)
    message(FATAL_ERROR "libtool not found")
  endif()

  find_package(Glog REQUIRED)
  find_package(LibEvent REQUIRED)

  set(EXTRA_CMAKE_ARGS)
  set(EXTRA_CMAKE_CACHE_ARGS)

  list(APPEND EXTRA_CMAKE_ARGS
    -DFOLLY_CXX_FLAGS=-Wno-error
    )

  find_package(OpenSSL REQUIRED)

  if(APPLE)
    list(APPEND EXTRA_CMAKE_ARGS
      -DFOLLY_HAVE_PTHREAD_ATFORK=YES
      -DFOLLY_HAVE_WEAK_SYMBOLS=YES
      -DFOLLY_LINK_LIBRARIES=${JEMALLOC_LIBRARY}
      -DFOLLY_USE_JEMALLOC=YES
      )
  endif()

  set(FOLLY_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)
  set(FOLLY_INCLUDE ${FOLLY_PREFIX}/include CACHE FILEPATH "" FORCE)
  set(FOLLY_LIBDIR ${FOLLY_PREFIX}/lib CACHE FILEPATH "" FORCE)
  set(FOLLY_LIBRARY ${FOLLY_LIBDIR}/libfolly.a CACHE FILEPATH "" FORCE)

  set(DEPS gflags double-conversion jemalloc boost)

  if(NOT APPLE)
    list(APPEND DEPS libunwind)
    list(APPEND EXTRA_CMAKE_CACHE_ARGS -DFOLLY_USE_SYMBOLIZER:BOOL=ON)
  endif()

  ExternalProject_Add(
    folly_sub
    DEPENDS ${DEPS}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}
    BUILD_IN_SOURCE 0
    INSTALL_DIR ${FOLLY_PREFIX}
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} folly
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} # Copy over the build type (debug, release)
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> # Install locally
      -Wno-dev # Don't need to know about cmake warnings that we can't do anything about
      -DBoost_INCLUDE_DIR=${Boost_INCLUDE_DIR}
      -DBoost_LIBRARIES=${Boost_LIBRARIES}
      -DLIBGFLAGS_INCLUDE_DIR=${GFLAGS_INCLUDE}
      -DLIBGFLAGS_LIBRARY_DEBUG=${GFLAGS_LIBRARY}
      -DLIBGFLAGS_LIBRARY_RELEASE=${GFLAGS_LIBRARY}
      -DOPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}
      -DOPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}
      -DOPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}
      ${EXTRA_CMAKE_ARGS}
      BUILD_BYPRODUCTS
        ${FOLLY_LIBRARY}
    CMAKE_CACHE_ARGS
      ${EXTRA_CMAKE_CACHE_ARGS}
    )

  add_dependencies(folly folly_sub icudata icutu icuio icui18n icuuc)
endif()

target_include_directories(folly INTERFACE ${FOLLY_INCLUDE} ${LIBEVENT_INCLUDE_DIR} ${LIBGLOG_INCLUDE_DIR})
target_link_libraries(folly INTERFACE
  ${FOLLY_LIBRARY} double-conversion gflags
  ${LIBGLOG_LIBRARY} jemalloc ${CMAKE_DL_LIBS} ${LIBEVENT_LIBRARY}
  boost ${LIBUNWIND_LIBRARIES} ${EXTRA_INTERFACE_LIBS} pthread icudata icutu icuio icui18n icuuc)
add_dependencies(folly folly_sub)
target_compile_options(folly INTERFACE -Wno-vla)
