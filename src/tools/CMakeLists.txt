# ------------------------------------------------------------------------------
# grammar.txt
# Extracts grammar description from the parser sources.
# Used to ensure that the parser and the language spec stay in sync.

add_skip_compiler_executable(grammar ${CMAKE_CURRENT_SOURCE_DIR}:grammar)

set(PARSER_SK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../frontend/SkipParser.sk)
set(GRAMMAR_TXT ${SKIP_BIN_PATH}/grammar.txt)

add_command_target(
  grammar.dep
  ALL
  OUTPUT ${GRAMMAR_TXT}
  DEPENDS grammar ${PARSER_SK_PATH}
  COMMAND ${SKIP_BIN_PATH}/grammar ${PARSER_SK_PATH} > ${GRAMMAR_TXT}
  )

# ------------------------------------------------------------------------------
# Pretty Printers
add_skip_compiler_executable(skip_to_parsetree ${CMAKE_CURRENT_SOURCE_DIR}:skip_to_parsetree)
add_skip_compiler_executable(skip_to_ast ${CMAKE_CURRENT_SOURCE_DIR}:skip_to_ast)
add_skip_compiler_executable(skip_printer ${CMAKE_CURRENT_SOURCE_DIR}:skip_printer)
add_skip_compiler_executable(skip_codemod ${CMAKE_CURRENT_SOURCE_DIR}:skip_codemod)
add_skip_compiler_executable(skip_search ${CMAKE_CURRENT_SOURCE_DIR}:skip_search)
add_skip_compiler_executable(skip_serialization_generator ${CMAKE_CURRENT_SOURCE_DIR}:skip_serialization_generator)
add_skip_compiler_executable(skip_collect_annotations ${CMAKE_CURRENT_SOURCE_DIR}:skip_collect_annotations)


# ------------------------------------------------------------------------------
# Used to generate API docs
add_skip_compiler_executable(skip_docgen ${CMAKE_CURRENT_SOURCE_DIR}:skip_docgen)

add_custom_target(
  skip_all_pp
  DEPENDS
  skip_docgen
  skip_printer
  )
