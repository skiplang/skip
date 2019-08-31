set(RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
set(RUNTIME_TOOLS_PATH ${RUNTIME_PATH}/tools)
set(RUNTIME_NATIVE_PATH ${RUNTIME_PATH}/native)
set(PREAMBLE ${CMAKE_CURRENT_BINARY_DIR}/runtime/native/lib/preamble.ll)

set(PHASE src)

add_subdirectory(runtime)  # Sets PRELUDE_SOURCES.
add_subdirectory(frontend) # Sets FRONTEND_SOURCES.
add_subdirectory(utils)    # Sets UTILS_SOURCES.
add_subdirectory(project)  # Sets PROJECT_SOURCES.
add_subdirectory(native)   # Sets COMPILER_SOURCES.
add_subdirectory(server)
add_subdirectory(tools)
add_subdirectory(nuclide)

set(PHASE)

# ------------------------------------------------------------------------------

# Bootstrap takes the current src/ code and promotes it to lkg/

set(src_ll ${CMAKE_CURRENT_BINARY_DIR}/native/lkg_prep_skip_to_llvm.ll)
set(stage_ll ${CMAKE_CURRENT_BINARY_DIR}/skip_to_llvm.ll)
set(lkg_ll ${LKG_DIR}/skip_to_llvm.ll.bz2)
set(cmd awk '{print >out}; /; -* END OF PREAMBLE -*/{out=\"${stage_ll}\"}' out=/dev/null ${src_ll})
add_command_target(
  stage_skip_to_llvm_ll
  OUTPUT ${stage_ll}
  COMMAND "${cmd}"
  DEPENDS ${src_ll}
)

set(SRC_RUNTIME ${CMAKE_SOURCE_DIR}/src/runtime)
set(LKG_RUNTIME ${CMAKE_SOURCE_DIR}/lkg/runtime)

add_custom_target(build_lkg
  DEPENDS stage_skip_to_llvm_ll
)

add_custom_target(check_lkg
  COMMAND ${TOOLS_DIR}/check_lkg_file ${stage_ll} ${lkg_ll}
  COMMAND ${TOOLS_DIR}/check_lkg_file ${SRC_RUNTIME}/CMakeLists.txt ${LKG_RUNTIME}/CMakeLists.txt
  COMMAND ${TOOLS_DIR}/check_lkg_file ${SRC_RUNTIME}/native ${LKG_RUNTIME}/native
  COMMAND ${TOOLS_DIR}/check_lkg_file ${SRC_RUNTIME}/tools ${LKG_RUNTIME}/tools
  DEPENDS stage_skip_to_llvm_ll
)

add_custom_target(update_lkg_compiler
  COMMAND ${TOOLS_DIR}/check_lkg_file --update ${stage_ll} ${lkg_ll}
  # Using rsync like this copies only if changed
  COMMAND rsync -rc ${SRC_RUNTIME}/CMakeLists.txt ${LKG_RUNTIME}/CMakeLists.txt
  COMMAND rsync -rc --delete ${SRC_RUNTIME}/native/ ${LKG_RUNTIME}/native
  COMMAND rsync -rc --delete ${SRC_RUNTIME}/tools/ ${LKG_RUNTIME}/tools
  DEPENDS stage_skip_to_llvm_ll
  )

add_custom_target(update_lkg
  DEPENDS update_lkg_compiler
  )

add_custom_target(check_pretty_printed
  COMMAND ${TOOLS_DIR}/check_pretty_printed ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pretty_printed.testok
  DEPENDS skip_printer
)
add_dependencies(test check_pretty_printed)

add_custom_target(check_cpp_format
  COMMAND ${CMAKE_SOURCE_DIR}/tools/check_cpp_format ${CMAKE_SOURCE_DIR} ${CLANG_EXECUTABLE}
)
add_dependencies(test check_cpp_format)
