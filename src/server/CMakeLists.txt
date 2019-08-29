# ------------------------------------------------------------------------------
# Reactive mode for Skip

add_skip_compiler_executable(skip_server
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_server
  DEPENDS
  ${COMPILER_SOURCES}
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES}
  )
