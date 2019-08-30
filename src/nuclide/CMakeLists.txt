# ------------------------------------------------------------------------------
# Nuclide Support

add_skip_compiler_executable(skip_check
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_check
  DEPENDS
  skipCheck.sk
  ../project/SkipAnalyze.sk
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES.${PHASE}}
  )

add_skip_compiler_executable(skip_lsp
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_lsp
  DEPENDS
  getDefinition.sk
  convert.sk
  logger.sk
  OpenFiles.sk
  LspServer.sk
  skipLsp.sk
  Jsonable.sk
  jsonRpc.sk
  languageServerProtocol.sk
  lspOutline.sk
  RingBuffer.sk
  native/FastOption.sk
  ../tools/printer/ParseTreeFactory.sk
  ../tools/printer/printer.sk
  ${PROJECT_SOURCES}
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES.${PHASE}}
)

add_skip_compiler_executable(skip_check_json
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_check_json
  DEPENDS
  skipCheckJson.sk
  ../project/SkipAnalyze.sk
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES.${PHASE}}
)

add_skip_compiler_executable(skip_outline
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_outline
  DEPENDS
  outline.sk
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES.${PHASE}}
)

add_skip_compiler_executable(skip_format
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_format
  DEPENDS
  skipFormat.sk
  ../tools/printer/ParseTreeFactory.sk
  ../tools/printer/printer.sk
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES.${PHASE}}
)

add_skip_compiler_executable(skip_get_definition
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_get_definition
  DEPENDS
  getDefinition.sk
  skipGetDefinition.sk
  ../project/SkipAnalyze.sk
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES.${PHASE}}
)

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
