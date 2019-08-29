# ------------------------------------------------------------------------------
# Skip Project System
add_skip_compiler_executable(skip_depends
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_depends
  DEPENDS
  ProjectConfig.sk
  SkipProject.sk
  SolutionLoader.sk
  ConfigSerialization.sk
  SkipDepends.sk
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
