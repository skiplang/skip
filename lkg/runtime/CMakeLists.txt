if(NOT PHASE)
  message(FATAL_ERROR "PHASE not set")
endif()

set(SKIP_VERSION_HASH "Unavailable")
set(SKIP_VERSION_DATE "Unavailable")

add_subdirectory(prelude)
set(PRELUDE_JS_SOURCES ${PRELUDE_JS_SOURCES} PARENT_SCOPE)
set(PRELUDE_NATIVE_SOURCES ${PRELUDE_NATIVE_SOURCES} PARENT_SCOPE)

set(BUILD_TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/tools PARENT_SCOPE)

add_subdirectory(native)

set(RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(RUNTIME_TOOLS_PATH ${RUNTIME_PATH}/tools)
set(RUNTIME_NATIVE_PATH ${RUNTIME_PATH}/native)
set(PREAMBLE ${CMAKE_CURRENT_BINARY_DIR}/native/lib/preamble.ll)

# Compile an executable from skip source using the RUNTIME_TOOLS_PATH compiler
# (usually the previous phase's compiler).
#
# By default includes the resulting target in ALL.
#
# Also adds a ${name}.js build target from skip source using the Current compiler.
# Does not add the ${name}.js target to ALL.
#
# skip_target is of the form 'directory:target_name'.
# For example: src/tools:skip_printer
#
# add_skip_executable(name skip_target path-to-skip_to_llvm path-for-object-files
#                     [DEPENDS dependencies...]
#                     [EXCLUDE_FROM_ALL]
# )
#
# Variables to control behavior:
#
# PREAMBLE
# SKIP_COMPILE_FLAGS
# SKIP_LINK_FLAGS
#
function(add_skip_executable name skip_target via_backend_path obj_path)
  set(DEPENDS)
  set(LLVM_TEMP_PATH)
  set(EXCLUDE_FROM_ALL)
  _parseArgs(DEPENDS "=EXCLUDE_FROM_ALL" LLVM_TEMP_PATH ARGN ${ARGN})

  if(EXCLUDE_FROM_ALL)
    set(all)
  else()
    set(all ALL)
  endif()

  if(NOT PHASE)
    message(FATAL_ERROR "PHASE not set")
  endif()
  if(NOT RUNTIME_TOOLS_PATH)
    message(FATAL_ERROR "RUNTIME_TOOLS_PATH not set")
  endif()

  if(LLVM_TEMP_PATH)
    set(llvm_temp_arg --llvm-temp ${LLVM_TEMP_PATH})
  else()
    set(llvm_temp_arg)
  endif()

  fetch_depends(SKIP_NATIVE_TARGET_DEPENDS native ${skip_target})

  # Create an executable.
  # TODO: Add build/obj path.
  set(OBJECT ${obj_path}/${name}.o)
  add_custom_command(
    OUTPUT ${OBJECT}
    COMMAND
      ${RUNTIME_TOOLS_PATH}/skip_to_native
      --output ${OBJECT}
      --preamble $<TARGET_FILE:preamble.${PHASE}>
      --via-backend ${via_backend_path}
      ${SKIP_COMPILE_FLAGS}
      ${skip_target}
      ${SKIP_LINK_FLAGS}
      ${llvm_temp_arg}
      --prelude ${CMAKE_SOURCE_DIR}/${PHASE}/runtime/prelude
      --profile ${CMAKE_BINARY_DIR}/profile/${name}
      ${COMPILE_OPTIONS_CFLAGS.${PHASE}}
    BYPRODUCTS ${LLVM_TEMP_PATH}
    DEPENDS
      ${RUNTIME_TOOLS_PATH}/skip_to_native
      ${via_backend_path}/skip_to_llvm
      preamble.${PHASE}
      native_cc.${PHASE}
      ${SKIP_NATIVE_TARGET_DEPENDS}
      ${DEPENDS}
    )

  add_executable(${name}
    ${OBJECT}
    )

  target_link_libraries(${name} sk_standalone.${PHASE} skip_runtime.${PHASE})
  target_include_directories(${name} PRIVATE ${RUNTIME_NATIVE_PATH}/include/skip/rvmi)
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${obj_path})
  set_target_properties(${name} PROPERTIES LLVM_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${name}.ll)

  add_skip_js_executable(
    "${name}.js"
    ${skip_target}
    DEPENDS ${DEPENDS}
    EXCLUDE_FROM_ALL
    )

  add_built_from_js_executable(
    ${name}
    ${skip_target}
    $<TARGET_FILE:preamble.${PHASE}>
  )

