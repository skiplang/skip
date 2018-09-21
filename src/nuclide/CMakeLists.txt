# ------------------------------------------------------------------------------
# Nuclide Support
add_skip_compiler_executable(skip_check ${CMAKE_CURRENT_SOURCE_DIR}:skip_check)
add_skip_compiler_executable(skip_server ${CMAKE_CURRENT_SOURCE_DIR}:skip_server)
add_skip_compiler_executable(skip_lsp ${CMAKE_CURRENT_SOURCE_DIR}:skip_lsp)
add_skip_compiler_executable(skip_check_json ${CMAKE_CURRENT_SOURCE_DIR}:skip_check_json)
add_skip_compiler_executable(skip_outline ${CMAKE_CURRENT_SOURCE_DIR}:skip_outline)
add_skip_compiler_executable(skip_format ${CMAKE_CURRENT_SOURCE_DIR}:skip_format)
add_skip_compiler_executable(skip_get_definition ${CMAKE_CURRENT_SOURCE_DIR}:skip_get_definition)

set(
  NUCLIDE_TARGETS
  skip_check skip_check_json skip_format skip_outline skip_get_definition skip_lsp
)
add_custom_target(nuclide DEPENDS ${NUCLIDE_TARGETS})

# ------------------------------------------------------------------------------
# Install nuclide targets built from src/ into nuclide/.
# Only 'ninja install_nuclide' when you are confident that your src/ dir
# is in a healthy state.
set(NUCLIDE_INSTALL_DIR ${CMAKE_SOURCE_DIR}/nuclide)
add_custom_target(install_nuclide)
foreach(NUCLIDE_TARGET ${NUCLIDE_TARGETS})
  set(INSTALL_TARGET install_${NUCLIDE_TARGET})
  set(INSTALL_FILE ${NUCLIDE_INSTALL_DIR}/${NUCLIDE_TARGET})
  add_command_target(
    ${INSTALL_TARGET}
    OUTPUT ${INSTALL_FILE}
    COMMAND mkdir -p ${NUCLIDE_INSTALL_DIR}
    COMMAND cp ${SKIP_BIN_PATH}/${NUCLIDE_TARGET} ${INSTALL_FILE}
    DEPENDS ${NUCLIDE_TARGET}
  )
  add_dependencies(install_nuclide ${INSTALL_TARGET})
endforeach()
