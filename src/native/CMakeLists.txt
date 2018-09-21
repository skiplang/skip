# ------------------------------------------------------------------------------
# Native Backend

add_skip_compiler_executable(skip_to_llvm ${CMAKE_CURRENT_SOURCE_DIR}:skip_to_llvm)
install(TARGETS skip_to_llvm RUNTIME DESTINATION bin)

# Single .ll skip_to_llvm used for update_lkg_compiler
add_skip_compiler_executable(
  lkg_prep_skip_to_llvm
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_to_llvm
  EXCLUDE_FROM_ALL
  LLVM_TEMP_PATH
    ${CMAKE_CURRENT_BINARY_DIR}/lkg_prep_skip_to_llvm.ll
)

add_subdirectory(tests)