endfunction()

# Convert skip source to llvm asm using the JS backend.
# Useful for debugging the compiler as the JS backend can be coaxed into
# printing stack traces with print_stack_trace.
#
# add_built_from_js_executable(name skip_target preamble
#                     [DEPENDS dependencies...]
#                     [EXCLUDE_FROM_ALL]
# )
#
function(add_built_from_js_executable name skip_target preamble)
  set(DEPENDS)
  set(EXCLUDE_FROM_ALL)
  _parseArgs(DEPENDS "=EXCLUDE_FROM_ALL" ARGN ${ARGN})

  if(EXCLUDE_FROM_ALL)
    set(all)
  else()
    set(all ALL)
  endif()

  fetch_depends(SKIP_TARGET_DEPENDS nonnative ${skip_target})

  # TODO: SKIP_NBE_FLAGS
  set(JS_FILE ${SKIP_BIN_PATH}/skip_to_llvm.js)
  set(TARGET ${SKIP_BIN_PATH}/${name}__js.llvm)
  add_custom_command(
    OUTPUT ${TARGET}
    DEPENDS
      ${JS_FILE}
      ${SKIP_TARGET_DEPENDS}
      ${DEPENDS}
    COMMAND
      NODE=${NODE}
      EXTRA_OLD_SPACE=1
      ${TOOLS_DIR}/run_js_file
      --no-unhandled-exception-stack
      ${JS_FILE}
      --preamble ${preamble}
      --export-function-as
      main=skip_main
      --nogoto
      ${skip_target}
      > ${TARGET}
    )
  add_custom_target(
    js__${name}
    DEPENDS ${TARGET}
  )

endfunction()


