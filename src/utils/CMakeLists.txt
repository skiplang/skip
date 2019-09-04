# ------------------------------------------------------------------------------
# Utils Sources

list(APPEND UTILS_SOURCES
  Assert.sk
  Comment.sk
  FilePosition.sk
  FileRange.sk
  Lexer.sk
  ParseTree.sk
  Parser.sk
  SMap.sk
  SSet.sk
  Token.sk
  TokenKind.sk
  annotations.sk
  arguments.sk
  skipError.sk
  syntaxError.sk
  skip.project.json
  )

# Make paths absolute.
prependEach(UTILS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/")

set(UTILS_SOURCES ${UTILS_SOURCES} PARENT_SCOPE)
