# ------------------------------------------------------------------------------
# Skip Project System

list(APPEND PROJECT_SOURCES
  ProjectConfig.sk
  SkipAnalyze.sk
  SkipProject.sk
  SolutionLoader.sk
  ConfigSerialization.sk
  skip.project.json
  )

# Make paths absolute.
prependEach(PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/")

set(PROJECT_SOURCES ${PROJECT_SOURCES} PARENT_SCOPE)

add_skip_compiler_executable(skip_depends
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_depends
  DEPENDS
  SkipDepends.sk
  ${PROJECT_SOURCES}
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES}
  )

# ------------------------------------------------------------------------------
# Used to type check sources
add_skip_compiler_executable(skip_test
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_test
  DEPENDS
  skipTest.sk
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES}
  )