# ---------------------------------------------------------------------------
# Compile an executable from skip source.
# By default includes the resulting target in ALL.
#
# Also adds a ${name}.js build target from skip source using the Current compiler.
# Does not add the ${name}.js target to ALL.
#
# skip_target is of the form 'directory:target_name'.
# For example: src/tools:skip_printer
#
# add_skip_compiler_executable_phase(
#                     name skip_target path-to-skip_to_llvm path-for-object-files
#                     [LLVM_TEMP_PATH path]_
#                     [DEPENDS dependencies...]
#                     [EXCLUDE_FROM_ALL]
# )
#
# Variables to control behavior:
#
# PREAMBLE
# SKIP_COMPILE_FLAGS
# SKIP_LINK_FLAGS
#
function(add_skip_compiler_executable_phase name skip_target via_backend_path obj_path)
  set(DEPENDS)
  set(LLVM_TEMP_PATH)
  set(EXCLUDE_FROM_ALL)
  _parseArgs(DEPENDS "=EXCLUDE_FROM_ALL" LLVM_TEMP_PATH ARGN ${ARGN})

  if(EXCLUDE_FROM_ALL)
    set(EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
  else()
    set(EXCLUDE_FROM_ALL)
  endif()

  add_skip_executable(
    ${name}
    ${skip_target}
    ${via_backend_path}
    ${obj_path}
    LLVM_TEMP_PATH
      ${LLVM_TEMP_PATH}
    DEPENDS
      ${DEPENDS}
    ${EXCLUDE_FROM_ALL}
  )
endfunction()


# ---------------------------------------------------------------------------
# Compile an executable from skip source.
# By default includes the resulting target in ALL.
#
# Also adds a ${name}.js build target from skip source using the Current compiler.
# Does not add the ${name}.js target to ALL.
#
# Do not include prelude to SOURCES; prelude files are added automatically.
#
# skip_target is of the form 'directory:target_name'.
# For example: src/tools:skip_printer
#
# add_skip_compiler_executable(
#                     name
#                     skip_target
#                     [LLVM_TEMP_PATH path]_
#                     [DEPENDS dependencies...]
#                     [EXCLUDE_FROM_ALL]
# )
#
# Variables to control behavior:
#
# PREAMBLE
# SKIP_COMPILE_FLAGS
# SKIP_LINK_FLAGS
#
# NOTE: This function is NOT PHASE aware. It only works in the src/ PHASE.
function(add_skip_compiler_executable name skip_target)
  set(DEPENDS)
  set(LLVM_TEMP_PATH)
  set(EXCLUDE_FROM_ALL)
  _parseArgs(DEPENDS "=EXCLUDE_FROM_ALL" LLVM_TEMP_PATH ARGN ${ARGN})

  if(EXCLUDE_FROM_ALL)
    set(EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
  else()
    set(EXCLUDE_FROM_ALL)
  endif()

  add_skip_compiler_executable_phase(
    ${name}
    ${skip_target}
    $<TARGET_PROPERTY:skip_to_llvm.lkg,RUNTIME_OUTPUT_DIRECTORY>
    ${SKIP_BIN_PATH}
    LLVM_TEMP_PATH
      ${LLVM_TEMP_PATH}
    DEPENDS
      ${DEPENDS}
    ${EXCLUDE_FROM_ALL}
  )
endfunction()

# ---------------------------------------------------------------------------
# Compile skip to JS using the Current compiler.
#
# add_skip_js_executable(name skip_target
#                        [DEPENDS dependencies...]
#                        [EXCLUDE_FROM_ALL]
# )
#
function(add_skip_js_executable name skip_target)
  set(EXCLUDE_FROM_ALL)
  set(DEPENDS)
  _parseArgs(SOURCES DEPENDS "=EXCLUDE_FROM_ALL" ARGN ${ARGN})

  if(EXCLUDE_FROM_ALL)
    set(all)
  else()
    set(all ALL)
  endif()

  set(TARGET ${SKIP_BIN_PATH}/${name})

  fetch_depends(SKIP_TARGET_DEPENDS nonnative ${skip_target})

  add_command_target(
    ${name}
    ${all}
    OUTPUT ${TARGET}
    COMMAND ${SKIP_BIN_PATH}/skip_to_js --output ${TARGET} ${skip_target}
    DEPENDS skip_to_js ${SKIP_BIN_PATH}/skip_to_js ${SKIP_TARGET_DEPENDS} ${DEPENDS}
    )
endfunction()

# ---------------------------------------------------------------------------
# Compile skip to JS using Current JS Transpiler
#
# Do not include prelude to SOURCES; prelude files are added automatically.
#
# add_skip_js_js_executable(name skip_target
#                           [DEPENDS dependencies...]
#                           [EXCLUDE_FROM_ALL]
# )
#
function(add_skip_js_js_executable name skip_target)
  set(EXCLUDE_FROM_ALL)
  _parseArgs("=EXCLUDE_FROM_ALL" ARGN ${ARGN})

  if(EXCLUDE_FROM_ALL)
    set(all)
  else()
    set(all ALL)
  endif()

  set(TARGET ${SKIP_BIN_PATH}/${name})

  fetch_depends(SKIP_TARGET_DEPENDS nonnative ${skip_target})

  add_command_target(
    ${name}
    ${all}
    OUTPUT ${TARGET}
    DEPENDS
      skip_to_js_js
      ${SKIP_BIN_PATH}/skip_to_js.js
      ${SKIP_TARGET_DEPENDS}
    COMMAND
      NODE=${NODE}
      EXTRA_OLD_SPACE=1
      ${TOOLS_DIR}/run_js_file
      --no-unhandled-exception-stack
      ${SKIP_BIN_PATH}/skip_to_js.js
      --output ${TARGET}
      ${skip_target}
    )
endfunction()
