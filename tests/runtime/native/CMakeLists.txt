find_package(Clang)

if(NOT PHASE)
  message(FATAL_ERROR "PHASE not set")
endif()

if (GIT_EXISTS)
  execute_process(
    COMMAND git rev-parse HEAD
    OUTPUT_VARIABLE SKIP_VERSION_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ERR
    ERROR_VARIABLE ERROR_VAR
    )
  if(NOT ERR EQUAL 0)
    message(FATAL_ERROR "Error '${ERR}' computing git commit hash with '${CMD}'")
  endif()

  execute_process(
    COMMAND git rev-parse HEAD
    COMMAND git show -s --format=%ci
    OUTPUT_VARIABLE SKIP_VERSION_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ERR
    ERROR_VARIABLE ERROR_VAR
    )
  if(NOT ERR EQUAL 0)
    message(FATAL_ERROR "Error '${ERR}' computing git commit date with '${CMD}'")
  endif()
endif ()

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in"
                "${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wmissing-declarations)

  if(CMAKE_COMPILER_IS_GNUCXX AND
      CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
    add_compile_options(-faligned-new)
  endif()
endif()

set(SKIP_RUNTIME_SOURCES
  src/AllocProfiler.cpp
  src/Arena.cpp
  src/Arena-malloc.cpp
  src/Async.cpp
  src/File.cpp
  src/InternTable.cpp
  src/LockManager.cpp
  src/Obstack.cpp
  src/Reactive.cpp
  src/Refcount.cpp
  src/Math.cpp
  src/Regex.cpp
  src/String.cpp
  src/System.cpp
  src/Task.cpp
  src/Type.cpp
  src/intern.cpp
  src/leak.cpp
  src/memoize.cpp
  src/memory.cpp
  src/objects.cpp
  src/parallel.cpp
  src/Process.cpp
  src/stats.cpp
  src/util.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp
  )

add_library(skip_runtime.${PHASE} ${SKIP_RUNTIME_SOURCES})
target_compile_options(skip_runtime.${PHASE} PRIVATE -Wmissing-declarations)
target_link_libraries(skip_runtime.${PHASE} pthread gtest_main)
target_link_libraries(skip_runtime.${PHASE} ${Boost_LIBRARIES})
target_link_libraries(skip_runtime.${PHASE} icui18n icuuc icudata pcre dl unwind)
set_target_properties(skip_runtime.${PHASE} PROPERTIES OUTPUT_NAME skip_runtime)
if(NOT APPLE)
  target_link_libraries(skip_runtime.${PHASE} rt)
endif()
target_link_libraries(skip_runtime.${PHASE} jemalloc)
target_include_directories(skip_runtime.${PHASE} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIR} ${LIBGLOG_INCLUDE_DIR})

add_custom_target(skip_runtime.${PHASE}.dep DEPENDS skip_runtime.${PHASE})

add_library(rvmi.${PHASE} INTERFACE)

# --- Compute native_cc.pc ---

find_library(DL_LIBRARIES dl)
if(NOT DL_LIBRARIES)
  message(FATAL_ERROR "dl library not found")
endif()

# Prepend <prefix> to each element of ARGN
function(append_flags vars prefix)
  set(_vars ${${vars}})
  foreach(val ${ARGN})
    list(APPEND _vars "${prefix}${val}")
  endforeach()
  set(${vars} ${_vars} PARENT_SCOPE)
endfunction()

# Get the property <prop> on target <target>, prepend <prefix> to each element
function(append_compiler_args vars target prop prefix)
  get_target_property(propval ${target} ${prop})
  if(NOT propval)
    return()
  endif()

  set(_vars ${${vars}})
  append_flags(_vars ${prefix} ${propval})
  set(${vars} ${_vars} PARENT_SCOPE)
endfunction()

# Get the compile properties for the target and append to <cc_vars>
function(append_target_flags cc_vars target)
  set(_cc_vars ${${cc_vars}})

  append_compiler_args(_cc_vars ${target} INTERFACE_COMPILE_DEFINITIONS "-D")
  append_compiler_args(_cc_vars ${target} INTERFACE_INCLUDE_DIRECTORIES "-I")
  append_link_libs(_cc_vars ${target})

  set(${cc_vars} ${_cc_vars} PARENT_SCOPE)
endfunction()

