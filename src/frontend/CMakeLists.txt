# ------------------------------------------------------------------------------
# Front End Sources

list(APPEND FRONTEND_SOURCES
  IMap.sk
  ISet.sk
  Parameters.sk
  SkipLexer.sk
  SkipParseTree.sk
  SkipParser.sk
  TokenIterator.sk
  UMap.sk
  bufferedPrinter.sk
  convertTree.sk
  keywords.sk
  parseTreeGenerated.sk
  predefinedName.sk
  sharedTuples.sk
  skipArguments.sk
  skipAst.sk
  skipAstPp.sk
  skipAstUtils.sk
  skipCheckVariance.sk
  skipComment.sk
  skipDidYouMean.sk
  skipExhaustivePatterns.sk
  skipExpand.sk
  skipInherit.sk
  skipMain.sk
  skipNamedAst.sk
  skipNamedAstPp.sk
  skipNaming.sk
  skipParse.sk
  skipSerGenPp.sk
  skipToken.sk
  skipTypedAst.sk
  skipTypedAstExpand.sk
  skipTypedAstPp.sk
  skipTypedAstRename.sk
  skipTypedAstUtils.sk
  skipTypes.sk
  skipTyping.sk
  skipTypingUtils.sk
  skipUtils.sk
  tokenKind.sk
  )

# Make paths absolute.
prependEach(FRONTEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/")
set(FRONTEND_SOURCES ${FRONTEND_SOURCES} PARENT_SCOPE)

# parseTreeGenerated.sk is generated from JSON.
set(PARSE_TREE_SK ${CMAKE_CURRENT_SOURCE_DIR}/parseTreeGenerated.sk)
set(BIN_PARSE_TREE_SK ${CMAKE_CURRENT_BINARY_DIR}/parseTreeGenerated.sk)

add_custom_command(
  OUTPUT ${BIN_PARSE_TREE_SK}
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/trees.json
    ${SKIP_TOOLS_PATH}/generate-parse-trees.js
    skip_printer
  COMMAND ${NODE} ${SKIP_TOOLS_PATH}/generate-parse-trees.js ${CMAKE_CURRENT_SOURCE_DIR}/trees.json > ${BIN_PARSE_TREE_SK}
  COMMAND ${SKIP_BIN_PATH}/skip_printer --write ${BIN_PARSE_TREE_SK}
)

add_custom_target(
  update_parse_tree
  DEPENDS ${BIN_PARSE_TREE_SK}
  COMMAND cp ${BIN_PARSE_TREE_SK} ${PARSE_TREE_SK}
)

add_custom_target(
  check_parse_tree
  DEPENDS ${BIN_PARSE_TREE_SK}
  COMMAND diff ${BIN_PARSE_TREE_SK} ${PARSE_TREE_SK} || (echo '${PARSE_TREE_SK} is out of date. Run \'ninja -C build update_parse_tree\' to update it.' && exit 1)
)

add_dependencies(test check_parse_tree)
