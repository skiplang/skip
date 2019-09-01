# ---------------------------------------------------------------------------
# Setup the tests runtime
set(TESTS_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(RUNTIME_PATH ${TESTS_PATH}/runtime)
set(RUNTIME_TOOLS_PATH ${RUNTIME_PATH}/tools)
set(RUNTIME_NATIVE_PATH ${RUNTIME_PATH}/native)
set(RUNTIME_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/runtime)
set(RUNTIME_NATIVE_BIN_PATH ${RUNTIME_BIN_PATH}/native)
set(PREAMBLE ${CMAKE_CURRENT_BINARY_DIR}/runtime/native/lib/preamble.ll)

set(TESTS_PATH ${TESTS_PATH} PARENT_SCOPE)
set(RUNTIME_PATH ${RUNTIME_PATH} PARENT_SCOPE)
set(RUNTIME_TOOLS_PATH ${RUNTIME_TOOLS_PATH} PARENT_SCOPE)
set(RUNTIME_NATIVE_PATH ${RUNTIME_NATIVE_PATH} PARENT_SCOPE)
set(RUNTIME_BIN_PATH ${RUNTIME_BIN_PATH} PARENT_SCOPE)
set(RUNTIME_NATIVE_BIN_PATH ${RUNTIME_NATIVE_BIN_PATH} PARENT_SCOPE)
set(PREAMBLE ${PREAMBLE} PARENT_SCOPE)

set(PHASE tests)
add_subdirectory(runtime)

# ---------------------------------------------------------------------------
# Ensure that the compiler can build itself.
add_skip_compiler_executable_phase(
  skip_to_llvm.tests
  ${CMAKE_SOURCE_DIR}/src/native:skip_to_llvm
  ${SKIP_BIN_PATH}
  ${CMAKE_BINARY_DIR}/${PHASE}/bin
  EXCLUDE_FROM_ALL
)
add_dependencies(test skip_to_llvm.tests)

set(PHASE)

set(BUILD_TOOLS ${BUILD_TOOLS} PARENT_SCOPE)

# ---------------------------------------------------------------------------
# Functions for adding test targets.

# The main target for this directory
add_custom_target(test_compiler)
add_dependencies(test test_compiler)

add_custom_target(test_benchmark)
add_custom_target(test_native)

# Functions in this file which start with an underscore are intended
# to be private to this file.

if(NOT FILTER)
  set(FILTER ".*")
endif()


# Given a file like ${CMAKE_SOURCE_DIR}/path/file.sk sets VAR to
# ${CMAKE_BINARY_DIR}/path/file.${NEWEXT}
function(_skipPathToBinary VAR PATH NEWEXT)
  # Can't just look for CMAKE_SOURCE_DIR because CMAKE_BINARY_DIR is usually a
  # subdir of CMAKE_SOURCE_DIR.
  string(REGEX REPLACE "^(${CMAKE_BINARY_DIR}|${CMAKE_SOURCE_DIR})" "${CMAKE_BINARY_DIR}" res ${PATH})
  if(NEWEXT)
    string(REGEX REPLACE "\\.sk$" ${NEWEXT} res ${res})
  endif()
  set(${VAR} ${res} PARENT_SCOPE)
endfunction()

# Given a file like ${CMAKE_BINARY_DIR}/path/file.sk sets VAR to
# ${CMAKE_SOURCE_DIR}/path/file.${NEWEXT}
function(_skipPathToSource VAR PATH NEWEXT)
  string(REGEX REPLACE "^${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}" res ${PATH})
  if(NEWEXT)
    string(REGEX REPLACE "\\.sk$" ${NEWEXT} res ${res})
  endif()
  set(${VAR} ${res} PARENT_SCOPE)
endfunction()

# Given a base like 'tests.src.frontend.expand.shadow_local' return the
# tree as a list:
#   tests.src.frontend.expand.shadow_local;tests.src.frontend.expand;tests.src.frontend;src
#
function(_computeDepTree varname base)
  set(result)
  while(base MATCHES "[A-Za-z0-9_\\.-]+\\.[A-Za-z0-9_-]+")
    string(REGEX REPLACE "\\.[A-Za-z0-9_-]+$" "" base ${base})
    list(APPEND result ${base})
  endwhile()
  set(${varname} ${result} PARENT_SCOPE)
endfunction()


# For a given test and backend add the test to the deptree for that
# backend.
function(skip_add_to_test_dep_tree backend test_target)
  _computeDepTree(depTargets ${test_target})
  foreach(dep ${depTargets})
    if(NOT TARGET test_${backend}.${dep})
      add_custom_target(test_${backend}.${dep})
    endif()
    add_dependencies(test_${backend}.${dep} test_${backend}.${test_target})
    if(NOT TARGET test_compiler.${dep})
      add_custom_target(test_compiler.${dep})
    endif()
    add_dependencies(test_compiler.${dep} test_${backend}.${test_target})
  endforeach()

  if(${test_target} MATCHES "${FILTER}")
    add_dependencies(test_${backend} test_${backend}.${test_target})
  endif()

endfunction()

add_dependencies(test_compiler test_native)

macro(_addIfExists VAR name)
  if(EXISTS ${name})
    list(APPEND ${VAR} ${name})
  endif()
endmacro()


# Register the test for the given backend
function(_addSkipTargetTest src backend_shortName backend_target exec test_target runner)
  _skipPathToBinary(testok ${src} ".${backend_shortName}.testok")
  _skipPathToBinary(testfail ${src} ".${backend_shortName}.testfail")

  set(ENV
    CC=${CMAKE_CXX_COMPILER}
    CLANG=${CLANG_EXECUTABLE}
    )

  set(PRELUDE ${PRELUDE_SOURCES.tests})

  if(NOT RELATIVE)
    set(RELATIVE ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  string(REGEX REPLACE ".sk$" "" path_we ${src})
  _skipPathToSource(srcPath_we ${path_we} "")

  if(EXISTS ${srcPath_we}.env_${backend_shortName})
    file(READ ${srcPath_we}.env_${backend_shortName} EXTRA_ENV)
    string(STRIP "${EXTRA_ENV}" EXTRA_ENV)
    string(REPLACE " " ";" EXTRA_ENV "${EXTRA_ENV}")
    list(APPEND ENV ${EXTRA_ENV})
  endif()

  set(build_cmd ${ENV} ${runner} ${exec} --relative ${RELATIVE} ${ARGN} ${src} --profile ${CMAKE_BINARY_DIR}/profile/${test_target}.${backend_shortName})

  set(depends ${ARGN} ${src} ${PRELUDE} skip_collect_annotations)
  _addIfExists(depends ${srcPath_we}_testhelper.cpp)
  _addIfExists(depends ${srcPath_we}.exp)
  _addIfExists(depends ${srcPath_we}.expectregex)
  _addIfExists(depends ${srcPath_we}.exp_err)
  _addIfExists(depends ${srcPath_we}.expectregex_err)

  if(backend_shortName STREQUAL native)
    if(NOT BUILD_TOOLS)
      message(FATAL_ERROR "BUILD_TOOLS not set")
    endif()
    list(APPEND depends
      ${BUILD_TOOLS}/${backend_target}
      sk_standalone.tests
      native_cc.tests
      skip_to_llvm
      skip_runtime.tests
      preamble.tests
      )
    list(APPEND build_cmd --sk-standalone $<TARGET_FILE:sk_standalone.tests>)
  else()
    list(APPEND depends ${backend_target} ${SKIP_BIN_PATH}/${backend_target})
  endif()

  add_custom_command(
    OUTPUT ${testok}
    COMMAND rm -f ${testok}
    COMMAND touch ${testfail}
    COMMAND ${build_cmd}
    COMMAND rm ${testfail}
    DEPENDS ${depends}
    )

  mkparents(${testok})
  add_custom_target(test_${backend_shortName}.${test_target} DEPENDS ${testok})

  if(NOT INCLUDE_FAILING AND (EXISTS ${srcPath_we}.failing OR EXISTS ${srcPath_we}.failing_${backend_shortName}))
    return()
  endif()

  skip_add_to_test_dep_tree(${backend_shortName} ${test_target})
endfunction()


# Starting at path look up the tree for 'run_test'
function(_findRunner varName path)
  set(origPath ${path})
  while(TRUE)
    if(EXISTS ${path}/run_test)
      set(${varName} ${path}/run_test PARENT_SCOPE)
      return()
    endif()
    if(path STREQUAL ${CMAKE_SOURCE_DIR})
      message(FATAL_ERROR "Runner not found in ${origPath}")
    endif()
    get_filename_component(path ${path} DIRECTORY)
  endwhile()
endfunction()


# Add a skip test which attempts to compile a single source file (with the prelude).
# If the directory the file is contained has the name 'invalid', then the compile
# is expected to fail. Otherwise it is expected to succeed.
#
# add_skip_test(src
#                 [BACKENDS backend...]
#                 [RUNNER runner]
#                 [LIBS libs...]
#                 [RELATIVE path]
# )
#
function(add_skip_test src)
  set(BACKENDS)
  set(RUNNER)
  set(LIBS)
  set(RELATIVE)
  _parseArgs(BACKENDS RUNNER LIBS RELATIVE ARGN ${ARGN})

  if(NOT BACKENDS)
    set(BACKENDS native)
  endif()

  if(NOT RUNNER)
    _findRunner(RUNNER ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(NOT src MATCHES ".sk$")
    message(FATAL_ERROR "Test ${src} doesn't end with .sk")
  endif()
  string(REGEX REPLACE ".sk$" "" path_we ${src})

  string(REPLACE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} relpath ${path_we})
  file(RELATIVE_PATH relpath ${CMAKE_SOURCE_DIR} ${relpath})
  string(REPLACE "/" "." relpath ${relpath})

  if(NOT src MATCHES "/invalid/")
    # Tests in src/tests/*/invalid/*.sk test compile errors.
    # Since backends can't produce compile errors, theres no need to run them
    # for each backend.

    if(BACKENDS MATCHES native)
      _addSkipTargetTest(${src} native skip_to_native skip_native_exec ${relpath} ${RUNNER} ${LIBS})
    endif()
  endif()
endfunction()

# Ensures that tests/runtime/tools/skip_to_native can be run
add_custom_target(
  skip_to_native
  DEPENDS
    skip_to_llvm
    skip_runtime.tests
    preamble.tests
)

# ---------------------------------------------------------------------------
# Turn skip_to_native into a stand-alone binary for install

set(SKIP_TO_NATIVE ${CMAKE_CURRENT_BINARY_DIR}/runtime/tools/skip_to_native)
mkparents(${SKIP_TO_NATIVE}.tmp/skip_to_native)

add_custom_command(
  OUTPUT ${SKIP_TO_NATIVE}.tmp/skip_to_native.py
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/skip_to_native
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/skip_to_native ${SKIP_TO_NATIVE}.tmp/skip_to_native.py
  )

add_custom_command(
  OUTPUT ${SKIP_TO_NATIVE}.tmp/common.py
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/common.py
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/common.py ${SKIP_TO_NATIVE}.tmp/common.py
  )

add_custom_command(
  OUTPUT ${SKIP_TO_NATIVE}.tmp/skip_to_native.zip
  DEPENDS ${SKIP_TO_NATIVE}.tmp/skip_to_native.py ${SKIP_TO_NATIVE}.tmp/common.py
  COMMAND ${CMAKE_COMMAND} -E tar "cf" skip_to_native.zip --format=zip skip_to_native.py common.py
  WORKING_DIRECTORY ${SKIP_TO_NATIVE}.tmp
  )

add_custom_command(
  OUTPUT ${SKIP_TO_NATIVE}.tmp/skip_to_native.b64
  DEPENDS ${SKIP_TO_NATIVE}.tmp/skip_to_native.zip
  COMMAND base64 ${SKIP_TO_NATIVE}.tmp/skip_to_native.zip > ${SKIP_TO_NATIVE}.tmp/skip_to_native.b64
  )

file(WRITE ${SKIP_TO_NATIVE}.tmp/header.sh
"#!/usr/bin/env bash
set -e
TMP=\$(mktemp -d)
trap \"{ rm -rf \$TMP; }\" EXIT
tail -n +8 \$0 | base64 -d > \$TMP/skip_to_native.zip
PYTHONPATH=\$TMP/skip_to_native.zip python -m skip_to_native --arg0 \"\$0\" \"\$@\"
exit \$? # Can't use exec because then the trap won't fire
")

add_command_target(
  skip_to_native.install
  ALL # install depends on 'all'
  OUTPUT ${SKIP_TO_NATIVE}
  DEPENDS ${SKIP_TO_NATIVE}.tmp/header.sh ${SKIP_TO_NATIVE}.tmp/skip_to_native.b64
  COMMAND cat ${SKIP_TO_NATIVE}.tmp/header.sh ${SKIP_TO_NATIVE}.tmp/skip_to_native.b64 > ${SKIP_TO_NATIVE}
  )

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/runtime/tools/skip_to_native
  DESTINATION bin
  PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  )

# Install runtime/prelude
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/prelude/ DESTINATION lib/skip/prelude)

# Install runtime/native sources
install(FILES ${PREAMBLE} DESTINATION lib/skip)
# The trailing slash means we copy the contents of the directory rather than
# the directory itself.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/include/skip/ DESTINATION include/skip)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/src/ DESTINATION lib/skip/runtime)

# ---------------------------------------------------------------------------

if(APPLE)
  add_custom_target(mac_smoke_test
    DEPENDS
      test_native.tests.src.native.hello_world
      test_native.tests.src.frontend.syntax.lvalue
      test_native.tests.src.runtime.prelude.VectorTest)

    # Add any test with a _testhelper.cpp helper smoke it so we make sure it
    # builds with mac's clang
    file(GLOB testhelpers
      RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/src/native/*_testhelper.cpp)
    foreach(testhelper ${testhelpers})
      string(REPLACE "_testhelper.cpp" "" testhelper ${testhelper})
      string(REPLACE "/" "." testhelper ${testhelper})
      add_dependencies(mac_smoke_test test_native.tests.${testhelper})
    endforeach()
endif()

# ---------------------------------------------------------------------------

add_subdirectory(src)
