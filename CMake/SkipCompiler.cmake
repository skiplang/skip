set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FREEBSD FALSE)
set(LINUX FALSE)
set(DARWIN FALSE)
set(WINDOWS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(FREEBSD TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LINUX TRUE)
  execute_process(COMMAND nproc OUTPUT_VARIABLE SYSTEM_NPROC OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(DARWIN TRUE)
  execute_process(COMMAND sysctl -n hw.ncpu OUTPUT_VARIABLE SYSTEM_NPROC OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(WINDOWS TRUE)
else()
  message(FATAL_ERROR "Unknown system type")
endif()

if(NOT SYSTEM_NPROC)
  set(SYSTEM_NPROC 1)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
    message(FATAL_ERROR "Insufficient GCC version: 5.0 required but you have ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0")
    message(FATAL_ERROR "Insufficient AppleClang version: 8.0 required but you have ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
    message(FATAL_ERROR "Insufficient Clang version: 5.0 required but you have ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
endif()


function(add_cxx_compile_options)
  foreach(opt ${ARGN})
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${opt}>)
  endforeach()
endfunction()

# using Clang or GCC
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

  add_compile_options(
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-DNDEBUG>
    -msse4.2
    -Wno-sign-compare
    )

  add_cxx_compile_options(
    -std=c++17
    )

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    add_compile_options(
      -stdlib=libc++
      -Wshadow-all
      -Wno-return-type-c-linkage
      )

  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")

    add_compile_options(
      -stdlib=libc++
      -Wno-nullability-completeness
      -Wshadow-all
      -Wno-return-type-c-linkage
      )

  else() # using GCC
    add_compile_options(
      -Wvla
      -fdata-sections
      -ffunction-sections
      -mcrc32
      # disabled pending folly update
      # -Wshadow
      )

    if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.1)
      add_compile_options(-DFOLLY_HAVE_MALLOC_H)
    endif()

    if(STATIC_CXX_LIB)
      link_libraries(-static-libgcc -static-libstdc++)
    endif()
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR
      CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
      CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_compile_options(
      -O3
      -fno-omit-frame-pointer
      -mno-omit-leaf-frame-pointer
      )
  else()
    add_compile_options(
      $<$<CONFIG:Debug>:-O0>
      $<$<CONFIG:Release>:-O3>
      -fno-omit-frame-pointer
      )
  endif()

else()
  message(WARNING "Warning: unknown/unsupported compiler, things may go wrong")
endif()