# Get the link properties for the given target and recurse for targets.
function(append_link_libs cc_vars target)
  get_target_property(propval ${target} INTERFACE_LINK_LIBRARIES)
  if(NOT propval)
    return()
  endif()

  set(_cc_vars ${${cc_vars}})
  foreach(val ${propval})
    if(val MATCHES "^-")
      list(APPEND _cc_vars ${val})
    elseif(TARGET ${val})
      append_target_flags(_cc_vars ${val})
    endif()
  endforeach()

  set(${cc_vars} ${_cc_vars} PARENT_SCOPE)
endfunction()

add_library(native_cc.${PHASE} INTERFACE)
target_link_libraries(native_cc.${PHASE} INTERFACE skip_runtime.${PHASE} gtest_main)
add_dependencies(native_cc.${PHASE} skip_runtime.${PHASE} gtest_main)

# Make a dummy executable that we can examine the compilation properties of
add_executable(_dummy_exe.${PHASE} EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/src/sk_standalone.cpp ${CMAKE_SOURCE_DIR}/tests/src/runtime/native/testutil.cpp)
target_link_libraries(_dummy_exe.${PHASE} native_cc.${PHASE})

find_package(LibEvent REQUIRED)

set(cc_args)
append_target_flags(cc_args _dummy_exe.${PHASE})
set(ld_args
  $<TARGET_LINKER_FILE:skip_runtime.${PHASE}>
  $<TARGET_LINKER_FILE:gtest_main>
  ${LIBGLOG_LIBRARY}
  ${GFLAGS_LIBRARY}
  ${DOUBLE_CONVERSION_LIBRARY}
  ${ICU4C_LIBRARIES}
  ${JEMALLOC_LIBRARY}
  ${PCRE_LIBRARY}
  ${Boost_LIBRARIES}
  ${LIBEVENT_LIBRARY}
  ${DL_LIBRARIES}
  -lpthread
)

if(NOT APPLE)
  list(APPEND ld_args -lrt)
  list(APPEND ld_args -llzma)
  list(APPEND ld_args -liberty)
endif()

get_property(prop_defs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)
append_flags(cc_args "-D" ${prop_defs})

get_property(prop_opts TARGET _dummy_exe.${PHASE} PROPERTY COMPILE_OPTIONS)
list(APPEND cc_args ${prop_opts})

# This option is GCC only
list(REMOVE_ITEM cc_args ld_args -mcrc32)

if(DARWIN)
  list(APPEND ld_args -dead_strip)
  list(APPEND cc_args -isysroot ${CMAKE_OSX_SYSROOT})
else()
  list(APPEND ld_args -Wl,--gc-sections)
  list(APPEND cc_args -Wno-vla)
endif()

list(REMOVE_DUPLICATES cc_args)

string(REPLACE "$<COMPILE_LANGUAGE:CXX>" 1 cc_args "${cc_args}")

set(COMPILE_OPTIONS_CFLAGS.${PHASE} ${cc_args} CACHE STRING "" FORCE)

# This replacement is necessary because the output below is done within a quoted
# string so the replacement doesn't happen automatically.
string(REPLACE ";" " " cc_args "${cc_args}")
string(REPLACE ";" " " ld_args "${ld_args}")

file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/native_cc.pc CONTENT
"Name: native_cc
Description: pkg-config for compiling against skip runtime
Version: 1
Cflags: ${cc_args}
Libs: ${ld_args}\n"
)

recursiveGlobSources(EXTC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/include/skip/*-extc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/skip/plugin/skip_plugin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/skip/rvmi/rvmi.h
)

set(PREAMBLE ${CMAKE_CURRENT_BINARY_DIR}/lib/preamble.ll)
add_command_target(
  preamble.${PHASE}
  OUTPUT ${PREAMBLE}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_preamble
  preamble
  --compiler ${CLANG_EXECUTABLE}
    -o ${PREAMBLE}
  $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:--ndebug>
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_preamble ${EXTC_FILES}
)

set(PREAMBLE32 ${CMAKE_CURRENT_BINARY_DIR}/lib/preamble32.ll)
add_command_target(
  preamble32.${PHASE}
  OUTPUT ${PREAMBLE32}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_preamble
  preamble
  --m32
  --compiler ${CLANG_EXECUTABLE}
    -o ${PREAMBLE32}
  $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:--ndebug>
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_preamble ${EXTC_FILES}
)

add_library(
  sk_standalone.${PHASE}
  STATIC
  src/sk_standalone.cpp
)
target_link_libraries(sk_standalone.${PHASE} skip_runtime.${PHASE})

if(NOT APPLE)
  target_link_libraries(sk_standalone.${PHASE} lzma iberty)
endif()
